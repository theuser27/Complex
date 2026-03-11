
// Created: 2023-12-11 16:49:17

#include "BaseComponent.hpp"

#include "ui_constants.hpp"
#include "Graphics.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "Generation/BaseProcessor.hpp"
#include "../Components/BaseControl.hpp"
#include "../Sections/MainInterface.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
#define GET_PRIMARY_FLAG(component, flag) ((flag) << (i32)(!!(component->componentFlags.vertical)))
#define GET_SECONDARY_FLAG(component, flag) ((flag) << (1 - (!!(component->componentFlags.vertical))))
#define TRANSPOSE(component, rect) ((component->componentFlags.vertical) ? (rect).transposed() : (rect))

  static void calculateFit(Component *component, Component *children, bool isCalculatingVertical)
  {
    if (!component->componentFlags.isVisible)
    {
      component->previousPosition = { -1, -1 };
      return;
    }

    if (!isCalculatingVertical)
    {
      // setup on first run
      component->fadeawayRatio = 0;
      component->lastBounds = component->bounds;
      component->bounds = {};
    }

    auto Rectangle<i32>:: *minMember = (isCalculatingVertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
    auto Rectangle<i32>:: *maxMember = (isCalculatingVertical) ? &Rectangle<i32>::h : &Rectangle<i32>::w;

    Range<i64> sizes{};
    Range<i64> nonPositionedSizes{};

    Range<i32> sameAsSiblingsSizes{};
    utils::vector<Component *> sameAsSiblingsComponents{};

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      calculateFit(child, child->children, isCalculatingVertical);

      auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

      sameAsSiblingsSizes.min = utils::max(sameAsSiblingsSizes.min, child->bounds.*minMember);
      sameAsSiblingsSizes.max = utils::max(sameAsSiblingsSizes.max, child->bounds.*minMember);

      // when parent has horizontal/vertical orientation but we're calculating vertical/horizontal sizes
      if (component->componentFlags.vertical ^ isCalculatingVertical)
      {
        if (test_enum(child->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
          sameAsSiblingsComponents.emplaceBack(child);
        else
        {
          sizes.min = utils::max(sizes.min, (i64)(child->bounds.*minMember) + extra);
          sizes.max = utils::max(sizes.max, (i64)(child->bounds.*maxMember) + extra);
        }
      }
      else
      {
        if (child->placement == Placement::custom)
        {
          nonPositionedSizes.min = utils::max(nonPositionedSizes.min, (i64)(child->bounds.*minMember) + extra);
          nonPositionedSizes.max = utils::max(nonPositionedSizes.max, (i64)(child->bounds.*maxMember) + extra);
        }
        else
        {
          sizes.min += (i64)(child->bounds.*minMember) + extra;
          sizes.max += (i64)(child->bounds.*maxMember) + extra;
        }

        // taking care of children that conform to siblings' primary size
        if (test_enum(child->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
        {
          sameAsSiblingsComponents.emplaceBack(child);
        }
      }
    }

    for (auto *child : sameAsSiblingsComponents)
    {
      if (component->componentFlags.vertical ^ isCalculatingVertical)
      {
        child->bounds.*minMember = (i32)sameAsSiblingsSizes.min;
        child->bounds.*maxMember = (i32)sameAsSiblingsSizes.max;
        continue;
      }

      if (child->placement == Placement::custom)
      {
        auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();
        nonPositionedSizes.min = utils::max(nonPositionedSizes.min, (i64)sameAsSiblingsSizes.min + extra);
        nonPositionedSizes.max = utils::max(nonPositionedSizes.max, (i64)sameAsSiblingsSizes.max + extra);
      }
      else
      {
        sizes.min += sameAsSiblingsSizes.min - child->bounds.*minMember;
        sizes.max += sameAsSiblingsSizes.max - child->bounds.*maxMember;
      }

      child->bounds.*minMember = (i32)sameAsSiblingsSizes.min;
      child->bounds.*maxMember = (i32)sameAsSiblingsSizes.max;
    }

    sizes.min = utils::clamp(nonPositionedSizes.min, sizes.min, (i64)utils::max_limit<i32>);
    sizes.max = utils::clamp(nonPositionedSizes.max, sizes.max, (i64)utils::max_limit<i32>);

    if (test_enum(component->sizingFlags, ((isCalculatingVertical) ? Component::FixedY : Component::FixedX)))
    {
      component->bounds.*minMember = component->desiredSize.*minMember;
      component->bounds.*maxMember = component->desiredSize.*maxMember;
    }
    else
    {
      component->bounds.*minMember = utils::max(component->desiredSize.*minMember, (i32)sizes.min);

      auto growableFlag = (isCalculatingVertical) ? Component::GrowableY : Component::GrowableX;

      component->bounds.*maxMember = (component->sizingFlags & growableFlag) ? utils::max_limit<i32> :
        utils::max(component->bounds.*minMember,
          utils::min(component->desiredSize.*maxMember, (i32)sizes.max));
    }

    // the user can override the above calculations if they so choose
    if (component->overrideDimensions)
    {
      auto [min, max] = component->overrideDimensions(component, isCalculatingVertical);
      // returned bounds are scaled so we need to unscale them
      // in order to work with the rest of the values
      if (min >= 0)
        component->bounds.*minMember = (i32)::ceilf(unscaleValue((float)min));
      if (max >= 0)
        component->bounds.*maxMember = (i32)::ceilf(unscaleValue((float)max));
    }

    // alogn the primary   axis SameAsSiblings must be zero, the parent will take care of it
    // along the secondary axis SameAsSiblings acts like Growable
    //if (test_enum(component->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
    //  component->bounds.*maxMember = (component->componentFlags.isVertical ^ isCalculatingVertical) ? utils::max_limit<i32> : 0;

    // add this component's padding to the total size
    auto padding = (isCalculatingVertical) ?
      component->padding.getBottom() : component->padding.getRight();

    component->bounds.*minMember = (i32)utils::min((i64)utils::max_limit<i32>,
      (i64)(component->bounds.*minMember) + padding);
    component->bounds.*maxMember = (i32)utils::min((i64)utils::max_limit<i32>,
      (i64)(component->bounds.*maxMember) + padding);
  }

  constinit utils::vector<Component *> *sortedSizesMin{};
  constinit utils::vector<Component *> *sortedSizesMax{};

  static void calculateGrow(Component *component, Component *children, bool isCalculatingVertical)
  {
    auto Rectangle<i32>:: *minMember = (isCalculatingVertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
    auto Rectangle<i32>:: *maxMember = (isCalculatingVertical) ? &Rectangle<i32>::h : &Rectangle<i32>::w;

    // when parent has horizontal/vertical orientation but we're calculating vertical/horizontal sizes
    if (component->componentFlags.vertical ^ isCalculatingVertical)
    {
      for (auto *child = children; child; child = child->next)
      {
        if (!child->componentFlags.isVisible)
          continue;

        auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

        auto maxSize = utils::min(child->bounds.*maxMember, component->bounds.*maxMember - extra);
        // scaling the final secondary size
        child->bounds.*maxMember = scaleValueRoundInt((float)maxSize);
        calculateGrow(child, child->children, isCalculatingVertical);
      }

      return;
    }

    if (!children)
      return;

    auto &sortedMin = *sortedSizesMin;
    auto &sortedMax = *sortedSizesMax;
    sortedMin.clear();
    sortedMax.clear();

    // account for this component's padding from the size we were assigned
    i32 minSize = (isCalculatingVertical) ?
      component->padding.getBottom() : component->padding.getRight();
    i32 maxSize = minSize;
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

      // not adding minMember because we're going to compare against it in the next pass
      minSize += extra;
      maxSize += child->bounds.*maxMember + extra;

      auto iter = utils::find_if(sortedMin, [&](const Interface::Component *c)
        { return c->bounds.*minMember > child->bounds.*minMember; });
      sortedMin.emplace(iter, child);

      iter = utils::find_if(sortedMax, [&](const Interface::Component *c)
        { return c->bounds.*maxMember > child->bounds.*maxMember; });
      sortedMax.emplace(iter, child);
    }

    if (component->bounds.*maxMember < minSize &&
      test_enum(component->sizingFlags, Component::ScrollableX))
    {
      // parent is scrollable so every child is set to their preferred max sizes
      // which is already done, therefore this is a no-op
      //
      // NB: if you have a scrollable parent and a growable child
      // along the same axis, this will fail spectacularly
      // (although does that even make sense?)

      ((isCalculatingVertical) ?
        component->scrollableArea.h : component->scrollableArea.w) = maxSize;
    }
    else
    {
      i32 remaining = component->bounds.*maxMember - minSize;

      COMPLEX_ASSERT(remaining >= 0);

      i32 smallestMax;
      i32 biggestMin;
      usize count;
      usize j = 0;
      {
        usize lastJ = 0;
        usize i = sortedMin.size(), lastI = sortedMin.size();

        while (true)
        {
          // if the remaining size cannot expand all smaller components to the current minimum, exclude the biggest minimum
          if (i > 1 &&
            (sortedMin[i - 1]->bounds.*minMember * (i32)(i - j)) > remaining)
          {
            --i;
          }
          // if the remaining size can now expand all smaller components to the next minimum, include the next biggest minimum
          else if (i < sortedMin.size() &&
            (sortedMin[i]->bounds.*minMember * (i32)(i - j)) < remaining)
          {
            ++i;
          }

          i32 currentSize = sortedMin[i - 1]->bounds.*minMember;
          if (i < sortedMin.size())
            currentSize += (remaining - (sortedMin[i]->bounds.*minMember * (i32)(i - j)));

          // if the most constrained component's maximum gets surpassed by the current size,
          // exclude it and update the remainder
          if (sortedMax[j]->bounds.*maxMember < currentSize)
          {
            // j can only get incremented and because we always assume that minMember <= maxMember
            // we don't have to erase the skipped component in sortedMin here after incrementing j
            if (j < sortedMax.size())
              remaining -= sortedMax[j]->bounds.*maxMember;
            ++j;
          }

          if (i == lastI && j == lastJ)
            break;

          lastI = i;
          lastJ = j;
        }

        biggestMin = sortedMin[i - 1]->bounds.*minMember;
        smallestMax = sortedMax[j]->bounds.*maxMember;
        count = i - j;
      }


      for (usize k = 0; k < count; ++k)
      {
        auto *c = sortedMax[j + k];
        if (c->bounds.*minMember <= biggestMin &&
          c->bounds.*maxMember >= smallestMax)
        {
          i32 size = remaining / (i32)(count - k);

          c->bounds.*maxMember = utils::clamp(size, c->bounds.*minMember, c->bounds.*maxMember);

          remaining -= c->bounds.*maxMember;
        }
      }
    }

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      // scaling the final primary size
      child->bounds.*maxMember = scaleValueRoundInt((float)(child->bounds.*maxMember));
      calculateGrow(child, child->children, isCalculatingVertical);
    }
  }

  void calculateSizes(Component *children, Component *component)
  {
    utils::vector<Component *> sortedSizesMin_{ localScratch, 32 };
    sortedSizesMin = &sortedSizesMin_;
    utils::vector<Component *> sortedSizesMax_{ localScratch, 32 };
    sortedSizesMax = &sortedSizesMax_;

    calculateFit(component, children, false);
    calculateGrow(component, children, false);
    calculateFit(component, children, true);
    calculateGrow(component, children, true);
  }

  extern utils::vector<Component *> *customPlacement;

#define IS_PRIMARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_PRIMARY_FLAG(parent, flag))
#define IS_SECONDARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_SECONDARY_FLAG(parent, flag))

  void calculatePositions(Component *children,
    Component *component, Rectangle<i32> boundsInTarget)
  {
    if (!component->componentFlags.isVisible)
      return;

    auto padding = scaleValueRoundInt(component->padding.toInt());
    if (boundsInTarget.isEmpty())
    {
      boundsInTarget = Rectangle{ padding.x, padding.y,
        component->bounds.w - padding.getRight(),
        component->bounds.h - padding.getBottom() };
    }

    if (test_enum(component->sizingFlags, Component::ScrollableX))
      boundsInTarget.x -= component->scrollOffset.x;
    if (test_enum(component->sizingFlags, Component::ScrollableY))
      boundsInTarget.y -= component->scrollOffset.y;

    auto Point<i32>:: *primary = (component->componentFlags.vertical) ? &Point<i32>::y : &Point<i32>::x;
    auto Point<i32>:: *secondary = (component->componentFlags.vertical) ? &Point<i32>::x : &Point<i32>::y;

    // setting the supposed position and
    // resetting previous position with current if the supposed position has changed from previous run
    auto setNextPosition = [&](bool isRight, Rectangle<i32> bounds, Component *child, i32 Point<i32>:: *coordinate)
    {
      auto change = child->nextPosition.*coordinate - bounds.getPosition().*coordinate;
      if (isRight)
      {
        // was shift caused by a change in size?
        auto sizeOffset = Point{ bounds.w, bounds.h }.*coordinate -
          Point{ child->lastBounds.w, child->lastBounds.h }.*coordinate;

        if (change && change - sizeOffset == 0)
          child->previousPosition.*coordinate += sizeOffset;
        change -= sizeOffset;
      }

      if (change)
      {
        child->previousPosition = child->lastBounds.getPosition();
        child->distanceToNextPositionRatio = 0;
      }
      child->nextPosition.*coordinate = bounds.getPosition().*coordinate;
    };

    boundsInTarget = TRANSPOSE(component, boundsInTarget);
    auto originalBounds = boundsInTarget;
    padding = TRANSPOSE(component, padding);

    i32 primaryUsed = 0;
    i32 groupCount = 0;
    bool isInGroup = false;
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      if (child->placement & Placement::custom)
      {
        // component has decided to take care of its own position
        customPlacement->emplaceBack(child);
        continue;
      }

      auto bounds = TRANSPOSE(component, child->bounds);
      auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());
      primaryUsed += bounds.w + margin.getRight();

      if (auto type = child->placement & GET_PRIMARY_FLAG(component, Placement::justifyX);
        type == GET_PRIMARY_FLAG(component, Placement::justifyX) || type == 0)
      {
        if (!isInGroup)
          ++groupCount;
        isInGroup = true;
      }
      else
        isInGroup = false;

      // we can calculate secondary position here
      if (IS_SECONDARY_POSITION_FLAG_SET(child, component, Placement::left))
        bounds.y = boundsInTarget.y + margin.y;
      else if (IS_SECONDARY_POSITION_FLAG_SET(child, component, Placement::right))
        bounds.y = boundsInTarget.h - margin.h - bounds.h;
      else
        bounds.y = utils::max(boundsInTarget.y + margin.y,
          (boundsInTarget.getBottom() - bounds.h) / 2);

      setNextPosition(IS_SECONDARY_POSITION_FLAG_SET(child, component, Placement::right),
        TRANSPOSE(component, bounds), child, secondary);

      //child->bounds.setPosition(TRANSPOSE(component, bounds.getPosition()));
    }

    groupCount = utils::max(1, groupCount);
    i32 sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / groupCount;

    for (auto *child = children; child;)
    {
      // skip if custom or invisible
      if ((child->placement & Placement::custom) ||
        !child->componentFlags.isVisible)
      {
        child = child->next;
        continue;
      }

      if (auto type = child->placement & GET_PRIMARY_FLAG(component, Placement::justifyX);
        type == GET_PRIMARY_FLAG(component, Placement::justifyX) || type == 0)
      {
        auto *nextChild = child;

        if (type == 0)
        {
          // centred components

          i32 offset = sizeLeftPerGroup / 2;
          boundsInTarget.x += offset;

          for (; nextChild && (nextChild->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type;
            nextChild = nextChild->next)
          {
            if (!nextChild->componentFlags.isVisible)
              continue;

            auto bounds = TRANSPOSE(component, nextChild->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, nextChild->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            setNextPosition(false, TRANSPOSE(component, bounds), nextChild, primary);
            //nextChild->bounds = TRANSPOSE(component, bounds);
          }

          boundsInTarget.x += sizeLeftPerGroup - offset;
        }
        else
        {
          // inside a justify group

          usize j = 0;

          for (; nextChild && (nextChild->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type;
            nextChild = nextChild->next) { if (nextChild->componentFlags.isVisible) ++j; }

          i32 size = sizeLeftPerGroup;
          i32 offset = 0;

          for (auto *groupChild = child; j; groupChild = groupChild->next)
          {
            if (!groupChild->componentFlags.isVisible)
              continue;

            boundsInTarget.x += offset;

            auto bounds = TRANSPOSE(component, groupChild->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, groupChild->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            setNextPosition(false, TRANSPOSE(component, bounds), groupChild, primary);
            //groupChild->bounds = TRANSPOSE(component, bounds);

            // we expect a division by zero on the last iteration
            offset = size / utils::max((i32)(j - 1), 1);
            size -= offset;
            --j;
          }
        }

        --groupCount;
        primaryUsed += sizeLeftPerGroup;
        // we expect a division by zero on the last iteration
        sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / utils::max(groupCount, 1);
        child = nextChild;
      }
      else if (IS_PRIMARY_POSITION_FLAG_SET(child, component, Placement::left))
      {
        auto bounds = TRANSPOSE(component, child->bounds);
        auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());

        bounds.x = boundsInTarget.x + margin.x;
        boundsInTarget.trimLeft(bounds.w + margin.getRight());

        setNextPosition(false, TRANSPOSE(component, bounds), child, primary);
        //child->bounds = TRANSPOSE(component, bounds);
        child = child->next;
      }
      else if (IS_PRIMARY_POSITION_FLAG_SET(child, component, Placement::right))
      {
        auto *nextChild = child;

        // start from the end
        for (; nextChild->next && IS_PRIMARY_POSITION_FLAG_SET(nextChild->next, component, Placement::right);
          nextChild = nextChild->next) { }

        child = nextChild->next;

        // in reverse start from the right-most
        do
        {
          if (nextChild->componentFlags.isVisible)
          {
            auto bounds = TRANSPOSE(component, nextChild->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, nextChild->margin).toInt());

            bounds.x = boundsInTarget.getRight() - margin.w - bounds.w;
            boundsInTarget.w -= bounds.w + margin.getRight();

            setNextPosition(true, TRANSPOSE(component, bounds), nextChild, primary);
            //nextChild->bounds = TRANSPOSE(component, bounds);
          }
          nextChild = nextChild->previous;

        } while (nextChild->next && IS_PRIMARY_POSITION_FLAG_SET(nextChild, component, Placement::right));
      }
      else
      {
        COMPLEX_ASSERT_FALSE("Unknown placement for component");
        child = child->next;
      }
    }

    bool wasThisResized = component->bounds.withZeroOrigin() != component->lastBounds.withZeroOrigin();
    for (auto *child = children; child; child = child->next)
    {
      static constexpr float kMoveDelay = 0.1f; //s

      if (child->componentFlags.animateMovement &&                        // are we animating to begin with?
        !wasThisResized &&                                                // skip animations if triggered by parent resize
        child->lastBounds.getPosition() != child->nextPosition &&         // are we already at the destination?
        child->previousPosition.x >= 0 && child->previousPosition.y >= 0) // did we have a previous position?
      {
        // we're interpolating between the previous position (animation start) and the supposed position

        float distanceToNextPositionRatio = (float)child->distanceToNextPositionRatio / (float)utils::max_limit<u16>;
        //distanceToNextPositionRatio = utils::smoothStep(distanceToNextPositionRatio);
        distanceToNextPositionRatio = utils::min(distanceToNextPositionRatio + uiRelated.deltaTime * 1.0f / kMoveDelay, 1.0f);

        child->bounds.x = (i32)::roundf(utils::lerp((float)child->previousPosition.x, (float)child->nextPosition.x, distanceToNextPositionRatio));
        child->bounds.y = (i32)::roundf(utils::lerp((float)child->previousPosition.y, (float)child->nextPosition.y, distanceToNextPositionRatio));

        child->distanceToNextPositionRatio = (u16)::ceilf(distanceToNextPositionRatio * (float)utils::max_limit<u16>);
      }
      else
      {
        // we're not animating position and just setting the supposed position directly in

        child->previousPosition = child->nextPosition;
        child->bounds.setPosition(child->nextPosition);
        child->distanceToNextPositionRatio = 0;
      }

      calculatePositions(child->children, child);
    }
  }

#undef IS_SECONDARY_POSITION_FLAG_SET
#undef IS_PRIMARY_POSITION_FLAG_SET
#undef TRANSPOSE
#undef GET_SECONDARY_FLAG
#undef GET_PRIMARY_FLAG

  utils::pair<i32, i32>
  getScrollOffsets(const MouseEvent &e, float singleStepX, float singleStepY)
  {
    auto rescaleMouseWheelDistance = [](float distance, float singleStepSize)
    {
      if (distance == 0.0f)
        return 0;

      distance *= 14.0f * singleStepSize;

      return (i32)::roundf(distance < 0 ? utils::min(distance, -1.0f)
        : utils::max(distance, 1.0f));
    };

    if (e.mods.test(ModifierKeys::ctrlAltCommandModifiers))
      return {};

    auto deltaX = rescaleMouseWheelDistance(e.wheelDeltaX, singleStepX);
    auto deltaY = rescaleMouseWheelDistance(e.wheelDeltaY, singleStepY);

    if (e.mods.test(ModifierKeys::shiftModifier))
    {
      auto temp = deltaX;
      deltaX = -deltaY;
      deltaY = temp;
    }

    return { deltaX, deltaY };
  }

  void deleteComponent(Component *component, bool freeArena)
  {
    if (component->componentFlags.isOpenGlInitialised)
    {
      component->componentFlags.isDestroyingOpenGl = true;
      component->render(getOpenGlContext(uiRelated.renderer));
    }

    auto *gui = getGui(uiRelated.renderer);
    if (gui->popupSelector.summoner == component)
    {
      if (gui->popupSelector.cancel)
        gui->popupSelector.cancel(&gui->popupSelector);

      gui->popupSelector.resetState();
      component->removeChildComponent(&gui->popupSelector);
    }
    if (gui->popupDisplay1.source == component)
    {
      gui->popupDisplay1.componentFlags.isVisible = false;
    }
    if (gui->popupDisplay2.source == component)
    {
      gui->popupDisplay2.componentFlags.isVisible = false;
    }

    component->deleteAllChildComponents(false);
    if (freeArena && component->arena)
      utils::bumpArena::destroy(component->arena);
    utils::bumpArena::remove(component);
  }

  bool
  Component::mouseWheelMove(const MouseEvent &event)
  {
    if ((sizingFlags & Component::ScrollableX) && event.wheelDeltaX != 0.0f)
    {
      // TODO:
      return true;
    }

    if ((sizingFlags & Component::ScrollableY) && event.wheelDeltaY != 0.0f)
    {
      // TODO:
      return true;
    }

    return false;
  }

  Point<i32>
  Component::getPositionInWindow() const
  {
    Point relativePosition = getPosition();
    auto *component = parent;
    while (component)
    {
      relativePosition = relativePosition + component->getPosition();
      component = component->parent;
    }
    return relativePosition;
  }

  Point<i32>
  Component::getRelativePoint(const Component *source, Point<i32> pointRelativeToSource) const
  {
    if (source->parent == parent)
      return pointRelativeToSource;

    Point otherPosition = pointRelativeToSource;
    if (source)
      otherPosition += source->getPositionInWindow();
    Point position = getPositionInWindow();
    return { otherPosition.x - position.x, otherPosition.y - position.y };
  }

  Component *
  Component::getComponentAt(i32 x, i32 y, bool onlyClickable)
  {
    if ((!onlyClickable || componentFlags.clickableChildren) && children)
    {
      // the last child doesn't have a next, which is why this works
      for (auto *child = children->previous; ; child = child->previous)
      {
        if (child->componentFlags.isVisible && child->contains(Point{ x, y }))
        {
          auto *result = child->getComponentAt(x - child->bounds.x,
            y - child->bounds.y, onlyClickable);
          if (result)
            return result;
        }

        if (!child->previous->next)
          break;
      }
    }

    return (onlyClickable && !componentFlags.clickable) ? nullptr : this;
  }

  bool
  Component::hasFocus(bool trueIfChildIsFocused) const
  {
    auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused;
    if (!trueIfChildIsFocused && this != focusedComponent)
      return false;

    return isParentOf(focusedComponent);
  }

  static bool
  isShowingInternal(Component *component)
  {
    while (component)
    {
      if (!component->componentFlags.isVisible)
        return false;

      component = component->parent;
    }

    return true;
  }

  static bool
  grabFocusInternal(Component *component, Component *origin = nullptr)
  {
    if (!isShowingInternal(component))
      return true;

    auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused;
    if (component->componentFlags.wantsFocus)
    {
      if (component == focusedComponent)
        return true;

      setFocusedComponent(uiRelated.renderer, component);
      if (focusedComponent)
        focusedComponent->handleFocus(false, Component::FocusGrabbed);

      component->handleFocus(true, Component::FocusGrabbed);
      return true;
    }

    if (component->isParentOf(focusedComponent) && isShowingInternal(focusedComponent))
      return true;

    if (component->children)
    {
      // shallow search
      for (auto *child = component->children->previous; ; child = child->previous)
      {
        if (component->componentFlags.wantsFocus && child != origin)
        {
          setFocusedComponent(uiRelated.renderer, child);
          if (focusedComponent)
            focusedComponent->handleFocus(false, Component::FocusGrabbed);

          child->handleFocus(true, Component::FocusGrabbed);
          return true;
        }

        if (!child->previous->next)
          break;
      }

      // deep search
      for (auto *child = component->children->previous; ; child = child->previous)
      {
        if (child != origin)
          if (grabFocusInternal(child))
            return true;

        if (!child->previous->next)
          break;
      }
    }

    // upwards search
    if (component->parent != nullptr)
      return grabFocusInternal(component->parent, component);

    return false;
  }

  void Component::grabFocus() { grabFocusInternal(this); }

  void Component::giveAwayFocus()
  {
    auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused;
    if (this == focusedComponent || isParentOf(focusedComponent))
    {
      setFocusedComponent(uiRelated.renderer, nullptr);
      focusedComponent->handleFocus(false, FocusGivenAway);
    }
  }

  bool
  Component::isObscured(const Component *ignoreClipIncluding) const
  {
    auto scissorBounds = bounds;

    bool isNotClipping = ignoreClipIncluding != nullptr;
    for (auto parentComponent = parent; parentComponent; parentComponent = parentComponent->parent)
    {
      auto parentBounds = parentComponent->bounds;

      if (!isNotClipping)
        parentBounds.withZeroOrigin().intersectRectangle(scissorBounds);

      scissorBounds = scissorBounds + parentBounds.getPosition();
      isNotClipping &= parentComponent != ignoreClipIncluding;

      if (scissorBounds.isEmpty())
        return true;
    }

    if (scissorBounds.isEmpty())
      return true;

    return false;
  }

  void Component::addChildComponent(Component *childToAdd, Component *insertBefore)
  {
    COMPLEX_ASSERT(childToAdd->parent != this);

    childToAdd->parent = this;
    childToAdd->next = nullptr;
    childToAdd->previous = childToAdd;

    if (!children)
    {
      children = childToAdd;
      return;
    }

    //auto *test = children;
    //for (; test; test = test->next)
    //  if (test->layerIndex > childToAdd->layerIndex)
    //    break;

    if (!insertBefore)
    {
      childToAdd->previous = children->previous;
      children->previous->next = childToAdd;
      children->previous = childToAdd;
    }
    else
    {
      if (insertBefore->previous->next)
        insertBefore->previous->next = childToAdd;
      childToAdd->previous = insertBefore->previous;
      childToAdd->next = insertBefore;
      insertBefore->previous = childToAdd;
    }
  }

  void Component::addChildComponent(Component *childToAdd, usize index)
  {
    Component *insertBefore = children;
    for (; index && insertBefore; (--index), (insertBefore = insertBefore->next)) { }
    addChildComponent(childToAdd, insertBefore);
  }

  void Component::removeChildComponent(Component *childToRemove, bool keepFocus)
  {
    if (!childToRemove || childToRemove->parent != this)
      return;

    if (!keepFocus)
      childToRemove->giveAwayFocus();

    childToRemove->next->previous = childToRemove->previous;
    if (childToRemove->previous->next)
      childToRemove->previous->next = childToRemove->next;
    else
      children = childToRemove->next;

    childToRemove->parent = nullptr;
    childToRemove->previous = nullptr;
    childToRemove->next = nullptr;
  }

  Component *
  Component::removeChildComponent(usize childIndexToRemove, bool keepFocus)
  {
    auto *child = children;
    for (; child && childIndexToRemove; --childIndexToRemove)
      child = child->next;

    if (!child)
      return nullptr;

    if (!keepFocus)
      child->giveAwayFocus();

    child->next->previous = child->previous;
    if (child->previous->next)
      child->previous->next = child->next;
    else
      children = child->next;

    child->parent = nullptr;
    child->previous = nullptr;
    child->next = nullptr;

    return child;
  }

  void Component::removeAllChildComponents()
  {
    if (!children)
      return;

    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (Component *child = children, *nextChild = nullptr; child; child = nextChild)
    {
      nextChild = child->next;
      child->parent = nullptr;
      child->previous = nullptr;
      child->next = nullptr;
    }

    children = nullptr;
  }

  void Component::deleteChildComponent(Component *childToDelete, bool freeArena)
  {
    removeChildComponent(childToDelete);
    deleteComponent(childToDelete, freeArena);
  }

  void Component::deleteChildComponent(usize childIndexToDelete, bool freeArena)
  {
    auto childToDelete = removeChildComponent(childIndexToDelete);
    deleteComponent(childToDelete, freeArena);
  }

  void Component::deleteAllChildComponents(bool freeChildArenas)
  {
    if (!children)
      return;

    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (auto *child = children; child; child = child->next)
      deleteComponent(child, freeChildArenas);
  }

  void Component::renderScrollbars(OpenGlWrapper &openGl, float scrollExpandPercent)
  {
    Rectangle<float> xScrollBounds{};
    Rectangle<float> yScrollBounds{};
    float xScrollBrightness = 0.0f;
    float yScrollBrightness = 0.0f;

    auto scaledPadding = scaleValueRoundInt(padding.toInt());
    Point<int> mousePosition{};
    if (componentFlags.isHovered)
    {
      auto interactions = getMouseInteractions(uiRelated.renderer);
      mousePosition = Point{ interactions.mouseState.x, interactions.mouseState.y } - getPositionInWindow();
    }

    auto calculateScrollWidth = [this, &mousePosition, scrollExpandPercent](Rectangle<i32> bounds,
      u16 &currentScrollWidth, float &brightness)
    {
      float percentExpand = (float)currentScrollWidth / (float)utils::max_limit<u16>;
      if (bounds.contains(mousePosition))
      {
        percentExpand += scrollExpandPercent;
        if (componentFlags.isClicked)
          brightness = 0.3f;
      }
      else
        percentExpand -= scrollExpandPercent;
      percentExpand = utils::clamp(percentExpand, getValue(Skin::kScrollShrinkPercent, false, this), 1.0f);
      currentScrollWidth = (u16)(percentExpand * utils::max_limit<u16>);
      return percentExpand;
    };

    if (test_enum(sizingFlags, Component::ScrollableWithBarX) && scrollableArea.w > bounds.w)
    {
      Rectangle<i32> scrollBounds{ scaledPadding.x, bounds.getBottom() - scaledPadding.h,
        bounds.w - scaledPadding.x - scaledPadding.w, scaledPadding.h };

      float length = utils::max((float)bounds.w / (float)scrollableArea.w, 0.1f) * (float)scrollBounds.w;
      float start = ((float)scrollOffset.x / (float)scrollableArea.w) * (float)scrollBounds.w;

      //if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      //{
      //  auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
      //  scrollBounds = scrollBounds.withShift(0, scrollWidth).withHeight(scrollWidth);
      //}

      float width = (float)scrollBounds.x * calculateScrollWidth(scrollBounds, scrollWidthsRatio.x, xScrollBrightness);
      xScrollBounds = { (float)scrollBounds.x + start, scrollBounds.getBottom() - width, length, width };
    }

    if (test_enum(sizingFlags, Component::ScrollableWithBarY) && scrollableArea.h > bounds.h)
    {
      Rectangle<i32> scrollBounds{ bounds.getRight() - scaledPadding.w, scaledPadding.y,
        scaledPadding.w, bounds.h - scaledPadding.y - scaledPadding.h };

      float length = utils::max((float)bounds.h / (float)scrollableArea.h, 0.1f) * (float)scrollBounds.h;
      float start = ((float)scrollOffset.y / (float)scrollableArea.h) * (float)scrollBounds.h;

      //if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      //{
      //  auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
      //  scrollBounds = scrollBounds.withShift(scrollWidth, 0).withWidth(scrollWidth);
      //}

      float width = (float)scrollBounds.y * calculateScrollWidth(scrollBounds, scrollWidthsRatio.y, yScrollBrightness);
      yScrollBounds = { (float)scrollBounds.getRight() - width, (float)scrollBounds.y + start, width, length };
    }

    if (!xScrollBounds.isEmpty())
    {
      auto c = getColour(Skin::kLightenScreen);
      if (xScrollBrightness > 0.0f)
        c = c.brighter(xScrollBrightness);

      nvgFillColor(openGl.g, c);
      nvgRoundedRect(openGl.g, xScrollBounds.x, xScrollBounds.y,
        xScrollBounds.w, xScrollBounds.h, xScrollBounds.w * 0.5f);
    }

    if (!yScrollBounds.isEmpty())
    {
      auto c = getColour(Skin::kLightenScreen);
      if (xScrollBrightness > 0.0f)
        c = c.brighter(xScrollBrightness);

      nvgFillColor(openGl.g, c);
      nvgRoundedRect(openGl.g, yScrollBounds.x, yScrollBounds.y,
        yScrollBounds.w, yScrollBounds.h, yScrollBounds.h * 0.5f);
    }
  }

  void Component::doRender(OpenGlWrapper &openGl)
  {
    bool continueRender = render(openGl);
    COMPLEX_CHECK_OPENGL_ERROR();

    if (!continueRender)
      return;

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      // is the child completely outside our bounds
      if (auto childBounds = child->bounds; !bounds.withZeroOrigin().intersectRectangle(childBounds))
        continue;

      nvgSave(openGl);
      nvgTranslate(openGl, (float)child->bounds.x, (float)child->bounds.y);
      child->doRender(openGl);
      nvgRestore(openGl);
    }
  }
}
