
// Created: 2023-12-11 16:49:17

#include "Component.hpp"

#include "ui_constants.hpp"
#include "Graphics.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "Generation/Processor.hpp"
#include "../Components/Control.hpp"
#include "../Sections/MainInterface.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
  static void calculateFit(Component *component, Component *children, bool isCalculatingVertical)
  {
    if (!component->componentFlags.isVisible)
    {
      component->previousPosition = invalidPosition;
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
    utils::vector<Component *> sameAsSiblingsComponents{ localScratch };

    for (auto *child = children; child; child = child->next)
    {
      COMPLEX_ASSERT(child->desiredSize.*minMember <= child->desiredSize.*maxMember);

      if (!child->componentFlags.isVisible)
        continue;

      calculateFit(child, child->children, isCalculatingVertical);

      auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

      sameAsSiblingsSizes.min = utils::max(sameAsSiblingsSizes.min, child->bounds.*minMember);
      sameAsSiblingsSizes.max = utils::max(sameAsSiblingsSizes.max, child->bounds.*maxMember);

      // when parent has horizontal/vertical orientation but we're calculating vertical/horizontal sizes
      if (component->componentFlags.vertical ^ isCalculatingVertical)
      {
        sizes.min = utils::max(sizes.min, (i64)(child->bounds.*minMember) + extra);
        sizes.max = utils::max(sizes.max, (i64)(child->bounds.*maxMember) + extra);
      }
      else if (child->placement == Placement::custom)
      {
        nonPositionedSizes.min = utils::max(nonPositionedSizes.min, (i64)(child->bounds.*minMember) + extra);
        nonPositionedSizes.max = utils::max(nonPositionedSizes.max, (i64)(child->bounds.*maxMember) + extra);
      }
      else
      {
        sizes.min += (i64)(child->bounds.*minMember) + extra;
        sizes.max += (i64)(child->bounds.*maxMember) + extra;
      }

      // taking care of children that conform to siblings' size
      if (test_enum(child->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
        sameAsSiblingsComponents.emplaceBack(child);
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

    sizes.min = utils::max(nonPositionedSizes.min, sizes.min);
    sizes.max = utils::max(nonPositionedSizes.max, sizes.max);
    sizes.min = utils::min(sizes.min, (i64)utils::max_limit<i32>);
    sizes.max = utils::min(sizes.max, (i64)utils::max_limit<i32>);

    if (test_enum(component->sizingFlags, ((isCalculatingVertical) ? Component::FixedY : Component::FixedX)))
    {
      component->bounds.*minMember = component->desiredSize.*minMember;
      component->bounds.*maxMember = component->desiredSize.*maxMember;
    }
    else
    {
      auto snapToMinFlag = (isCalculatingVertical) ? Component::SnapToMinY : Component::SnapToMinX;
      if (component->sizingFlags & snapToMinFlag)
      {
        component->bounds.*minMember = utils::max((i32)sizes.min, component->desiredSize.*minMember);
        component->bounds.*maxMember = utils::min(component->bounds.*minMember, component->desiredSize.*maxMember);
      }
      else
      {
        component->bounds.*minMember = utils::max((i32)sizes.min, component->desiredSize.*minMember);
        component->bounds.*maxMember = utils::clamp((i32)sizes.max, component->bounds.*minMember, component->desiredSize.*maxMember);
      }

      auto growableFlag = (isCalculatingVertical) ? Component::GrowableY : Component::GrowableX;
      if (component->sizingFlags & growableFlag)
      {
        // if you have a scrollable parent and a growable child
        // along the same axis, the algorithm will fail spectacularly
        [[maybe_unused]] auto scrollableFlag = (isCalculatingVertical) ? 
          Component::ScrollableY : Component::ScrollableX;
        COMPLEX_ASSERT((component->parent->sizingFlags & scrollableFlag) == 0);

        component->bounds.*maxMember = utils::max_limit<i32>;
      }

      auto scrollableFlag = (isCalculatingVertical) ? Component::ScrollableY : Component::ScrollableX;
      if (component->sizingFlags & scrollableFlag)
        component->bounds.*minMember = component->desiredSize.*minMember;
    }

    COMPLEX_ASSERT(component->bounds.*minMember >= 0);
    COMPLEX_ASSERT(component->bounds.*maxMember >= 0);

    // the user can override the above calculations if they so choose
    if (component->overrideSize)
    {
      auto [min, max] = component->overrideSize(component, isCalculatingVertical);
      // returned bounds are scaled so we need to unscale them
      // in order to work with the rest of the values
      if (min >= 0)
        component->bounds.*minMember = (i32)::ceilf(unscaleValue((float)min));
      if (max >= 0)
        component->bounds.*maxMember = (i32)::ceilf(unscaleValue((float)max));
    }

    COMPLEX_ASSERT(component->bounds.*minMember >= 0);
    COMPLEX_ASSERT(component->bounds.*maxMember >= 0);

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

    COMPLEX_ASSERT(component->bounds.*minMember >= 0);
    COMPLEX_ASSERT(component->bounds.*maxMember >= 0);
  }

  extern utils::vector<Component *> *sortedSizesMin;
  extern utils::vector<Component *> *sortedSizesMax;

  static void calculateGrow(Component *component, Component *children, bool isCalculatingVertical)
  {
    auto Rectangle<i32>:: *minMember = (isCalculatingVertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
    auto Rectangle<i32>:: *maxMember = (isCalculatingVertical) ? &Rectangle<i32>::h : &Rectangle<i32>::w;
    auto Rectangle<i32>:: *actualSize = maxMember;

    // when parent has horizontal/vertical orientation but we're calculating vertical/horizontal sizes
    if (component->componentFlags.vertical ^ isCalculatingVertical)
    {
      auto padding = (isCalculatingVertical) ? 
        component->padding.getBottom() : component->padding.getRight();

      for (auto *child = children; child; child = child->next)
      {
        if (!child->componentFlags.isVisible)
          continue;

        auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

        auto maxSize = utils::min(child->bounds.*maxMember, component->bounds.*maxMember - extra - padding);
        child->bounds.*maxMember = maxSize;
        calculateGrow(child, child->children, isCalculatingVertical);
      }

      // scaling the final size
      component->bounds.*actualSize = scaleValueRoundInt((float)(component->bounds.*actualSize));
      COMPLEX_ASSERT(component->bounds.*actualSize >= 0);

      return;
    }

    auto &sortedMin = *sortedSizesMin;
    auto &sortedMax = *sortedSizesMax;
    sortedMin.clear();
    sortedMax.clear();

    // account for this component's padding from the size we were assigned
    Range<i64> sizes{};
    i64 childrenMinSizes{};
    {
      sizes.min = (isCalculatingVertical) ?
        component->padding.getBottom() : component->padding.getRight();
      sizes.max = sizes.min;
    }

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      if (child->placement == Placement::custom)
      {
        child->bounds.*actualSize = utils::clamp(component->bounds.*actualSize,
          child->bounds.*minMember, child->bounds.*maxMember);
        
        continue;
      }

      auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

      // not adding minMember because we're going to compare against it in the next pass
      sizes.min += (i64)extra;
      childrenMinSizes += (i64)(child->bounds.*minMember);
      sizes.max += (i64)(child->bounds.*maxMember) + extra;

      auto iter = utils::findIf(sortedMin, [&](const Interface::Component *c)
        { return c->bounds.*minMember > child->bounds.*minMember; });
      sortedMin.emplace(iter, child);

      iter = utils::findIf(sortedMax, [&](const Interface::Component *c)
        { return c->bounds.*maxMember > child->bounds.*maxMember; });
      sortedMax.emplace(iter, child);
    }

    COMPLEX_ASSERT(sizes.min >= 0);
    COMPLEX_ASSERT(sizes.max >= 0);

    sizes.min = utils::min(sizes.min, (i64)utils::max_limit<i32>);
    sizes.max = utils::min(sizes.max, (i64)utils::max_limit<i32>);

    auto scrollableFlag = (isCalculatingVertical) ? Component::ScrollableY : Component::ScrollableX;

    if (test_enum(component->sizingFlags, scrollableFlag))
    {
      i32 *scrollableDirection = (isCalculatingVertical) ?
        &component->scrollableArea.h : &component->scrollableArea.w;

      if ((i64)(component->bounds.*maxMember) < sizes.min + childrenMinSizes || 
        (i64)(*scrollableDirection) != sizes.min + childrenMinSizes)
      {
        // parent is scrollable so every child is set to their preferred max sizes
        // which is already done, therefore we only need to update scrollable size
        // and update the position in case we've been shrunk

        *scrollableDirection = scaleValueRoundInt((float)sizes.max);
        //offsetScroll(component, 0.0f, 0.0f, false);
      }
    }
    else if (!sortedMin.empty() && !sortedMax.empty())
    {
      i32 remaining = component->bounds.*actualSize - (i32)sizes.min;

      COMPLEX_ASSERT(remaining >= 0);

      i32 smallestMaxSize;
      i32 biggestMinSize;
      usize count;
      usize j = 0;
      {
        usize lastJ = 0;
        usize i = sortedMin.size(), lastI = sortedMin.size();

        while (true)
        {
          auto *biggestMin = sortedMin[i - 1];
          auto *smallestMax = sortedMax[j];

          // if the most constrained component's maximum gets surpassed by the current size,
          // exclude it and update the remainder
          if (smallestMax->bounds.*maxMember < biggestMin->bounds.*minMember)
          {
            // j can only get incremented and because we always assume that minMember <= maxMember
            // we don't have to erase the skipped component in sortedMin here after incrementing j
            if (j < sortedMax.size())
              remaining -= smallestMax->bounds.*maxMember;
            ++j;
          }
          // if the remaining size cannot expand all smaller components to the current minimum, exclude the biggest minimum
          else if (i > 1 && (biggestMin->bounds.*minMember * (i32)(i - j)) > remaining)
          {
            biggestMin->bounds.*maxMember = biggestMin->bounds.*minMember;
            --i;
          }

          if ((i == lastI && j == lastJ) || j >= sortedMax.size())
            break;

          lastI = i;
          lastJ = j;
        }

        biggestMinSize = sortedMin[i - 1]->bounds.*minMember;
        smallestMaxSize = sortedMax[utils::min(j, sortedMax.size() - 1)]->bounds.*maxMember;
        count = i - j;
      }


      for (usize k = 0; k < count; ++k)
      {
        auto *c = sortedMax[j + k];
        if (c->bounds.*minMember <= biggestMinSize &&
          c->bounds.*maxMember >= smallestMaxSize)
        {
          i32 size = remaining / (i32)(count - k);

          c->bounds.*maxMember = utils::clamp(size, c->bounds.*minMember, c->bounds.*maxMember);
          COMPLEX_ASSERT(c->bounds.*maxMember >= 0);

          remaining -= c->bounds.*maxMember;
        }
      }
    }

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      COMPLEX_ASSERT(child->bounds.*actualSize != utils::max_limit<i32>);
      calculateGrow(child, child->children, isCalculatingVertical);
    }

    // scaling the final size
    component->bounds.*actualSize = scaleValueRoundInt((float)(component->bounds.*actualSize));
    COMPLEX_ASSERT(component->bounds.*actualSize >= 0);
  }

  void calculateSizes(Component *children, Component *component)
  {
    calculateFit(component, children, false);
    calculateGrow(component, children, false);
    calculateFit(component, children, true);
    calculateGrow(component, children, true);
  }

  extern utils::vector<Component *> *customPlacement;

#define GET_PRIMARY_FLAG(component, flag) ((flag) << (i32)(!!(component->componentFlags.vertical)))
#define GET_SECONDARY_FLAG(component, flag) ((flag) << (1 - (!!(component->componentFlags.vertical))))
#define TRANSPOSE(component, rect) ((component->componentFlags.vertical) ? (rect).transposed() : (rect))
#define IS_PRIMARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_PRIMARY_FLAG(parent, flag))
#define IS_SECONDARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_SECONDARY_FLAG(parent, flag))

  void calculatePositions(Component *children,
    Component *component, Rectangle<i32> boundsInTarget)
  {
    static constexpr float kAutoscrollMultiplier = 2.0f;

    if (!component->componentFlags.isVisible)
      return;

    auto padding = scaleValueRoundInt(component->padding.toInt());
    if (boundsInTarget.isEmpty())
    {
      boundsInTarget = Rectangle{ padding.x, padding.y,
        component->bounds.w - padding.getRight(),
        component->bounds.h - padding.getBottom() };
    }

    offsetScroll(component,
      component->autoScrollIncrements.x * uiRelated.deltaTime * kAutoscrollMultiplier,
      component->autoScrollIncrements.y * uiRelated.deltaTime * kAutoscrollMultiplier, false);

    if (test_enum(component->sizingFlags, Component::ScrollableX))
      boundsInTarget.x -= (i32)::roundf(component->scrollOffset.x);
    if (test_enum(component->sizingFlags, Component::ScrollableY))
      boundsInTarget.y -= (i32)::roundf(component->scrollOffset.y);

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

      if (change && child->previousPosition != invalidPosition)
      {
        child->previousPosition = child->lastBounds.getPosition();
        child->distanceToNextPositionRatio = 0;
      }
      child->nextPosition.*coordinate = bounds.getPosition().*coordinate;
    };

    boundsInTarget = TRANSPOSE(component, boundsInTarget);
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
        child->componentFlags.isPositionSet = false;
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

    auto scrollableFlag = (component->componentFlags.vertical) ? Component::ScrollableY : Component::ScrollableX;
    if (component->sizingFlags & scrollableFlag)
      sizeLeftPerGroup = utils::max(sizeLeftPerGroup, 0);

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
            if (!nextChild->componentFlags.isVisible || (nextChild->placement & Placement::custom))
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
            nextChild = nextChild->next) 
          {
            if (nextChild->componentFlags.isVisible && 
              !(nextChild->placement & Placement::custom))
              ++j;
          }

          i32 size = sizeLeftPerGroup;
          i32 offset = 0;

          for (auto *groupChild = child; j; groupChild = groupChild->next)
          {
            if (!groupChild->componentFlags.isVisible || (groupChild->placement & Placement::custom))
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

        setNextPosition(false, TRANSPOSE(component, bounds), child, primary);
        boundsInTarget.trimLeft(bounds.w + margin.getRight());
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
          if (nextChild->componentFlags.isVisible && 
            !(nextChild->placement & Placement::custom))
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
      animatePosition(child, wasThisResized);
      calculatePositions(child->children, child);
    }
  }

  void animatePosition(Component *component, bool wasParentResized)
  {
    static constexpr float kMoveDelay = 1.0f; //s

    if (component->componentFlags.animateMovement &&                    // are we animating to begin with?
      !wasParentResized &&                                              // skip animations if triggered by parent resize
      component->lastBounds.getPosition() != component->nextPosition && // are we already at the destination?
      component->previousPosition != invalidPosition)                   // did we have a previous position?
    {
      // we're interpolating between the previous position (animation start) and the supposed position

      float distanceToNextPositionRatio = (float)component->distanceToNextPositionRatio / (float)utils::max_limit<u16>;
      distanceToNextPositionRatio = utils::min(distanceToNextPositionRatio + uiRelated.deltaTime * 1.0f / kMoveDelay, 1.0f);
      auto temp = 1.0f - (1.0f - distanceToNextPositionRatio) * (1.0f - distanceToNextPositionRatio);
      distanceToNextPositionRatio = temp;
      //distanceToNextPositionRatio = utils::smoothStep(distanceToNextPositionRatio);

      component->bounds.x = (i32)::roundf(utils::lerp((float)component->previousPosition.x, (float)component->nextPosition.x, distanceToNextPositionRatio));
      component->bounds.y = (i32)::roundf(utils::lerp((float)component->previousPosition.y, (float)component->nextPosition.y, distanceToNextPositionRatio));

      component->distanceToNextPositionRatio = (u16)::ceilf(distanceToNextPositionRatio * (float)utils::max_limit<u16>);
    }
    else
    {
      // we're not animating position and just setting the supposed position directly in

      component->previousPosition = component->nextPosition;
      component->bounds.setPosition(component->nextPosition);
      component->distanceToNextPositionRatio = 0;
    }
  }

#undef IS_SECONDARY_POSITION_FLAG_SET
#undef IS_PRIMARY_POSITION_FLAG_SET
#undef TRANSPOSE
#undef GET_SECONDARY_FLAG
#undef GET_PRIMARY_FLAG

  void deleteComponent(Component *component, bool freeArena)
  {
    if (component->componentFlags.isOpenGlInitialised)
    {
      auto &context = getOpenGlContext(uiRelated.renderer);
      bool save = context.isDestroyingOpenGl;
      context.isDestroyingOpenGl = true;

      component->render(context);

      context.isDestroyingOpenGl = save;
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

  thread_local Point<float> initialScrollOffset;

  void checkScrollClick(Component *component, const MouseEvent &e)
  {
    auto scaledPadding = scaleValueRoundInt(component->padding.toInt());
    Rectangle scrollBoundsX{ scaledPadding.x, component->bounds.h - scaledPadding.h,
      component->bounds.w - scaledPadding.getRight(), scaledPadding.h };
    Rectangle scrollBoundsY{ component->bounds.w - scaledPadding.w, scaledPadding.y,
      scaledPadding.w, component->bounds.h - scaledPadding.getBottom() };

    component->componentFlags.isScrollbarXClicked = 
      test_enum(component->sizingFlags, Component::ScrollableWithBarX) &&
      component->scrollableArea.w > component->bounds.w && scrollBoundsX.contains(e.x, e.y);

    component->componentFlags.isScrollbarYClicked = 
      test_enum(component->sizingFlags, Component::ScrollableWithBarY) &&
      component->scrollableArea.h > component->bounds.h && scrollBoundsY.contains(e.x, e.y);

    initialScrollOffset = component->scrollOffset;
  }

  static void compensateScrollChange(Component *component, Point<float> oldScrollOffset)
  {
    if (oldScrollOffset != component->scrollOffset)
    {
      auto offset = Point{ (i32)::roundf(oldScrollOffset.x) - (i32)::roundf(component->scrollOffset.x),
        (i32)::roundf(oldScrollOffset.y) - (i32)::roundf(component->scrollOffset.y) };

      for (auto child = component->children; child; child = child->next)
      {
        if (child->placement & Placement::custom)
          continue;

        child->previousPosition += offset;
        child->nextPosition += offset;
      }
    }
  }

  void dragScroll(Component *component, const MouseEvent &e)
  {
    float deltaX = 0.0f, deltaY = 0.0f;
    auto offset = -e.getOffsetFromDragStart();
    auto scaledPadding = scaleValueRoundInt(component->padding.toInt());

    if (component->componentFlags.isScrollbarXClicked)
      deltaX = (float)component->scrollableArea.w * (float)offset.x / 
        (float)(component->bounds.w - scaledPadding.getRight());

    if (component->componentFlags.isScrollbarYClicked)
      deltaY = (float)component->scrollableArea.h * (float)offset.y / 
        (float)(component->bounds.h - scaledPadding.getBottom());

    auto old = component->scrollOffset;
    if (component->sizingFlags & Component::ScrollableX)
    {
      component->scrollOffset.x = utils::clamp(initialScrollOffset.x - deltaX,
        0.0f, utils::max(0.0f, (float)(component->scrollableArea.w - component->bounds.w)));
    }

    if (component->sizingFlags & Component::ScrollableY)
    {
      component->scrollOffset.y = utils::clamp(initialScrollOffset.y - deltaY,
        0.0f, utils::max(0.0f, (float)(component->scrollableArea.h - component->bounds.h)));
    }

    compensateScrollChange(component, old);
  }

  bool
  offsetScroll(Component *component, float deltaX, float deltaY, bool switchDirections)
  {
    if (switchDirections)
      COMPLEX_SWAP(deltaX, deltaY);

    auto old = component->scrollOffset;
    if (component->sizingFlags & Component::ScrollableX)
    {
      component->scrollOffset.x = utils::clamp(component->scrollOffset.x - deltaX,
        0.0f, utils::max(0.0f, (float)(component->scrollableArea.w - component->bounds.w)));
    }

    if (component->sizingFlags & Component::ScrollableY)
    {
      component->scrollOffset.y = utils::clamp(component->scrollOffset.y - deltaY,
        0.0f, utils::max(0.0f, (float)(component->scrollableArea.h - component->bounds.h)));
    }

    compensateScrollChange(component, old);

    return old != component->scrollOffset;
  }
  
  bool
  Component::mouseDown(const MouseEvent &event)
  {
    checkScrollClick(this, event);
    return componentFlags.isScrollbarYClicked || componentFlags.isScrollbarXClicked;
  }

  bool 
  Component::mouseDrag(const MouseEvent &event)
  {
    if (componentFlags.isScrollbarYClicked || componentFlags.isScrollbarXClicked)
      dragScroll(this, event);

    return componentFlags.isScrollbarYClicked || componentFlags.isScrollbarXClicked;
  }

  bool
  Component::mouseWheelMove(const MouseEvent &event)
  {
    if (event.mods.test(ModifierKeys::ctrlModifier))
      return false;

    bool isHorizontal = event.mods.test(ModifierKeys::shiftModifier);
    auto multiplier = 20.0f * uiRelated.scale;
    offsetScroll(this, event.wheelDeltaX * multiplier, 
      event.wheelDeltaY * multiplier, isHorizontal);

    return (isHorizontal) ? (sizingFlags & Component::ScrollableX) :
      (sizingFlags & Component::ScrollableY);
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
    if (source && source->parent == parent)
      return pointRelativeToSource;

    Point otherPosition = pointRelativeToSource;
    if (source)
      otherPosition += source->getPositionInWindow();
    Point position = getPositionInWindow();
    return { otherPosition.x - position.x, otherPosition.y - position.y };
  }

  Component *
  Component::getComponentAt(i32 x, i32 y, bool onlyClickable, 
    bool recursive, Component *startingAt)
  {
    COMPLEX_ASSERT(!startingAt || startingAt->parent == this);

    if ((!onlyClickable || componentFlags.clickableChildren) && children)
    {
      // reverse iteration over the children, starting at the end or sine user specified component
      // this is to support getting components that might be overlapping
      for (auto *child = ((startingAt) ? startingAt : children)->previous; ; child = child->previous)
      {
        if (child->componentFlags.isVisible && 
          child->contains(Point{ x - child->bounds.x, y - child->bounds.y }))
        {
          if (!recursive)
            return child;

          auto *result = child->getComponentAt(x - child->bounds.x,
            y - child->bounds.y, onlyClickable);
          if (result)
            return result;
        }

        // the last child doesn't have a next, which is why this works
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
    if (component == focusedComponent)
      return true;

    if (!focusedComponent || focusedComponent->handleFocus(false, Component::FocusGrabbed, component))
    {
      setFocusedComponent(uiRelated.renderer, component);
      component->handleFocus(true, Component::FocusGrabbed, focusedComponent);
      return true;
    }

    if (component->isParentOf(focusedComponent) && isShowingInternal(focusedComponent))
      return true;

    if (component->children)
    {
      // shallow search
      for (auto *child = component->children->previous; ; child = child->previous)
      {
        if (child != origin && (!focusedComponent || focusedComponent->handleFocus(false, Component::FocusGrabbed, child)))
        {
          setFocusedComponent(uiRelated.renderer, child);
          child->handleFocus(true, Component::FocusGrabbed, focusedComponent);
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

  bool 
  Component::giveAwayFocusTo(Component *component)
  {
    auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused;
    if (focusedComponent != this)
      return false;

    if (focusedComponent->handleFocus(false, Component::FocusGivenAway, component) &&
      (!component || component->handleFocus(true, Component::FocusGivenAway, focusedComponent)))
    {
      setFocusedComponent(uiRelated.renderer, component);
      return true;
    }

    return false;
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
        scissorBounds = parentBounds.withZeroOrigin().getIntersection(scissorBounds);

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
    COMPLEX_ASSERT(childToAdd != insertBefore);
    COMPLEX_ASSERT(childToAdd->parent != this);
    COMPLEX_ASSERT(!insertBefore || insertBefore->parent == this);

    childToAdd->parent = this;
    utils::insertDllHalfConnected(childToAdd, insertBefore, children);
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
      childToRemove->giveAwayFocusTo();

    childToRemove->parent = nullptr;
    utils::removeDllHalfConnected(childToRemove, children);
  }

  Component *
  Component::removeChildComponent(usize childIndexToRemove, bool keepFocus)
  {
    auto *child = children;
    for (; child && childIndexToRemove; --childIndexToRemove)
      child = child->next;

    removeChildComponent(child, keepFocus);

    return child;
  }

  void Component::removeAllChildComponents()
  {
    if (!children)
      return;

    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocusTo();

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
        focusedComponent->giveAwayFocusTo();

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
      if (bounds.contains(mousePosition) || componentFlags.isClicked)
      {
        percentExpand += scrollExpandPercent;
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
      Rectangle scrollBounds{ scaledPadding.x, bounds.h - scaledPadding.h,
        bounds.w - scaledPadding.getRight(), scaledPadding.h };

      float length = utils::max((float)bounds.w / (float)scrollableArea.w, 0.1f) * (float)scrollBounds.w;
      float start = (scrollOffset.x / (float)scrollableArea.w) * (float)scrollBounds.w;

      //if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      //{
      //  auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
      //  scrollBounds = scrollBounds.withShift(0, scrollWidth).withHeight(scrollWidth);
      //}

      float width = 0.5f * (float)scrollBounds.h * calculateScrollWidth(scrollBounds, scrollWidthsRatio.x, xScrollBrightness);
      xScrollBounds = { (float)scrollBounds.x + start, 
        scrollBounds.getBottom() - width - 0.25f * scrollBounds.h, length, width };
    }

    if (test_enum(sizingFlags, Component::ScrollableWithBarY) && scrollableArea.h > bounds.h)
    {
      Rectangle scrollBounds{ bounds.w - scaledPadding.w, scaledPadding.y,
        scaledPadding.w, bounds.h - scaledPadding.getBottom() };

      float length = utils::max((float)bounds.h / (float)scrollableArea.h, 0.1f) * (float)scrollBounds.h;
      float start = (scrollOffset.y / (float)scrollableArea.h) * (float)scrollBounds.h;

      //if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      //{
      //  auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
      //  scrollBounds = scrollBounds.withShift(scrollWidth, 0).withWidth(scrollWidth);
      //}

      float width = 0.5f * (float)scrollBounds.w * calculateScrollWidth(scrollBounds, scrollWidthsRatio.y, yScrollBrightness);
      yScrollBounds = { (float)scrollBounds.getRight() - width - 0.25f * scrollBounds.w,
        (float)scrollBounds.y + start, width, length };
    }

    if (!xScrollBounds.isEmpty())
    {
      auto c = getColour(Skin::kLightenScreen, this);
      if (xScrollBrightness > 0.0f)
        c = c.lighter(xScrollBrightness);

      fillRect(openGl, xScrollBounds, c, xScrollBounds.h * 0.5f);
    }

    if (!yScrollBounds.isEmpty())
    {
      auto c = getColour(Skin::kLightenScreen, this);
      if (yScrollBrightness > 0.0f)
        c = c.lighter(yScrollBrightness);

      fillRect(openGl, yScrollBounds, c, yScrollBounds.w * 0.5f);
    }
  }
  
  void Component::addCommandMessageHandler(utils::sll<CommandMessages::HandleMessageFn *> &handler, 
    utils::sll<CommandMessages::HandleMessageFn *> *insertBefore)
  {
    COMPLEX_ASSERT(!handler.next);

    if (!commandMessageHandler)
    {
      commandMessageHandler = &handler;
      return;
    }

    if (insertBefore == commandMessageHandler)
    {
      handler.next = commandMessageHandler;
      commandMessageHandler = &handler;
      return;
    }

    auto *nextFreeHandler = commandMessageHandler;
    for (; nextFreeHandler->next && nextFreeHandler->next != insertBefore; 
      nextFreeHandler = nextFreeHandler->next) { }

    handler.next = nextFreeHandler->next;
    nextFreeHandler->next = &handler;
  }

  void Component::removeCommandMessageHandler(utils::sll<CommandMessages::HandleMessageFn *> &handler)
  {
    if (commandMessageHandler == &handler)
    {
      commandMessageHandler = commandMessageHandler->next;
      handler.next = {};
      return;
    }

    for (auto *node = commandMessageHandler; node; node = node->next)
    {
      if (node == &handler)
      {
        node = node->next;
        handler.next = {};
        break;
      }
    }
  }

  bool 
  Component::handleCommandMessage(u64 commandId, void *extraData, bool useFallbackHandler)
  {
    for (auto *handler = commandMessageHandler; handler; handler = handler->next)
      if (handler->object && handler->object(this, commandId, extraData))
        return true;
    
    if (useFallbackHandler && componentHandleCommandMessage(this, commandId, extraData))
      return true;

    return false;
  }

  bool 
  Component::componentHandleCommandMessage([[maybe_unused]] Component *c, 
    [[maybe_unused]] u64 commandId, [[maybe_unused]] void *extraData)
  {
    return false;
  }

  Skin::Override
  Component::getSkinOverride() const
  {
    auto *c = this;
    while (c->skinOverride == Skin::kUseParentOverride && c->parent)
      c = c->parent;

    return (c->skinOverride == Skin::kUseParentOverride) ? Skin::kNone : c->skinOverride;
  }

  void Component::doRenderChildren(OpenGlWrapper &openGl)
  {
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      // is the child completely outside our bounds
      if (getLocalBounds().getIntersection(child->bounds).isEmpty())
        continue;

      nvgSave(openGl);
      if (!child->componentFlags.noclip)
        nvgIntersectScissor(openGl, (float)child->bounds.x, (float)child->bounds.y,
          (float)child->bounds.w, (float)child->bounds.h);
      nvgTranslate(openGl, (float)child->bounds.x, (float)child->bounds.y);
      child->doRender(openGl);
      nvgRestore(openGl);
    }
  }

  void Component::doRender(OpenGlWrapper &openGl)
  {
    bool continueRender = render(openGl);
    COMPLEX_CHECK_OPENGL_ERROR();

    if (!continueRender)
      return;

    doRenderChildren(openGl);
  }


}
