
// Created: 2023-20-02 19:49:54

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  class BaseControl;

  class PopupDisplay final : public Component
  {
  public:
    static constexpr int kLineHeight = 16;

    PopupDisplay();

    bool render(OpenGlWrapper &openGl) override;
    virtual void handleCommandMessage(u64 commandId, utils::whatever) override;

    void setContent(Component *sourceComponent, utils::string displayText, 
      Placement relativePlacement)
    {
      source = sourceComponent;
      isControl = false;
      placement = relativePlacement;
      text = COMPLEX_MOVE(displayText);
      componentFlags.isVisible = true;
    }
    void setContentControl(BaseControl *sourceControl, Placement relativePlacement)
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

  struct PopupItem : Component
  {
    PopupItem()
    {
      skinOverride = Skin::kUseParentOverride;
    }

    bool render(OpenGlWrapper &openGl) override;

    i32 id = 0;
    i32 shortcutKeyCode = '\0';
    Area<i32> sublistMinSize{ 0, 0 };
    void *extraData = nullptr;
    bool closesPopup = true;
    bool canBeChosen = true;

    bool isActive = true;
    PopupSelector *selector = nullptr;
  };

  class PopupSelector final : public Component
  {
  public:
    static constexpr float kPrimaryFontHeight = 13.0f;
    static constexpr float kSecondaryFontHeight = 11.0f;

    PopupSelector();

    bool keyPressed(const KeyPress &key) override;
    bool handleFocus(bool hasFocus, FocusChange focusChange) override;
    void handleCommandMessage(u64 commandId, utils::whatever extraData) override;

    void newSelection(PopupItem *entry);
    void summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items);
    void closeSubList(PopupItem *items);

    void resetState();
    void summon(Component *summoningComponent, Point<i32> position);

    utils::smallFn<void(PopupSelector *, PopupItem *)> callback{};
    utils::smallFn<void(PopupSelector *)> cancel{};
    PopupItem *items{};

    // state
    Component *summoner{};
    Point<i32> summoningPoint{};
    Placement lastPlacement = Placement::right;
    utils::vector<PopupList *> lists{};
    PopupItem *deepestHoveredItem{};
    // TODO: timeout until any of the parents' siblings
    //       for longer than a specified time
  };

}
