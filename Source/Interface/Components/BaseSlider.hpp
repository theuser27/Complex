/*
  ==============================================================================

    BaseSlider.hpp
    Created: 14 Dec 2022 6:59:59am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/Miscellaneous.hpp"
#include "OpenGlQuad.hpp"
#include "OpenGlImage.hpp"
#include "BaseControl.hpp"

namespace Framework
{
  class ParameterBridge;
}

namespace Interface
{
  class BaseSlider : public BaseControl, public OpenGlTextEditorListener
  {
  public:
    static constexpr int kDefaultMaxTotalCharacters = 5;
    static constexpr int kDefaultMaxDecimalCharacters = 2;

    static constexpr float kSlowDragMultiplier = 0.1f;
    static constexpr float kDefaultSensitivity = 1.0f;

    enum SliderType : u8
    {
      IsHorizontalDrag = 1,
      Bipolar = 1 << 1,
      CanLoopAround = 1 << 2,
      ShouldMoveOnValueChange = 1 << 3,
    };

    enum DragMode
    {
      notDragging,    /// Dragging is not active.
      absoluteDrag,   /// The dragging corresponds directly to the value that is displayed.
      velocityDrag    /// The dragging value change is relative to the velocity of the mouse movement.
    };

    BaseSlider(Framework::ParameterValue *parameter);
    ~BaseSlider() override;
    
    // ========================================================== Mouse related
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) override;

    // ========================================================== Value related
    auto getValue() const noexcept { return getValueRaw(); }
    void setValue(double newValue, juce::NotificationType notification = juce::sendNotificationSync) final;
    void valueChanged() override
    {
      redoImage();
      BaseControl::valueChanged();
    }
    Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
      bool getValueFromParameter = true) override;
    void updateValueFromTextEntry();
    juce::String getScaledValueString(double value, bool addPrefix = true) const override;
    double snapValue(double attemptedValue, DragMode dragMode);
    void snapToValue(bool shouldSnap, float snapValue = 0.0f)
    {
      shouldSnapToValue_ = shouldSnap;
      snapValue_ = snapValue;
    }
    juce::String formatValue(double value) const noexcept;
    float getNumericTextMaxWidth(const juce::Font &usedFont) const;
    void setValueInterval();
    void setResetValue(double resetValue, bool resetWithDoubleClick = true, 
      juce::ModifierKeys mods = juce::ModifierKeys::ctrlAltCommandModifiers) noexcept
    {
      resetValueOnDoubleClick_ = resetWithDoubleClick;
      resetValue_ = resetValue;
      resetValueModifiers_ = mods;
    }

    // ===================================================== Text Entry related
    void addTextEntry();
    void removeTextEntry();
    void changeTextEntryFont(juce::Font font);
    // override to set the colours of the entry box
    void showTextEntry() override;
    void textEditorReturnKeyPressed(TextEditor &editor) override;

    // ========================================================== Popup related
    void showPopup(bool primary);
    void hidePopup(bool primary);

    // ======================================================== Drawing related
    bool isVertical() const noexcept { return !isHorizontal(); }
    bool isHorizontal() const noexcept { return type_ & SliderType::IsHorizontalDrag; }
    bool isBipolar() const noexcept { return type_ & SliderType::Bipolar; }
    bool hasModulationArea() const noexcept { return modulationArea_.getWidth(); }

    Placement getPopupPlacement() const noexcept { return popupPlacement_; }
    Placement getModulationPlacement() const noexcept { return modulationControlPlacement_; }
    juce::Rectangle<int> getModulationArea() const noexcept 
    { return (modulationArea_.getWidth()) ? modulationArea_ : getLocalBounds(); }

    virtual juce::Colour getSelectedColor() const { return getColour(Skin::kWidgetPrimary1); }
    virtual juce::Colour getUnselectedColor() const { return juce::Colours::transparentBlack; }
    virtual juce::Colour getThumbColor() const { return juce::Colours::white; }
    virtual juce::Colour getBackgroundColor() const { return getColour(Skin::kWidgetBackground1); }
    virtual juce::Colour getModColor() const { return getColour(Skin::kModulationMeterControl); }
    virtual juce::Rectangle<int> getModulationMeterBounds() const { return getLocalBounds(); }

    void setBipolar(bool bipolar = true)
    {
      if (isBipolar() == bipolar)
        return;

      type_ = (SliderType)(type_ ^ SliderType::Bipolar);
      redoImage();
    }

    void setActive(bool active = true)
    {
      if (isActive() == active)
        return;

      isActive_ = active;
      setColours();
      redoImage();
    }

    void setColours() override
    {
      BaseControl::setColours();

      thumbColor_ = getThumbColor();
      selectedColor_ = getSelectedColor();
      unselectedColor_ = getUnselectedColor();
      backgroundColor_ = getBackgroundColor();
      modColor_ = getModColor();
    }

    void setShouldShowPopup(bool shouldShowPopup) noexcept { shouldShowPopup_ = shouldShowPopup; }
    void setShowPopupOnHover(bool show) noexcept { showPopupOnHover_ = show; }
    void setShouldUsePlusMinusPrefix(bool shouldUsePlusMinus) noexcept { shouldUsePlusMinusPrefix_ = shouldUsePlusMinus; }
    void setCanInputValue(bool canInputValue) noexcept { canInputValue_ = canInputValue; }
    void setCanUseScrollWheel(bool canUseScrollWheel) noexcept { canUseScrollWheel_ = canUseScrollWheel; }

    void setSensitivity(double sensitivity) noexcept { sensitivity_ = sensitivity; }
    void setPopupPlacement(Placement placement) { popupPlacement_ = placement; }
    void setModulationPlacement(Placement placement) { modulationControlPlacement_ = placement; }
    void setModulationArea(juce::Rectangle<int> area) noexcept { modulationArea_ = area; }
    void setMaxTotalCharacters(int totalCharacters) noexcept
    { COMPLEX_ASSERT(totalCharacters > 0); maxTotalCharacters_ = totalCharacters; }
    void setMaxDecimalCharacters(int decimalCharacters) noexcept
    { COMPLEX_ASSERT(decimalCharacters >= 0); maxDecimalCharacters_ = decimalCharacters; }
    void setPopupPrefix(juce::String prefix) noexcept { popupPrefix_ = COMPLEX_MOVE(prefix); }

    // ========================================================== Miscellaneous
    using BaseControl::getValue;
    void modifierKeysChanged(const juce::ModifierKeys &) override { }
  protected:
    float getSampleRate() const noexcept;
    void setImmediateSensitivity(double immediateSensitivity) noexcept { immediateSensitivity_ = immediateSensitivity; }

    // ============================================================== Variables
    utils::shared_value<juce::Colour> thumbColor_;
    utils::shared_value<juce::Colour> selectedColor_;
    utils::shared_value<juce::Colour> unselectedColor_;
    utils::shared_value<juce::Colour> backgroundColor_;
    utils::shared_value<juce::Colour> modColor_;

    SliderType type_{};
    bool shouldShowPopup_ = false;
    bool showPopupOnHover_ = false;
    bool shouldUsePlusMinusPrefix_ = false;
    bool canUseScrollWheel_ = false;
    bool shouldSnapToValue_ = false;
    bool sensitiveMode_ = false;

    double snapValue_ = 0.0;
    double sensitivity_ = kDefaultSensitivity;
    int maxTotalCharacters_ = kDefaultMaxTotalCharacters;
    int maxDecimalCharacters_ = kDefaultMaxDecimalCharacters;

    Placement popupPlacement_ = Placement::below;
    juce::String popupPrefix_{};

    juce::Rectangle<int> modulationArea_{};
    Placement modulationControlPlacement_ = Placement::below;

    OpenGlQuad quadComponent_{ Shaders::kRotarySliderFragment, "Slider Quad" };
    OpenGlImage imageComponent_{ "Slider Image" };
    utils::up<TextEditor> textEntry_;

    // ============================================================== Internals
  private:
    double immediateSensitivity_ = 250.0;
    double valueInterval_ = 0.0;
    juce::Point<float> lastMouseDragPosition_{};
    juce::Time lastMouseWheelTime_;
    juce::ModifierKeys resetValueModifiers_;
    bool resetValueOnDoubleClick_ = true;
    bool useDragEvents_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseSlider)
  };

  class TextSelector;

  class RotarySlider final : public BaseSlider
  {
  public:
    static constexpr float kRotaryAngle = 0.75f * kPi;
    static constexpr double kDefaultRotaryDragLength = 200.0;
    static constexpr int kDefaultWidthHeight = 36;
    static constexpr int kDefaultArcDimensions = 36;
    static constexpr int kDefaultBodyDimensions = 23;
    static constexpr int kLabelOffset = 6;
    static constexpr int kLabelVerticalPadding = 3;

    RotarySlider(Framework::ParameterValue *parameter);
    ~RotarySlider() override;

    void resized() override
    {
      setColours();
      setComponentsBounds();
      repositionExtraElements();
    }
    void mouseDrag(const juce::MouseEvent &e) override;

    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;
    void drawShadow(juce::Graphics &g) const;
    void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) override;
    juce::Rectangle<int> setSizes(int height, int width = 0) override;

    void showTextEntry() override;
    void textEditorReturnKeyPressed([[maybe_unused]] TextEditor &editor) override;
    void textEditorEscapeKeyPressed(TextEditor &editor) override;
    void textEditorFocusLost(TextEditor &editor) override { textEditorEscapeKeyPressed(editor); }

    juce::Colour getUnselectedColor() const override
    {
      if (isActive())
        return getColour(Skin::kRotaryArcUnselected);
      return getColour(Skin::kRotaryArcUnselectedDisabled);
    }
    juce::Colour getThumbColor() const override { return getColour(Skin::kRotaryHand); }

    float getKnobSizeScale() const noexcept { return knobSizeScale_; }
    void setKnobSizeScale(float scale) noexcept { knobSizeScale_ = scale; }
    void setMaxArc(float arc) noexcept;

    void setModifier(TextSelector *modifier) noexcept;

  protected:
    utils::shared_value<float> knobArcThickness_{};
    float knobSizeScale_ = 1.0f;
    TextSelector *modifier_ = nullptr;
  };

  class LinearSlider : public BaseSlider
  {
  public:
    static constexpr float kLinearWidthPercent = 0.26f;
    static constexpr float kLinearModulationPercent = 0.1f;

    LinearSlider(Framework::ParameterValue *parameter);

    void mouseDown(const juce::MouseEvent &e) override
    {
      if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
      {
        setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
      }

      BaseSlider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
      float multiply = 1.0f;

      sensitiveMode_ = e.mods.isCommandDown();
      if (sensitiveMode_)
        multiply *= kSlowDragMultiplier;

      setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

      BaseSlider::mouseDrag(e);
    }

    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;
    void showTextEntry() override;

    juce::Rectangle<int> getModulationMeterBounds() const override
    {
      juce::Rectangle<int> mod_bounds = getModulationArea();
      int buffer = (int)getValue(Skin::kWidgetMargin);

      if (isHorizontal())
      {
        return { mod_bounds.getX() + buffer, mod_bounds.getY(),
          mod_bounds.getWidth() - 2 * buffer, mod_bounds.getHeight() };
      }
      return { mod_bounds.getX(), mod_bounds.getY() + buffer,
        mod_bounds.getWidth(), mod_bounds.getHeight() - 2 * buffer };
    }

    juce::Colour getSelectedColor() const override
    {
      if (isActive_)
        return getColour(Skin::kLinearSlider);
      return getColour(Skin::kLinearSliderDisabled);
    }
    juce::Colour getUnselectedColor() const override { return getColour(Skin::kLinearSliderUnselected); }
    juce::Colour getThumbColor() const override
    {
      if (isActive_)
        return getColour(Skin::kLinearSliderThumb);
      return getColour(Skin::kLinearSliderThumbDisabled);
    }

    void setHorizontal() { type_ = (SliderType)(type_ | SliderType::IsHorizontalDrag); }
    void setVertical() { type_ = (SliderType)(type_ ^ (type_ & SliderType::IsHorizontalDrag)); }
  };

  class ImageSlider : public BaseSlider
  {
  public:
    ImageSlider(Framework::ParameterValue *parameter);

    void mouseDown(const juce::MouseEvent &e) override
    {
      if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
      {
        setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
      }

      BaseSlider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
      float multiply = 1.0f;

      sensitiveMode_ = e.mods.isCommandDown();
      if (sensitiveMode_)
        multiply *= kSlowDragMultiplier;

      setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

      BaseSlider::mouseDrag(e);
    }

    void redoImage() override;
  };

  class PinSlider : public BaseSlider
  {
  public:
    static constexpr int kDefaultPinSliderWidth = 10;

    PinSlider(Framework::ParameterValue *parameter);

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;

    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;
    void setExtraElementsPositions([[maybe_unused]] juce::Rectangle<int> anchorBounds) override { }
    juce::Rectangle<int> setSizes(int height, int width = 0) override;

    juce::Colour getThumbColor() const override { return getSelectedColor(); }

    void setTotalRange(double totalRange) noexcept { totalRange_ = totalRange; }

  private:
    double totalRange_ = 0.0;
    juce::Point<double> lastDragPosition_{ 0.0, 0.0 };
    double runningTotal_ = 0.0;
  };

  class NumberBox;

  class TextSelector final : public BaseSlider
  {
  public:
    static constexpr int kDefaultTextSelectorHeight = 16;
    static constexpr int kLabelHMargin = 8;
    static constexpr int kLabelVMargin = 4;

    static constexpr float kHeightFontAscentRatio = 0.5f;
    static constexpr float kPaddingHeightRatio = 0.25f;
    static constexpr float kBetweenElementsMarginHeightRatio = 0.25f;
    static constexpr float kNumberBoxMarginToHeightRatio = 0.25f;
    static constexpr float kHeightToArrowWidthRatio = 5.0f / 16.0f;
    static constexpr float kArrowWidthHeightRatio = 0.5f;

    TextSelector(Framework::ParameterValue *parameter, std::optional<juce::Font> usedFont = {});

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &) override { }
    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) override;

    void valueChanged() override;
    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;
    void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) override;
    juce::Rectangle<int> setSizes(int height, int width = 0) override;

    void addListener(auto *listener) noexcept
    { setTextSelectorListener(listener); BaseSlider::addListener(listener); }
    void removeListener(auto *listener) noexcept
    { setTextSelectorListener(nullptr); BaseSlider::removeListener(listener); }

    juce::Colour getBackgroundColor() const override { return getColour(Skin::kWidgetBackground2); }

    // if the stringLookup changes we need to resize and redraw to fit the new text
    void setParameterDetails(const Framework::ParameterDetails &details) override
    {
      BaseControl::setParameterDetails(details);
      resizeForText();
    }

    void setUsedFont(juce::Font usedFont) noexcept { usedFont_ = COMPLEX_MOVE(usedFont); isDirty_ = true; }
    void setDrawArrow(bool drawArrow) noexcept { drawArrow_ = drawArrow; isDirty_ = true; }
    void setAnchor(Placement anchor) noexcept { anchor_ = anchor; }

    void setExtraIcon(PlainShapeComponent *icon) noexcept;
    void setExtraNumberBox(NumberBox *numberBox) noexcept;
    void setExtraNumberBoxPlacement(Placement placement) noexcept { extraNumberBoxPlacement_ = placement; }
    void setTextSelectorListener(TextSelectorListener *listener) noexcept { textSelectorListener_ = listener; }

    void setOptionsTitle(std::string title) { optionsTitle_ = COMPLEX_MOVE(title); }
    void setItemIgnoreFunction(clg::small_fn<bool(const Framework::IndexedData &, int)> fn) noexcept
    { ignoreItemFunction_ = COMPLEX_MOVE(fn); }

    utils::string_view getTextValue(bool fromParameter = false);

  protected:
    void resizeForText() noexcept;

    std::string optionsTitle_{};
    clg::small_fn<bool(const Framework::IndexedData &, int)> ignoreItemFunction_{};
    juce::Font usedFont_{};
    double lastValue_ = 0.0;
    int textWidth_{};
    bool drawArrow_ = true;
    bool isDirty_ = false;
    bool isDropDownVisible_ = false;
    Placement anchor_ = Placement::left;
    Placement extraNumberBoxPlacement_ = Placement::right;

    PlainShapeComponent *extraIcon_ = nullptr;
    NumberBox *extraNumberBox_ = nullptr;
    TextSelectorListener *textSelectorListener_ = nullptr;
  };

  class NumberBox final : public BaseSlider
  {
  public:
    static constexpr int kDefaultNumberBoxHeight = 16;
    static constexpr int kLabelOffset = 4;

    static constexpr float kTriangleWidthRatio = 0.5f;
    static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
    static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

    NumberBox(Framework::ParameterValue *parameter);

    void mouseDrag(const juce::MouseEvent &e) override;
    void setVisible(bool shouldBeVisible) override;

    void textEditorReturnKeyPressed(TextEditor &editor) override;
    void textEditorEscapeKeyPressed(TextEditor &editor) override;
    void textEditorFocusLost(TextEditor &editor) override { textEditorEscapeKeyPressed(editor); }

    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;
    void showTextEntry() override;
    void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) override;

    juce::Colour getBackgroundColor() const override { return (drawBackground_) ? 
      getColour(Skin::kWidgetBackground1) : getColour(Skin::kWidgetBackground2); }

    juce::Rectangle<int> setSizes(int height, int width = 0) override;

    void setAlternativeMode(bool isAlternativeMode) noexcept;

  protected:
    bool drawBackground_ = true;
    bool isEditing_ = false;
  };

  class ModulationSlider : public BaseSlider
  {
  public:
    ModulationSlider(Framework::ParameterValue *parameter);

    void mouseDown(const juce::MouseEvent &e) override
    {
      if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
      {
        setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
      }

      BaseSlider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
      float multiply = 1.0f;

      sensitiveMode_ = e.mods.isCommandDown();
      if (sensitiveMode_)
        multiply *= kSlowDragMultiplier;

      setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

      BaseSlider::mouseDrag(e);
    }

    void redoImage() override;
    void setComponentsBounds(bool redoImage = true) override;


    juce::Colour getSelectedColor() const override
    {
      juce::Colour background = getColour(Skin::kWidgetBackground1);
      if (isActive_)
        return getColour(Skin::kRotaryArc).interpolatedWith(background, 0.5f);
      return getColour(Skin::kRotaryArcDisabled).interpolatedWith(background, 0.5f);
    }
    juce::Colour getUnselectedColor() const override { return getColour(Skin::kWidgetBackground1); }
    juce::Colour getThumbColor() const override { return getColour(Skin::kRotaryArc); }

    void setDrawWhenNotVisible(bool draw) noexcept;
  };

}
