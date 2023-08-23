/*
	==============================================================================

		BaseControl.cpp
		Created: 31 Jul 2023 7:37:43pm
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "BaseControl.h"
#include "Framework/parameter_bridge.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
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

		setName(parameter.getParameterDetails().id.data());
		auto replacedLink = setParameterLink(parameter.getParameterLink());

		Framework::ParameterValue *replacedParameter = nullptr;
		if (replacedLink)
			replacedParameter = replacedLink->parameter;

		if (getValueFromParameter)
			setValueFromParameter();
		else
			setValueToParameter();

		setParameterDetails(parameter.getParameterDetails());

		return replacedParameter;
	}

	void BaseControl::setValueFromHost() noexcept
	{
		if (parameterLink_ && parameterLink_->hostControl)
			setValueSafe(parameterLink_->hostControl->getValue());
	}

	void BaseControl::setValueFromParameter() noexcept
	{
		if (parameterLink_ && parameterLink_->parameter)
		{
			auto value = parameterLink_->parameter->getNormalisedValue();
			setValueSafe(value);
			setValueInternal(value, NotificationType::dontSendNotification);
		}
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
				parent_->getInterfaceLink()->getPlugin().getSampleRate(), (float)getValueSafe());
	}

	bool BaseControl::updateValue()
	{
		if (!hasParameter())
			return false;

		auto newValue = getValueSafe();
		bool needsUpdate = getValueInternal() != newValue;
		if (needsUpdate)
			setValueInternal(newValue, NotificationType::sendNotificationSync);

		return needsUpdate;
	}

	void BaseControl::beginChange(double oldValue) noexcept
	{
		valueBeforeChange_ = oldValue;
		hasBegunChange_ = true;
	}

	void BaseControl::endChange()
	{
		hasBegunChange_ = false;
		parent_->getInterfaceLink()->getPlugin().pushUndo(
			new Framework::ParameterUpdate(this, valueBeforeChange_, getValueInternal()));
	}

	void BaseControl::setOverallBounds(Point<int> position)
	{
		auto scaledAddedHitbox = BorderSize{ 
			parent_->scaleValueRoundInt((float)addedHitBox_.getTop()),
			parent_->scaleValueRoundInt((float)addedHitBox_.getLeft()),
			parent_->scaleValueRoundInt((float)addedHitBox_.getBottom()),
			parent_->scaleValueRoundInt((float)addedHitBox_.getRight()) };

		Rectangle bounds{ drawBounds_.getWidth() + scaledAddedHitbox.getLeftAndRight(),
			drawBounds_.getHeight() + scaledAddedHitbox.getTopAndBottom() };
		bounds.setPosition(position - drawBounds_.getPosition() - Point{ scaledAddedHitbox.getLeft(), scaledAddedHitbox.getTop() });

		// offsetting the drawBounds and subsequently the extra elements bounds if origin position was changed
		drawBounds_.setPosition(drawBounds_.getPosition() + Point{ scaledAddedHitbox.getLeft(), scaledAddedHitbox.getTop() });
		for (auto &relativeBound : extraElements_.data)
			relativeBound.second.setPosition(relativeBound.second.getPosition() + 
				Point{ scaledAddedHitbox.getLeft(), scaledAddedHitbox.getTop() });

		setBoundsInternal(bounds);
	}

	void BaseControl::addLabel()
	{
		if (label_)
			return;
		
		label_ = makeOpenGlComponent<PlainTextComponent>("Control Label",
			(hasParameter()) ? details_.displayName.data() : getName());
		label_->setFontType(PlainTextComponent::kText);
		label_->setTextHeight(Fonts::kInterVDefaultHeight);
		extraElements_.add(label_.get(), {});
	}

	void BaseControl::removeLabel()
	{ 
		if (!label_)
			return;

		extraElements_.erase(label_.get());
		label_ = nullptr;
	}

	float BaseControl::findValue(Skin::ValueId valueId) const
	{
		if (parent_)
			return parent_->getValue(valueId);
		return 0.0f;
	}

	Colour BaseControl::getColour(Skin::ColorId colourId) const noexcept
	{ return parent_->getColour(colourId); }

}
