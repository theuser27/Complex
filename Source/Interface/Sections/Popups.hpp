
// Created: 2023-20-02 19:49:54

#pragma once

#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../Components/BaseControl.hpp"

namespace Interface
{
  class Control;

  class PopupDisplay final : public Component
  {
  public:
    static constexpr int kLineHeight = 16;

    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;
    bool handleCommandMessage(u64 commandId, utils::whatever<64>) override;

    void setContent(Component *sourceComponent, 
      utils::string_view displayText, Placement relativePlacement)
    {
      source = sourceComponent;
      isControl = false;
      placement = relativePlacement;
      text.copy(displayText);
      componentFlags.isVisible = true;
    }
    void setContentControl(Control *sourceControl, Placement relativePlacement)
    {
      source = sourceControl;
      isControl = true;
      placement = relativePlacement;
      componentFlags.isVisible = true;
    }

    utils::string text;
    FontId textFontId;
    FontId numericFontId;
    i32 cachedFontWidth{};
    bool isControl = false;
    Placement placement{};
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
    bool handleCommandMessage(u64 commandId, utils::whatever<64>) override;

    void summonChildList(PopupItem *summoningItem, 
      const MouseEvent &summoningMouseEvent);

    bool (*draw)(OpenGlWrapper &openGl, PopupList *self){};

    PopupItem *parentItem{};
    
    PopupSelector *parentSelector{};
    PopupList *parentList{};
    PopupItem *currentSublistItem{};

    Point<i32> sublistAnchorPoint{};
    double lastSummonTime{};
    MouseEvent lastMouseEvent{};
  };

  struct PopupItem : public Component
  {
    PopupItem()
    {
      componentFlags.clickable = true;
      componentFlags.clickableChildren = false;
      componentFlags.acceptsOrphanedMouseEvents = true;
    }
    
    bool mouseMove(const MouseEvent &e) override;
    bool mouseDown(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool render(OpenGlWrapper &openGl) override;

    i32 id = 0;
    i32 shortcutKeyCode = 0;
    void *extraData = nullptr;          // user-provided pointer
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

    bool keyPressed(const KeyPress &key) override;
    bool handleFocus(bool hasFocus, FocusChange focusChange, Component *correspondent) override;
    bool handleCommandMessage(u64 commandId, utils::whatever<64> extraData) override;

    void newSelection(PopupItem *entry);
    void summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items);
    void closeSubList(PopupItem *items);

    void resetState();
    void summon(Component *summoningComponent, 
      Placement newListPlacement, Point<i32> customPosition = {});

    utils::smallFn<void(PopupSelector *, PopupItem *)> callback{};
    utils::smallFn<void(PopupSelector *)> cancel{};
    PopupList *list{};

    // state
    Component *summoner{};
    Placement listPlacement = Placement::centered;
    Point<i32> summoningPoint{};
    Placement lastPlacement = Placement::right;
    PopupItem *deepestHoveredItem{};
    PopupList *selectedList{};
  };

}
