
// Created: 2023-07-31 19:37:15

#pragma once

#include "Framework/memory.hpp"
#include "Framework/parameter_types.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "TextEditor.hpp"
#include "OpenGlImage.hpp"

namespace Framework
{
  class ParameterValue;
  struct ParameterLink;
  class ParameterBridge;
}

namespace Interface
{
  class ControlListener;
  class PlainTextComponent;
  class BaseSection;

  class BaseControl : public Component
  {
  public:
    static constexpr int kDefaultMaxTotalCharacters = 5;
    static constexpr int kDefaultMaxDecimalCharacters = 2;
    static constexpr float kSlowDragMultiplier = 0.1f;
    static constexpr float kDefaultSensitivity = 1.0f;
    static constexpr float kPopupPrimaryFontHeight = 13.0f;
    static constexpr float kPopupSecondaryFontHeight = 11.0f;
    static constexpr int kPopupMinWidth = 150;

    BaseControl(Framework::ParameterValue *parameter);

    // returns the replaced link
    Framework::ParameterLink *setParameterLink(Framework::ParameterLink *parameterLink);

    Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
      bool getValueFromParameter = true);

    bool setValueFromHost(double value, Framework::ParameterBridge *notifyingBridge);
    void setValueToHost() const;

    double getValue() const { return value.load(satomi::memory_order_relaxed); }
    bool setValue(double newValue, bool notify = true);
    void resetValue();
    virtual void valueChanged();

    utils::string getScaledValueString(utils::Allocator allocator, 
      double value, bool addPrefix = true) const;
    double getValueFromText(utils::string_view text) const;

    void beginChange(double oldValue);
    void endChange();

    void addListener(ControlListener *listener) { controlListeners.emplace_back(listener); }
    void removeListener(ControlListener *listener) { controlListeners.erase(listener); }

    void createPopupMenu(PopupSelector *selector, Point<i32> position);

    void showTextEntry(Font font);
    float getNumericTextMaxWidth(const Font &usedFont) const;

    void showPopup(bool primary = true);
    void hidePopup(bool primary = true);

    satomi::atomic<double> value = 0.0;
    double valueBeforeChange = 0.0;
    double valueInterval = 0.0;
    float sensitivity = kDefaultSensitivity;
    ModifierKeys resetValueModifiers;

    utils::string popupPrefix{};
    Placement popupPlacement = Placement::bottom;

    struct
    {
      // feature flags
      bool shouldShowPopup : 1 = false;
      bool showPopupOnHover : 1 = false;
      bool shouldUsePlusMinusPrefix : 1 = false;
      bool canUseScrollWheel : 1 = false;
      bool shouldCheckDbInfinities : 1 = false;
      bool canInputValue : 1 = false;
      bool shouldRepaintOnHover : 1 = false;
      bool isDraggable : 1 = true;
      bool isHorizotalDraggable : 1 = false;
      bool canLoopAround : 1 = false;
      bool isBipolar : 1 = false;
      bool shouldMoveOnValueChange : 1 = false;
      bool resetValueOnDoubleClick : 1 = true;
      bool isEnabled : 1 = true;

      // state flags
      bool hasParameter : 1 = false;
      bool isInSensitiveMode : 1 = false;
      bool hasBegunChange : 1 = false;
      bool isTextEntryVisible : 1 = false;
      bool isInModalState : 1 = false;
    } controlFlags{};

    Animator animator{};

    u32 maxTotalCharacters = kDefaultMaxTotalCharacters;
    u32 maxDecimalCharacters = kDefaultMaxDecimalCharacters;
    Point<i32> lastMouseDragPosition{};

    Framework::ParameterLink *parameterLink = nullptr;
    Framework::ParameterDetails details{};

    utils::vector<ControlListener *> controlListeners{};
  };

  class Label final : public Component
  {
  public:
    Label();

    bool render(OpenGlWrapper &openGl) override;

    BaseControl *control{};
    utils::string currentString{};
    // default implementation to get the human readable value
    // but it can be exchanged for anything
    utils::string (*getString)(BaseControl *control) = [](BaseControl *control)
    {
      return control->getScaledValueString(control->arena, control->getValue());
    };
    // colour id to use from control when rendering
    Skin::ColourId textColour = Skin::kNormalText;
    Font font = uiRelated.cache->getInterFont();
  };

  class BaseButton : public BaseControl
  {
  public:
    static constexpr int kLabelOffset = 8;
    static constexpr float kHoverIncrement = 0.1f;

    BaseButton(Framework::ParameterValue *parameter) : BaseControl{ parameter } { }

    bool mouseDown(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;

    bool isOn() const { return ::round(getValue()) != 0.0; }
  };

  class PowerButton final : public BaseButton
  {
  public:
    static constexpr int kAddedMargin = 4;

    PowerButton(Framework::ParameterValue *parameter);

    bool render(OpenGlWrapper &openGl) override;
  };

  class RadioButton final : public BaseButton
  {
  public:
    static constexpr int kAddedMargin = 4;

    RadioButton(Framework::ParameterValue *parameter);

    bool render(OpenGlWrapper &openGl) override;


    float rounding = 0.0f;
  };

  class BaseSlider : public BaseControl
  {
  public:
    BaseSlider(Framework::ParameterValue *parameter) : BaseControl{ parameter } { }
    
    bool mouseDown(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseEnter(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &e) override;
  };

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

    bool render(OpenGlWrapper &openGl) override;
    bool mouseDrag(const MouseEvent &e) override;

    float maxArc{};
    float knobArcThickness{};
    float knobSizeScale = 1.0f;
  };

  class PinSlider : public BaseSlider
  {
  public:
    static constexpr int kDefaultPinSliderWidth = 10;

    PinSlider(Framework::ParameterValue *parameter);

    bool render(OpenGlWrapper &openGl) override;

    bool mouseDown(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;

    double totalRange = 0.0;
    double runningTotal = 0.0;
  };

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

    TextSelector(Framework::ParameterValue *parameter);

    bool render(OpenGlWrapper &openGl) override;

    bool mouseDown(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &e) override;

    utils::string_view getTextValue(bool fromParameter = false);

    void setExtraIcon(Paths::DrawingFn *drawFn);

    Label text{};
    PlainShapeComponent downArrow{};

    utils::string dropdownTitle{};
    double lastValue = 0.0;

  private:
    PlainShapeComponent *extraIcon = nullptr;
  };

  class Numberbox final : public BaseSlider
  {
  public:
    static constexpr int kDefaultNumberBoxHeight = 16;
    static constexpr int kLabelOffset = 4;

    static constexpr float kTriangleWidthRatio = 0.5f;
    static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
    static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

    Numberbox(Framework::ParameterValue *parameter);

    bool render(OpenGlWrapper &openGl) override;

    bool mouseDrag(const MouseEvent &e) override;

    bool drawBackground = true;
    bool isEditing = false;

    TextEditor editor{};
  };

  struct CombinationRotarySlider final : Component
  {
  private:
    TextSelector *modifier{};

  public:
    RotarySlider *rotary{};
    Label label{};
    TextEditor valueEditor{};

    CombinationRotarySlider(utils::bumpArena *arenaToUse)
    {
      arena = arenaToUse;
      addChildComponent(&label);
      addChildComponent(&valueEditor);
    }

    void setModifier(TextSelector *newModifier)
    {
      if (modifier)
        removeChildComponent(modifier);

      modifier = newModifier;
      rotary->controlFlags.shouldShowPopup = modifier;
      valueEditor.componentFlags.isVisible = !modifier;

      if (modifier)
        addChildComponent(modifier);
    }
  };

}
