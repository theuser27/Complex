/*
	==============================================================================

		OpenGLComponent.cpp
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#include "OpenGlComponent.h"
#include "OpenGlMultiQuad.h"
#include "../Sections/MainInterface.h"

namespace
{
	struct BoundsDetails
	{
		int topLevelheight{};
		Rectangle<int> totalBounds{};
		Rectangle<int> visibleBounds{};
	};

	BoundsDetails getBoundsDetails(const Component *component, Rectangle<int> bounds)
	{
		if (dynamic_cast<const Interface::MainInterface *>(component) != nullptr)
			return { component->getHeight(), bounds, bounds };

		Rectangle totalBounds = bounds + component->getPosition();
		Rectangle visibleBounds = bounds;

		Component *parent = component->getParentComponent();
		while (dynamic_cast<Interface::MainInterface *>(parent) == nullptr)
		{
			totalBounds = totalBounds + parent->getPosition();
			visibleBounds = visibleBounds + component->getPosition();
			parent->getLocalBounds().intersectRectangle(visibleBounds);
			component = parent;
			parent = component->getParentComponent();
			if (!parent)
				return {};
		}

		return { parent->getHeight(), totalBounds, visibleBounds + component->getPosition() };
	}
}

namespace Interface
{
	using namespace juce::gl;

	bool OpenGlComponent::setViewPort(const Component *component, Rectangle<int> bounds, 
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

	bool OpenGlComponent::setViewPort(const Component *component, const OpenGlWrapper &openGl)
	{ return setViewPort(component, component->getLocalBounds(), openGl); }

	bool OpenGlComponent::setViewPort(const OpenGlWrapper &openGl) const
	{ return setViewPort(this, openGl); }

	void OpenGlComponent::setScissor(const Component *component, const OpenGlWrapper &openGl)
	{ setScissorBounds(component, component->getLocalBounds(), openGl); }

	void OpenGlComponent::setScissorBounds(const Component *component, Rectangle<int> bounds, 
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

	OpenGlComponent::OpenGlComponent(String name) : Component(name) { }
	OpenGlComponent::~OpenGlComponent() = default;

	void OpenGlComponent::paintBackground(Graphics &g)
	{
		if (!isVisible())
			return;

		g.fillAll(getColour(Skin::kWidgetBackground1));
	}

	void OpenGlComponent::repaintBackground()
	{
		if (!isShowing())
			return;

		if (parent_)
			parent_->repaintOpenGlBackground(this);
	}

	void OpenGlComponent::resized()
	{
		if (corners_)
			corners_->setBounds(getLocalBounds());

		bodyColor_ = getColour(Skin::kBody);
	}

	void OpenGlComponent::addRoundedCorners()
	{
		corners_ = std::make_unique<OpenGlCorners>();
		addAndMakeVisible(corners_.get());
	}

	void OpenGlComponent::addBottomRoundedCorners()
	{
		onlyBottomCorners_ = true;
		addRoundedCorners();
	}

	void OpenGlComponent::init(OpenGlWrapper &openGl)
	{
		if (corners_)
			corners_->init(openGl);
	}

	void OpenGlComponent::destroy()
	{
		if (corners_)
			corners_->destroy();
	}

	void OpenGlComponent::renderCorners(OpenGlWrapper &openGl, bool animate, Colour color, float rounding)
	{
		if (corners_)
		{
			if (onlyBottomCorners_)
				corners_->setBottomCorners(getLocalBounds(), rounding);
			else
				corners_->setCorners(getLocalBounds(), rounding);
			corners_->setColor(color);
			corners_->render(openGl, animate);
		}
	}

	void OpenGlComponent::renderCorners(OpenGlWrapper &openGl, bool animate)
	{
		renderCorners(openGl, animate, bodyColor_, getValue(Skin::kWidgetRoundedCorner));
	}

	float OpenGlComponent::getValue(Skin::ValueId valueId) const 
	{
		if (parent_)
			return parent_->getValue(valueId);

		return 0.0f;
	}

	Colour OpenGlComponent::getColour(Skin::ColorId colourId) const
	{
		if (parent_)
			return parent_->getColour(colourId);

		return Colours::black;
	}

	void OpenGlComponent::pushForDeletion()
	{ parent_->getInterfaceLink()->pushOpenGlComponentToDelete(this); }
}
