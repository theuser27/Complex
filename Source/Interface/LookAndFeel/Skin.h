/*
  ==============================================================================

    Skin.h
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include <Third Party/json/json.hpp>

using json = nlohmann::json;

namespace Interface
{
  class MainInterface;
  class BaseSection;

  class Skin
  {
  public:
    enum SectionOverride
    {
      kNone,
      kLogo,
      kHeader,
      kOverlay,
      kEffectsChain,
      kEffectModule,
      kUtilityEffect,
      kFilterEffect,
      kContrastEffect,
      kDynamicsEffect,
      kPhaseEffect,
      kPitchEffect,
      kStretchEffect,
      kWarpEffect,
      kDestroyEffect,
      kAllEffects,
      kNumSectionOverrides
    };

    enum ValueId
    {
      kBodyRoundingTop,
      kBodyRoundingBottom,

      kLabelHeight,
      kLabelBackgroundHeight,
      kLabelBackgroundRounding,
      kLabelOffset,
      kTextComponentLabelOffset,

      kRotaryOptionXOffset,
      kRotaryOptionYOffset,
      kRotaryOptionWidth,

      kTitleWidth,
      kPadding,
      kLargePadding,
      kSliderWidth,

      kTextComponentHeight,
      kTextComponentOffset,
      kTextComponentFontSize,
      kTextButtonHeight,

      kButtonFontSize,

      kKnobArcSize,
      kKnobArcThickness,
      kKnobBodySize,
      kKnobHandleLength,

      kKnobModAmountArcSize,
      kKnobModAmountArcThickness,
      kKnobModMeterArcSize,
      kKnobModMeterArcThickness,

      kKnobOffset,
      kKnobSectionHeight,
      kKnobShadowWidth,
      kKnobShadowOffset,

      kModulationButtonWidth,
      kModulationFontSize,

      kWidgetMargin,
      kWidgetRoundedCorner,
      kWidgetLineWidth,
      kWidgetLineBoost,
      kWidgetFillCenter,
      kWidgetFillFade,
      kWidgetFillBoost,

      kNumSkinValueIds,
      kFrequencyDisplay = kNumSkinValueIds,
      kNumAllValueIds,
    };

    enum ColorId
    {
      kInitialColor = 0x42345678,
      kBackground = kInitialColor,
      kBody,
      kBodyHeading,
      kHeadingText,
      kPresetText,
      kBodyText,
      kBorder,
      kLabelBackground,
      kLabelConnection,
      kPowerButtonOn,
      kPowerButtonOff,

      kOverlayScreen,
      kLightenScreen,
      kShadow,
      kPopupSelectorBackground,
      kPopupBackground,
      kPopupBorder,

      kTextComponentBackground,
      kTextComponentText,

      kRotaryArc,
      kRotaryArcDisabled,
      kRotaryArcUnselected,
      kRotaryArcUnselectedDisabled,
      kRotaryHand,
      kRotaryBody,
      kRotaryBodyBorder,

      kLinearSlider,
      kLinearSliderDisabled,
      kLinearSliderUnselected,
      kLinearSliderThumb,
      kLinearSliderThumbDisabled,

      kPinSlider,
      kPinSliderDisabled,
      kPinThumb,
      kPinThumbDisabled,

      kWidgetCenterLine,
      kWidgetPrimary1,
      kWidgetPrimary2,
      kWidgetPrimaryDisabled,
      kWidgetSecondary1,
      kWidgetSecondary2,
      kWidgetSecondaryDisabled,
      kWidgetAccent1,
      kWidgetAccent2,
      kWidgetBackground,

      kModulationMeter,
      kModulationMeterLeft,
      kModulationMeterRight,
      kModulationMeterControl,
      kModulationButtonSelected,
      kModulationButtonDragging,
      kModulationButtonUnselected,

      kIconSelectorIcon,

      kIconButtonOff,
      kIconButtonOffHover,
      kIconButtonOffPressed,
      kIconButtonOn,
      kIconButtonOnHover,
      kIconButtonOnPressed,

      kUiButton,
      kUiButtonText,
      kUiButtonHover,
      kUiButtonPressed,
      kUiActionButton,
      kUiActionButtonHover,
      kUiActionButtonPressed,

      kTextEditorBackground,
      kTextEditorBorder,
      kTextEditorCaret,
      kTextEditorSelection,

      kFinalColor
    };

    static constexpr int kNumColors = kFinalColor - kInitialColor;
    static bool shouldScaleValue(ValueId valueId);

    Skin();

    void applyComponentColors(Component *component) const;
    void applyComponentColors(Component *component, SectionOverride sectionOverride, bool topLevel = false) const;
    void applyComponentValues(BaseSection *component) const;
    void applyComponentValues(BaseSection *component, SectionOverride sectionOverride, bool topLevel = false) const;

    void setColor(ColorId colorId, const Colour &color) { colors_[colorId - kInitialColor] = color; }
    Colour getColor(ColorId colorId) const { return colors_[colorId - kInitialColor]; }
    Colour getColor(int section, ColorId colorId) const;
    bool overridesColor(int section, ColorId colorId) const;
    bool overridesValue(int section, ValueId colorId) const;
    void copyValuesToLookAndFeel(LookAndFeel *lookAndFeel) const;

    void setValue(ValueId valueId, float value) { values_[valueId] = value; }
    float getValue(ValueId valueId) const { return values_[valueId]; }
    float getValue(int section, ValueId valueId) const;

    void addOverrideColor(int section, ColorId colorId, Colour color);
    void removeOverrideColor(int section, ColorId colorId);

    void addOverrideValue(int section, ValueId valueId, float value);
    void removeOverrideValue(int section, ValueId valueId);

    json stateToJson();
    String stateToString();
    void saveToFile(File destination);

    json updateJson(json data);
    void jsonToState(json skin_var);
    bool stringToState(String skin_string);
    bool loadFromFile(File source);
    void clearSkin();

  protected:
    Colour colors_[kNumColors];
    float values_[kNumSkinValueIds];
    std::map<ColorId, Colour> colorOverrides_[kNumSectionOverrides];
    std::map<ValueId, float> valueOverrides_[kNumSectionOverrides];
  };
}