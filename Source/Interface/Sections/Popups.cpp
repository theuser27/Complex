
// Created: 2023-02-02 19:49:54

#include "Popups.hpp"

#include "Framework/parameter_value.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../Components/Control.hpp"

namespace Interface
{
  static Range<i32>
  getPopupDisplayDimensions(Component *c, bool isCalculatingVertical)
  {
    const int lineHeight = scaleValueRoundInt(PopupDisplay::kLineHeight);

    auto *self = (PopupDisplay *)c;
    if (!isCalculatingVertical)
    {
      if (self->isControl)
      {
        auto *control = (Control *)self->source;
        control->getScaledValueString(self->text, control->getValue());
      }

      uiRelated.cache->setFont((self->isControl) ? FontId::DDinType : FontId::InterType, (float)lineHeight);
      self->cachedFontWidth = (i32)::ceilf(uiRelated.cache->getStringWidthFloat(self->text));
      return Range<i32>{ 0, self->cachedFontWidth };
    }
    else
    {
      auto lineCount = (i32)::ceilf((float)self->cachedFontWidth / (float)self->bounds.w);
      return Range<i32>{ lineHeight, lineCount * lineHeight };
    }
  }

  void PopupDisplay::reinitialise()
  {
    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(8));

    placement = Placement::custom;
    padding = { kLineHeight / 4, 0, kLineHeight / 4, 0 };
    overrideSize = getPopupDisplayDimensions;
    text = { arena };

    overridePosition = [](Component *c)
    {
      auto *self = (PopupDisplay *)c;

      Point<i32> position{};
      COMPLEX_ASSERT((self->relativePlacement & Placement::custom) == 0);

      switch ((Placement)(self->relativePlacement & Placement::justifyX))
      {
      case Placement::left:
        position.x -= self->bounds.w + self->offset.x;
        break;

      case Placement::right:
        position.x += self->source->bounds.w + self->offset.x;
        break;

      default:
      case Placement::justifyX:
        position.x += (self->source->bounds.w - self->bounds.w) / 2;
        break;
      }

      switch ((Placement)(self->relativePlacement & Placement::justifyY))
      {
      case Placement::top:
        position.y -= self->bounds.h + self->offset.y;
        break;

      default:
      case Placement::bottom:
        position.y += self->source->bounds.h + self->offset.y;
        break;

      case Placement::justifyY:
        position.y += (self->source->bounds.h - self->bounds.h) / 2;
        break;
      }

      if (!self->source->componentFlags.isPositionSet)
        return false;

      auto [x, y] = self->parent->getRelativePoint(self->source, position);
      self->bounds.x = utils::clamp(x, 0, self->parent->bounds.w - self->bounds.w);
      self->bounds.y = utils::clamp(y, 0, self->parent->bounds.h - self->bounds.h);

      return true;
    };
  }

  bool 
  PopupDisplay::render(OpenGlWrapper &openGl)
  {
    auto localBounds = getLocalBounds().toFloat();
    float rounding = getValue(Skin::kBodyRoundingTop, true, this);
    fillRect(openGl, localBounds, getColour(Skin::kPopupDisplayBackground, this), rounding);
    strokeRect(openGl, localBounds, scaleValue(1.0f), getColour(Skin::kPopupDisplayBorder, this), rounding);

    auto textBounds = localBounds.withTrim(scaleValue(padding.toFloat()));
    auto usedFontId = (isControl) ? FontId::DDinType : FontId::InterType;
    renderText(text, usedFontId, textBounds, openGl, 
      getColour(Skin::kWidgetPrimary1, source), Placement::left, true);

    //reinitialise();

    return true;
  }

  static void positionInitialPopupList(PopupList *list, PopupSelector *selector,
    Component *relativeComponent, Placement placement, Point<i32> extraPosition);

  PopupList::PopupList(PopupSelector *parentSelector) : parentSelector{ parentSelector }
  {
    placement = Placement::custom;
    sizingFlags |= Component::ScrollableWithBarY | Component::SnapToMinX;
    componentFlags.vertical = true;
    componentFlags.clickable = true;
    desiredSize = { kPopupMinWidth, 0, utils::max_limit<i32>, utils::max_limit<i32> };
    padding = { 0, 4, 0, 4 };

    overridePosition = [](Component *c)
    {
      auto *self = (PopupList *)c;
      auto *selector = (PopupSelector *)self->parent;

      if (!self->parentList)
        positionInitialPopupList(self, selector, selector->summoner, selector->listPlacement, selector->summoningPoint);
      else
        positionInitialPopupList(self, selector, self->parentItem, selector->lastPlacement, { 4, 0 });

      calculatePositions(self->children, self);

      return true;
    };
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

      list->bounds.x = x;
      list->bounds.y = y;

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

    list->bounds.x = finalX;
    list->bounds.y = finalY;
  }

  void PopupList::summonChildList(PopupItem *summoningItem,
    const MouseEvent &summoningMouseEvent, bool force)
  {
    static constexpr double kSublistConeTimeout = 0.25; //s

    bool shouldCloseSublist = force || uiRelated.steadyTime - lastSummonTime >= kSublistConeTimeout;
    auto event = summoningMouseEvent.getEventRelativeTo(parentSelector);
    if (currentSublistItem)
    {
      // adjust triangle cone
      auto childListBounds = parentSelector->getRelativeArea(currentSublistItem->childList);
      auto topPoint = (childListBounds.x > bounds.x) ? childListBounds.getTopLeft() : childListBounds.getTopRight();
      auto bottomPoint = (childListBounds.x > bounds.x) ? childListBounds.getBottomLeft() : childListBounds.getBottomRight();

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
  PopupList::drawV1(OpenGlWrapper &openGl, PopupList *self)
  {
    auto topPadding = scaleValue((float)self->padding.y);
    auto bottomPadding = scaleValue((float)self->padding.h);

    fillRect(openGl, self->getLocalBounds().toFloat(), getColour(Skin::kBody, self),
      topPadding, topPadding, bottomPadding, bottomPadding);

    self->doRenderChildren(openGl);

    strokeRect(openGl, self->getLocalBounds().toFloat(), scaleValue(1.0f),
      Colour{ 45, 45, 45 }, topPadding);

    return false;
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
  PopupItem::mouseDown(const MouseEvent &e)
  {
    componentFlags.isClicked = false;
    if (childList)
    {
      associatedList->lastMouseEvent = e;

      associatedList->summonChildList(this, e, true);
    }

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
      fillRect(openGl, getLocalBounds().toFloat(), getHighlightColour());

    return true;
  }

  void PopupSelector::reinitialise()
  {
    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(128));

    placement = Placement::custom;
    sizingFlags |= GrowableX | GrowableY;

    overridePosition = [](Component *c)
    {
      auto *self = (PopupSelector *)c;

      // position stays relative to component we're attached to
      self->bounds = self->bounds.withPosition(self->parent->bounds.getPosition());

      for (auto *child = self->children; child; child = child->next)
        child->componentFlags.isVisible = !self->summoner->isObscured();

      return true;
    };
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

    resetState(false);
  }

  bool 
  PopupSelector::handleFocus(bool hasFocus, FocusChange, Component *correspondent)
  {
    if (!hasFocus)
    {
      if ((correspondent == summoner && toggleable) || isParentOf(correspondent))
        return false;

      componentFlags.isVisible = false;
      if (cancel)
        cancel(this);

      resetState(false);
    }

    return true;
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

  void PopupSelector::resetState(bool callCancelFn)
  {
    removeAllChildComponents();
    summoner = {};
    callback = {};
    if (cancel && callCancelFn)
      cancel(this);
    cancel = {};
    lastPlacement = Placement::right;
    utils::bumpArena::clear(arena);
    skinOverride = Skin::kNone;
    toggleable = true;
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
  
  OptionPopupItem::OptionPopupItem()
  {
    componentFlags.acceptsOrphanMouseEvents = true;
    sizingFlags = Component::GrowableX;
    desiredSize = kPrimarySizeRect;
    padding = kPrimaryPaddingRect;
    overrideSize = [](Component *c, bool isCalculatingVertical)
    {
      auto *item = (OptionPopupItem *)c;

      i32 minSize{ }, maxSize{ };
      auto [text, canTextWrap] = item->getTextAndWrap();

      float height = scaleValue((float)item->desiredSize.h);
      uiRelated.cache->setFont(FontId::InterType, height);
      nvgTextAlign(uiRelated.cache->context, NVG_ALIGN_LEFT | NVG_ALIGN_CENTER);

      if (!isCalculatingVertical)
      {
        minSize = (canTextWrap) ? 0 : (i32)::ceilf(uiRelated.cache->getStringWidthFloat(text));
        maxSize = -1;
      }
      else
      {
        auto lineCount = uiRelated.cache->getStringNumberOfLines(text, (float)item->bounds.w);
        minSize = (i32)::roundf(height * (float)lineCount);
        maxSize = minSize;
      }

      return Range<i32>{ minSize, maxSize };
    };
  }

  bool 
  OptionPopupItem::render(OpenGlWrapper &openGl)
  {
    PopupItem::render(openGl);

    auto [text, canTextWrap] = getTextAndWrap();
    Skin::ColourId textColourId = Skin::kTextComponentText1;

    if (!canBeChosen)
    {
      textColourId = Skin::kTextComponentText2;
      // draw background
      fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kPopupSelectorDelimiter, this));
    }

    renderText(text, FontId::InterType, getLocalBounds().withTrim(scaleValueRoundInt(padding.toInt())).toFloat(),
      openGl, getColour(textColourId, this), Placement::left, canTextWrap);

    return true;
  }

  utils::pair<utils::string_view, bool> 
  OptionPopupItem::getTextAndWrap()
  {
    // label whose extra data is a string
    if (dataTag == StringData)
      return { *((utils::stringnd *)extraData), true };
    else if (dataTag == OptionData)
    {
      auto *option = (Framework::IndexedData *)extraData;
      return { option->getText(), !option->canBeChosen() };
    }

    COMPLEX_ASSERT("Unknown tag");
    return {};
  }

  OptionPopupItem *
  OptionPopupItem::createTitle(PopupList *list, utils::string_view text)
  {
    auto *itemArena = list->parentSelector->arena;
    auto *item = anew(itemArena, OptionPopupItem, {});
    item->extraData = anew(itemArena, utils::stringnd, { itemArena, text });
    item->dataTag = StringData;
    item->canBeChosen = false;
    item->associatedList = list;
    item->padding = kSecondaryPaddingRect;
    item->desiredSize = kSecondarySizeRect;

    return item;
  }

  OptionPopupItem *
  OptionPopupItem::createOption(PopupList *list, Framework::IndexedData &option)
  {
    auto *item = anew(list->parentSelector->arena, OptionPopupItem, {});
    item->extraData = &option;
    item->dataTag = OptionData;
    item->canBeChosen = option.canBeChosen();
    item->associatedList = list;
    item->padding = (item->canBeChosen) ? item->padding : kSecondaryPaddingRect;
    item->desiredSize = (item->canBeChosen) ? item->desiredSize : kSecondarySizeRect;

    return item;
  }
}
