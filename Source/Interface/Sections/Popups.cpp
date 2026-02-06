
// Created: 2023-02-02 19:49:54

#include "Popups.hpp"

#include "Framework/parameter_value.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/BaseControl.hpp"

namespace Interface
{
  static Range<i32>
  getPopupDisplayDimensions(Component *c, i32 *availablePrimarySize)
  {
    const int lineHeight = scaleValueRoundInt(PopupDisplay::kLineHeight);

    auto *display = (PopupDisplay *)c;
    if (!availablePrimarySize)
    {
      if (display->isControl)
      {
        auto *control = (BaseControl *)display->source;
        display->text = COMPLEX_MOVE(control->getScaledValueString(
          display->arena, control->getValue()));
      }

      display->cachedFontWidth = (i32)::ceilf(uiRelated.cache->getStringWidthFloat(display->text));
      return Range<i32>{ 0, display->cachedFontWidth };
    }
    else
    {
      auto lineCount = (i32)::ceilf((float)display->cachedFontWidth / (float)(*availablePrimarySize));
      return Range<i32>{ lineHeight, lineCount *lineHeight };
    }
  }

  PopupDisplay::PopupDisplay()
  {
    arena = utils::bumpArena::createNested(getUIArena(), COMPLEX_KB(8));
    skinOverride = Skin::kUseParentOverride;
    placement = Placement::custom;
    sizingFlags = Component::CustomDimensions;
    padding = { kLineHeight, kLineHeight, kLineHeight, kLineHeight };
    desiredSize.getDimensions = getPopupDisplayDimensions;

    textFontId = uiRelated.cache->InterFontId;
    numericFontId = uiRelated.cache->DDinFontId;
  }

  bool PopupDisplay::render(OpenGlWrapper &openGl)
  {
    float rounding = getValue(Skin::kBodyRoundingTop, true);
    nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.y, rounding);
    nvgFillColor(openGl.g, getColour(Skin::kPopupDisplayBackground));
    nvgFill(openGl.g);

    nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.y, rounding);
    nvgStrokeColor(openGl.g, getColour(Skin::kPopupDisplayBorder));
    nvgStroke(openGl.g);

    auto usedFontId = (isControl) ? numericFontId : textFontId;
    auto fontHeight = uiRelated.cache->getFontHeightFromAscent(usedFontId, (float)bounds.h * 0.5f);
    auto fontKerning = uiRelated.cache->getKerningForHeight(usedFontId, (float)bounds.h * 0.5f);
    uiRelated.cache->setFont({ .id = usedFontId, .height = fontHeight, .kerning = fontKerning });

    auto textPadding = scaleValue(padding.toFloat());
    float width = scaleValue((float)bounds.w);

    nvgStrokeColor(openGl.g, getColour(Skin::kWidgetPrimary1));
    nvgTextAlign(openGl.g, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgTextBox(openGl.g, textPadding.x, textPadding.y, width, text.data(), text.data() + text.size());
    nvgText(openGl.g, textPadding.x, textPadding.y, text.data(), text.data() + text.size());

    return true;
  }

  void PopupDisplay::handleCommandMessage(u64 commandId, utils::whatever)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
    {
      Point<i32> position{};
      COMPLEX_ASSERT((placement & Placement::custom) == 0);
      i32 extraOffsetX = bounds.w / 2;
      i32 extraOffsetY = bounds.h / 2;

      switch ((Placement)(placement & Placement::justifyX))
      {
      case Placement::left:
        position.x -= bounds.w + extraOffsetX;
        break;

      case Placement::right:
        position.x += source->bounds.w + extraOffsetX;
        break;

      default:
      case Placement::justifyX:
        position.x += (source->bounds.w - bounds.w) / 2;
        break;
      }
      
      switch ((Placement)(placement & Placement::justifyY))
      {
      case Placement::top:
        position.y -= bounds.h + extraOffsetY;
        break;

      default:
      case Placement::bottom:
        position.y += source->bounds.h + extraOffsetY;
        break;

      case Placement::justifyY:
        position.y += (source->bounds.h - bounds.h) / 2;
        break;
      }

      bounds.setPosition(getRelativePoint(source, position));

    } break;
    }
  }

  bool 
  PopupItem::render(OpenGlWrapper &openGl)
  {
    (void)openGl;

    return false;
  }

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
    static constexpr float kInlineGroupHeight = 28.0f;	// serves also as minimum width for the elements

    static constexpr float kVPadding = 4.0f;
    static constexpr float kHEntryPadding = 12.0f;
    static constexpr float kHEntryToSideArrowMinMargin = 16.0f;
    static constexpr float kVEntryToHintMargin = 3.0f;

    PopupList()
    {
      placement = Placement::custom;
      sizingFlags |= ScrollableWithBarY;
    }

    bool render(OpenGlWrapper &openGl) override
    {
      (void)openGl;
      // TODO:
    }

    bool mouseEnter(const MouseEvent &e) override
    {
      (void)e;
      // TODO:
    }
    bool mouseExit(const MouseEvent &e) override
    {
      (void)e;
      // TODO:
    }

    void handleCommandMessage(u64 commandId, utils::whatever)
    {
      switch (commandId)
      {
      case Component::HandleCustomPosition:
        if (!parentList)
        {
          auto *selector = (PopupSelector *)parent;

          auto newPosition = getRelativePoint(selector->summoner,
            selector->summoningPoint);
          bounds.setPosition(newPosition);

          componentFlags.isVisible = selector->bounds.contains(newPosition);
        }
        else
        {
          // position stays relative to the parent list
          auto parentItemBounds = getRelativeArea(parentItem, parentItem->bounds);

          bounds.x = utils::min(parentItemBounds.getRight(),
            parent->bounds.getRight() - bounds.w);

          if (parent->bounds.contains(parentItemBounds.x - bounds.w, 0) &&
            !parent->bounds.contains(parentItemBounds.getRight() + bounds.w, 0))
          {
            bounds.x = parentItemBounds.x - bounds.w;
          }

          bounds.y = utils::clamp(parent->bounds.h - bounds.h, 0, parentItemBounds.y);

          componentFlags.isVisible = parentList->componentFlags.isVisible;
        }

        calculatePositions(parentItem->childComponents, this, bounds);

        break;
      }
    }

    PopupItem *parentItem = nullptr;
    PopupList *parentList = nullptr;
    bool isUsed = false;
  };

  PopupSelector::PopupSelector()
  {
    arena = utils::bumpArena::createNested(getUIArena(), COMPLEX_KB(512));
    placement = Placement::custom;
    sizingFlags |= GrowableX | GrowableY;
    componentFlags.wantsFocus = true;
  }

  bool PopupSelector::keyPressed(const KeyPress &key)
  {
    // get currently focused sublist
    PopupList *selectedList = nullptr;
    for (auto &list : lists)
    {
      if (!list->isUsed)
        break;
      selectedList = list;
    }

    COMPLEX_ASSERT(selectedList);

    for (usize i = 0; i < selectedList->parentItem->childComponents.size(); ++i)
    {
      // this is safe because only popup items can be childen
      auto *item = utils::as<PopupItem>(selectedList->parentItem->childComponents[i]);
      if (key.keyCode != item->shortcutKeyCode)
        continue;
      
      newSelection(item);
      return true;
    }

    return false;
  }

  void PopupSelector::newSelection(PopupItem *entry)
  {
    if (entry->id >= 0)
    {
      if (callback)
      {
        callback(this, entry);
        if (!entry->closesPopup)
          return;
      }

      componentFlags.isVisible = false;
    }
    else if (cancel)
      cancel(this);

    resetState();
  }

  /*void PopupSelector::summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items)
  {
    PopupList *newList = nullptr;
    for (auto *list : lists)
      if (!list->isUsed)
        newList = list;

    if (!newList)
      newList = anew(arena, PopupList, {});

    // we may have already used it with these items
    bool isCached = newList->getItems() == items && 
      items->type != PopupItem::AutomationList;
    if (!isCached)
    {
      newList->setItems(items);
      newList->recalculateSizes();
    }

    newList->setIsUsed(true);

    int width = newList->getListWidth();
    int height = newList->getListHeight() + 2 * commonInfo_.listRounding;
    auto [_, __, windowWidth, windowHeight] = getLocalBounds();

    int y = sourceBounds.y;
    if (y + height >= getHeight())
    {
      y = utils::max(sourceBounds.getBottom() - height, 0);
      height = utils::min(height, windowHeight);
    }

    int x = sourceBounds.getRight();
    if ((lastPlacement_ == Placement::left || x + width >= windowWidth) 
      && sourceBounds.x - width > 0)
    {
      x = sourceBounds.x - width;
      lastPlacement_ = Placement::left;
    }
    else
    {
      x = utils::min(x + width, windowWidth) - width;
      lastPlacement_ = Placement::right;
    }

    newList->setBounds(x, y, width, height);
    if (!isCached)
      newList->setComponentsBounds();
    newList->setVisible(true);
  }

  void PopupSelector::closeSubList(PopupItem *items)
  {
    usize index = 0;
    for (; index < lists_.size(); ++index)
      if (lists_[index]->getItems() == items)
        break;

    if (index == lists_.size())
      return;

    COMPLEX_ASSERT(index > 0);

    lastPlacement_ = ((lists_[index]->getX() - lists_[index - 1]->getX()) > 0) ? 
      Placement::right : Placement::left;

    for (; index < lists_.size(); ++index)
    {
      lists_[index]->setVisible(false);
      lists_[index]->setIsUsed(false);
    }
  }*/

  bool PopupSelector::handleFocus(bool hasFocus, FocusChange)
  {
    if (!hasFocus)
    {
      componentFlags.isVisible = false;
      if (cancel)
        cancel(this);

      resetState();
    }

    return true;
  }

  void PopupSelector::handleCommandMessage(u64 commandId, utils::whatever)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
      // position stays relative to component we're attached to
      bounds.setPosition(parent->bounds.getPosition());
      break;
    }
  }

  void PopupSelector::resetState()
  {
    callback = {};
    cancel = {};
    for (auto &list : lists)
    {
      list->componentFlags.isVisible = false;
      list->isUsed = false;
    }
    lastPlacement = Placement::right;
    utils::bumpArena::clear(arena);
    skinOverride = Skin::kNone;
    componentFlags.isVisible = false;
  }

  void PopupSelector::summon(Component *summoningComponent, Point<i32> position)
  {
    summoner = summoningComponent;
    summoningPoint = position;
    grabFocus();

    // TODO: summon new list
  }
}
