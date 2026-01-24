/*
  ==============================================================================

    Miscellaneous.hpp
    Created: 9 Feb 2024 2:01:31am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/platform_definitions.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/stl_utils.hpp"
#include "Framework/memory.hpp"
#include "Framework/utils.hpp"
#include "gui_utils.hpp"

extern "C" struct NSVGimage;

namespace Generation
{
  class BaseProcessor;
}

namespace Plugin
{
  struct State;
}

namespace Interface
{
  class Renderer;
  class Skin;
  class Graphics;
  class Component;
  class BaseSlider;
  class BaseControl;
  class TextSelector;
  class BaseButton;
  class TextEditor;
  class ScrollBar;
  class EffectsLaneSection;

  struct InterfaceRelated
  {
    Plugin::State *state = nullptr;
    Renderer *renderer = nullptr;
    Graphics *cache = nullptr;
    Skin *skin = nullptr;
    float scale = 1.0f;
  };

  // thread_local variable for the message thread so that we don't need to pass pointers around
  extern thread_local InterfaceRelated uiRelated;
  strict_inline float scaleValue(float value) noexcept { return uiRelated.scale * value; }
  strict_inline Rectangle<float>
  scaleValue(Rectangle<float> bounds) noexcept
  {
    auto values = utils::bit_cast<simd_float>(bounds);
    values = uiRelated.scale * utils::toFloat(values);
    return utils::bit_cast<Rectangle<float>>(values);
  }
  strict_inline float scaleValueRound(float value) noexcept { return ::roundf(uiRelated.scale * value); }
  strict_inline i32 scaleValueRoundInt(float value) noexcept { return (int)::roundf(uiRelated.scale * value); }
  strict_inline Rectangle<i32> 
  scaleValueRoundInt(Rectangle<i32> bounds)
  {
    auto values = utils::bit_cast<simd_int>(bounds);
    values = utils::toInt(simd_float::round(uiRelated.scale * utils::toFloat(values)));
    return utils::bit_cast<Rectangle<i32>>(values);
  }

  inline constexpr int kScrollbarMinThickness = 4;
  inline constexpr int kScrollbarMaxThickness = 8;

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
  inline constexpr int kPopupToElement = 4;

  class ControlListener
  {
  public:
    virtual void controlValueChanged(BaseControl *control) = 0;
    virtual void automationMappingChanged([[maybe_unused]] BaseControl *control, [[maybe_unused]] bool isUnmapping) { }
    virtual void hoverStarted([[maybe_unused]] BaseControl *slider, [[maybe_unused]] const MouseEvent &e) { }
    virtual void hoverEnded([[maybe_unused]] BaseControl *slider, [[maybe_unused]] const MouseEvent &e) { }
    virtual void mouseDown([[maybe_unused]] BaseControl *slider, [[maybe_unused]] const MouseEvent &e) { }
    virtual void mouseUp([[maybe_unused]] BaseControl *slider, [[maybe_unused]] const MouseEvent &e) { }
  };

  struct SVG
  {
    NSVGimage *image = nullptr;

    SVG(utils::bumpArena *arena, const void *data, usize size);
    ~SVG();

    void draw(Graphics &g, Colour colour,
      Rectangle<float> bounds, float scale, float strokeWidth) const;
  };

  // encompasses a manually programmed draw or 
  // a precalculated SVG (stored in Graphics) captured inside the closure
  struct Shape
  {
    using DrawingFunc = void(Graphics &g, Colour colour,
      Rectangle<float> bounds, float scale, float strokeWidth);

    utils::vector<utils::pair<utils::smallFn<DrawingFunc>, Colour>> paths{};

    void drawAll(Graphics &g, Rectangle<float> bounds, float scale,
      float strokeWidth, utils::span<Colour> colours) const;
  };

  namespace Paths
  {
    Shape pasteValueIcon();
    Shape enterValueIcon();
    Shape copyNormalisedValueIcon();
    Shape copyScaledValueIcon();

    Shape filterIcon();
    Shape dynamicsIcon();
    Shape phaseIcon();
    Shape pitchIcon();
    Shape destroyIcon();

    Shape powerButtonIcon();
  }

  struct ViewportChange
  {
    Component *component = nullptr;
    Rectangle<int> change{};
    bool isClipping = true;

    constexpr bool operator==(const ViewportChange &) const noexcept = default;
  };

  utils::pair<i32, i32> 
  getScrollOffsets(const MouseEvent &e, float singleStepX = 16, float singleStepY = 16);

}
