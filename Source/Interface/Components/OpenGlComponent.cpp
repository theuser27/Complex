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
	Rectangle<int> getGlobalBounds(const Component *component, Rectangle<int> bounds)
	{
		Component *parent = component->getParentComponent();
		while (parent && dynamic_cast<const Interface::MainInterface *>(component) == nullptr)
		{
			bounds = bounds + component->getPosition();
			component = parent;
			parent = component->getParentComponent();
		}

		return bounds;
	}

	Rectangle<int> getGlobalVisibleBounds(const Component *component, Rectangle<int> visibleBounds)
	{
		Component *parent = component->getParentComponent();
		while (parent && dynamic_cast<Interface::MainInterface *>(parent) == nullptr)
		{
			visibleBounds = visibleBounds + component->getPosition();
			parent->getLocalBounds().intersectRectangle(visibleBounds);
			component = parent;
			parent = component->getParentComponent();
		}

		return visibleBounds + component->getPosition();
	}
}

namespace Interface
{
	using namespace juce::gl;

	bool OpenGlComponent::setViewPort(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl)
	{
		Rectangle<int> visibleBounds = getGlobalVisibleBounds(component, bounds);
		if (visibleBounds.getWidth() <= 0 || visibleBounds.getHeight() <= 0)
			return false;

		float scale = openGl.displayScale;
		float topLevelHeight = (float)component->findParentComponentOfClass<MainInterface>()->getHeight();
		Rectangle<int> globalBounds = getGlobalBounds(component, bounds);

		/*glViewport(globalBounds.getX(),
			(int)std::ceil(scale * topLevelHeight) - globalBounds.getBottom(),
			globalBounds.getWidth(), globalBounds.getHeight());*/
		glViewport(globalBounds.getX(), (int)std::ceil(scale * topLevelHeight) - globalBounds.getBottom(), 
			globalBounds.getWidth(), globalBounds.getHeight());

		glScissor(visibleBounds.getX(),
			(int)std::ceil(scale * topLevelHeight) - visibleBounds.getBottom(),
			visibleBounds.getWidth(), visibleBounds.getHeight());

		return true;
	}

	bool OpenGlComponent::setViewPort(const Component *component, const OpenGlWrapper &openGl)
	{ return setViewPort(component, component->getLocalBounds(), openGl); }

	bool OpenGlComponent::setViewPort(const OpenGlWrapper &openGl) const
	{ return setViewPort(this, openGl); }

	void OpenGlComponent::setScissor(const Component *component, const OpenGlWrapper &openGl)
	{ setScissorBounds(component, component->getLocalBounds(), openGl); }

	void OpenGlComponent::setScissorBounds(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl)
	{
		if (component == nullptr)
			return;

		Rectangle<int> visibleBounds = getGlobalVisibleBounds(component, bounds);
		if (visibleBounds.getWidth() <= 0 || visibleBounds.getHeight() <= 0)
			return;

		float scale = openGl.displayScale;
		float topLevelHeight = (float)component->findParentComponentOfClass<MainInterface>()->getHeight();

		glScissor(visibleBounds.getX(),
			(int)std::ceil(scale * topLevelHeight) - visibleBounds.getBottom(),
			visibleBounds.getWidth(), visibleBounds.getHeight());
	}

	OpenGlComponent::OpenGlComponent(String name) : Component(name) { }
	// destructor is here because the destructor of OpenGlCorners needs to be visible
	// so that the deleter of the unique_ptr corners_ can be created
	OpenGlComponent::~OpenGlComponent() = default;

	void OpenGlComponent::paint(Graphics &g)
	{
		if (!isVisible())
			return;

		g.fillAll(getColour(Skin::kWidgetBackground1));
	}

	void OpenGlComponent::repaintBackground()
	{
		if (!isShowing())
			return;

		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->repaintOpenGlBackground(this);
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

	void OpenGlComponent::destroy(OpenGlWrapper &openGl)
	{
		if (corners_)
			corners_->destroy(openGl);
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
}
