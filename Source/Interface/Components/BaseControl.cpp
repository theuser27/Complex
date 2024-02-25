/*
	==============================================================================

		BaseControl.cpp
		Created: 31 Jul 2023 7:37:43pm
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "Framework/parameter_bridge.h"
#include "Plugin/Complex.h"
#include "Plugin/Renderer.h"
#include "../LookAndFeel/Fonts.h"
#include "OpenGlImageComponent.h"
#include "BaseControl.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
	BaseControl::BaseControl() = default;
	BaseControl::~BaseControl() { setParameterLink(nullptr); }

	Framework::ParameterLink *BaseControl::setParameterLink(Framework::ParameterLink *parameterLink) noexcept
	{
		auto replacedLink = parameterLink_;
		if (replacedLink)
			replacedLink->parameter->changeControl(std::variant<BaseControl *,
				Framework::ParameterBridge *>(std::in_place_index<0>, nullptr));

		parameterLink_ = parameterLink;

		if (parameterLink_)
			parameterLink_->parameter->changeControl(this);

		return replacedLink;
	}

	Framework::ParameterValue *BaseControl::changeLinkedParameter(Framework::ParameterValue &parameter, bool getValueFromParameter)
	{
		hasParameter_ = true;

		setName(toJuceString(parameter.getParameterDetails().name));
		auto replacedLink = setParameterLink(parameter.getParameterLink());

		Framework::ParameterValue *replacedParameter = nullptr;
		if (replacedLink)
			replacedParameter = replacedLink->parameter;

		setParameterDetails(parameter.getParameterDetails());
		
		if (getValueFromParameter)
			setValueFromParameter();
		else
			setValueToParameter();

		return replacedParameter;
	}

	bool BaseControl::setValueFromHost() noexcept
	{
		if (!parameterLink_ || !parameterLink_->hostControl)
			return false;

		double value = parameterLink_->hostControl->getValue();
		if (value == getValueSafe())
			return false;

		setValueSafe(value);
		return true;
	}

	void BaseControl::setValueFromParameter() noexcept
	{
		if (!parameterLink_ || !parameterLink_->parameter)
			return;

		double value = parameterLink_->parameter->getNormalisedValue();
		if (value == getValueSafe())
			return;

		setValueSafe(value);
		valueChanged();
	}

	void BaseControl::setValueToHost() const noexcept
	{
		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->setValueFromUI((float)getValueSafe());
	}

	void BaseControl::setValueToParameter() const noexcept
	{
		if (parameterLink_ && parameterLink_->parameter)
			parameterLink_->parameter->updateValues(
				getRenderer()->getPlugin().getSampleRate(), (float)getValueSafe());
	}

	void BaseControl::beginChange(double oldValue) noexcept
	{
		valueBeforeChange_ = oldValue;
		hasBegunChange_ = true;
	}

	void BaseControl::endChange()
	{
		hasBegunChange_ = false;
		getRenderer()->getPlugin().pushUndo(
			new Framework::ParameterUpdate(this, valueBeforeChange_, getValueSafe()));
	}

	void BaseControl::resized()
	{
		setColours();
		setComponentsBounds();
		setExtraElementsPositions(drawBounds_.isEmpty() ? getLocalBounds() : drawBounds_);
		repositionExtraElements();
	}

	void BaseControl::moved()
	{
		repositionExtraElements();
	}

	void BaseControl::parentHierarchyChanged()
	{
		BaseComponent::parentHierarchyChanged();
		parent_ = findParentComponentOfClass<BaseSection>();
	}

	void BaseControl::setPosition(Point<int> position)
	{
		auto scaledAddedHitbox = BorderSize
		{
			scaleValueRoundInt((float)addedHitbox_.getTop()),
			scaleValueRoundInt((float)addedHitbox_.getLeft()),
			scaleValueRoundInt((float)addedHitbox_.getBottom()),
			scaleValueRoundInt((float)addedHitbox_.getRight()) 
		};

		COMPLEX_ASSERT(!drawBounds_.isEmpty() && "You need to call getBoundsForSizes with \
			specific dimensions so that size can be calculated and stored");

		Rectangle bounds{ drawBounds_.getWidth() + scaledAddedHitbox.getLeftAndRight(),
			drawBounds_.getHeight() + scaledAddedHitbox.getTopAndBottom() };
		bounds.setPosition(position - Point{ scaledAddedHitbox.getLeft(), scaledAddedHitbox.getTop() });

		// offsetting the drawBounds and subsequently the extra elements bounds if origin position was changed
		drawBounds_.setPosition({ scaledAddedHitbox.getLeft(), scaledAddedHitbox.getTop() });
		isDrawBoundsSet_ = true;
		setBounds(bounds);
	}

	void BaseControl::setBounds(int x, int y, int width, int height)
	{
		if (isDrawBoundsSet_)
		{
			OpenGlContainer::setBounds(x, y, width, height);
			isDrawBoundsSet_ = false;
			return;
		}

		// if for some reason we didn't use the getBoundsForSizes -> setBounds(Point<int>) path 
		// for setting bounds of this particular control, we set drawBounds to the specified size
		// and expand the overall size to accommodate an added hitbox if necessary
		
		
		auto hitbox = BorderSize
		{
			scaleValueRoundInt((float)addedHitbox_.getTop()),
			scaleValueRoundInt((float)addedHitbox_.getLeft()),
			scaleValueRoundInt((float)addedHitbox_.getBottom()),
			scaleValueRoundInt((float)addedHitbox_.getRight())
		};
		drawBounds_ = Rectangle{ hitbox.getLeft(), hitbox.getTop(), width, height };
		
		OpenGlContainer::setBounds(x - hitbox.getLeft(), y - hitbox.getTop(), 
			width + hitbox.getLeftAndRight(), height + hitbox.getTopAndBottom());
	}

	void BaseControl::repositionExtraElements()
	{
		for (auto &extraElement : extraElements_.data)
			extraElement.first->setBounds(extraElement.first->getParentComponent()->getLocalArea(this, extraElement.second));
	}

	void BaseControl::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		utils::ScopedSpinLock g(isRendering_);

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisibleSafe() && !openGlComponent->isAlwaysOnTopSafe())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisibleSafe() && openGlComponent->isAlwaysOnTopSafe())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}
	}

	void BaseControl::addLabel()
	{
		if (label_)
			return;
		
		label_ = makeOpenGlComponent<PlainTextComponent>("Control Label",
			(hasParameter()) ? toJuceString(details_.displayName) : getName());
		label_->setFontType(PlainTextComponent::kText);
		label_->setTextHeight(Fonts::kInterVDefaultHeight);
		addOpenGlComponent(label_);
		addUnclippedChild(label_.get());
	}

	void BaseControl::removeLabel()
	{ 
		if (!label_)
			return;

		removeUnclippedChild(label_.get());
		removeOpenGlComponent(label_.get());
		label_ = nullptr;
	}
}
