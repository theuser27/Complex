/*
	==============================================================================

		OpenGlComponent.h
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

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
		{ COMPLEX_ASSERT(increment > 0.0f); utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; hoverIncrement_ = increment; }

		void setClickIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; clickIncrement_ = increment; }

		void setIsHovered(bool isHovered) noexcept { utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; isHovered_ = isHovered; }
		void setIsClicked(bool isClicked) noexcept { utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; isClicked_ = isClicked; }

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

		static std::optional<OpenGlUniform> getUniform(const OpenGlShaderProgram &program, const char *name);
		static std::optional<OpenGlAttribute> getAttribute(const OpenGlShaderProgram &program, const char *name);

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
		float getValue(Skin::ValueId valueId, bool isScaled = true) const;
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
		utils::shared_value<std::function<void(OpenGlWrapper &, bool)>> renderFunction_{};
		utils::shared_value<RenderFlag> renderFlag_ = RenderFlag::Dirty;
		std::atomic<std::thread::id> waitLock_ = {};

		utils::shared_value<OpenGlContainer *> container_ = nullptr;

		template<typename T>
		friend class gl_ptr;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
	};
}
