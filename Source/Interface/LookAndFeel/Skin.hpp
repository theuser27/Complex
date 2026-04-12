
// Created: 2022-11-16 06:55:11

#pragma once

#include "gui_utils.hpp"

namespace Interface
{
  class Component;

  class Skin
  {
  public:
    enum Override : u8
    {
      kNone,
      kEffectsLane,
      kFilterModule,
      kDynamicsModule,
      kPhaseModule,
      kPitchModule,
      kDestroyModule,

      kUseParentOverride,
      kSectionsCount = kUseParentOverride
    };

    enum ValueId : u32
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

      kScrollHitboxWidth,
      kScrollPadding,
      kScrollShrinkPercent,

      kValueIdCount
    };

    enum ColourId : u32
    {
      kBackground,
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

      kColorIdCount
    };

    Skin();

    Colour getColour(ColourId colorId, const Component *component) const;
    float getValue(ValueId valueId, const Component *component) const;

    void saveToFile(char *saveFile);
    void jsonToState(void *jsonData);
    bool stringToState(utils::string_view skinString);

    Colour colours[kSectionsCount][kColorIdCount]{};
    float values[kSectionsCount][kValueIdCount]{};
  };

  float getValue(Skin::ValueId valueId, bool isScaled, Skin::Override skinOverride = Skin::kNone);
  float getValue(Skin::ValueId valueId, bool isScaled, Component *component);
  Colour getColour(Skin::ColourId colorId, Skin::Override skinOverride = Skin::kNone);
  Colour getColour(Skin::ColourId colorId, Component *component);
}