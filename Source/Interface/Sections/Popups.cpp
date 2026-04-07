
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
    overrideSize = getPopupDisplayDimensions;
    text = { arena };

    textFontId = FontId::InterType;
    numericFontId = FontId::DDinType;

    overridePosition = [](Component *c)
    {
      auto *self = (PopupDisplay *)c;

      Point<i32> position{};
      COMPLEX_ASSERT((self->placement & Placement::custom) == 0);
      i32 extraOffsetX = self->bounds.w / 2;
      i32 extraOffsetY = self->bounds.h / 2;

      switch ((Placement)(self->placement & Placement::justifyX))
      {
      case Placement::left:
        position.x -= self->bounds.w + extraOffsetX;
        break;

      case Placement::right:
        position.x += self->source->bounds.w + extraOffsetX;
        break;

      default:
      case Placement::justifyX:
        position.x += (self->source->bounds.w - self->bounds.w) / 2;
        break;
      }

      switch ((Placement)(self->placement & Placement::justifyY))
      {
      case Placement::top:
        position.y -= self->bounds.h + extraOffsetY;
        break;

      default:
      case Placement::bottom:
        position.y += self->source->bounds.h + extraOffsetY;
        break;

      case Placement::justifyY:
        position.y += (self->source->bounds.h - self->bounds.h) / 2;
        break;
      }

      self->bounds.setPosition(self->getRelativePoint(self->source, position));
    };
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

  static void positionInitialPopupList(PopupList *list, PopupSelector *selector,
    Component *relativeComponent, Placement placement, Point<i32> extraPosition);

  PopupList::PopupList(PopupSelector *parentSelector) : parentSelector{ parentSelector }
  {
    placement = Placement::custom;
    sizingFlags |= Component::ScrollableWithBarY | Component::SnapToMinX;
    componentFlags.vertical = true;
    componentFlags.clickable = true;

    overridePosition = [](Component *c)
    {
      auto *self = (PopupList *)c;
      auto *selector = (PopupSelector *)self->parent;

      if (!self->parentList)
        positionInitialPopupList(self, selector, selector->summoner, selector->listPlacement, selector->summoningPoint);
      else
        positionInitialPopupList(self, selector, self->parentItem, selector->lastPlacement, { 4, 0 });

      calculatePositions(self->children, self);
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
      self->bounds.setPosition(self->parent->bounds.getPosition());

      for (auto *child = self->children; child; child = child->next)
        child->componentFlags.isVisible = !self->summoner->isObscured();

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

    resetState();
  }

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
  
  OptionPopupItem::OptionPopupItem()
  {
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

    renderText(text, FontId::InterType, getLocalBounds().trimmed(scaleValueRoundInt(padding.toInt())).toFloat(),
      openGl.cache, getColour(textColourId, this), Placement::left, canTextWrap);

    return true;
  }

  utils::pair<utils::string_view, bool> 
  OptionPopupItem::getTextAndWrap()
  {
    // label whose extra data is a string
    if (id)
      return { *((utils::stringnd *)extraData), true };

    auto *option = (Framework::IndexedData *)extraData;
    return { option->getText(), !option->canBeChosen() };
  }

  PopupList *
  OptionPopupItem::createPopupList(PopupSelector *selector, 
    utils::string_view title, Framework::IndexedData *options)
  {
    auto *itemArena = selector->arena;

    PopupList *list = anew(itemArena, PopupList, { selector });
    list->draw = [](OpenGlWrapper &openGl, PopupList *self)
    {
      auto topPadding = scaleValue((float)self->padding.y);
      auto bottomPadding = scaleValue((float)self->padding.h);

      fillRect(openGl, self->getLocalBounds().toFloat(), getColour(Skin::kBody, self),
        topPadding, topPadding, bottomPadding, bottomPadding);

      self->doRenderChildren(openGl);

      strokeRect(openGl, self->getLocalBounds().toFloat(), scaleValue(1.0f),
        Colour{ 45, 45, 45 }, topPadding);

      return false;
    };

    static constexpr Rectangle<u16> kPrimaryPaddingRect =
    { kPopupHorizontalPadding, kPopupVerticalPrimaryPadding, kPopupHorizontalPadding, kPopupVerticalPrimaryPadding };
    static constexpr Rectangle<u16> kSecondaryPaddingRect =
    { kPopupHorizontalPadding, kPopupVerticalSecondaryPadding, kPopupHorizontalPadding, kPopupVerticalSecondaryPadding };
    static constexpr Rectangle kPrimarySizeRect = { 0, kPrimaryTextLineHeight, 0, kPrimaryTextLineHeight };
    static constexpr Rectangle kSecondarySizeRect = { 0, kSecondaryTextLineHeight, 0, kSecondaryTextLineHeight };

    if (!title.empty())
    {
      auto *name = anew(itemArena, OptionPopupItem, {});
      name->arena = itemArena;
      name->id = 1;
      name->canBeChosen = false;
      name->sizingFlags = (Component::SizingFlags)(Component::GrowableX);
      name->extraData = anew(itemArena, utils::stringnd, { itemArena, title });
      name->associatedList = list;
      name->padding = kSecondaryPaddingRect;
      name->desiredSize = kSecondarySizeRect;
      list->addChildComponent(name);
    }

    (void)Framework::iterateOverIndexedData(options,
      [itemArena, list](Framework::IndexedData &option)
      {
        if (!option.childrenCount && !option.canBeChosen())
          return false;

        auto *item = anew(itemArena, OptionPopupItem, {});
        item->arena = itemArena;
        item->extraData = &option;
        item->canBeChosen = option.canBeChosen();
        item->associatedList = list;
        item->sizingFlags = (Component::SizingFlags)(Component::GrowableX);
        item->componentFlags.acceptsOrphanedMouseEvents = item->canBeChosen;
        item->padding = (item->canBeChosen) ? kPrimaryPaddingRect : kSecondaryPaddingRect;
        item->desiredSize = (item->canBeChosen) ? kPrimarySizeRect : kSecondarySizeRect;
        list->addChildComponent(item);

        return false;
      });

    return list;
  }

}
