/*
	==============================================================================

		OpenGLComponent.cpp
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#include "AppConfig.h"
#include <juce_opengl/juce_opengl.h>

#include "OpenGlComponent.h"
#include "Plugin/Renderer.h"
#include "../Sections/MainInterface.h"

namespace
{
	struct BoundsDetails
	{
		int topLevelheight{};
		juce::Rectangle<int> globalBounds{};
		juce::Rectangle<int> visibleBounds{};
	};

	BoundsDetails getBoundsDetails(const Interface::BaseComponent *component, juce::Rectangle<int> bounds)
	{
		if (dynamic_cast<const Interface::MainInterface *>(component) != nullptr)
			return { component->getHeightSafe(), bounds, bounds };

		juce::Rectangle globalBounds = bounds + component->getPositionSafe();
		juce::Rectangle visibleBounds = bounds;

		Interface::BaseComponent *parent = component->getParentSafe();
		while (dynamic_cast<Interface::MainInterface *>(parent) == nullptr)
		{
			globalBounds = globalBounds + parent->getPositionSafe();
			visibleBounds = visibleBounds + component->getPositionSafe();
			parent->clipChildBounds(component, visibleBounds);
			component = parent;
			parent = component->getParentSafe();

			// if at any time the message thread changes the parent hierarchy 
			// and suddenly the current component winds up without a parent, 
			// we abort rendering this particular object
			// although this seems excessive it should help identify bugs more easily
			if (!parent)
				return {};
		}

		return { parent->getHeightSafe(), globalBounds, visibleBounds + component->getPositionSafe() };
	}
}

namespace Interface
{
	using namespace juce::gl;

	bool OpenGlComponent::setViewPort(const BaseComponent *component, Rectangle<int> bounds,
	                                  [[maybe_unused]] const OpenGlWrapper &openGl)
	{
		auto [topLevelHeight, globalBounds, visibleBounds] = getBoundsDetails(component, bounds);
		if (visibleBounds.getWidth() <= 0 || visibleBounds.getHeight() <= 0)
			return false;

		glViewport(globalBounds.getX(), topLevelHeight - globalBounds.getBottom(), 
			globalBounds.getWidth(), globalBounds.getHeight());

		glScissor(visibleBounds.getX(), topLevelHeight - visibleBounds.getBottom(),
			visibleBounds.getWidth(), visibleBounds.getHeight());

		return true;
	}

	bool OpenGlComponent::setViewPort(const BaseComponent *component, const OpenGlWrapper &openGl)
	{ return setViewPort(component, component->getLocalBounds(), openGl); }

	bool OpenGlComponent::setViewPort(const OpenGlWrapper &openGl) const
	{ return setViewPort(this, openGl); }

	void OpenGlComponent::setScissor(const BaseComponent *component, const OpenGlWrapper &openGl)
	{ setScissorBounds(component, component->getLocalBounds(), openGl); }

	void OpenGlComponent::setScissorBounds(const BaseComponent *component, Rectangle<int> bounds,
		[[maybe_unused]] const OpenGlWrapper &openGl)
	{
		if (component == nullptr)
			return;

		auto [topLevelHeight, globalBounds, visibleBounds] = getBoundsDetails(component, bounds);
		if (visibleBounds.getWidth() <= 0 || visibleBounds.getHeight() <= 0)
			return;

		glScissor(visibleBounds.getX(), topLevelHeight - visibleBounds.getBottom(),
			visibleBounds.getWidth(), visibleBounds.getHeight());
	}

	String OpenGlComponent::translateFragmentShader(const String &code) {
	#if OPENGL_ES
		return String("#version 300 es\n") + "out mediump vec4 fragColor;\n" +
			code.replace("varying", "in").replace("texture2D", "texture").replace("gl_FragColor", "fragColor");
	#else
		return OpenGLHelpers::translateFragmentShaderToV3(code);
	#endif
	}

	String OpenGlComponent::translateVertexShader(const String &code) {
	#if OPENGL_ES
		return String("#version 300 es\n") + code.replace("attribute", "in").replace("varying", "out");
	#else
		return OpenGLHelpers::translateVertexShaderToV3(code);
	#endif
	}

	OpenGlComponent::OpenGlComponent(String name) : BaseComponent(name) { }
	OpenGlComponent::~OpenGlComponent() = default;

	float OpenGlComponent::getValue(Skin::ValueId valueId) const 
	{
		if (container_)
			return container_.get()->getValue(valueId);

		return 0.0f;
	}

	Colour OpenGlComponent::getColour(Skin::ColorId colourId) const
	{
		if (container_)
			return container_.get()->getColour(colourId);

		return Colours::black;
	}

	void OpenGlComponent::pushForDeletion()
	{ container_.get()->getRenderer()->pushOpenGlComponentToDelete(this); }

	void Animator::tick(bool isAnimating) noexcept
	{
		utils::ScopedSpinLock g(guard_);

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

	float Animator::getValue(EventType type, float min, float max) const noexcept
	{
		utils::ScopedSpinLock g(guard_);

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
}
