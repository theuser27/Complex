
// Created: 2023-20-02 19:49:54

#pragma once

#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Component.hpp"
#include "../Components/Control.hpp"

namespace Interface
{
  class Control;

  class PopupDisplay final : public Component
  {
  public:
    static constexpr int kLineHeight = 16;

    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;

    void setContent(Component *sourceComponent, 
      utils::string_view displayText, Placement relativeSourcePlacement, 
      Point<i32> customPosition = {})
    {
      source = sourceComponent;
      isControl = false;
      relativePlacement = relativeSourcePlacement;
      offset = customPosition;
      text.copy(displayText);
      componentFlags.isVisible = true;
    }
    void setContentControl(Control *sourceControl, 
      Placement relativeSourcePlacement, Point<i32> customPosition = {})
    {
      source = sourceControl;
      isControl = true;
      relativePlacement = relativeSourcePlacement;
      offset = customPosition;
      componentFlags.isVisible = true;
    }
    void reset()
    {
      source = nullptr;
      componentFlags.isVisible = false;
    }

    utils::string text;
    i32 cachedFontWidth{};
    bool isControl = false;
    Placement relativePlacement{};
    Point<i32> offset{};
    Component *source{};
  };

  class PopupSelector;
  class PopupList;

  class PopupList final : public Component
  {
  public:
    static constexpr float kScrollSensitivity = 150.0f;
    static constexpr float kAutomationListWidth = 150.0f;

    static constexpr float kIconSize = 16.0f;
    static constexpr float kScrollBarWidth = 8.0f;
    static constexpr float kSideArrowWidth = 4.0f;
    static constexpr float kCrossWidth = 8.0f;

    static constexpr float kPrimaryTextLineHeight = 16.0f;
    static constexpr float kSecondaryTextLineHeight = 12.0f;
    static constexpr float kDelimiterHeight = 20.0f;

    static constexpr float kVPadding = 4.0f;
    static constexpr float kHEntryPadding = 12.0f;
    static constexpr float kHEntryToSideArrowMinMargin = 16.0f;
    static constexpr float kVEntryToHintMargin = 3.0f;

    PopupList(PopupSelector *parentSelector);

    bool render(OpenGlWrapper &openGl) override;

    bool mouseEnter(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &e) override;

    void summonChildList(PopupItem *summoningItem, 
      const MouseEvent &summoningMouseEvent, bool force = false);

    static bool drawV1(OpenGlWrapper &openGl, PopupList *self);

    bool (*draw)(OpenGlWrapper &openGl, PopupList *self) = drawV1;

    PopupItem *parentItem{};
    
    PopupSelector *parentSelector{};
    PopupList *parentList{};
    PopupItem *currentSublistItem{};

    Point<i32> sublistAnchorPoint{};
    double lastSummonTime{};
    MouseEvent lastMouseEvent{};

    utils::sll<CommandMessages::HandleMessageFn *> popupListHandler{};
  };

  struct PopupItem : public Component
  {
    PopupItem()
    {
      componentFlags.clickable = true;
      componentFlags.clickableChildren = false;
      componentFlags.acceptsOrphanMouseEvents = true;
    }
    
    bool mouseMove(const MouseEvent &e) override;
    bool mouseDown(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool render(OpenGlWrapper &openGl) override;

    Colour getHighlightColour() { return getColour(Skin::kWidgetPrimary1, this).darker(0.8f); }

    i32 id = 0;
    i32 shortcutKeyCode = 0;
    void *extraData = nullptr;          // user-provided pointer
    u8 dataTag = 0;                     // tag to differentiate extra data
    bool closesPopup = true;            // if selector should get closed after being chosen
    bool canBeChosen = true;            // to designate labels/titles
    bool isActive = true;               // option that exists but isn't currently chooseable
    bool opensNewList = false;          // should create a new list to show contained items

    PopupList *associatedList = nullptr;
    PopupList *childList = nullptr;
  };

  class PopupSelector final : public Component
  {
  public:
    static constexpr float kPrimaryFontHeight = 13.0f;
    static constexpr float kSecondaryFontHeight = 11.0f;

    void reinitialise();

    bool handleFocus(bool hasFocus, FocusChange focusChange, Component *correspondent) override;
    bool keyPressed(const KeyPress &key) override;

    void newSelection(PopupItem *entry);

    void resetState(bool callCancelFn = true);
    void summon(Component *summoningComponent, 
      Placement newListPlacement, Point<i32> customPosition = {});

    utils::smallFn<void(PopupSelector *, PopupItem *)> callback{};
    utils::smallFn<void(PopupSelector *)> cancel{};
    PopupList *list{};
    bool toggleable = true;

    // state
    Component *summoner{};
    Placement listPlacement = Placement::centered;
    Point<i32> summoningPoint{};
    Placement lastPlacement = Placement::right;
    PopupItem *deepestHoveredItem{};
    PopupList *selectedList{};

    utils::sll<CommandMessages::HandleMessageFn *> popupSelectorHandler{};
  };

  struct OptionPopupItem : public PopupItem
  {
    enum ExtraDataTypes : u8 { StringData, OptionData };

    static constexpr Rectangle<u16> kPrimaryPaddingRect =
    { kPopupHorizontalPadding, kPopupVerticalPrimaryPadding,
      kPopupHorizontalPadding, kPopupVerticalPrimaryPadding };
    static constexpr Rectangle<u16> kSecondaryPaddingRect =
    { kPopupHorizontalPadding, kPopupVerticalSecondaryPadding,
      kPopupHorizontalPadding, kPopupVerticalSecondaryPadding };
    static constexpr Rectangle kPrimarySizeRect = { 0, kPrimaryTextLineHeight, 0, kPrimaryTextLineHeight };
    static constexpr Rectangle kSecondarySizeRect = { 0, kSecondaryTextLineHeight, 0, kSecondaryTextLineHeight };

    OptionPopupItem();

    bool render(OpenGlWrapper &openGl) override;

    utils::pair<utils::string_view, bool> getTextAndWrap();

    static OptionPopupItem *createTitle(PopupList *list, utils::string_view text);
    static OptionPopupItem *createOption(PopupList *list, Framework::IndexedData &option);
  };
}
