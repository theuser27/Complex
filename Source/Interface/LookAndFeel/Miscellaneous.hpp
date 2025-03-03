/*
  ==============================================================================

    Miscellaneous.hpp
    Created: 9 Feb 2024 2:01:31am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <math.h>
#include <string>
#include <vector>
#include "Framework/platform_definitions.hpp"
#include "Framework/stl_utils.hpp"

namespace juce
{
  class String;
  class Path;
  class Graphics;
  class AffineTransform;
  class PathStrokeType;
  class Colour;
}

namespace Generation
{
  class BaseProcessor;
  class BaseProcessorListener
  {
  public:
    virtual ~BaseProcessorListener() = default;
    virtual void insertedSubProcessor([[maybe_unused]] usize index, [[maybe_unused]] BaseProcessor &newSubProcessor) { }
    virtual void deletedSubProcessor([[maybe_unused]] usize index, [[maybe_unused]] BaseProcessor &deletedSubProcessor) { }
    virtual void updatedSubProcessor([[maybe_unused]] usize index, [[maybe_unused]] BaseProcessor &oldSubProcessor,
      [[maybe_unused]] BaseProcessor &newSubProcessor) { }
    // recommended to call only once if source == destination
    virtual void movedSubProcessor([[maybe_unused]] BaseProcessor &subProcessor, 
      [[maybe_unused]] BaseProcessor &sourceProcessor, [[maybe_unused]] usize sourceIndex,
      [[maybe_unused]] BaseProcessor &destinationProcessor, [[maybe_unused]] usize destinationIndex) { }
  };
}

namespace Interface
{
  #define COMPLEX_UNDERSCORE_LITERAL "\xcc\xb2"
  #define COMPLEX_MIDDLE_DOT_LITERAL "\xc2\xb7"

  class Renderer;
  class Skin;
  class BaseSlider;
  class BaseControl;
  class TextSelector;
  class BaseButton;
  class TextEditor;
  class ScrollBar;
  class EffectsLaneSection;

  enum class Placement : u8
  {
    none = 0,
    above = 1,
    below = 2,
    left = 4,
    right = 8,
    centerVertical = above | below,
    centerHorizontal = left | right,
    center = centerVertical | centerHorizontal,
  };

  constexpr Placement operator|(Placement one, Placement two)
  {
    using underlying = std::underlying_type_t<Placement>;
    return (Placement)((underlying)one | (underlying)two);
  }
  constexpr Placement operator&(Placement one, Placement two)
  {
    using underlying = std::underlying_type_t<Placement>;
    return (Placement)((underlying)one & (underlying)two);
  }

  struct InterfaceRelated
  {
    Renderer *renderer = nullptr;
    Skin *skin = nullptr;
    float scale = 1.0f;
  };

  // thread_local variable for the message thread so that we don't need to pass pointers around
  extern thread_local InterfaceRelated uiRelated;
  inline float scaleValue(float value) noexcept { return uiRelated.scale * value; }
  inline int scaleValueRoundInt(float value) noexcept { return (int)roundf(uiRelated.scale * value); }

  // HeaderFooter sizes
  static constexpr int kHeaderHeight = 40;
  static constexpr int kFooterHeight = 24;

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
    virtual ~ControlListener() = default;
    virtual void controlValueChanged(BaseControl *control) = 0;
    virtual void automationMappingChanged([[maybe_unused]] BaseControl *control, 
      [[maybe_unused]] bool isUnmapping) { }
    virtual void hoverStarted([[maybe_unused]] BaseControl *slider) { }
    virtual void hoverEnded([[maybe_unused]] BaseControl *slider) { }
    virtual void mouseDown([[maybe_unused]] BaseControl *slider) { }
    virtual void mouseUp([[maybe_unused]] BaseControl *slider) { }
    virtual void mouseDoubleClick([[maybe_unused]] BaseControl *slider) { }
  };

  class TextSelectorListener
  {
  public:
    virtual ~TextSelectorListener() = default;
    virtual void resizeForText(TextSelector *textSelector, int widthChange, Placement anchor) = 0;
  };

  class OpenGlScrollBarListener
  {
  public:
    virtual ~OpenGlScrollBarListener() = default;
    virtual void scrollBarMoved(ScrollBar *scrollBarThatHasMoved, double newRangeStart) = 0;
  };

  class OpenGlViewportListener
  {
  public:
    virtual ~OpenGlViewportListener() = default;
    virtual void visibleAreaChanged(int newX, int newY, int newWidth, int newHeight) = 0;
  };

  class OpenGlTextEditorListener
  {
  public:
    /** Destructor. */
    virtual ~OpenGlTextEditorListener() = default;

    /** Called when the user changes the text in some way. */
    virtual void textEditorTextChanged(TextEditor &) { }

    /** Called when the user presses the return key. */
    virtual void textEditorReturnKeyPressed(TextEditor &) { }

    /** Called when the user presses the escape key. */
    virtual void textEditorEscapeKeyPressed(TextEditor &) { }

    /** Called when the text editor loses focus. */
    virtual void textEditorFocusLost(TextEditor &) { }
  };

  class EffectsLaneListener
  {
  public:
    virtual ~EffectsLaneListener() = default;
    virtual void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) = 0;
  };

  class LaneSelectorListener
  {
    virtual ~LaneSelectorListener() = default;
    virtual void addNewLane() = 0;
    virtual void cloneLane(EffectsLaneSection *lane) = 0;
    virtual void removeLane(EffectsLaneSection *lane) = 0;
    virtual void selectorChangedStartIndex(u32 newStartIndex) = 0;
  };

  struct Shape
  {
    enum PathType { Stroke, Fill };

    Shape();
    Shape(const Shape &);
    Shape(Shape &&) noexcept;
    ~Shape() noexcept;

    Shape &operator=(const Shape &);
    Shape &operator=(Shape &&) noexcept;

    // false for stroke, true for fill paths
    std::vector<std::tuple<juce::Path, PathType, juce::Colour>> paths;

    void drawAll(juce::Graphics &g, const juce::PathStrokeType &strokeType, 
      const juce::AffineTransform &transform, utils::span<juce::Colour> colours) const;
  };

  struct PopupItems
  {
    enum Type { Entry, Delimiter, InlineGroup, AutomationList };

    std::vector<PopupItems> items{};
    Shape icon{};
    std::string name{};
    // extra text underneath/beside entries/delimiters and popup text for entries inside groups
    std::string hint{};
    Type type = Entry;
    // doubles as number of row for automationList
    int id = 0;
    int shortcut = '\0';
    bool isActive = true;

    PopupItems(Type type = Entry, int id = 0, std::string name = {}, std::string hint = {}, Shape icon = {}, bool active = true);
    PopupItems(const PopupItems &);
    PopupItems(PopupItems &&) noexcept;

    PopupItems &operator=(const PopupItems &);
    PopupItems &operator=(PopupItems &&) noexcept;

    decltype(auto) addEntry(int subId, std::string subName, std::string subHint = {}, bool active = true, Shape shape = {})
    { return items.emplace_back(Entry, subId, COMPLEX_MOVE(subName), COMPLEX_MOVE(subHint), COMPLEX_MOVE(shape), active); }

    decltype(auto) addDelimiter(std::string subName, std::string subHint = {})
    { return items.emplace_back(Delimiter, 0, COMPLEX_MOVE(subName), COMPLEX_MOVE(subHint)); }

    decltype(auto) addInlineGroup() { return items.emplace_back(InlineGroup); }

    void addItem(const PopupItems &item) { items.push_back(item); }
    void addItem(PopupItems &&item) noexcept { items.emplace_back(COMPLEX_MOVE(item)); }
    int size() const noexcept { return (int)items.size(); }
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

}
