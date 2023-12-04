/*
	==============================================================================

		OpenGlComponent.h
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Third Party/clog/small_function.hpp"
#include "Framework/constexpr_utils.h"
#include "../LookAndFeel/Shaders.h"
#include "../LookAndFeel/Skin.h"

namespace Interface
{
	class BaseSection;
	class OpenGlCorners;

	// Event - on manual calling
	// Resized - on component resizing
	// Realtime - every frame
	enum class RefreshFrequency : u32
	{
		Event = 1,
		Resized = 2,
		Realtime = 4,

		ResizedAndEvent = Event | Resized,
		RealtimeAndEvent = Event | Realtime,
		RealtimeAndResized = Resized | Realtime,

		All = Event | Resized | Realtime
	};

	class Animator
	{
	public:
		enum EventType { Hover, Click };

		void tick(bool isAnimating = true) noexcept
		{
			if (isAnimating)
			{
				if (isHovered_)
					hoverValue_ = std::min(1.0f, hoverValue_ + hoverIncrement_);
				else
					hoverValue_ = std::max(0.0f, hoverValue_ - hoverIncrement_);

				if (isClicked_)
					clickValue_ = std::min(1.0f, clickValue_ + clickIncrement_);
				else
					clickValue_ = std::max(0.0f, clickValue_ - clickIncrement_);
			}
			else
			{
				hoverValue_ = isHovered_;
				clickValue_ = isClicked_;
			}
		}

		float getValue(EventType type, float min = 0.0f, float max = 1.0f) const noexcept
		{
			float value;
			switch (type)
			{
			case Hover:
				value = hoverValue_;
				break;
			case Click:
				value = clickValue_;
				break;
			default:
				value = 1.0f;
				break;
			}

			value *= max - min;
			return value + min;
		}

		void setHoverIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); hoverIncrement_ = increment; }

		void setClickIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); clickIncrement_ = increment; }

		void setIsHovered(bool isHovered) noexcept { isHovered_ = isHovered; }
		void setIsClicked(bool isClicked) noexcept { isClicked_ = isClicked; }

	private:
		float hoverValue_ = 0.0f;
		float clickValue_ = 0.0f;

		float hoverIncrement_ = 1.0f;
		float clickIncrement_ = 1.0f;

		bool isHovered_ = false;
		bool isClicked_ = false;
	};


	class OpenGlComponent : public Component
	{
	public:
		static bool setViewPort(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);
		static bool setViewPort(const Component *component, const OpenGlWrapper &openGl);
		static void setScissor(const Component *component, const OpenGlWrapper &openGl);
		static void setScissorBounds(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);

		static std::unique_ptr<OpenGLShaderProgram::Uniform> getUniform(const OpenGLShaderProgram &program, const char *name)
		{
			if (juce::gl::glGetUniformLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Uniform>(program, name);
			return nullptr;
		}

		static std::unique_ptr<OpenGLShaderProgram::Attribute> getAttribute(const OpenGLShaderProgram &program, const char *name)
		{
			if (juce::gl::glGetAttribLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Attribute>(program, name);
			return nullptr;
		}

		static String translateFragmentShader(const String &code)
		{
		#if OPENGL_ES
			return String("#version 300 es\n") + "out mediump vec4 fragColor;\n" +
				code.replace("varying", "in").replace("texture2D", "texture").replace("gl_FragColor", "fragColor");
		#else
			return OpenGLHelpers::translateFragmentShaderToV3(code);
		#endif
		}

		static String translateVertexShader(const String &code)
		{
		#if OPENGL_ES
			return String("#version 300 es\n") + code.replace("attribute", "in").replace("varying", "out");
		#else
			return OpenGLHelpers::translateVertexShaderToV3(code);
		#endif
		}

		OpenGlComponent(String name = typeid(OpenGlComponent).name());
		~OpenGlComponent() override;

		void resized() override;
		void paint(Graphics &) override { }
		virtual void paintBackground(Graphics &g);
		virtual void refresh([[maybe_unused]] RefreshFrequency refreshFrequency) { }

		virtual void init(OpenGlWrapper &openGl);
		virtual void render(OpenGlWrapper &openGl, bool animate) = 0;
		virtual void destroy();

		void renderCorners(OpenGlWrapper &openGl, bool animate, Colour color, float rounding);
		void renderCorners(OpenGlWrapper &openGl, bool animate);
		void addRoundedCorners();
		void addBottomRoundedCorners();
		void repaintBackground();

		Colour getBodyColor() const noexcept { return bodyColor_; }
		Animator &getAnimator() noexcept { return animator_; }
		RefreshFrequency getRefreshFrequency() const noexcept { return refreshFrequency_; }
		float getValue(Skin::ValueId valueId) const;
		Colour getColour(Skin::ColorId colourId) const;

		void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
		void setBackgroundColor(const Colour &color) noexcept { backgroundColor_ = color; }
		void setRefreshFrequency(RefreshFrequency frequency) noexcept { refreshFrequency_ = frequency; }
		void setCustomRenderFunction(clg::small_function<void(OpenGlWrapper &, bool)> function) noexcept
		{ customRenderFunction_ = std::move(function); }
		virtual void setParent(BaseSection *parent) noexcept { parent_ = parent; }

	private:
		void pushForDeletion();

	protected:
		bool setViewPort(const OpenGlWrapper &openGl) const;

		Animator animator_{};
		clg::small_function<void(OpenGlWrapper &, bool)> customRenderFunction_{};
		RefreshFrequency refreshFrequency_ = RefreshFrequency::ResizedAndEvent;
		std::unique_ptr<OpenGlCorners> corners_;
		bool onlyBottomCorners_ = false;
		Colour backgroundColor_ = Colours::transparentBlack;
		Colour bodyColor_;
		Skin::SectionOverride skinOverride_ = Skin::kNone;

		BaseSection *parent_ = nullptr;

		template<CommonConcepts::DerivedOrIs<OpenGlComponent> T>
		friend class gl_ptr;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
	};

	template<CommonConcepts::DerivedOrIs<OpenGlComponent> T>
	class gl_ptr
	{
	public:
		gl_ptr() noexcept = default;
		gl_ptr(nullptr_t) noexcept { }

		explicit gl_ptr(T *component) : component_(component), 
			controlBlock_(new control_block{}) { }

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
		template<CommonConcepts::DerivedOrIs<OpenGlComponent> U>
		gl_ptr(const gl_ptr<U> &other) noexcept : component_(other.component_), 
			controlBlock_(reinterpret_cast<control_block *>(other.controlBlock_))
		{
			if (controlBlock_)
				controlBlock_->refCount.fetch_add(1, std::memory_order_relaxed);
		}

		template<CommonConcepts::DerivedOrIs<OpenGlComponent> U>
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
		template<CommonConcepts::DerivedOrIs<OpenGlComponent> U = OpenGlComponent>
		gl_ptr<U> upcast() requires std::derived_from<T, U>
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
				component_->init(openGl);
				controlBlock_->isInitialised.store(true, std::memory_order_release);
			}

			if (component_->customRenderFunction_)
				component_->customRenderFunction_(openGl, animate);
			else
				component_->render(openGl, animate);
		}

		void deinitialise()
		{
			if (controlBlock_->isInitialised.load(std::memory_order_acquire))
			{
				component_->destroy();
				controlBlock_->isInitialised.store(false, std::memory_order_release);
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

		template <CommonConcepts::DerivedOrIs<OpenGlComponent> U>
		friend class gl_ptr;
	};

	template <CommonConcepts::DerivedOrIs<OpenGlComponent> T, CommonConcepts::DerivedOrIs<OpenGlComponent> U>
	inline bool operator==(const gl_ptr<T> &left, const gl_ptr<U> &right) noexcept
	{ return left.get() == right.get(); }

	// TODO: maybe optimise it to allocate space for both the object and control block at once

	template<std::derived_from<OpenGlComponent> T, typename ... Args>
	inline gl_ptr<T> makeOpenGlComponent(Args&& ... args) { return gl_ptr<T>{ new T(std::forward<Args>(args)...) }; }
}
