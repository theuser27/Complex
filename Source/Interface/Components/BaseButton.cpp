/*
	==============================================================================

		PluginButton.cpp
		Created: 14 Dec 2022 7:01:32am
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "BaseButton.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
	void ButtonLabel::setParent(BaseButton *parent) noexcept
	{
		parent_ = parent;
		labelText_ = (!parent->hasParameter()) ? parent->getName() :
			parent->getParameterDetails().displayName.data();
	}

	void ButtonLabel::paint(Graphics &g)
	{
		// painting only the label text, the other components will be handled by the owning section
		if (!parent_)
			return;

		auto textRectangle = Rectangle{ 0.0f, 0.0f, (float)getWidth(), (float)getHeight() };
		g.setFont(usedFont_);
		g.setColour(parent_->getColour(Skin::kNormalText));
		g.drawText(parent_->getParameterDetails().displayName.data(), textRectangle, Justification::centredLeft);
	}

	ButtonComponent::ButtonComponent(BaseButton *button) : button_(button)
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
			if (down_)
				activeColor = onDownColor_;
			else
				activeColor = onNormalColor_;

			hoverColor = onHoverColor_;
		}
		else
		{
			if (down_)
				activeColor = offDownColor_;
			else
				activeColor = offNormalColor_;

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

		Colour active_color;
		if (down_)
			active_color = onDownColor_;
		else
			active_color = onNormalColor_;

		if (!down_ && enabled)
			active_color = active_color.interpolatedWith(onHoverColor_, animator_.getValue(Animator::Hover));

		background_.setRounding(getValue(Skin::kLabelBackgroundRounding));
		background_.setColor(active_color);
		background_.render(openGl, animate);

		text_.setColor(backgroundColor_);
		if (!enabled)
		{
			text_.setColor(onNormalColor_);

			float border_x = 4.0f / (float)button_->getWidth();
			float border_y = 4.0f / (float)button_->getHeight();
			background_.setQuad(0, -1.0f + border_x, -1.0f + border_y, 2.0f - 2.0f * border_x, 2.0f - 2.0f * border_y);
			background_.setColor(bodyColor_);
			background_.render(openGl, animate);

			background_.setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
		}

		text_.render(openGl, animate);
	}

	void ButtonComponent::renderLightenButton(OpenGlWrapper &openGl, bool animate)
	{
		bool enabled = button_->isEnabled();

		Colour active_color;
		if (down_)
			active_color = onDownColor_;
		else
			active_color = onNormalColor_;

		if (!down_ && enabled)
			active_color = active_color.interpolatedWith(onHoverColor_, animator_.getValue(Animator::Hover));

		background_.setRounding(getValue(Skin::kLabelBackgroundRounding));
		background_.setColor(active_color);
		background_.render(openGl, animate);
	}

	void ButtonComponent::renderShapeButton(OpenGlWrapper &openGl, bool animate)
	{
		Colour activeColor;
		Colour hoverColor;
		if (button_->getToggleState())
		{
			if (down_)
				activeColor = onDownColor_;
			else
				activeColor = onNormalColor_;

			hoverColor = onHoverColor_;
		}
		else
		{
			if (down_)
				activeColor = offDownColor_;
			else
				activeColor = offNormalColor_;

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
		else if (style_ == kLightenButton)
		{
			onNormalColor_ = Colours::transparentWhite;
			onDownColor_ = button_->getColour(Skin::kOverlayScreen);
			onHoverColor_ = button_->getColour(Skin::kLightenScreen);
			offNormalColor_ = onNormalColor_;
			offDownColor_ = onDownColor_;
			offHoverColor_ = onHoverColor_;
			backgroundColor_ = onNormalColor_;
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
	}

	void ButtonComponent::setText() noexcept
	{
		String text = button_->getButtonText();
		if (!text.isEmpty())
		{
			text_.setActive(true);
			text_.setText(text);
		}
	}

	BaseButton::BaseButton(Framework::ParameterValue *parameter, String name)
	{
		if (!parameter)
		{
			setName(name);
			hasParameter_ = false;
			return;
		}

		hasParameter_ = true;
		setName(parameter->getParameterDetails().name.data());

		setParameterLink(parameter->getParameterLink());
		setParameterDetails(parameter->getParameterDetails());

		setToggleState(std::round(details_.defaultNormalisedValue) == 1.0f, NotificationType::dontSendNotification);
		setValueSafe(std::round(details_.defaultNormalisedValue));
	}

	void BaseButton::resized()
	{
		static constexpr float kUiButtonSizeMult = 0.45f;

		ToggleButton::resized();
		buttonComponent_.setText();
		buttonComponent_.background().markDirty();

		if (buttonComponent_.style() == ButtonComponent::kActionButton)
		{
			buttonComponent_.text().setFontType(PlainTextComponent::kText);
			buttonComponent_.text().setTextHeight(kUiButtonSizeMult * (float)getHeight());
		}
		else if (buttonComponent_.style() == ButtonComponent::kShapeButton || 
			buttonComponent_.style() == ButtonComponent::kPowerButton)
			buttonComponent_.shape().redrawImage();
		else
			buttonComponent_.text().setTextHeight(parent_->getValue(Skin::kButtonFontSize));

		buttonComponent_.setColors();
	}

	void BaseButton::mouseDown(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
		{
			ToggleButton::mouseExit(e);

			// this implies that the button is not a parameter
			if (details_.name.empty())
				return;

			PopupItems options;
			//options.addItem(kArmMidiLearn, "Learn MIDI Assignment");
			//if (synth->isMidiMapped(getName().toStdString()))
			//  options.addItem(kClearMidiLearn, "Clear MIDI Assignment");

			parent_->showPopupSelector(this, e.getPosition(), options, 
				[this](int selection) { handlePopupResult(selection); });
		}
		else
		{
			ToggleButton::mouseDown(e);

			beginChange(getToggleState());

			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->beginChangeGesture();
		}

		buttonComponent_.getAnimator().setIsClicked(true);
		buttonComponent_.setDown(true);
	}

	void BaseButton::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			return;

		ToggleButton::mouseUp(e);

		setValueSafe(getToggleState());
		setValueToHost();
		endChange();

		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->endChangeGesture();

		buttonComponent_.getAnimator().setIsClicked(false);
		buttonComponent_.setDown(false);
	}

	void BaseButton::parentHierarchyChanged()
	{
		parent_ = findParentComponentOfClass<BaseSection>();
		ToggleButton::parentHierarchyChanged();
	}

	void BaseButton::clicked() { setValueSafe(getToggleState()); }

	void BaseButton::endChange()
	{
		auto *mainInterface = findParentComponentOfClass<MainInterface>();
		mainInterface->getPlugin().pushUndo(new Framework::ParameterUpdate(this, valueBeforeChange_, (double)getToggleState()));
	}

	Colour BaseButton::getColour(Skin::ColorId colourId) const noexcept { return parent_->getColour(colourId); }

	String BaseButton::getTextFromValue(bool value) const noexcept
	{
		size_t lookup = value;
		if (!details_.stringLookup.empty())
			return details_.stringLookup[lookup].data();

		return Framework::kOffOnNames[lookup].data();
	}

	void BaseButton::handlePopupResult(int result)
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

	void BaseButton::clicked(const ModifierKeys &modifiers)
	{
		ToggleButton::clicked(modifiers);

		if (!modifiers.isPopupMenu())
			for (ButtonListener *listener : buttonListeners_)
				listener->guiChanged(this);

		if (!details_.stringLookup.empty())
			setText(details_.stringLookup[getToggleState() ? 1 : 0].data());
	}

}
