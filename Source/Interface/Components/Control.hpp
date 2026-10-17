
// Created: 2023-07-31 19:37:15

#pragma once

#include "Framework/metadata.hpp"
#include "../LookAndFeel/Component.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "TextEditor.hpp"

class Parameter;
class HostParameter;

class Control : public Component
{
public:
  static constexpr int kLabelMargin = 4;
  static constexpr int kDefaultPreferredIntegerCharacters = 2;
  static constexpr int kDefaultMaxDecimalCharacters = 2;
  static constexpr float kSlowDragMultiplier = 0.1f;
  static constexpr float kDefaultSensitivity = 1.0f;
  static constexpr float kPopupPrimaryFontHeight = 13.0f;
  static constexpr float kPopupSecondaryFontHeight = 11.0f;

  static constexpr double kUndoTimeout = 0.5; //s

  Control();

  // returns the replaced parameter
  Parameter *setParameter(Parameter *parameter);

  Parameter *changeLinkedParameter(Parameter &parameter,
    bool getValueFromParameter = true);

  float getValue() const { return value.load(satomi::memory_order_relaxed); }
  bool setValue(float newValue, bool setParameter);
  void resetValue();
  void setValueToHost() const;

  void getScaledValueString(utils::string &outString,
    float value, bool addPrefix = true) const;
  float getValueFromText(utils::string_view text) const;

  void createPopupMenu(PopupSelector *selector, Point<i32> position);
  void createParameterUpdate(float previousValue, bool forceOverwrite);

  void showTextEntry();
  float getNumericTextMaxWidth(FontId usedFont, float lineHeight);

  void showPopup(bool primary = true);
  void hidePopup(bool primary = true);

  satomi::atomic<float> value = 0.0;
  float sensitivity = kDefaultSensitivity;
  ModifierKeys resetValueModifiers;
  double lastEditTime{};

  utils::string prefix{};
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
    bool horizotalDrag : 1 = false;
    bool canLoopAround : 1 = false;
    bool bipolar : 1 = false;
    bool shouldMoveOnValueChange : 1 = false;
    bool resetValueOnDoubleClick : 1 = true;

    // state flags
    bool isEnabled : 1 = true;
    bool isInSensitiveMode : 1 = false;
    bool isTextEntryVisible : 1 = false;
    bool isInModalState : 1 = false;
  } controlFlags{};

  Point<i8> popupOffset{ 0, kPopupToElement };
  u8 preferredIntegerCharacters = kDefaultPreferredIntegerCharacters;
  u8 maxDecimalCharacters = kDefaultMaxDecimalCharacters;
  Point<i32> lastMouseDragPosition{};

  Parameter *parameter = nullptr;
  ParameterDetails details{};

  void (*valueChangedCallback)(Control *control,
    float newValue, float oldValue) = nullptr;
};

struct ParameterUpdate final : public UndoAction
{
  ParameterUpdate(Control *control, float oldValue, float newValue) :
    control{ control }, oldValue{ oldValue }, newValue{ newValue }
  {
    UndoAction::redo = redo;
    UndoAction::undo = undo;
  }

  static void redo(UndoAction *a)
  {
    auto *self = (ParameterUpdate *)a;

    if (self->control->setValue(self->newValue, true))
      self->control->setValueToHost();
  }

  static void undo(UndoAction *a)
  {
    auto *self = (ParameterUpdate *)a;

    if (self->control->setValue(self->oldValue, true))
      self->control->setValueToHost();
  }

  static bool isOfType(UndoAction *a) { return a->redo == redo && a->undo == undo; }

  Control *control;
  float oldValue;
  float newValue;
};

class Label : public TextEditor
{
public:
  Label();

  static Range<i32> getSizeMetrics(Component *c, bool isCalculatingVertical);

  Control *control{};
  void (*cacheString)(TextEditor *self, Control *control) =
    [](TextEditor *self, Control *control)
  {
    self->text.copy(control->details.displayName);
  };
};

class SliderValueEditor final : public Label
{
public:
  SliderValueEditor();

  float cachedValue = 0.0;
};

class Button : public Control
{
public:
  static constexpr int kLabelOffset = 8;
  static constexpr float kHoverIncrement = 0.1f;

  bool mouseDown(const MouseEvent &e) override;
  bool mouseUp(const MouseEvent &e) override;

  bool isOn() const { return ::round(getValue()) != 0.0; }
};

class PowerButton final : public Button
{
public:
  static constexpr int kAddedMargin = 4;

  PowerButton();

  bool render(Graphics &g) override;

  float animationValues[1]{};
};

class RadioButton final : public Button
{
public:
  static constexpr int kAddedMargin = 4;
  static constexpr int kDimensions = 10;

  RadioButton();

  bool render(Graphics &g) override;

  float roundingRatio = 0.25f;

  float animationValues[1]{};
};

class Slider : public Control
{
public:
  bool mouseEnter(const MouseEvent &e) override;
  bool mouseExit(const MouseEvent &e) override;
  bool mouseDown(const MouseEvent &e) override;
  bool mouseDrag(const MouseEvent &e) override;
  bool mouseUp(const MouseEvent &e) override;
  bool mouseWheelMove(const MouseEvent &e) override;
};

class RotarySlider final : public Slider
{
public:
  static constexpr float kRotaryAngle = 0.75f * kPi;
  static constexpr float kDefaultRotaryDragLength = 200.0f;
  static constexpr int kDefaultWidthHeight = 36;
  static constexpr int kLabelOffset = 6;
  static constexpr int kLabelVerticalPadding = 3;

  RotarySlider();

  bool render(Graphics &g) override;
  bool mouseDrag(const MouseEvent &e) override;

  float animationValues[1]{};
  float maxArc = kRotaryAngle;
};

class PinSlider : public Slider
{
public:
  static constexpr int kDefaultWidth = 10;

  PinSlider();

  bool mouseDown(const MouseEvent &e) override;
  bool mouseDrag(const MouseEvent &e) override;

  bool render(Graphics &g) override;

  double runningTotal = 0.0;
};

class TextSelector final : public Slider
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

  TextSelector();

  bool render(Graphics &g) override;

  bool mouseDown(const MouseEvent &e) override;
  bool mouseDrag(const MouseEvent &) override { return false; }
  bool mouseUp(const MouseEvent &) override { return true; }
  bool mouseWheelMove(const MouseEvent &e) override;

  SliderValueEditor text{};
  DrawComponent downArrow{};

  utils::string dropdownTitle{};

  float animationValues[1]{};
  Placement dropdownPlacement = Placement::bottom;
  Point<i8> dropdownOffset{ 0, kPopupToElement };
  bool isDropdownOpen = false;
};

class Numberbox final : public Slider
{
public:
  static constexpr int kLabelOffset = 4;

  static constexpr float kTriangleWidthRatio = 0.5f;
  static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
  static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

  Numberbox();

  bool render(Graphics &g) override;

  bool mouseDrag(const MouseEvent &e) override;

  SliderValueEditor editor{};

  float animationValues[1]{};
  float backgroundRounding = 4.0f;
  bool drawBackgroundArrow = true;
  bool isEditing = false;
};

struct CombinationRotarySlider final : Component
{
  CombinationRotarySlider();

  void setModifier(TextSelector *newModifier);

  RotarySlider rotary{};
  Component infoSection{};
  Label label{};
  SliderValueEditor valueEditor{};

  TextSelector *modifier{};
};

struct PinBoundsBox : public Component
{
  static constexpr int kAdditionalPinWidth = 24;

  PinBoundsBox();

  bool render(Graphics &g) override;

  static void paintHighlightBox(Component *component, Graphics &g, float lowBoundValue,
    float highBoundValue, Colour colour, Colour backgroundColour);

  float rounding[4]{};

  PinSlider lowBound{};
  PinSlider highBound{};

  Colour backgroundColour{};
};
