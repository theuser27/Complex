/*
  ==============================================================================

    Skin.hpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/platform_definitions.hpp"
#include "Framework/memory.hpp"
#include "gui_utils.hpp"
#include "Miscellaneous.hpp"

namespace Interface
{
  class Component;

  class Skin
  {
  public:
    enum SectionOverride : u8
    {
      kNone,
      kEffectsLane,
      kPopupBrowser,
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

    static bool shouldScaleValue(ValueId valueId) noexcept
    {
      return valueId != kWidgetFillFade && valueId != kWidgetFillBoost &&
        valueId != kWidgetLineBoost && valueId != kWidgetFillCenter;
    }

    Colour colours[kSectionsCount][kColorIdCount]{};
    float values[kSectionsCount][kValueIdCount]{};
  };

  strict_inline float
  getValue(Skin::ValueId valueId, bool isScaled, Skin::SectionOverride skinOverride = Skin::kNone) noexcept
  {
    if (uiRelated.skin)
    {
      COMPLEX_ASSERT(skinOverride < Skin::kSectionsCount);
      COMPLEX_ASSERT(valueId < Skin::kValueIdCount);
      auto value = uiRelated.skin->values[skinOverride][valueId];
      return (isScaled) ? scaleValue(value) : value;
    }

    return 0.0f;
  }

  strict_inline float 
  getValue(Skin::ValueId valueId, bool isScaled, Component *component) noexcept
  {
    if (uiRelated.skin)
    {
      auto value = uiRelated.skin->getValue(valueId, component);
      return (isScaled) ? scaleValue(value) : value;
    }

    return 0.0f;
  }

  strict_inline Colour
  getColour(Skin::ColourId colorId, Skin::SectionOverride skinOverride = Skin::kNone) noexcept
  {
    if (uiRelated.skin)
    {
      COMPLEX_ASSERT(skinOverride < Skin::kSectionsCount);
      COMPLEX_ASSERT(colorId < Skin::kColorIdCount);
      return uiRelated.skin->colours[skinOverride][colorId];
    }

    return Colours::black;
  }

  strict_inline Colour
  getColour(Skin::ColourId colorId, Component *component) noexcept
  {
    if (uiRelated.skin)
      return uiRelated.skin->getColour(colorId, component);

    return Colours::black;
  }

}