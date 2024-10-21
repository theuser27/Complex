/*
  ==============================================================================

    Skin.hpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <array>
#include "Framework/platform_definitions.hpp"
#include "Framework/vector_map.hpp"

namespace juce
{
  class File;
  class String;
  class Colour;
  class LookAndFeel;
}

namespace Interface
{
  class OpenGlContainer;
  class MainInterface;
  class BaseSection;

  class Skin
  {
  public:
    enum SectionOverride
    {
      kNone,
      kOverlay,
      kEffectsLane,
      kPopupBrowser,
      kFilterModule,
      kDynamicsModule,
      kPhaseModule,
      kPitchModule,

      kSectionsCount,
      kUseParentOverride = kSectionsCount
    };

    enum ValueId
    {
      kBodyRoundingTop,
      kBodyRoundingBottom,

      kWidgetLineWidth,
      kWidgetLineBoost,
      kWidgetFillCenter,
      kWidgetFillFade,
      kWidgetFillBoost,
      kWidgetMargin,
      kWidgetRoundedCorner,

      kLabelHeight,
      kLabelBackgroundHeight,
      kLabelBackgroundRounding,
      kLabelOffset,

      kKnobArcSize,
      kKnobArcThickness,
      kKnobBodySize,
      kKnobHandleLength,
      kKnobShadowWidth,
      kKnobShadowOffset,

      kValueIdCount
    };

    enum ColourId
    {
      kInitialColor = 0x42345678,
      kBackground = kInitialColor,
      kBody,
      kBackgroundElement,
      kHeadingText,
      kNormalText,
      kBorder,

      kWidgetPrimary1,
      kWidgetPrimary2,
      kWidgetPrimaryDisabled,
      kWidgetSecondary1,
      kWidgetSecondary2,
      kWidgetSecondaryDisabled,
      kWidgetAccent1,
      kWidgetAccent2,
      kWidgetBackground1,
      kWidgetBackground2,
      kWidgetCenterLine,

      kOverlayScreen,
      kLightenScreen,
      kShadow,

      kPopupDisplayBackground,
      kPopupDisplayBorder,
      kPopupSelectorBackground,
      kPopupSelectorDelimiter,

      kTextComponentBackground,
      kTextComponentText1,
      kTextComponentText2,

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

      kModulationMeter,
      kModulationMeterLeft,
      kModulationMeterRight,
      kModulationMeterControl,

      kIconButtonOff,
      kIconButtonOffHover,
      kIconButtonOffPressed,
      kIconButtonOn,
      kIconButtonOnHover,
      kIconButtonOnPressed,

      kActionButtonPrimary,
      kActionButtonPrimaryHover,
      kActionButtonPrimaryPressed,
      kActionButtonSecondary,
      kActionButtonSecondaryHover,
      kActionButtonSecondaryPressed,
      kActionButtonText,

      kPowerButtonOn,
      kPowerButtonOff,

      kTextEditorBackground,
      kTextEditorBorder,
      kTextEditorCaret,
      kTextEditorSelection,

      kFinalColor
    };

    static constexpr auto kColorIdCount = kFinalColor - kInitialColor;

    Skin();

    void clearSkin();

    void setColour(ColourId colorId, const juce::Colour &color) noexcept;
    u32 getColour(ColourId colorId) const;
    u32 getColour(SectionOverride section, ColourId colorId) const;
    u32 getColour(const OpenGlContainer *section, ColourId colorId) const;

    void setValue(ValueId valueId, float value);
    float getValue(ValueId valueId) const;
    float getValue(SectionOverride section, ValueId valueId) const;
    float getValue(const OpenGlContainer *section, ValueId valueId) const;

    void addColourOverride(int section, ColourId colorId, const juce::Colour &color);
    void removeColourOverride(int section, ColourId colorId);
    bool overridesColour(int section, ColourId colorId) const;

    void addOverrideValue(int section, ValueId valueId, float value);
    void removeOverrideValue(int section, ValueId valueId);
    bool overridesValue(int section, ValueId valueId) const;

    void saveToFile(const juce::File &destination);

    void jsonToState(void *jsonData);
    bool stringToState(const juce::String &skin_string);
    bool loadFromFile(const juce::File &source);
    void loadDefaultSkin();

    static bool shouldScaleValue(ValueId valueId) noexcept
    {
      return valueId != kWidgetFillFade && valueId != kWidgetFillBoost &&
        valueId != kWidgetLineBoost && valueId != kKnobHandleLength &&
        valueId != kWidgetFillCenter;
    }

  protected:
    u32 colors_[kColorIdCount]{};
    float values_[kValueIdCount]{};
    std::array<Framework::VectorMap<ColourId, u32>, kSectionsCount> colorOverrides_{};
    std::array<Framework::VectorMap<ValueId, float>, kSectionsCount> valueOverrides_{};
  };
}