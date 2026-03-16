
// Created: 2023-02-02 19:49:54

#include "Popups.hpp"

#include "Framework/parameter_value.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../Components/BaseControl.hpp"

namespace Interface
{
  static Range<i32>
  getPopupDisplayDimensions(Component *c, bool isCalculatingVertical)
  {
    const int lineHeight = scaleValueRoundInt(PopupDisplay::kLineHeight);

    auto *display = (PopupDisplay *)c;
    if (!isCalculatingVertical)
    {
      if (display->isControl)
      {
        auto *control = (Control *)display->source;
        control->getScaledValueString(display->text, control->getValue());
      }

      display->cachedFontWidth = (i32)::ceilf(uiRelated.cache->getStringWidthFloat(display->text));
      return Range<i32>{ 0, display->cachedFontWidth };
    }
    else
    {
      auto lineCount = (i32)::ceilf((float)display->cachedFontWidth / (float)display->bounds.w);
      return Range<i32>{ lineHeight, lineCount *lineHeight };
    }
  }

  void PopupDisplay::reinitialise()
  {
    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(8));

    skinOverride = Skin::kUseParentOverride;
    placement = Placement::custom;
    padding = { kLineHeight, kLineHeight, kLineHeight, kLineHeight };
    overrideDimensions = getPopupDisplayDimensions;
    text = { arena };

    textFontId = FontId::InterType;
    numericFontId = FontId::DDinType;
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
    uiRelated.cache->setFont(usedFontId, (float)bounds.h * 0.5f);

    auto textPadding = scaleValue(padding.toFloat());
    float width = scaleValue((float)bounds.w);

    nvgStrokeColor(openGl.g, getColour(Skin::kWidgetPrimary1));
    nvgTextAlign(openGl.g, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgTextBox(openGl.g, textPadding.x, textPadding.y, width, text.data(), text.data() + text.size());
    nvgText(openGl.g, textPadding.x, textPadding.y, text.data(), text.data() + text.size());

    return true;
  }

  bool 
  PopupDisplay::handleCommandMessage(u64 commandId, utils::whatever<64>)
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

      return true;
    }
    }

    return false;
  }

  PopupList::PopupList(PopupSelector *parentSelector) : parentSelector{ parentSelector }
  {
    placement = Placement::custom;
    sizingFlags |= Component::ScrollableWithBarY | Component::SnapToMinX;
    componentFlags.vertical = true;
  }

  bool 
  PopupList::render(OpenGlWrapper &openGl)
  {
    // if the user is currently holding down a mouse button, we shouldn't change anything
    if (auto mouseInteractions = getMouseInteractions(uiRelated.renderer); !mouseInteractions.clicked)
    {
      if (currentSublistItem && componentFlags.isHovered)
      {
        // TODO: check if mouse is inside the prediction triangle
        //       if not, hide the sublist
      }
      else if (isParentOf(mouseInteractions.hovered))
      {
        // TODO: check if the currently hovered item has a sublist and display it
      }
    }

    if (draw)
      return draw(openGl, this);
    return true;
  }

  bool 
  PopupList::mouseEnter(const MouseEvent &e)
  {
    (void)e;
    // TODO:
    return true;
  }

  bool 
  PopupList::mouseExit(const MouseEvent &e)
  {
    (void)e;
    // TODO:
    return true;
  }

  static void positionInitialPopupList(PopupList *list, PopupSelector *selector, Placement placement, Point<i32> extraPosition = {})
  {
    if (placement == Placement::custom)
    {
      //auto newPosition = getRelativePoint(selector->summoner,
      //  selector->summoningPoint);
      //bounds.setPosition(newPosition);

      return;
    }


    auto sourceBounds = selector->getRelativeArea(selector->summoner);
    auto sourcePosition = sourceBounds.getPosition();
    auto [_, __, width, height] = list->getLocalBounds();
    i32 windowWidth = selector->bounds.w;
    i32 windowHeight = selector->bounds.h;
    i32 popupToElement = scaleValueRoundInt(kPopupToElement);

    auto [finalX, finalY] = sourcePosition;

    auto checkHorizontal = [&](i32 offset = 0)
    {
      if (finalX + width <= windowWidth)
        return;

      // popup cannot be fit horizontally, find the side with the most width
      if (sourcePosition.x + sourceBounds.w >= windowWidth - sourcePosition.x)
      {
        width = utils::min(width, sourcePosition.x + sourceBounds.w - offset);
        finalX = sourcePosition.x + sourceBounds.w - offset - width;
      }
      else
      {
        finalX = sourcePosition.x + offset;
        width = utils::min(width, windowWidth - finalX);
      }
    };

    auto checkVertical = [&](i32 offset = 0)
    {
      if (finalY + height <= windowHeight)
        return;

      // popup cannot be fit vertically, find the side with the most height
      if (sourcePosition.y + sourceBounds.h >= windowHeight - sourcePosition.y)
      {
        height = utils::min(height, sourcePosition.y + sourceBounds.h - offset);
        finalY = sourcePosition.y + sourceBounds.h - offset - height;
      }
      else
      {
        finalY = sourcePosition.y + offset;
        height = utils::min(height, windowHeight - finalY);
      }
    };

    if (placement == Placement::bottom || placement == Placement::top)
    {
      checkHorizontal();
      finalY = (placement == Placement::bottom) ? finalY + sourceBounds.h + popupToElement :
        finalY - height - popupToElement;
      checkVertical(sourceBounds.h + popupToElement);
    }
    else if (placement == Placement::left || placement == Placement::right)
    {
      checkVertical();
      finalX = (placement == Placement::left) ? finalX - width - popupToElement :
        finalX + sourceBounds.w + popupToElement;
      checkHorizontal(sourceBounds.w + popupToElement);
    }
    else
    {
      COMPLEX_ASSERT_FALSE("You need to choose a one of the placements for the popup");
      checkHorizontal();
      finalY = finalY + sourceBounds.h;
      checkVertical(sourceBounds.h);
    }

    list->bounds.setPosition(finalX, finalY);
    list->componentFlags.isVisible = !selector->summoner->isObscured();

    calculatePositions(list->children, list, list->bounds);
  }

  //static void positionSubsequentPopupList(PopupList *list, PopupSelector *selector, Placement placement, Point<i32> extraPosition = {})
  //{

  //}

  bool
  PopupList::handleCommandMessage(u64 commandId, utils::whatever<64>)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
      if (!parentList)
      {
        auto *selector = (PopupSelector *)parent;
        positionInitialPopupList(this, selector, selector->listPlacement, selector->summoningPoint);
      }
      else
      {
        // position stays relative to the parent list
        auto associatedItemBounds = getRelativeArea(parentItem, parentItem->bounds);

        bounds.x = utils::min(associatedItemBounds.getRight(),
          parent->bounds.getRight() - bounds.w);

        if (parent->bounds.contains(associatedItemBounds.x - bounds.w, 0) &&
          !parent->bounds.contains(associatedItemBounds.getRight() + bounds.w, 0))
        {
          bounds.x = associatedItemBounds.x - bounds.w;
        }

        bounds.y = utils::clamp(parent->bounds.h - bounds.h, 0, associatedItemBounds.y);

        componentFlags.isVisible = parentList->componentFlags.isVisible;
      }

      calculatePositions(children, this);

      return true;
    }

    return false;
  }

  void PopupList::summonChildList(PopupList *childList)
  {
    //childList->

  }

  bool 
  PopupItem::mouseUp(const MouseEvent &)
  {
    if (!canBeChosen)
      return false;

    associatedList->parentSelector->newSelection(this);

    return true;
  }

  bool
  PopupItem::render(OpenGlWrapper &openGl)
  {
    if ((componentFlags.isHovered || componentFlags.isClicked) && canBeChosen)
      fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kWidgetPrimary1, this).darker(0.8f));

    return false;
  }

  void PopupSelector::reinitialise()
  {
    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(128));

    placement = Placement::custom;
    sizingFlags |= GrowableX | GrowableY;
  }

  bool 
  PopupSelector::keyPressed(const KeyPress &key)
  {
    for (auto *child = selectedList->children; child; child = child->next)
    {
      // this is safe because only popup items can be childen
      auto *item = utils::as<PopupItem>(child);
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

  //void PopupSelector::summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items)
  //{
  //  PopupList *newList = nullptr;
  //  for (auto *list : lists)
  //    if (!list->isUsed)
  //      newList = list;

  //  if (!newList)
  //    newList = anew(arena, PopupList, {});

  //  newList->associatedItem = items;

  //  newList->recalculateSizes();
  //  newList->isUsed = true;
  //}

  /*void PopupSelector::summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items)
  {




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

  bool 
  PopupSelector::handleFocus(bool hasFocus, FocusChange, Component *correspondent)
  {
    if (!hasFocus)
    {
      if (correspondent == summoner || isParentOf(correspondent))
        return false;

      componentFlags.isVisible = false;
      if (cancel)
        cancel(this);

      resetState();
    }

    return true;
  }

  bool 
  PopupSelector::handleCommandMessage(u64 commandId, utils::whatever<64>)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
      // position stays relative to component we're attached to
      bounds.setPosition(parent->bounds.getPosition());
      return true;
    }

    return false;
  }

  void PopupSelector::resetState()
  {
    removeAllChildComponents();
    summoner = {};
    callback = {};
    cancel = {};
    lastPlacement = Placement::right;
    utils::bumpArena::clear(arena);
    skinOverride = Skin::kNone;
    componentFlags.isVisible = false;
  }

  void PopupSelector::summon(Component *summoningComponent,
    Placement newListPlacement, Point<i32> customPosition)
  {
    summoner = summoningComponent;
    listPlacement = newListPlacement;
    summoningPoint = customPosition;
    addChildComponent(list);

    componentFlags.isVisible = true;
    grabFocus();
  }
}
