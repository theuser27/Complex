/*
  ==============================================================================

    PluginButton.cpp
    Created: 14 Dec 2022 7:01:32am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseButton.h"

#include "../Sections/BaseSection.h"

namespace Interface
{
  String BaseButton::getTextFromValue(bool value) const noexcept
  {
    int lookup = value ? 1 : 0;

    if (details_.stringLookup)
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

      BaseSection *parent = findParentComponentOfClass<BaseSection>();
      parent->showPopupSelector(this, e.getPosition(), options, [=](int selection) { handlePopupResult(selection); });
    }
    else
    {
      ToggleButton::mouseDown(e);

      // this implies that the button is not a parameter
      if (details_.name.empty())
        return;

      if (parameterLink_ && parameterLink_->hostControl)
        parameterLink_->hostControl->beginChangeGesture();
    }
  }

  void BaseButton::mouseUp(const MouseEvent &e)
  {
    if (!e.mods.isPopupMenu())
    {
      ToggleButton::mouseUp(e);

      // this implies that the button is not a parameter
      if (details_.name.empty())
        return;

      if (parameterLink_ && parameterLink_->hostControl)
        parameterLink_->hostControl->endChangeGesture();
    }
  }

  void BaseButton::clicked() { setValueSafe(getToggleState()); }
  void BaseButton::clicked(const ModifierKeys &modifiers)
  {
    ToggleButton::clicked(modifiers);

    if (!modifiers.isPopupMenu())
      for (ButtonListener *listener : button_listeners_)
        listener->guiChanged(this);
  }


  void ShapeButtonComponent::render(OpenGlWrapper &open_gl, bool animate)
  {
    incrementHover();

    Colour active_color;
    Colour hover_color;
    if (button_->getToggleState() && use_on_colors_)
    {
      if (down_)
        active_color = on_down_color_;
      else
        active_color = on_normal_color_;

      hover_color = on_hover_color_;
    }
    else
    {
      if (down_)
        active_color = off_down_color_;
      else
        active_color = off_normal_color_;

      hover_color = off_hover_color_;
    }

    if (!down_)
      active_color = active_color.interpolatedWith(hover_color, hover_amount_);

    shape_.setColor(active_color);
    shape_.render(open_gl, animate);
  }

  GeneralButtonComponent::GeneralButtonComponent(Button *button) : button_(button)
  {
    background_.setTargetComponent(button);
    background_.setColor(Colours::orange);
    background_.setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);

    addChildComponent(text_);
    text_.setActive(false);
    text_.setScissor(true);
    text_.setComponent(button);
    text_.setFontType(PlainTextComponent::kText);
  }

  void GeneralButtonComponent::setColors()
  {
    if (button_->findParentComponentOfClass<InterfaceEngineLink>() == nullptr)
      return;

    body_color_ = button_->findColour(Skin::kBody, true);
    if (style_ == kTextButton || style_ == kJustText)
    {
      on_color_ = button_->findColour(Skin::kIconButtonOn, true);
      on_pressed_color_ = button_->findColour(Skin::kIconButtonOnPressed, true);
      on_hover_color_ = button_->findColour(Skin::kIconButtonOnHover, true);
      off_color_ = button_->findColour(Skin::kIconButtonOff, true);
      off_pressed_color_ = button_->findColour(Skin::kIconButtonOffPressed, true);
      off_hover_color_ = button_->findColour(Skin::kIconButtonOffHover, true);
      background_color_ = button_->findColour(Skin::kTextComponentBackground, true);
    }
    else if (style_ == kPowerButton)
    {
      on_color_ = button_->findColour(Skin::kPowerButtonOn, true);
      on_pressed_color_ = button_->findColour(Skin::kOverlayScreen, true);
      on_hover_color_ = button_->findColour(Skin::kLightenScreen, true);
      off_color_ = button_->findColour(Skin::kPowerButtonOff, true);
      off_pressed_color_ = on_pressed_color_;
      off_hover_color_ = on_hover_color_;
      background_color_ = on_color_;
    }
    else if (style_ == kUiButton)
    {
      if (primary_ui_button_)
      {
        on_color_ = button_->findColour(Skin::kUiActionButton, true);
        on_pressed_color_ = button_->findColour(Skin::kUiActionButtonPressed, true);
        on_hover_color_ = button_->findColour(Skin::kUiActionButtonHover, true);
      }
      else
      {
        on_color_ = button_->findColour(Skin::kUiButton, true);
        on_pressed_color_ = button_->findColour(Skin::kUiButtonPressed, true);
        on_hover_color_ = button_->findColour(Skin::kUiButtonHover, true);
      }
      background_color_ = button_->findColour(Skin::kUiButtonText, true);
    }
    else if (style_ == kLightenButton)
    {
      on_color_ = Colours::transparentWhite;
      on_pressed_color_ = button_->findColour(Skin::kOverlayScreen, true);
      on_hover_color_ = button_->findColour(Skin::kLightenScreen, true);
      off_color_ = on_color_;
      off_pressed_color_ = on_pressed_color_;
      off_hover_color_ = on_hover_color_;
      background_color_ = on_color_;
    }
  }

  void GeneralButtonComponent::renderTextButton(OpenGlWrapper &open_gl, bool animate)
  {
    incrementHover();

    Colour active_color;
    Colour hover_color;
    if (button_->getToggleState() && show_on_colors_)
    {
      if (down_)
        active_color = on_pressed_color_;
      else
        active_color = on_color_;

      hover_color = on_hover_color_;
    }
    else
    {
      if (down_)
        active_color = off_pressed_color_;
      else
        active_color = off_color_;

      hover_color = off_hover_color_;
    }

    if (!down_)
      active_color = active_color.interpolatedWith(hover_color, hover_amount_);

    background_.setRounding(findValue(Skin::kLabelBackgroundRounding));
    if (!text_.isActive())
    {
      background_.setColor(active_color);
      background_.render(open_gl, animate);
      return;
    }

    if (style_ != kJustText)
    {
      background_.setColor(background_color_);
      background_.render(open_gl, animate);
    }
    text_.setColor(active_color);
    text_.render(open_gl, animate);
  }

  void GeneralButtonComponent::renderPowerButton(OpenGlWrapper &open_gl, bool animate)
  {
    static constexpr float kPowerRadius = 0.45f;
    static constexpr float kPowerHoverRadius = 0.65f;

    if (button_->getToggleState())
      background_.setColor(on_color_);
    else
      background_.setColor(off_color_);

    background_.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
    background_.render(open_gl, animate);

    incrementHover();

    background_.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
    if (down_)
    {
      background_.setColor(on_pressed_color_);
      background_.render(open_gl, animate);
    }
    else if (hover_amount_ != 0.0f)
    {
      background_.setColor(on_hover_color_.withMultipliedAlpha(hover_amount_));
      background_.render(open_gl, animate);
    }
  }

  void GeneralButtonComponent::renderUiButton(OpenGlWrapper &open_gl, bool animate)
  {
    bool enabled = button_->isEnabled();
    incrementHover();

    Colour active_color;
    if (down_)
      active_color = on_pressed_color_;
    else
      active_color = on_color_;

    if (!down_ && enabled)
      active_color = active_color.interpolatedWith(on_hover_color_, hover_amount_);

    background_.setRounding(findValue(Skin::kLabelBackgroundRounding));
    background_.setColor(active_color);
    background_.render(open_gl, animate);

    text_.setColor(background_color_);
    if (!enabled)
    {
      text_.setColor(on_color_);

      float border_x = 4.0f / (float)button_->getWidth();
      float border_y = 4.0f / (float)button_->getHeight();
      background_.setQuad(0, -1.0f + border_x, -1.0f + border_y, 2.0f - 2.0f * border_x, 2.0f - 2.0f * border_y);
      background_.setColor(body_color_);
      background_.render(open_gl, animate);

      background_.setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
    }

    text_.render(open_gl, animate);
  }

  void GeneralButtonComponent::renderLightenButton(OpenGlWrapper &open_gl, bool animate)
  {
    bool enabled = button_->isEnabled();
    incrementHover();

    Colour active_color;
    if (down_)
      active_color = on_pressed_color_;
    else
      active_color = on_color_;

    if (!down_ && enabled)
      active_color = active_color.interpolatedWith(on_hover_color_, hover_amount_);

    background_.setRounding(findValue(Skin::kLabelBackgroundRounding));
    background_.setColor(active_color);
    background_.render(open_gl, animate);
  }

  void GeneralButton::resized()
  {
    static constexpr float kUiButtonSizeMult = 0.45f;

    ToggleButton::resized();
    BaseSection *section = findParentComponentOfClass<BaseSection>();
    getGlComponent()->setText();
    getGlComponent()->background().markDirty();
    if (section)
    {
      if (getGlComponent()->style() == GeneralButtonComponent::kUiButton)
      {
        getGlComponent()->text().setFontType(PlainTextComponent::kText);
        getGlComponent()->text().setTextSize(kUiButtonSizeMult * (float)getHeight());
      }
      else
        getGlComponent()->text().setTextSize(section->findValue(Skin::kButtonFontSize));
      button_component_.setColors();
    }
  }

  void GeneralButton::clicked()
  {
    ToggleButton::clicked();
    if (details_.stringLookup)
      setText(details_.stringLookup[getToggleState() ? 1 : 0].data());
  }

}
