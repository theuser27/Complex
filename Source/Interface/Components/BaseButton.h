/*
  ==============================================================================

    PluginButton.h
    Created: 14 Dec 2022 7:01:32am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/parameter_value.h"
#include "OpenGlImageComponent.h"
#include "OpenGlMultiQuad.h"

namespace Interface
{
  class BaseButton : public ToggleButton, public ParameterUI
  {
  public:
    enum MenuId
    {
      kCancel = 0,
      kArmMidiLearn,
      kClearMidiLearn
    };

    class ButtonListener
    {
    public:
      virtual ~ButtonListener() = default;
      virtual void guiChanged([[maybe_unused]] BaseButton *button) { }
    };

    BaseButton(Framework::ParameterValue *parameter, String name = {});
    ~BaseButton() override = default;

    virtual OpenGlComponent *getGlComponent() = 0;

    // Inherited via ToggleButton
    void mouseDown(const MouseEvent &e) override;
    void mouseUp(const MouseEvent &e) override;

    void clicked() override;

    // Inherited via ParameterUI
    bool updateValue() override
    {
      bool newValue = (bool)std::round(getValueSafe());
      bool needsUpdate = getToggleState() != newValue;
      if (needsUpdate)
        setToggleState(newValue, NotificationType::dontSendNotification);
      return needsUpdate;
    }

    void endChange() override;

    void updateValueFromParameter()
    {
      auto value = (bool)std::round(parameterLink_->parameter->getNormalisedValue());
      setToggleState(value, sendNotificationSync);
      setValueSafe(value);
    }

    std::span<const std::string_view> getStringLookup() const { return details_.stringLookup; }
    void setStringLookup(std::span<const std::string_view> lookup) { details_.stringLookup = lookup; }

    String getTextFromValue(bool value) const noexcept;
    void handlePopupResult(int result);

    void addButtonListener(ButtonListener *listener) { button_listeners_.push_back(listener); }

  protected:
    void clicked(const ModifierKeys &modifiers) override;

    bool hasParameterAssignment_ = false;

    std::vector<ButtonListener *> button_listeners_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
  };

  // TODO: try to combine the shape and text buttons
  class ShapeButtonComponent : public OpenGlComponent
  {
  public:
    static constexpr float kHoverInc = 0.2f;

    ShapeButtonComponent(Button *button) : button_(button)
    {
      shape_.setComponent(button);
      shape_.setScissor(true);
    }

    void parentHierarchyChanged() override;

    void setColors()
    {
      off_normal_color_ = button_->findColour(Skin::kIconButtonOff, true);
      off_hover_color_ = button_->findColour(Skin::kIconButtonOffHover, true);
      off_down_color_ = button_->findColour(Skin::kIconButtonOffPressed, true);
      on_normal_color_ = button_->findColour(Skin::kIconButtonOn, true);
      on_hover_color_ = button_->findColour(Skin::kIconButtonOnHover, true);
      on_down_color_ = button_->findColour(Skin::kIconButtonOnPressed, true);
    }

    void incrementHover()
    {
      if (hover_)
        hover_amount_ = std::min(1.0f, hover_amount_ + kHoverInc);
      else
        hover_amount_ = std::max(0.0f, hover_amount_ - kHoverInc);
    }

    void init(OpenGlWrapper &open_gl) override
    {
      OpenGlComponent::init(open_gl);
      shape_.init(open_gl);
    };

    void render(OpenGlWrapper &open_gl, bool animate) override;

    void destroy(OpenGlWrapper &open_gl) override
    {
      OpenGlComponent::destroy(open_gl);
      shape_.destroy(open_gl);
    };

    void redoImage() { shape_.redrawImage(); setColors(); }

    void useOnColors(bool use) { use_on_colors_ = use; }

    void setShape(const Path &shape) { shape_.setShape(shape); }
    void setDown(bool down) { down_ = down; }
    void setHover(bool hover) { hover_ = hover; }

  private:
    Button *button_ = nullptr;

    bool down_ = false;
    bool hover_ = false;
    float hover_amount_ = 0.0f;
    bool use_on_colors_ = false;
    PlainShapeComponent shape_{ "shape" };

    Colour off_normal_color_{};
    Colour off_hover_color_{};
    Colour off_down_color_{};
    Colour on_normal_color_{};
    Colour on_hover_color_{};
    Colour on_down_color_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeButtonComponent)
  };

  class ShapeButton : public BaseButton
  {
  public:
    ShapeButton(Framework::ParameterValue *parameter, String name = {}) :
  		BaseButton(parameter, std::move(name)) { }

    OpenGlComponent *getGlComponent() override { return &gl_component_; }

    void setShape(const Path &shape) { gl_component_.setShape(shape); }
    void useOnColors(bool use) { gl_component_.useOnColors(use); }

    void resized() override
    {
      BaseButton::resized();
      gl_component_.redoImage();
    }

    void mouseEnter(const MouseEvent &e) override
    {
      BaseButton::mouseEnter(e);
      gl_component_.setHover(true);
    }

    void mouseExit(const MouseEvent &e) override
    {
      BaseButton::mouseExit(e);
      gl_component_.setHover(false);
    }

    void mouseDown(const MouseEvent &e) override
    {
      BaseButton::mouseDown(e);
      gl_component_.setDown(true);
    }

    void mouseUp(const MouseEvent &e) override
    {
      BaseButton::mouseUp(e);
      gl_component_.setDown(false);
    }

  private:
    ShapeButtonComponent gl_component_{ this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeButton)
  };

  class GeneralButtonComponent : public OpenGlComponent
  {
  public:
    static constexpr float kHoverInc = 0.2f;

    enum ButtonStyle
    {
      kTextButton,
      kJustText,
      kPowerButton,
      kUiButton,
      kLightenButton,
      kNumButtonStyles
    };

    GeneralButtonComponent(Button *button);

    void init(OpenGlWrapper &open_gl) override
    {
      if (style_ == kPowerButton)
        background_.setFragmentShader(Shaders::kCircleFragment);

      background_.init(open_gl);
      text_.init(open_gl);

      setColors();
    }

    void setColors();

    void renderTextButton(OpenGlWrapper &open_gl, bool animate);
    void renderPowerButton(OpenGlWrapper &open_gl, bool animate);
    void renderUiButton(OpenGlWrapper &open_gl, bool animate);
    void renderLightenButton(OpenGlWrapper &open_gl, bool animate);

    void incrementHover()
    {
      if (hover_)
        hover_amount_ = std::min(1.0f, hover_amount_ + kHoverInc);
      else
        hover_amount_ = std::max(0.0f, hover_amount_ - kHoverInc);
    }

    void render(OpenGlWrapper &open_gl, bool animate) override
    {
      if (style_ == kTextButton || style_ == kJustText)
        renderTextButton(open_gl, animate);
      else if (style_ == kPowerButton)
        renderPowerButton(open_gl, animate);
      else if (style_ == kUiButton)
        renderUiButton(open_gl, animate);
      else if (style_ == kLightenButton)
        renderLightenButton(open_gl, animate);
    }

    void setText() noexcept
    {
      String text = button_->getButtonText();
      if (!text.isEmpty())
      {
        text_.setActive(true);
        text_.setText(text);
      }
    }

    void setDown(bool down) noexcept { down_ = down; }
    void setHover(bool hover) noexcept { hover_ = hover; }

    void destroy(OpenGlWrapper &open_gl) override
    {
      background_.destroy(open_gl);
      text_.destroy(open_gl);
    }

    void setJustification(Justification justification) noexcept { text_.setJustification(justification); }
    void setStyle(ButtonStyle style) noexcept { style_ = style; }
    void setShowOnColors(bool show) noexcept { show_on_colors_ = show; }
    void setPrimaryUiButton(bool primary) noexcept { primary_ui_button_ = primary; }

    void paint([[maybe_unused]] Graphics &g) override { }

    OpenGlQuad &background() noexcept { return background_; }
    PlainTextComponent &text() noexcept { return text_; }
    ButtonStyle style() const noexcept { return style_; }

  protected:
    ButtonStyle style_ = kTextButton;
    Button *button_ = nullptr;
    bool show_on_colors_ = true;
    bool primary_ui_button_ = false;
    bool down_ = false;
    bool hover_ = false;
    float hover_amount_ = 0.0f;
    OpenGlQuad background_{ Shaders::kRoundedRectangleFragment };
    PlainTextComponent text_{ "text", "" };

    Colour on_color_{};
    Colour on_pressed_color_{};
    Colour on_hover_color_{};
    Colour off_color_{};
    Colour off_pressed_color_{};
    Colour off_hover_color_{};
    Colour background_color_{};
    Colour body_color_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralButtonComponent)
  };

  class GeneralButton : public BaseButton
  {
  public:
    GeneralButton(Framework::ParameterValue *parameter, String name = {}) :
  		BaseButton(parameter, std::move(name)) { }

    GeneralButtonComponent *getGlComponent() override { return &button_component_; }

    void setActive(bool active = true) { active_ = active; }
    bool isActive() const { return active_; }

    void resized() override;

    void setText(const String &newText)
    {
      setButtonText(newText);
      button_component_.setText();
    }

    void setPowerButton()
    { button_component_.setStyle(GeneralButtonComponent::kPowerButton); }

    void setNoBackground()
    { button_component_.setStyle(GeneralButtonComponent::kJustText); }

  	void setTextButton()
    { button_component_.setStyle(GeneralButtonComponent::kTextButton); }

    void setJustification(Justification justification)
    { button_component_.setJustification(justification); }

    void setLightenButton()
    { button_component_.setStyle(GeneralButtonComponent::kLightenButton); }

    void setShowOnColors(bool show)
    { button_component_.setShowOnColors(show); }

    void setUiButton(bool primary)
    {
      button_component_.setStyle(GeneralButtonComponent::kUiButton);
      button_component_.setPrimaryUiButton(primary);
    }

    void enablementChanged() override
    {
      BaseButton::enablementChanged();
      button_component_.setColors();
    }

    void mouseEnter(const MouseEvent &e) override
    {
      BaseButton::mouseEnter(e);
      button_component_.setHover(true);
    }

    void mouseExit(const MouseEvent &e) override
    {
      BaseButton::mouseExit(e);
      button_component_.setHover(false);
    }

    void mouseDown(const MouseEvent &e) override
    {
      BaseButton::mouseDown(e);
      button_component_.setDown(true);
    }

    void mouseUp(const MouseEvent &e) override
    {
      BaseButton::mouseUp(e);
      button_component_.setDown(false);
    }

    void clicked() override;

  private:
    bool active_ = true;
    GeneralButtonComponent button_component_{ this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralButton)
  };

}
