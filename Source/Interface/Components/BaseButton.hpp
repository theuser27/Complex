/*
  ==============================================================================

    PluginButton.hpp
    Created: 14 Dec 2022 7:01:32am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "BaseControl.hpp"

#include "../Components/OpenGlQuad.hpp"
#include "../Components/OpenGlImage.hpp"

namespace Interface
{
  class BaseButton;
  class PlainShapeComponent;
  class OpenGlQuad;

  class BaseButton : public BaseControl
  {
  public:
    static constexpr int kLabelOffset = 8;
    static constexpr float kHoverIncrement = 0.1f;

    BaseButton(Framework::ParameterValue *parameter);
    ~BaseButton() override;

    void setExtraElementsPositions([[maybe_unused]] juce::Rectangle<int> anchorBounds) override { }
    void showTextEntry() final { }
    void enablementChanged() override
    {
      updateState(isMouseOver(true), isMouseButtonDown());
      setColours();
    }

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;

    bool getValue() const noexcept { return std::round(getValueRaw()) == 1.0; }
    void setValue(double shouldBeOn, juce::NotificationType notification) final;
    void valueChanged() override
    {
      redoImage();
      BaseControl::valueChanged();
    }
    juce::String getScaledValueString(double value, bool) const override
    { return juce::String{ juce::roundToInt(value) }; }

    bool isHeldDown() const noexcept { return isHeldDown_; }
    bool isHoveredOver() const noexcept { return isHoveredOver_; }

    juce::String getTextFromValue(bool value) const noexcept;

  protected:
    void updateState(bool isHeldDown, bool isHoveredOver) noexcept;

    utils::shared_value<bool> isHeldDown_ = false;
    utils::shared_value<bool> isHoveredOver_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
  };

  class PowerButton final : public BaseButton
  {
  public:
    static constexpr int kAddedMargin = 4;

    PowerButton(Framework::ParameterValue *parameter);
    ~PowerButton() override;

    void setColours() override;
    juce::Rectangle<int> setSizes(int height, int width = 0) override;
    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;

  private:
    PlainShapeComponent shapeComponent_{ "Power Button Shape" };

    juce::Colour onNormalColor_{};

    utils::shared_value<juce::Colour> activeColour_{};
    utils::shared_value<juce::Colour> hoverColour_{};
  };

  class RadioButton final : public BaseButton
  {
  public:
    static constexpr int kAddedMargin = 4;

    RadioButton(Framework::ParameterValue *parameter);
    ~RadioButton() override;

    void setColours() override;
    juce::Rectangle<int> setSizes(int height, int width = 0) override;
    void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) override;
    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;

    void setRounding(float rounding) noexcept;

  private:
    OpenGlQuad backgroundComponent_{ Shaders::kRoundedRectangleFragment, "Radio Button Background" };

    utils::shared_value<juce::Colour> onNormalColor_{};
    utils::shared_value<juce::Colour> offNormalColor_{};
    utils::shared_value<juce::Colour> backgroundColor_{};
  };

  class OptionsButton final : public BaseButton
  {
  public:
    static constexpr float kPlusRelativeSize = 7;
    static constexpr float kBorderRounding = 8.0f;

    OptionsButton(Framework::ParameterValue *parameter, 
      juce::String name = {}, juce::String displayText = {});
    ~OptionsButton() override;

    void mouseDown(const juce::MouseEvent &e) override
    {
      if (!e.mods.isPopupMenu())
        BaseButton::mouseDown(e);
    }
    void valueChanged() override
    {
      showPopupSelector(this, popupPlacement_, popupOptions_, 
        popupHandler_, cancelHandler_);
    }

    void setColours() override;
    juce::Rectangle<int> setSizes(int height, int width) override;
    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;

    void setText(juce::String text) { text_ = std::move(text); }
    void setOptions(PopupItems options) { popupOptions_ = std::move(options); }
    void setPopupPlacement(Placement placement) { popupPlacement_ = placement; }
    void setPopupHandler(std::function<void(int)> handler) { popupHandler_ = std::move(handler); }
    void setCancelHandler(std::function<void()> handler) { cancelHandler_ = std::move(handler); }

  protected:
    OpenGlQuad plusComponent_{ Shaders::kPlusFragment, "Options Button Plus Icon" };
    OpenGlQuad borderComponent_{ Shaders::kRoundedRectangleBorderFragment, "Options Button Border" };
    PlainTextComponent textComponent_;

    PopupItems popupOptions_{};
    Placement popupPlacement_ = Placement::below;
    std::function<void(int)> popupHandler_{ [](int) { } };
    std::function<void()> cancelHandler_{ []() { } };

    juce::String text_{};
    juce::Colour borderColour_{};
  };

  class ActionButton final : public BaseButton
  {
  public:
    static constexpr float kBorderRounding = 8.0f;

    ActionButton(juce::String name = {}, juce::String displayText = {});
    ~ActionButton() override;

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void setColours() override;
    juce::Rectangle<int> setSizes(int height, int width) override;
    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;

    void setText(juce::String text) { text_ = std::move(text); }
    void setAction(std::function<void()> action) { action_ = std::move(action); }
  protected:
    OpenGlQuad fillComponent_{ Shaders::kRoundedRectangleFragment, "Action Button Fill" };
    PlainTextComponent textComponent_;

    std::function<void()> action_{};

    juce::String text_{};
    juce::Colour fillColour_{};
    juce::Colour textColour_{};
  };

}
