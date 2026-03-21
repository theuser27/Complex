
// Created: 2024-02-09 02:01:31

#pragma once

namespace Interface
{
  inline constexpr int kScrollbarMinThickness = 4;
  inline constexpr int kScrollbarMaxThickness = 8;

  inline constexpr int kPrimaryTextLineHeight = 16;
  inline constexpr int kSecondaryTextLineHeight = 12;

  inline constexpr int kDefaultActivatorSize = 12;

  // HeaderFooter sizes
  inline constexpr int kHeaderHeight = 40;
  inline constexpr int kFooterHeight = 24;

  // EffectModule sizes
  inline constexpr int kSpectralMaskContractedHeight = 20;
  inline constexpr int kEffectModuleMainBodyHeight = 144;
  
  inline constexpr int kSpectralMaskMargin = 2;

  inline constexpr int kEffectModuleWidth = 400;
  inline constexpr int kEffectModuleMinHeight = kSpectralMaskContractedHeight + 
    kSpectralMaskMargin + kEffectModuleMainBodyHeight;

  // EffectsLane sizes
  inline constexpr int kEffectsLaneTopBarHeight = 28;
  inline constexpr int kEffectsLaneBottomBarHeight = 28;
  inline constexpr int kAddModuleButtonHeight = 32;

  inline constexpr int kHVModuleToLaneMargin = 8;
  inline constexpr int kVModuleToModuleMargin = 8;
  inline constexpr int kEffectsLaneOutlineThickness = 1;

  inline constexpr int kEffectsLaneWidth = kEffectModuleWidth + 2 * kHVModuleToLaneMargin + 2 * kEffectsLaneOutlineThickness;
  inline constexpr int kEffectsLaneMinHeight = kEffectsLaneTopBarHeight + kHVModuleToLaneMargin + kEffectModuleMinHeight +
    kVModuleToModuleMargin + kAddModuleButtonHeight + kHVModuleToLaneMargin + kEffectsLaneBottomBarHeight;

  // EffectsState sizes
  inline constexpr int kLaneSelectorHeight = 38;
  inline constexpr int kLaneSelectorToLanesMargin = 8;
  inline constexpr int kHLaneToLaneMargin = 4;

  inline constexpr int kAddedWidthPerLane = kHLaneToLaneMargin + kEffectsLaneWidth;

  inline constexpr int kEffectsStateMinWidth = kEffectsLaneWidth;
  inline constexpr int kEffectsStateMinHeight = kLaneSelectorHeight + kLaneSelectorToLanesMargin + kEffectsLaneMinHeight;

  // MainInterface sizes
  inline constexpr int kMainVisualiserHeight = 112;

  inline constexpr int kVGlobalMargin = 8;
  inline constexpr int kHWindowEdgeMargin = 4;
  inline constexpr int kLaneToBottomSettingsMargin = 20;

  inline constexpr int kMinWidth = kEffectsStateMinWidth + 2 * kHWindowEdgeMargin;
  inline constexpr int kMinHeight = kHeaderHeight + kMainVisualiserHeight +
    kVGlobalMargin + kEffectsStateMinHeight + kLaneToBottomSettingsMargin + kFooterHeight;

  inline constexpr int kMinPopupWidth = 150;
  inline constexpr int kPopupToElement = 8;
  inline constexpr int kPopupSubwindowArrow = 4;
  inline constexpr int kPopupSubwindowArrowMargin = 16;
  inline constexpr int kPopupSubwindowArrowWidth = 4;

}
