
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
    componentFlags.clickable = true;
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
    {
      if (draw(openGl, this))
      {
        doRenderChildren(openGl);
      }

      renderScrollbars(openGl, 0.2f);

      auto mouseState = getMouseInteractions(uiRelated.renderer).mouseState.getEventRelativeTo(this);
      auto *c = contains(Point{ mouseState.x, mouseState.y }) ? lastMouseEvent.eventComponent : currentSublistItem;
      summonChildList((PopupItem *)c, lastMouseEvent);

      //if (currentSublistItem)
      //{
      //  // adjust triangle cone
      //  auto childListBounds = parentSelector->getRelativeArea(currentSublistItem->childList);
      //  auto topPoint = (childListBounds.x > bounds.x) ? childListBounds.getTopLeft() : childListBounds.getTopRight();
      //  auto bottomPoint = (childListBounds.x > bounds.x) ? childListBounds.getBottomLeft() : childListBounds.getBottomRight();

      //  auto A = sublistAnchorPoint - bounds.getPosition();
      //  auto B = topPoint - bounds.getPosition();
      //  auto C = bottomPoint - bounds.getPosition();

      //  strokePolygon(openGl, 1.0f, { { A.toFloat(), B.toFloat(), C.toFloat() } });
      //}

      return false;
    }
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

  static void positionInitialPopupList(PopupList *list, PopupSelector *selector,
    Component *relativeComponent, Placement placement, Point<i32> extraPosition)
  {
    auto [_, __, width, height] = list->getLocalBounds();
    i32 windowWidth = selector->bounds.w;
    i32 windowHeight = selector->bounds.h;

    if (placement == Placement::custom)
    {
      auto [x, y] = list->getRelativePoint(relativeComponent, extraPosition);

      // do we have space to the right
      if (x + width > windowWidth)
      {
        // do we have enough space to the left
        if (x > width)
          x -= width;
        else
        {
          // neither have enough space, offset and shrink if necessary
          width = utils::min(width, windowWidth);
          x = windowWidth - width;
        }
      }
      if (y + height > windowHeight)
      {
        if (y > height)
          y -= height;
        else
        {
          height = utils::min(windowHeight, height);
          y = windowHeight - height;
        }
      }

      list->bounds.setPosition(x, y);

      return;
    }


    auto sourceBounds = selector->getRelativeArea(relativeComponent);
    auto sourcePosition = sourceBounds.getPosition();

    i32 yOffset = scaleValueRoundInt((float)extraPosition.y);
    i32 xOffset = scaleValueRoundInt((float)extraPosition.x);

    auto [finalX, finalY] = sourcePosition;

    auto checkHorizontal = [&](i32 offset = 0)
    {
      if (finalX + width <= windowWidth)
        return;

      // popup cannot be fit horizontally, find the side with the most width
      if (sourcePosition.x + sourceBounds.w >= windowWidth - sourcePosition.x)
      {
        // left side
        finalX = utils::max(0, sourcePosition.x + sourceBounds.w - offset - width);
      }
      else
      {
        // right side
        finalX = utils::clamp(windowWidth - width, 0, sourcePosition.x + offset);
      }
    };

    auto checkVertical = [&](i32 offset = 0)
    {
      if (finalY + height <= windowHeight)
        return;

      // popup cannot be fit vertically, find if the upper or lower side has the most height
      if (sourcePosition.y + sourceBounds.h >= windowHeight - sourcePosition.y)
      {
        // upper side
        finalY = utils::max(0, sourcePosition.y + sourceBounds.h - offset - height);
      }
      else
      {
        // lower side
        finalY = utils::clamp(windowHeight - height, 0, sourcePosition.y + offset);
      }
    };

    if (placement == Placement::bottom || placement == Placement::top)
    {
      checkHorizontal();
      finalY = (placement == Placement::bottom) ? finalY + sourceBounds.h + yOffset :
        finalY - height - yOffset;
      checkVertical(sourceBounds.h + yOffset);
    }
    else if (placement == Placement::left || placement == Placement::right)
    {
      checkVertical();
      finalX = (placement == Placement::left) ? finalX - width - xOffset :
        finalX + sourceBounds.w + xOffset;
      checkHorizontal(sourceBounds.w + xOffset);
    }
    else
    {
      COMPLEX_ASSERT_FALSE("You need to choose a one of the placements for the popup");
      checkHorizontal();
      finalY = finalY + sourceBounds.h;
      checkVertical(sourceBounds.h);
    }

    list->bounds.setPosition(finalX, finalY);
  }

  bool
  PopupList::handleCommandMessage(u64 commandId, utils::whatever<64>)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
    {
      auto *selector = (PopupSelector *)parent;
      
      if (!parentList)
        positionInitialPopupList(this, selector, selector->summoner, selector->listPlacement, selector->summoningPoint);
      else
        positionInitialPopupList(this, selector, parentItem, selector->lastPlacement, { 4, 0 });

      calculatePositions(children, this);

      return true;
    }
    }

    return false;
  }

  static bool 
  isPointInsideTriangle(Point<float> point, Point<float> A, Point<float> B, Point<float> C)
  {
    // https://math.stackexchange.com/a/51459

    auto cross = [](Point<float> a, Point<float> b) { return a.x * b.y - a.y * b.x; };
    
    auto crossAB = cross(A, B);
    auto crossBC = cross(B, C);
    auto crossCA = cross(C, A);

    float xd = crossAB * crossBC * crossCA;
    if (utils::abs(xd) <= 1e-13f)
      return false;
    
    float wA = (crossBC + cross(point, B - C)) / xd;
    float wB = (crossCA + cross(point, C - A)) / xd;
    float wC = (crossAB + cross(point, A - B)) / xd;

    if (wA < 0 && wB < 0 && wC < 0)
    {
      wA = -wA;
      wB = -wB;
      wC = -wC;
    }
    
    return (0.0f <= wA && wA <= 1.0f) &&
      (0.0f <= wB && wB <= 1.0f) && (0.0f <= wC && wC <= 1.0f);
  }

  void PopupList::summonChildList(PopupItem *summoningItem,
    const MouseEvent &summoningMouseEvent)
  {
    static constexpr double kSublistConeTimeout = 0.25; //s



    bool shouldCloseSublist = uiRelated.steadyTime - lastSummonTime >= kSublistConeTimeout;
    auto event = summoningMouseEvent.getEventRelativeTo(parentSelector);
    if (currentSublistItem)
    {
      // adjust triangle cone
      auto childListBounds = parentSelector->getRelativeArea(currentSublistItem->childList);
      auto topPoint = (childListBounds.x > bounds.x) ? childListBounds.getTopLeft() : childListBounds.getTopRight();
      auto bottomPoint = (childListBounds.x > bounds.x) ? childListBounds.getBottomLeft() : childListBounds.getBottomRight();

      //shouldCloseSublist = shouldCloseSublist || !isPointInsideTriangle({ (float)event.x, (float)event.y },
      //  sublistAnchorPoint.toFloat(), topPoint.toFloat(), bottomPoint.toFloat());

      if (shouldCloseSublist)
        sublistAnchorPoint = { event.x, event.y };
    }

    // refreshing the last summon time while we're on the summoning item
    lastSummonTime = (currentSublistItem != summoningItem && summoningItem && (summoningItem->childList || currentSublistItem) &&
      summoningItem->contains(Point{ summoningMouseEvent.x, summoningMouseEvent.y })) ?
      lastSummonTime : uiRelated.steadyTime;

    if (currentSublistItem != summoningItem && shouldCloseSublist)
    {
      // hide previous child list if it exists
      if (currentSublistItem)
      {
        parentSelector->removeChildComponent(currentSublistItem->childList);
        currentSublistItem = {};
        sublistAnchorPoint = {};
      }
      
      if (summoningItem && summoningItem->childList)
      {
        auto newSublist = summoningItem->childList;
        newSublist->parentSelector = parentSelector;
        newSublist->parentItem = summoningItem;
        newSublist->parentList = this;
        parentSelector->addChildComponent(newSublist);
        currentSublistItem = summoningItem;
        sublistAnchorPoint = { event.x, event.y };
      }
    }
  }

  bool 
  PopupItem::mouseMove(const MouseEvent &e)
  {
    if (!canBeChosen)
      return false;

    associatedList->lastMouseEvent = e;

    associatedList->summonChildList(this, e);

    return true;
  }

  bool
  PopupItem::mouseDown(const MouseEvent &)
  {
    componentFlags.isClicked = false;

    return true;
  }

  bool
  PopupItem::mouseUp(const MouseEvent &)
  {
    if (!canBeChosen || childList)
      return false;

    associatedList->parentSelector->newSelection(this);

    return true;
  }

  bool
  PopupItem::render(OpenGlWrapper &openGl)
  {
    if ((componentFlags.isHovered || componentFlags.isClicked) && canBeChosen)
      fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kWidgetPrimary1, this).darker(0.8f));

    return true;
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

      for (auto *child = children; child; child = child->next)
        child->componentFlags.isVisible = !summoner->isObscured();

      return true;
    }

    return false;
  }

  void PopupSelector::resetState()
  {
    removeAllChildComponents();
    summoner = {};
    callback = {};
    if (cancel)
      cancel(this);
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
