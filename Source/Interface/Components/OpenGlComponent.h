/*
	==============================================================================

		OpenGlComponent.h
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/constexpr_utils.h"
#include "../LookAndFeel/BaseComponent.h"
#include "../LookAndFeel/Shaders.h"
#include "../LookAndFeel/Skin.h"

namespace Interface
{
	using namespace juce;

	class OpenGlContainer;

	// NoWork - skip rendering on component
	// Dirty - render and update flag to NoWork
	// Realtime - render every frame
	enum class RenderFlag : u32 { NoWork = 0, Dirty = 1, Realtime = 2 };

	class Animator
	{
	public:
		enum EventType { Hover, Click };

		void tick(bool isAnimating = true) noexcept;
		float getValue(EventType type, float min = 0.0f, float max = 1.0f) const noexcept;

		void setHoverIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); utils::ScopedSpinLock g(guard_); hoverIncrement_ = increment; }

		void setClickIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); utils::ScopedSpinLock g(guard_); clickIncrement_ = increment; }

		void setIsHovered(bool isHovered) noexcept { utils::ScopedSpinLock g(guard_); isHovered_ = isHovered; }
		void setIsClicked(bool isClicked) noexcept { utils::ScopedSpinLock g(guard_); isClicked_ = isClicked; }

	private:
		mutable std::atomic<bool> guard_ = false;

		float hoverValue_ = 0.0f;
		float clickValue_ = 0.0f;

		float hoverIncrement_ = 1.0f;
		float clickIncrement_ = 1.0f;

		bool isHovered_ = false;
		bool isClicked_ = false;
	};


	class OpenGlComponent : public BaseComponent
	{
	public:
		static bool setViewPort(const BaseComponent *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);
		static bool setViewPort(const BaseComponent *component, const OpenGlWrapper &openGl);
		static void setScissor(const BaseComponent *component, const OpenGlWrapper &openGl);
		static void setScissorBounds(const BaseComponent *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);

		static std::unique_ptr<OpenGLShaderProgram::Uniform> getUniform(const OpenGLShaderProgram &program, const char *name)
		{
			if (gl::glGetUniformLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Uniform>(program, name);
			return nullptr;
		}

		static std::unique_ptr<OpenGLShaderProgram::Attribute> getAttribute(const OpenGLShaderProgram &program, const char *name)
		{
			if (gl::glGetAttribLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Attribute>(program, name);
			return nullptr;
		}

		static String translateFragmentShader(const String &code);
		static String translateVertexShader(const String &code);

		OpenGlComponent(String name = typeid(OpenGlComponent).name());
		~OpenGlComponent() override;

		void setBounds(int x, int y, int width, int height) final { BaseComponent::setBounds(x, y, width, height); }
		using BaseComponent::setBounds;
		void paint(Graphics &) override { }

		virtual void init(OpenGlWrapper &openGl) = 0;
		virtual void render(OpenGlWrapper &openGl, bool animate) = 0;
		virtual void destroy() = 0;

		Animator &getAnimator() noexcept { return animator_; }
		RenderFlag getRefreshFrequency() const noexcept { return renderFlag_; }
		float getValue(Skin::ValueId valueId) const;
		Colour getColour(Skin::ColorId colourId) const;

		void setRefreshFrequency(RenderFlag frequency) noexcept { renderFlag_ = frequency; }
		void setRenderFunction(std::function<void(OpenGlWrapper &, bool)> function) noexcept
		{ renderFunction_ = std::move(function); }
		void setContainer(OpenGlContainer *container) noexcept { container_ = container; }

	private:
		void pushForDeletion();

	protected:
		bool setViewPort(const OpenGlWrapper &openGl) const;

		Animator animator_{};
		shared_value<std::function<void(OpenGlWrapper &, bool)>> renderFunction_{};
		shared_value<RenderFlag> renderFlag_ = RenderFlag::Dirty;
		std::atomic<std::thread::id> waitLock_ = {};

		shared_value<OpenGlContainer *> container_ = nullptr;

		template<typename T>
		friend class gl_ptr;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
	};

	template<typename T>
	class gl_ptr
	{
	public:
		gl_ptr() noexcept = default;
		gl_ptr(nullptr_t) noexcept { }

		explicit gl_ptr(T *component) requires CommonConcepts::DerivedOrIs<T, OpenGlComponent> : 
			component_(component), controlBlock_(new control_block{}) { }

		gl_ptr(const gl_ptr &other) noexcept : component_(other.component_), 
			controlBlock_(other.controlBlock_)
		{
			if (controlBlock_)
				controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
		}
		gl_ptr &operator=(const gl_ptr &other) noexcept
		{
			if (this != &other)
				gl_ptr{ other }.swap(*this);
			return *this;
		}

		gl_ptr(gl_ptr &&other) noexcept : component_(other.component_), 
			controlBlock_(other.controlBlock_)
		{
			other.component_ = nullptr;
			other.controlBlock_ = nullptr;
		}
		gl_ptr &operator=(gl_ptr &&other) noexcept
		{
			gl_ptr{ std::move(other) }.swap(*this);
			return *this;
		}

		// aliasing constructors through upcasting
		template<CommonConcepts::DerivedOrIs<T> U>
		gl_ptr(const gl_ptr<U> &other) noexcept : component_(other.component_),
			controlBlock_(reinterpret_cast<control_block *>(other.controlBlock_))
		{
			if (controlBlock_)
				controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
		}

		template<CommonConcepts::DerivedOrIs<T> U>
		gl_ptr(gl_ptr<U> &&other) noexcept : component_(other.component_),
			controlBlock_(reinterpret_cast<control_block *>(other.controlBlock_))
		{
			other.component_ = nullptr;
			other.controlBlock_ = nullptr;
		}

		~gl_ptr()
		{
			if (!controlBlock_)
				return;

			if (controlBlock_->refCount.fetch_sub(1, std::memory_order_acq_rel) > 1)
				return;

			COMPLEX_ASSERT(component_ && "How do we have a control block to a non-existing object??");
			COMPLEX_ASSERT(controlBlock_->refCount.load() == 0 && "We have somehow underflowed");

			// we're already synchronised through refCount.fetch_sub, so we only care about atomicity here
			if (!controlBlock_->isInitialised.load(std::memory_order_relaxed))
				delete component_;
			else
				component_->pushForDeletion();

			delete controlBlock_;
		}

		// explicit upcasting
		template<typename U>
		gl_ptr<U> upcast() noexcept requires CommonConcepts::DerivedOrIs<T, U>
		{
			using control_block_u = typename gl_ptr<U>::control_block;

			controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);

			gl_ptr<U> obj{};
			obj.component_ = component_;
			obj.controlBlock_ = reinterpret_cast<control_block_u *>(controlBlock_);
			return obj;
		}

		void doWorkOnComponent(OpenGlWrapper &openGl, bool animate = false)
		{
			if (!controlBlock_->isInitialised.load(std::memory_order_acquire))
			{
				controlBlock_->isInitialised.store(true, std::memory_order_release);
				component_->init(openGl);
			}

			auto customRenderFunction = component_->renderFunction_.get();
			if (customRenderFunction)
				customRenderFunction(openGl, animate);
			else
				component_->render(openGl, animate);
		}

		void deinitialise()
		{
			if (controlBlock_->isInitialised.load(std::memory_order_acquire))
			{
				controlBlock_->isInitialised.store(false, std::memory_order_release);
				component_->destroy();
			}
		}

		T *get() const noexcept { return component_; }
		T *operator->() const noexcept { return get(); }
		T &operator*() const noexcept { return *get(); }

		void swap(gl_ptr &other) noexcept
		{
			std::swap(component_, other.component_);
			std::swap(controlBlock_, other.controlBlock_);
		}

		explicit operator bool() const noexcept { return get() != nullptr; }

	private:
		struct control_block
		{
			std::atomic<size_t> refCount = 1;
			std::atomic<bool> isInitialised = false;
		};

		T *component_ = nullptr;
		control_block *controlBlock_ = nullptr;

		template <typename U>
		friend class gl_ptr;
	};

	template <typename T, typename U>
	inline bool operator==(const gl_ptr<T> &left, const gl_ptr<U> &right) noexcept
	{ return left.get() == right.get(); }

	// TODO: maybe optimise it to allocate space for both the object and control block at once

	template<std::derived_from<OpenGlComponent> T, typename ... Args>
	inline gl_ptr<T> makeOpenGlComponent(Args&& ... args) { return gl_ptr<T>{ new T(std::forward<Args>(args)...) }; }
}
