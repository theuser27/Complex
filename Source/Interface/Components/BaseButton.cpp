/*
	==============================================================================

		PluginButton.cpp
		Created: 14 Dec 2022 7:01:32am
		Author:  theuser27

	==============================================================================
*/

#include "Framework/parameter_bridge.h"
#include "Framework/parameter_value.h"
#include "../LookAndFeel/Miscellaneous.h"
#include "../LookAndFeel/Paths.h"
#include "OpenGlImageComponent.h"
#include "OpenGlMultiQuad.h"
#include "BaseButton.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
	/*ButtonComponent::ButtonComponent(BaseButton *button) : button_(button)
	{
		animator_.setHoverIncrement(kHoverIncrement);

		background_.setTargetComponent(button);
		background_.setColor(Colours::orange);
		background_.setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);

		text_.setActive(false);
		text_.setScissor(true);
		text_.setTargetComponent(button);
		text_.setFontType(PlainTextComponent::kText);
		addChildComponent(text_);

		shape_.setTargetComponent(button);
		shape_.setScissor(true);
	}

	void ButtonComponent::renderTextButton(OpenGlWrapper &openGl, bool animate)
	{
		Colour activeColor;
		Colour hoverColor;
		if (button_->getToggleState() && isButtonAccented_)
		{
			activeColor = (down_) ? onDownColor_ : onNormalColor_;
			hoverColor = onHoverColor_;
		}
		else
		{
			activeColor = (down_) ? offDownColor_ : offNormalColor_;
			hoverColor = offHoverColor_;
		}

		if (!down_)
			activeColor = activeColor.interpolatedWith(hoverColor, animator_.getValue(Animator::Hover));

		background_.setRounding(getValue(Skin::kLabelBackgroundRounding));
		if (!text_.isActive())
		{
			background_.setColor(activeColor);
			background_.render(openGl, animate);
			return;
		}

		if (style_ != kJustText)
		{
			background_.setColor(backgroundColor_);
			background_.render(openGl, animate);
		}
		text_.setColor(activeColor);
		text_.render(openGl, animate);
	}

	void ButtonComponent::renderRadioButton(OpenGlWrapper &openGl, bool animate)
	{
		static constexpr float kPowerRadius = 0.8f;
		static constexpr float kPowerHoverRadius = 1.0f;

		auto hoverAmount = animator_.getValue(Animator::Hover);

		background_.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
		if (down_ || !button_->getToggleState())
		{
			background_.setColor(backgroundColor_);
			background_.render(openGl, animate);
		}
		else if (hoverAmount != 0.0f)
		{
			background_.setColor(backgroundColor_.withMultipliedAlpha(hoverAmount));
			background_.render(openGl, animate);
		}

		if (button_->getToggleState())
			background_.setColor(onNormalColor_);
		else
			background_.setColor(offNormalColor_);

		background_.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
		background_.render(openGl, animate);
	}

	void ButtonComponent::renderActionButton(OpenGlWrapper &openGl, bool animate)
	{
		bool enabled = button_->isEnabled();

		Colour activeColor = (down_) ? onDownColor_ : onNormalColor_;

		if (!down_ && enabled)
			activeColor = activeColor.interpolatedWith(onHoverColor_, animator_.getValue(Animator::Hover));

		background_.setRounding(getValue(Skin::kLabelBackgroundRounding));
		background_.setColor(activeColor);
		background_.render(openGl, animate);

		text_.setColor(backgroundColor_);
		if (!enabled)
		{
			text_.setColor(onNormalColor_);

			float borderX = 4.0f / (float)button_->getWidth();
			float borderY = 4.0f / (float)button_->getHeight();
			background_.setQuad(0, -1.0f + borderX, -1.0f + borderY, 2.0f - 2.0f * borderX, 2.0f - 2.0f * borderY);
			background_.setColor(bodyColor_);
			background_.render(openGl, animate);

			background_.setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
		}

		text_.render(openGl, animate);
	}

	void ButtonComponent::renderOptionsButton(OpenGlWrapper &openGl, bool animate)
	{
		bool enabled = button_->isEnabled();

		Colour activeColor = (down_) ? onDownColor_ : onNormalColor_;
		Colour borderColour = backgroundColor_;

		if (!down_ && enabled)
			activeColor = activeColor.interpolatedWith(onHoverColor_, animator_.getValue(Animator::Hover));

		shape_.setColor(borderColour);
		shape_.render(openGl, animate);

		background_.setRounding(getValue(Skin::kLabelBackgroundRounding));
		background_.setColor(activeColor);
		background_.render(openGl, animate);

		text_.setColor(getColour(Skin::kHeadingText));
		text_.render(openGl, animate);
	}

	void ButtonComponent::renderShapeButton(OpenGlWrapper &openGl, bool animate)
	{
		Colour activeColor;
		Colour hoverColor;
		if (button_->getToggleState())
		{
			activeColor = (down_) ? onDownColor_ : onNormalColor_;
			hoverColor = onHoverColor_;
		}
		else
		{
			activeColor = (down_) ? offDownColor_ : offNormalColor_;
			hoverColor = offHoverColor_;
		}

		if (!down_)
			activeColor = activeColor.interpolatedWith(hoverColor, animator_.getValue(Animator::Hover));

		shape_.setColor(activeColor);
		shape_.render(openGl, animate);
	}

	void ButtonComponent::setColors()
	{
		if (!button_->findParentComponentOfClass<MainInterface>())
			return;

		bodyColor_ = button_->getColour(Skin::kBody);
		if (style_ == kTextButton || style_ == kJustText)
		{
			onNormalColor_ = button_->getColour(Skin::kIconButtonOn);
			onDownColor_ = button_->getColour(Skin::kIconButtonOnPressed);
			onHoverColor_ = button_->getColour(Skin::kIconButtonOnHover);
			offNormalColor_ = button_->getColour(Skin::kIconButtonOff);
			offDownColor_ = button_->getColour(Skin::kIconButtonOffPressed);
			offHoverColor_ = button_->getColour(Skin::kIconButtonOffHover);
			backgroundColor_ = button_->getColour(Skin::kTextComponentBackground);
		}
		else if (style_ == kRadioButton)
		{
			onNormalColor_ = button_->getColour(Skin::kWidgetAccent1);
			onDownColor_ = button_->getColour(Skin::kOverlayScreen);
			onHoverColor_ = button_->getColour(Skin::kLightenScreen);
			offNormalColor_ = button_->getColour(Skin::kPowerButtonOff);
			offDownColor_ = onDownColor_;
			offHoverColor_ = onHoverColor_;
			backgroundColor_ = button_->getColour(Skin::kBackground);
		}
		else if (style_ == kActionButton)
		{
			if (isButtonAccented_)
			{
				onNormalColor_ = button_->getColour(Skin::kActionButtonSecondary);
				onDownColor_ = button_->getColour(Skin::kActionButtonSecondaryPressed);
				onHoverColor_ = button_->getColour(Skin::kActionButtonSecondaryHover);
			}
			else
			{
				onNormalColor_ = button_->getColour(Skin::kActionButtonPrimary);
				onDownColor_ = button_->getColour(Skin::kActionButtonPrimaryPressed);
				onHoverColor_ = button_->getColour(Skin::kActionButtonPrimaryHover);
			}
			backgroundColor_ = button_->getColour(Skin::kActionButtonText);
		}
		else if (style_ == kOptionsButton)
		{
			onNormalColor_ = Colours::transparentWhite;
			onDownColor_ = button_->getColour(Skin::kOverlayScreen);
			onHoverColor_ = button_->getColour(Skin::kLightenScreen);
			offNormalColor_ = onNormalColor_;
			offDownColor_ = onDownColor_;
			offHoverColor_ = onHoverColor_;
			backgroundColor_ = button_->getColour(Skin::kBody);
		}
		else if (style_ == kShapeButton || style_ == kPowerButton)
		{
			onNormalColor_ = button_->getColour(Skin::kWidgetAccent1);
			onHoverColor_ = onNormalColor_.brighter(0.6f);
			onDownColor_ = onNormalColor_.withBrightness(0.7f);
			offNormalColor_ = onDownColor_;
			offHoverColor_ = onNormalColor_;
			offDownColor_ = onDownColor_;
		}
	}*/

	BaseButton::BaseButton(Framework::ParameterValue *parameter)
	{
		setRepaintsOnMouseActivity(false);
		
		if (!parameter)
			return;

		hasParameter_ = true;

		auto name = parameter->getParameterDetails().pluginName;
		setName({ name.data(), name.size() });
		setParameterLink(parameter->getParameterLink());
		setParameterDetails(parameter->getParameterDetails());
		setValueSafe(parameterLink_->parameter->getNormalisedValue());
	}

	BaseButton::~BaseButton() = default;

	/*void BaseButton::resized()
	{
		static constexpr float kUiButtonSizeMult = 0.45f;

		buttonComponent_->background().markDirty();

		if (buttonComponent_->style() == ButtonComponent::kActionButton)
		{
			buttonComponent_->text().setFontType(PlainTextComponent::kText);
			buttonComponent_->text().setTextHeight(kUiButtonSizeMult * (float)getHeight());
		}
		else if (buttonComponent_->style() == ButtonComponent::kShapeButton || buttonComponent_->style() == ButtonComponent::kPowerButton)
			buttonComponent_->shape().redrawImage();
		else if (buttonComponent_->style() == ButtonComponent::kOptionsButton)
		{
			Path path;
			path.addRoundedRectangle(getLocalBounds(), parent_->scaleValue(8.0f));
			buttonComponent_->setShape(std::pair{ path, Path{} });
			buttonComponent_->text().setTextHeight(parent_->getValue(Skin::kButtonFontSize));
		}
		else
			buttonComponent_->text().setTextHeight(parent_->getValue(Skin::kButtonFontSize));

		buttonComponent_->setColors();
	}*/

	void BaseButton::mouseDown(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
		{
			if (!hasParameter())
				return;

			mouseExit(e);

			PopupItems options;
			//options.addItem(kArmMidiLearn, "Learn MIDI Assignment");
			//if (synth->isMidiMapped(getName().toStdString()))
			//  options.addItem(kClearMidiLearn, "Clear MIDI Assignment");

			parent_->showPopupSelector(this, e.getPosition(), std::move(options),
				[this](int selection) { handlePopupResult(selection); });

			return;
		}

		updateState(true, true);

		for (auto &component : openGlComponents_)
			component->getAnimator().setIsClicked(true);
	}

	void BaseButton::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			return;

		bool wasDown = isHeldDown();
		bool isOver = isMouseOver(true);
		updateState(false, isOver);

		if (!wasDown || !isOver)
		{
			if (wasDown)
			{
				for (auto &component : openGlComponents_)
					component->getAnimator().setIsClicked(false);
			}
			return;
		}

		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->beginChangeGesture();

		beginChange(getToggleState());

		setToggleState(!getToggleState(), NotificationType::sendNotificationSync);

		setValueToHost();
		endChange();

		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->endChangeGesture();

		for (auto *listener : buttonListeners_)
			listener->buttonClicked(this);

		for (auto &component : openGlComponents_)
			component->getAnimator().setIsClicked(false);
	}

	void BaseButton::mouseEnter(const MouseEvent &)
	{
		updateState(false, true);
		for (auto &component : openGlComponents_)
			component->getAnimator().setIsHovered(true);
	}

	void BaseButton::mouseExit(const MouseEvent &)
	{
		updateState(false, false);
		for (auto &component : openGlComponents_)
			component->getAnimator().setIsHovered(false);
	}

	void BaseButton::setToggleState(bool shouldBeOn, NotificationType notification)
	{
		if (shouldBeOn == getToggleState())
			return;

		setValueSafe(shouldBeOn);

		if (notification != NotificationType::dontSendNotification)
		{
			valueChanged();

			for (auto *listener : buttonListeners_)
				listener->buttonClicked(this);
		}
	}

	void BaseButton::valueChanged()
	{
		for (auto *listener : buttonListeners_)
			listener->guiChanged(this);

		redoImage();
	}

	String BaseButton::getTextFromValue(bool value) const noexcept
	{
		if (!details_.stringLookup.empty())
		{
			auto string = details_.stringLookup[(size_t)value];
			return { string.data(), string.size() };
		}

		return (value) ? "On" : "Off";
	}

	void BaseButton::handlePopupResult([[maybe_unused]] int result)
	{
		//InterfaceEngineLink *parent = findParentComponentOfClass<InterfaceEngineLink>();
		//if (parent == nullptr)
		//  return;

		//SynthBase *synth = parent->getSynth();

		//if (result == kArmMidiLearn)
		//  synth->armMidiLearn(getName().toStdString());
		//else if (result == kClearMidiLearn)
		//  synth->clearMidiLearn(getName().toStdString());
	}

	void BaseButton::addListener(BaseSection *listener)
	{
		buttonListeners_.push_back(listener);
	}

	void BaseButton::removeListener(BaseSection *listener)
	{
		std::erase(buttonListeners_, listener);
	}

	void BaseButton::updateState(bool isHeldDown, bool isHoveredOver) noexcept
	{
		if (isEnabled() && isVisible() && !isCurrentlyBlockedByAnotherModalComponent())
		{
			isHeldDown_ = isHeldDown;
			isHoveredOver_ = isHoveredOver;
		}
	}

	PowerButton::PowerButton(Framework::ParameterValue* parameter) : BaseButton(parameter)
	{
		shapeComponent_ = makeOpenGlComponent<PlainShapeComponent>("Power Button Shape");
		shapeComponent_->getAnimator().setHoverIncrement(kHoverIncrement);
		shapeComponent_->setShapes(Paths::powerButtonIcon());
		shapeComponent_->setRenderFunction([this](OpenGlWrapper &openGl, bool animate)
			{
				Colour finalColour = activeColour_;

				auto &animator = shapeComponent_->getAnimator();
				animator.tick(animate);

				if (!isHeldDown_)
					finalColour = finalColour.interpolatedWith(hoverColour_, animator.getValue(Animator::Hover));

				shapeComponent_->setColor(finalColour);
				shapeComponent_->render(openGl, animate);
			});

		addOpenGlComponent(shapeComponent_);

		setAddedHitbox(BorderSize{ kAddedMargin });
	}

	PowerButton::~PowerButton() = default;

	void PowerButton::setColours()
	{
		onNormalColor_ = getColour(Skin::kWidgetAccent1);
	}

	Rectangle<int> PowerButton::setBoundsForSizes(int height, int)
	{
		if (drawBounds_.getHeight() != height)
		{
			drawBounds_ = { height, height };
		}
		return drawBounds_;
	}

	void PowerButton::redoImage()
	{
		Colour onHoverColor_ = onNormalColor_.brighter(0.6f);
		Colour onDownColor_ = onNormalColor_.withBrightness(0.7f);
		Colour offHoverColor_ = onNormalColor_;
		Colour offDownColor_ = onDownColor_;

		if (getToggleState())
		{
			activeColour_ = (isHeldDown_) ? onDownColor_ : onNormalColor_;
			hoverColour_ = onHoverColor_;
		}
		else
		{
			activeColour_ = (isHeldDown_) ? offDownColor_ : onDownColor_;
			hoverColour_ = offHoverColor_;
		}

		shapeComponent_->redrawImage();
	}

	void PowerButton::setComponentsBounds()
	{
		shapeComponent_->setBounds(drawBounds_);

		redoImage();
	}


	RadioButton::RadioButton(Framework::ParameterValue *parameter) : BaseButton(parameter)
	{
		backgroundComponent_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment, "Radio Button Background");
		backgroundComponent_->getAnimator().setHoverIncrement(kHoverIncrement);
		backgroundComponent_->setTargetComponent(this);
		backgroundComponent_->setRenderFunction([this](OpenGlWrapper &openGl, bool animate)
			{
				static constexpr float kPowerRadius = 0.8f;
				static constexpr float kPowerHoverRadius = 1.0f;

				auto &animator = backgroundComponent_->getAnimator();
				animator.tick(animate);

				auto hoverAmount = animator.getValue(Animator::Hover);

				backgroundComponent_->setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
				if (isHeldDown_ || !getToggleState())
				{
					backgroundComponent_->setColor(backgroundColor_);
					backgroundComponent_->render(openGl, animate);
				}
				else if (hoverAmount != 0.0f)
				{
					backgroundComponent_->setColor(backgroundColor_.get().withMultipliedAlpha(hoverAmount));
					backgroundComponent_->render(openGl, animate);
				}

				if (getToggleState())
					backgroundComponent_->setColor(onNormalColor_);
				else
					backgroundComponent_->setColor(offNormalColor_);

				backgroundComponent_->setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
				backgroundComponent_->render(openGl, animate);
			});

		addOpenGlComponent(backgroundComponent_);

		addLabel();
		setAddedHitbox(BorderSize{ kAddedMargin });
	}

	RadioButton::~RadioButton() = default;

	void RadioButton::setColours()
	{
		onNormalColor_ = getColour(Skin::kWidgetAccent1);
		offNormalColor_ = getColour(Skin::kPowerButtonOff);
		backgroundColor_ = getColour(Skin::kBackground);
	}

	Rectangle<int> RadioButton::setBoundsForSizes(int height, int)
	{
		if (drawBounds_.getHeight() != height)
		{
			drawBounds_ = { height, height };
		}
		
		setExtraElementsPositions(drawBounds_);
		if (label_)
			return drawBounds_.getUnion(label_->getBounds());
		return drawBounds_;
	}

	void RadioButton::setExtraElementsPositions(Rectangle<int> anchorBounds)
	{
		if (!label_)
			return;

		label_->updateState();
		auto labelTextWidth = label_->getTotalWidth();
		auto labelX = anchorBounds.getX();
		switch (labelPlacement_)
		{
		case BubblePlacement::right:
			labelX += anchorBounds.getWidth() + scaleValueRoundInt(kLabelOffset);
			label_->setJustification(Justification::centredLeft);
			break;
		default:
		case BubblePlacement::above:
		case BubblePlacement::below:
		case BubblePlacement::left:
			labelX -= scaleValueRoundInt(kLabelOffset) + labelTextWidth;
			label_->setJustification(Justification::centredRight);
			break;
		}

		label_->setBounds(labelX, 
			anchorBounds.getY() - (label_->getTotalHeight() - anchorBounds.getHeight()) / 2, 
			labelTextWidth, label_->getTotalHeight());
	}

	void RadioButton::redoImage()
	{
		if (getToggleState())
			backgroundComponent_->setColor(onNormalColor_);
		else
			backgroundComponent_->setColor(offNormalColor_);
	}

	void RadioButton::setComponentsBounds()
	{
		backgroundComponent_->setCustomDrawBounds(drawBounds_);

		redoImage();
	}

	void RadioButton::setRounding(float rounding) noexcept { backgroundComponent_->setRounding(rounding); }


	OptionsButton::OptionsButton(Framework::ParameterValue *parameter, String name, String displayText) : BaseButton(parameter)
	{
		if (!parameter)
			setName(name);

		setText(std::move(displayText));

		borderComponent_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleBorderFragment, "Options Button Border");
		borderComponent_->setTargetComponent(this);
		plusComponent_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kPlusFragment, "Options Button Plus Icon");
		plusComponent_->setTargetComponent(this);
		textComponent_ = makeOpenGlComponent<PlainTextComponent>("Options Button Text", text_);
		textComponent_->setTargetComponent(this);

		addOpenGlComponent(borderComponent_);
		addOpenGlComponent(plusComponent_);
		addOpenGlComponent(textComponent_);
	}

	OptionsButton::~OptionsButton() = default;

	void OptionsButton::setColours()
	{
		borderColour_ = getColour(Skin::kBody);
	}

	Rectangle<int> OptionsButton::setBoundsForSizes(int height, int width)
	{
		drawBounds_ = { width, height };
		return drawBounds_;
	}

	void OptionsButton::redoImage()
	{
		borderComponent_->setRounding(scaleValue(kBorderRounding));
		borderComponent_->setColor(borderColour_);

		plusComponent_->setThickness(scaleValue(1.0f / (float)kPlusRelativeSize));
		plusComponent_->setColor(getColour(Skin::kNormalText));

		textComponent_->setFontType(PlainTextComponent::kText);
		textComponent_->setJustification(Justification::centredLeft);
		textComponent_->setText(text_);
	}

	void OptionsButton::setComponentsBounds()
	{
		borderComponent_->setCustomDrawBounds(drawBounds_);
		int plusSize = scaleValueRoundInt(kPlusRelativeSize);
		int halfHeight = drawBounds_.getHeight() / 2;
		Rectangle plusBounds{ drawBounds_.getX() + halfHeight, drawBounds_.getY() + halfHeight - plusSize / 2, plusSize, plusSize };
		plusComponent_->setCustomDrawBounds(plusBounds);
		Rectangle textBounds{ plusBounds.getRight() + halfHeight, 0, getWidth() - plusBounds.getRight() - halfHeight, getHeight() };
		textComponent_->setCustomDrawBounds(textBounds);

		redoImage();
	}

	ActionButton::ActionButton(String name, String displayText) :
		BaseButton(nullptr), text_(std::move(displayText))
	{
		setName(name);

		fillComponent_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment, "Action Button Fill");
		textComponent_ = makeOpenGlComponent<PlainTextComponent>("Action Button Text", text_);

		addOpenGlComponent(fillComponent_);
		addOpenGlComponent(textComponent_);
	}

	ActionButton::~ActionButton() = default;

	void ActionButton::mouseDown(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			return;
		
		updateState(true, true);

		for (auto &component : openGlComponents_)
			component->getAnimator().setIsClicked(true);
	}

	void ActionButton::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			return;

		bool wasDown = isHeldDown();
		bool isOver = isMouseOver(true);
		updateState(false, isOver);

		if (!wasDown || !isOver)
		{
			if (wasDown)
			{
				for (auto &component : openGlComponents_)
					component->getAnimator().setIsClicked(false);
			}
			return;
		}

		setToggleState(!getToggleState(), NotificationType::sendNotificationSync);

		action_();

		for (auto *listener : buttonListeners_)
			listener->buttonClicked(this);

		for (auto &component : openGlComponents_)
			component->getAnimator().setIsClicked(false);
	}

	void ActionButton::setColours()
	{
		fillColour_ = getColour(Skin::kActionButtonPrimary);
		textColour_ = getColour(Skin::kActionButtonText);
	}

	Rectangle<int> ActionButton::setBoundsForSizes(int height, int width)
	{
		drawBounds_ = { width, height };
		return drawBounds_;
	}

	void ActionButton::redoImage()
	{
		fillComponent_->setRounding(scaleValue(kBorderRounding));
		fillComponent_->setColor(fillColour_);

		textComponent_->setFontType(PlainTextComponent::kText);
		textComponent_->setTextColour(textColour_);
		textComponent_->setJustification(Justification::centred);
		textComponent_->setText(text_);
	}

	void ActionButton::setComponentsBounds()
	{
		fillComponent_->setCustomDrawBounds(drawBounds_);
		textComponent_->setCustomDrawBounds(drawBounds_);

		redoImage();
	}
}
