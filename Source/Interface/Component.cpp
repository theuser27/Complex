
// Created: 2023-12-11 16:49:17

#include "Component.hpp"

#include "gui_constants.hpp"
#include "Graphics.hpp"
#include "Plugin/Complex.hpp"
#include "Generation/Processor.hpp"
#include "Control.hpp"
#include "MainInterface.hpp"
#include "Popups.hpp"

static bool
calculateFit(Component *component, Component *children, bool isCalculatingVertical)
{
  static constexpr float kFadeawayTime = 0.15f; //s

  if (!isCalculatingVertical)
  {
    // setup on first run
    component->componentFlags.isPositionSet = false;
    component->lastBounds = component->bounds;
    component->bounds = {};

    if (component->componentFlags.animateFadeAway)
    {
      auto increment = (i32)roundf(getUiRelated()->deltaTime / kFadeawayTime *
        utils::int_max<decltype(component->fadeawayRatio)>);
      increment = (component->componentFlags.isVisible) ? increment : -increment;

      component->fadeawayRatio = (decltype(component->fadeawayRatio))utils::clamp(
        (i32)component->fadeawayRatio + increment, 0, (i32)utils::int_max<decltype(component->fadeawayRatio)>);
    }
    else
      component->fadeawayRatio = utils::int_max<decltype(component->fadeawayRatio)>;
  }

  if (!component->isStillVisible())
  {
    component->previousPosition = invalidPosition;
    return false;
  }

  auto Rectangle<i32>:: *minMember = (isCalculatingVertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
  auto Rectangle<i32>:: *maxMember = (isCalculatingVertical) ? &Rectangle<i32>::h : &Rectangle<i32>::w;

  Range<i64> sizes{};
  Range<i64> nonPositionedSizes{};

  Range<i32> sameAsSiblingsSizes{};
  utils::vector<Component *> sameAsSiblingsComponents{ getLocalScratch() };

  for (auto *child = children; child; child = child->next)
  {
    COMPLEX_ASSERT(child->desiredSize.*minMember <= child->desiredSize.*maxMember);

    if (!calculateFit(child, child->children, isCalculatingVertical))
      continue;

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
    if (test_flag(child->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
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
  sizes.min = utils::min(sizes.min, (i64)utils::int_max<i32>);
  sizes.max = utils::min(sizes.max, (i64)utils::int_max<i32>);

  if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::FixedY : Component::FixedX))
  {
    component->bounds.*minMember = component->desiredSize.*minMember;
    component->bounds.*maxMember = component->desiredSize.*maxMember;
  }
  else
  {
    component->bounds.*minMember = utils::max((i32)sizes.min, component->desiredSize.*minMember);

    if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::SnapToMinY : Component::SnapToMinX))
      component->bounds.*maxMember = utils::min(component->bounds.*minMember, component->desiredSize.*maxMember);
    else
      component->bounds.*maxMember = utils::clamp((i32)sizes.max, component->bounds.*minMember, component->desiredSize.*maxMember);

    if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::GrowableY : Component::GrowableX))
    {
      // if you have a scrollable parent and a growable child
      // along the same axis, the algorithm will fail spectacularly
      //[[maybe_unused]] auto scrollableFlag = (isCalculatingVertical) ?
      //  Component::ScrollableY : Component::ScrollableX;
      //COMPLEX_ASSERT((component->parent->sizingFlags & scrollableFlag) == 0);

      component->bounds.*maxMember = utils::int_max<i32>;
    }

    if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::ScrollableY : Component::ScrollableX))
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
  //if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::SameAsSiblingsY : Component::SameAsSiblingsX))
  //  component->bounds.*maxMember = (component->componentFlags.isVertical ^ isCalculatingVertical) ? utils::int_max<i32> : 0;

  // add this component's padding to the total size
  auto padding = (isCalculatingVertical) ?
    component->padding.getBottom() : component->padding.getRight();

  component->bounds.*minMember = (i32)utils::min((i64)utils::int_max<i32>,
    (i64)(component->bounds.*minMember) + padding);
  component->bounds.*maxMember = (i32)utils::min((i64)utils::int_max<i32>,
    (i64)(component->bounds.*maxMember) + padding);

  COMPLEX_ASSERT(component->bounds.*minMember >= 0);
  COMPLEX_ASSERT(component->bounds.*maxMember >= 0);

  return true;
}

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
      if (!child->isStillVisible())
        continue;

      auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

      auto maxSize = utils::min(child->bounds.*maxMember, component->bounds.*maxMember - extra - padding);
      child->bounds.*maxMember = maxSize;
      calculateGrow(child, child->children, isCalculatingVertical);
    }

    // scaling the final size
    component->bounds.*actualSize = scaleValueRoundInt((float)(component->bounds.*actualSize));
    COMPLEX_ASSERT(component->bounds.*actualSize >= 0);
    component->bounds.*actualSize = (component->componentFlags.keepSize) ?
      component->lastBounds.*actualSize : component->bounds.*actualSize;

    return;
  }

  auto &sortedMin = getUiRelated()->renderer->sortedSizesMin;
  auto &sortedMax = getUiRelated()->renderer->sortedSizesMax;
  sortedMin.clear();
  sortedMax.clear();

  // account for this component's padding from the size we were assigned
  Range<i64> sizes{};
  i64 childrenMinSizes{};
  usize fadeawayComponents{};
  {
    sizes.min = (isCalculatingVertical) ?
      component->padding.getBottom() : component->padding.getRight();
    sizes.max = sizes.min;
  }

  for (auto *child = children; child; child = child->next)
  {
    if (!child->isStillVisible())
      continue;

    if (child->placement == Placement::custom)
    {
      child->bounds.*actualSize = utils::clamp(component->bounds.*actualSize,
        child->bounds.*minMember, child->bounds.*maxMember);

      continue;
    }

    if (child->fadeawayRatio != utils::int_max<decltype(Component::fadeawayRatio)>)
      ++fadeawayComponents;

    auto extra = (isCalculatingVertical) ? child->margin.getBottom() : child->margin.getRight();

    // not adding minMember because we're going to compare against it in the next pass
    sizes.min += (i64)extra;
    childrenMinSizes += (i64)(child->bounds.*minMember);
    sizes.max += (i64)(child->bounds.*maxMember) + extra;

    auto iter = utils::findIf(sortedMin, [&](const Component *c)
      { return c->bounds.*minMember > child->bounds.*minMember; });
    sortedMin.emplaceAt(iter, child);

    iter = utils::findIf(sortedMax, [&](const Component *c)
      { return c->bounds.*maxMember > child->bounds.*maxMember; });
    sortedMax.emplaceAt(iter, child);
  }

  COMPLEX_ASSERT(sizes.min >= 0);
  COMPLEX_ASSERT(sizes.max >= 0);

  sizes.min = utils::min(sizes.min, (i64)utils::int_max<i32>);
  sizes.max = utils::min(sizes.max, (i64)utils::int_max<i32>);

  bool skipResizingChildren = test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::ScrollableY : Component::ScrollableX) &&
    !test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::SnapToMinY : Component::SnapToMinX);

  if (!skipResizingChildren && (!sortedMin.empty() && !sortedMax.empty()))
  {
    i32 remaining = component->bounds.*actualSize - (i32)sizes.min;

    if ((remaining - (i32)childrenMinSizes) < 0)
    {
      // size only components that are pinned left/top and right/bottom
      // from the beginning and end respectively inwards first
      // then if there's enough space size justfied components

      remaining = component->bounds.*actualSize;
      Component *start = component->children;
      Component *end = (component->children) ? component->children->previous : nullptr;
      Component *previousStart = start, *previousEnd = nullptr;
      Component *componentToSize = start;

      while (remaining && previousStart != previousEnd)
      {
        auto extra = (isCalculatingVertical) ?
          componentToSize->margin.getBottom() : componentToSize->margin.getRight();

        // we take the minimum sizes as the actual sizes because we don't have enough space anyway
        componentToSize->bounds.*actualSize = utils::min(remaining, componentToSize->desiredSize.*actualSize + extra);
        if (componentToSize->fadeawayRatio != utils::int_max<decltype(Component::fadeawayRatio)>)
          componentToSize->bounds.*maxMember = (i32)roundf(componentToSize->bounds.*maxMember *
            ((float)componentToSize->fadeawayRatio / utils::int_max<decltype(Component::fadeawayRatio)>));

        remaining -= componentToSize->bounds.*actualSize;

        if (end != previousEnd)
        {
          componentToSize = end;
          previousEnd = end;
          start = start->next;
        }
        else
        {
          componentToSize = start;
          previousStart = start;
          end = end->previous;
        }
      }

      // no space left for all other components so set their size to 0
      for (; start && start->previous != end; start = start->next)
        start->bounds.*actualSize = 0;
    }
    else
    {
      i32 smallestMaxSize;
      i32 biggestMinSize;
      usize count;
      usize j = 0;
      if (test_flag(component->sizingFlags, (isCalculatingVertical) ?
        Component::ScrollableSnapToMinY : Component::ScrollableSnapToMinX))
      {
        smallestMaxSize = 0;
        biggestMinSize = utils::int_max<i32>;
        count = sortedMax.size();
      }
      else
      {
        usize lastJ = 0;
        usize i = sortedMin.size(), lastI = sortedMin.size();

        Component *biggestMin, *smallestMax;
        while (true)
        {
          biggestMin = sortedMin[i - 1];
          smallestMax = sortedMax[j];

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

        if (j + count != sortedMax.size())
          for (auto k = j + count; k < sortedMin.size(); ++k)
            COMPLEX_ASSERT(sortedMin[k]->bounds.*maxMember != utils::int_max<i32>);
      }

      // put fadeaway components to be sized first since they will be smaller than calculated above
      if (fadeawayComponents)
      {
        utils::insertionsort(sortedMax, j, j + count, [](Component *one, Component *two)
          {
            return one->fadeawayRatio != utils::int_max<decltype(Component::fadeawayRatio)> &&
              two->fadeawayRatio == utils::int_max<decltype(Component::fadeawayRatio)>;
          });
      }

      for (usize k = 0; k < count; ++k)
      {
        auto *c = sortedMax[j + k];
        COMPLEX_ASSERT(c->bounds.*minMember <= biggestMinSize &&
          c->bounds.*maxMember >= smallestMaxSize);

        i32 size = remaining / (i32)(count - k);

        c->bounds.*maxMember = utils::clamp(size, c->bounds.*minMember, c->bounds.*maxMember);
        if (c->fadeawayRatio != utils::int_max<decltype(c->fadeawayRatio)>)
          c->bounds.*maxMember = (i32)(c->bounds.*maxMember *
            ((float)c->fadeawayRatio / utils::int_max<decltype(Component::fadeawayRatio)>));

        COMPLEX_ASSERT(c->bounds.*maxMember >= 0);
        remaining -= c->bounds.*maxMember;
      }
    }
  }
  else if (fadeawayComponents)
  {
    for (auto *c : sortedMax)
    {
      if (c->fadeawayRatio != utils::int_max<decltype(c->fadeawayRatio)>)
      {
        c->bounds.*maxMember = (i32)(c->bounds.*maxMember *
          ((float)c->fadeawayRatio / utils::int_max<decltype(Component::fadeawayRatio)>));
        --fadeawayComponents;
      }

      if (!fadeawayComponents)
        break;
    }
  }

  if (test_flag(component->sizingFlags, (isCalculatingVertical) ? Component::ScrollableY : Component::ScrollableX))
  {
    i32 *scrollableDirection = (isCalculatingVertical) ?
      &component->scrollableArea.h : &component->scrollableArea.w;

    if ((i64)(component->bounds.*maxMember) < sizes.min + childrenMinSizes ||
      (i64)(*scrollableDirection) != sizes.min + childrenMinSizes)
    {
      // parent is scrollable so every child is set to their preferred max sizes
      // which is already done, therefore we only need to update scrollable size

      if (test_flag(component->sizingFlags, (isCalculatingVertical) ?
        Component::ScrollableSnapToMinY : Component::ScrollableSnapToMinX))
        *scrollableDirection = scaleValueRoundInt((float)(sizes.min + childrenMinSizes));
      else
        *scrollableDirection = scaleValueRoundInt((float)sizes.max);
    }
  }

  for (auto *child = children; child; child = child->next)
  {
    if (!child->isStillVisible() || child->bounds.*actualSize <= 0)
      continue;

    COMPLEX_ASSERT(child->bounds.*actualSize != utils::int_max<i32>);
    calculateGrow(child, child->children, isCalculatingVertical);
  }

  // scaling the final size
  component->bounds.*actualSize = scaleValueRoundInt((float)(component->bounds.*actualSize));
  COMPLEX_ASSERT(component->bounds.*actualSize >= 0);
  component->bounds.*actualSize = (component->componentFlags.keepSize) ?
    component->lastBounds.*actualSize : component->bounds.*actualSize;

  // refresh scroll position in case we've been shrunk
  offsetScroll(component, 0.0f, 0.0f, false);
}

void calculateSizes(Component *children, Component *component)
{
  (void)calculateFit(component, children, false);
  calculateGrow(component, children, false);
  (void)calculateFit(component, children, true);
  calculateGrow(component, children, true);
}

#define GET_PRIMARY_FLAG(component, flag) ((flag) << (i32)(!!(component->componentFlags.vertical)))
#define GET_SECONDARY_FLAG(component, flag) ((flag) << (1 - (!!(component->componentFlags.vertical))))
#define TRANSPOSE(component, rect) ((component->componentFlags.vertical) ? (rect).transposed() : (rect))
#define IS_PRIMARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_PRIMARY_FLAG(parent, flag))
#define IS_SECONDARY_POSITION_FLAG_SET(child, parent, flag) (child->placement & GET_SECONDARY_FLAG(parent, flag))

static bool animatePosition(Component *component, bool wasParentResized, bool recursive);

void calculatePositions(Component *children,
  Component *component, Rectangle<i32> boundsInTarget)
{
  if (!component->isStillVisible())
    return;

  auto padding = scaleValueRoundInt(component->padding.toInt());
  if (boundsInTarget.isEmpty())
  {
    boundsInTarget = Rectangle{ padding.x, padding.y,
      component->bounds.w - padding.getRight(),
      component->bounds.h - padding.getBottom() };
  }

  if (test_flag(component->sizingFlags, Component::ScrollableX))
    boundsInTarget.x -= (i32)::roundf(component->scrollOffset.x);
  if (test_flag(component->sizingFlags, Component::ScrollableY))
    boundsInTarget.y -= (i32)::roundf(component->scrollOffset.y);

  auto Rectangle<i32>:: *primaryPosition = (component->componentFlags.vertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
  auto Rectangle<i32>:: *secondaryPosition = (component->componentFlags.vertical) ? &Rectangle<i32>::x : &Rectangle<i32>::y;
  auto Point<i32>:: *primary = (component->componentFlags.vertical) ? &Point<i32>::y : &Point<i32>::x;

  boundsInTarget = TRANSPOSE(component, boundsInTarget);
  padding = TRANSPOSE(component, padding);

  auto &customPlacement = getUiRelated()->renderer->customPlacement;

  i32 primaryUsed = 0;
  i32 groupCount = 0;
  bool isInGroup = false;
  for (auto *child = children; child; child = child->next)
  {
    if (!child->isStillVisible())
      continue;

    if (child->placement & Placement::custom)
    {
      // component has decided to take care of its own position
      customPlacement.emplaceBack(child,
        [](Component *c) { return c->overridePosition(c); });
      continue;
    }

    // setting this here so that we can later skip these components in the animation step
    child->componentFlags.isPositionSet = true;

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

    child->bounds.*secondaryPosition = bounds.y;
  }

  groupCount = utils::max(1, groupCount);
  i32 sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / groupCount;

  auto scrollableFlag = (component->componentFlags.vertical) ? Component::ScrollableY : Component::ScrollableX;
  if (component->sizingFlags & scrollableFlag)
    sizeLeftPerGroup = utils::max(sizeLeftPerGroup, 0);

  i32 sizeOffset{};
  for (auto *child = children; child;)
  {
    // skip components that won't be sized right now
    if (!child->componentFlags.isPositionSet)
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
          if (!nextChild->isStillVisible() || (nextChild->placement & Placement::custom))
            continue;

          auto bounds = TRANSPOSE(component, nextChild->bounds);
          auto margin = scaleValueRoundInt(TRANSPOSE(component, nextChild->margin).toInt());

          bounds.x = boundsInTarget.x + margin.x;
          boundsInTarget.x += bounds.w + margin.getRight();

          nextChild->bounds.*primaryPosition = bounds.x;
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
          if (nextChild->isStillVisible() &&
            !(nextChild->placement & Placement::custom))
            ++j;
        }

        i32 size = sizeLeftPerGroup;
        i32 offset = 0;

        for (auto *groupChild = child; j; groupChild = groupChild->next)
        {
          if (!groupChild->isStillVisible() || (groupChild->placement & Placement::custom))
            continue;

          boundsInTarget.x += offset;

          auto bounds = TRANSPOSE(component, groupChild->bounds);
          auto margin = scaleValueRoundInt(TRANSPOSE(component, groupChild->margin).toInt());

          bounds.x = boundsInTarget.x + margin.x;
          boundsInTarget.x += bounds.w + margin.getRight();

          groupChild->bounds.*primaryPosition = bounds.x;

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
      
      if (child->previousPosition != invalidPosition)
        child->previousPosition.*primary += sizeOffset;
      child->nextPosition.*primary += sizeOffset;
      child->lastBounds.*primaryPosition += sizeOffset;

      child->bounds.*primaryPosition = bounds.x;
      boundsInTarget = boundsInTarget.withTrimLeft(bounds.w + margin.getRight());

      if (child->componentFlags.animateFadeAway)
        sizeOffset += bounds.w - TRANSPOSE(component, child->lastBounds).w;

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
        if (nextChild->isStillVisible() &&
          !(nextChild->placement & Placement::custom))
        {
          auto bounds = TRANSPOSE(component, nextChild->bounds);
          auto margin = scaleValueRoundInt(TRANSPOSE(component, nextChild->margin).toInt());

          bounds.x = boundsInTarget.getRight() - margin.w - bounds.w;
          boundsInTarget.w -= bounds.w + margin.getRight();

          if (nextChild->previousPosition != invalidPosition)
            nextChild->previousPosition.*primary -= sizeOffset;
          nextChild->nextPosition.*primary -= sizeOffset;
          nextChild->lastBounds.*primaryPosition -= sizeOffset;

          if (nextChild->componentFlags.animateFadeAway)
            sizeOffset += bounds.w - TRANSPOSE(component, nextChild->lastBounds).w;

          nextChild->bounds.*primaryPosition = bounds.x;
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
    // skip components that won't be sized right now
    if (child->componentFlags.isPositionSet)
      (void)animatePosition(child, wasThisResized, true);

    calculatePositions(child->children, child);
  }
}

static bool
animatePosition(Component *component, bool wasParentResized, bool recursive)
{
  static constexpr float kMoveTime = 0.15f; //s

  {
    auto change = component->nextPosition - component->bounds.getPosition();
    auto sizeOffset = Point{ component->bounds.w, component->bounds.h } -
      Point{ component->lastBounds.w, component->lastBounds.h };

    if (test_flag(component->placement, Placement::justifyX) ||
      test_flag(component->placement, Placement::right))
    {
      if (test_flag(component->placement, Placement::justifyX))
        sizeOffset.x = sizeOffset.x / 2;

      if (component->previousPosition != invalidPosition)
        component->previousPosition.x += sizeOffset.x;
      change.x -= sizeOffset.x;
    }

    if (test_flag(component->placement, Placement::justifyY) ||
      test_flag(component->placement, Placement::bottom))
    {
      if (test_flag(component->placement, Placement::justifyY))
        sizeOffset.y = sizeOffset.y / 2;

      if (component->previousPosition != invalidPosition)
        component->previousPosition.y += sizeOffset.y;
      change.y -= sizeOffset.y;
    }

    if (change != Point<i32>{} && component->previousPosition != invalidPosition)
    {
      component->previousPosition = component->lastBounds.getPosition();
      component->distanceToNextPositionRatio = 0;
    }

    component->nextPosition = component->bounds.getPosition();
  }

  bool changedParents = component->lastParent != component->parent &&   // did we change parent components?
    component->lastParent && component->parent;

  bool requireReposition = changedParents || (!wasParentResized &&      // skip animations if triggered by parent resize
    component->lastBounds.getPosition() != component->nextPosition);    // have we reached the final destination?

  if (requireReposition &&
    component->componentFlags.animateMovement &&                        // are we animating to begin with?
    component->previousPosition != invalidPosition)                     // did we have a previous position?
  {
    // we're interpolating between the previous position (animation start) and the supposed position
    using T = decltype(component->distanceToNextPositionRatio);

    float ratio = (float)component->distanceToNextPositionRatio / utils::int_max<T>;
    ratio = utils::min(1.0f, ratio + getUiRelated()->deltaTime / kMoveTime);
    auto t = smoothstep(ratio);

    // translate the last position if it was in a different component
    if (changedParents)
    {
      // if the position of the previous/current parent hasn't been set yet
      // delay the position setting for this component
      if (!component->lastParent->componentFlags.isPositionSet ||
        !component->parent->componentFlags.isPositionSet)
      {
        if (recursive)
          getUiRelated()->renderer->customPlacement.emplaceBack(component,
            [wasParentResized](Component *c) { return animatePosition(c, wasParentResized, false); });

        // unset position flag, so that others can see
        component->componentFlags.isPositionSet = false;
        return false;
      }

      component->previousPosition = component->parent->getRelativePoint(
        component->lastParent, component->previousPosition);
    }

    component->bounds.x = (i32)::roundf(utils::lerp((float)component->previousPosition.x, (float)component->nextPosition.x, t));
    component->bounds.y = (i32)::roundf(utils::lerp((float)component->previousPosition.y, (float)component->nextPosition.y, t));

    component->distanceToNextPositionRatio = (T)::ceilf(ratio * utils::int_max<T>);
  }
  else
  {
    // we're not animating position and just setting the supposed position directly in

    component->previousPosition = component->nextPosition;
    component->bounds = component->bounds.withPosition(component->nextPosition);
    component->distanceToNextPositionRatio = 0;
  }

  component->componentFlags.isPositionSet = true;
  component->lastParent = component->parent;

  return true;
}

#undef IS_SECONDARY_POSITION_FLAG_SET
#undef IS_PRIMARY_POSITION_FLAG_SET
#undef TRANSPOSE
#undef GET_SECONDARY_FLAG
#undef GET_PRIMARY_FLAG

static void clearLinkedComponents(Component *c, bool checkSelf)
{
  auto *renderer = getUiRelated()->renderer;
  auto *gui = renderer->gui;
  if ((checkSelf && c == gui->popupSelector.summoner) || c->isParentOf(gui->popupSelector.summoner))
  {
    if (gui->popupSelector.cancel)
      gui->popupSelector.cancel(&gui->popupSelector);

    gui->popupSelector.resetState();
    gui->popupSelector.summoner->removeChildComponent(&gui->popupSelector);
  }
  if ((checkSelf && c == gui->popupDisplay1.source) || c->isParentOf(gui->popupDisplay1.source))
  {
    gui->popupDisplay1.componentFlags.isVisible = false;
  }
  if ((checkSelf && c == gui->popupDisplay2.source) || c->isParentOf(gui->popupDisplay2.source))
  {
    gui->popupDisplay2.componentFlags.isVisible = false;
  }

  auto mouseInteractions = renderer->getMouseInteractions();
  if (auto *focusedComponent = mouseInteractions.focused)
    if ((checkSelf && c == focusedComponent) || c->isParentOf(focusedComponent))
      focusedComponent->giveAwayFocusTo();

  if (auto *hoveredComponent = mouseInteractions.hovered)
  {
    if ((checkSelf && c == hoveredComponent) || c->isParentOf(hoveredComponent))
    {
      // last chance to execute anything from this component (i.e. reset cursor shape)
      hoveredComponent->mouseExit(mouseInteractions.mouseState);
      renderer->setHoveredComponent(nullptr);
    }
  }

  if (auto *clickedComponent = mouseInteractions.clicked)
    if ((checkSelf && c == clickedComponent) || c->isParentOf(clickedComponent))
      renderer->setClickedComponent(nullptr);
}

void deleteComponent(Component *component)
{
  clearLinkedComponents(component, true);
  if (component->pool)
    utils::Pool::destroy(component->pool);
  utils::Pool::remove(component);
}

void checkScrollClick(Component *component, const MouseEvent &e)
{
  auto scaledPadding = scaleValueRoundInt(component->padding.toInt());
  Rectangle scrollBoundsX{ scaledPadding.x, component->bounds.h - scaledPadding.h,
    component->bounds.w - scaledPadding.getRight(), scaledPadding.h };
  Rectangle scrollBoundsY{ component->bounds.w - scaledPadding.w, scaledPadding.y,
    scaledPadding.w, component->bounds.h - scaledPadding.getBottom() };

  component->componentFlags.isScrollbarXClicked =
    test_flag(component->sizingFlags, Component::ScrollableWithBarX) &&
    component->scrollableArea.w > component->bounds.w && scrollBoundsX.contains(e.x, e.y);

  component->componentFlags.isScrollbarYClicked =
    test_flag(component->sizingFlags, Component::ScrollableWithBarY) &&
    component->scrollableArea.h > component->bounds.h && scrollBoundsY.contains(e.x, e.y);

  component->scrollOffsetBeforeClick = component->scrollOffset;
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

      if (child->previousPosition != invalidPosition)
        child->previousPosition += offset;
      child->nextPosition += offset;
      child->lastBounds += offset;
      child->bounds += offset;
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
    component->scrollOffset.x = utils::clamp(component->scrollOffsetBeforeClick.x - deltaX,
      0.0f, utils::max(0.0f, (float)(component->scrollableArea.w - component->bounds.w)));
  }

  if (component->sizingFlags & Component::ScrollableY)
  {
    component->scrollOffset.y = utils::clamp(component->scrollOffsetBeforeClick.y - deltaY,
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

Range<i32>
calculateSingleLineTextSize(Component *c, bool isCalculatingVertical,
  FontId fontId, utils::string_view text)
{
  float lineHeight = scaleValue((float)((c->componentFlags.vertical) ?
    c->desiredSize.y : c->desiredSize.x));

  if (!isCalculatingVertical)
  {
    getUiRelated()->g->setFont(fontId, lineHeight);
    auto max = (i32)::ceilf(getUiRelated()->g->getStringWidthFloat(text));

    return Range<i32>{ 0, max };
  }
  else
  {
    // single line text is single line lmao
    auto height = (i32)::ceilf(lineHeight);
    return Range<i32>{ height, height };
  }
}

bool
moveFocusTo(Renderer *renderer, Component *component, FocusChange reason)
{
  if (renderer->focusedComponent_ == component)
    return true;

  if ((!renderer->focusedComponent_ || renderer->focusedComponent_->handleFocus(false, reason, component)) &&
    (!component || component->handleFocus(true, reason, renderer->focusedComponent_)))
  {
    renderer->focusedComponent_ = component;
    return true;
  }

  return false;
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
Component::mouseWheelMove(const MouseEvent &e)
{
  if (e.mods.test(ModifierKeys::ctrlModifier))
    return false;

  bool isHorizontal = e.mods.test(ModifierKeys::shiftModifier);
  auto multiplier = 30.0f * getUiRelated()->scale;
  offsetScroll(this, e.wheelDeltaX * multiplier,
    e.wheelDeltaY * multiplier, isHorizontal);

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
  if (source == this)
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
      if (child->isStillVisible() &&
        child->contains(Point{ x - child->bounds.x, y - child->bounds.y }))
      {
        if (!recursive)
          return child;

        auto *result = child->getComponentAt(x - child->bounds.x, y - child->bounds.y, onlyClickable);
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
Component::isFocused(bool trueIfChildIsFocused) const
{
  auto *focusedComponent = getUiRelated()->renderer->getMouseInteractions().focused;
  if (!trueIfChildIsFocused && this != focusedComponent)
    return false;

  return isParentOf(focusedComponent);
}

// the focusing strategy is a little unusual
// main idea is that if a component tries to grab focus,
// it can delegate it to its children inside the handleFocus() callback
// 
// the reason for making the system as such is to support elements, that have combined functionality, work correctly
// i.e. text dialogue with a bar of options appears and grabs the focus,
// but itself can't do anything with the focus, instead it gets passed down to the textbox within it
// 
// if none of the (indirect) children can acquire focus, we try to transfer focus upwards if it's allowed
// and repeat the process above while ignoring the tree we came from
static bool
grabFocusInternal(Renderer *renderer, Component *component, 
  Component *origin, bool tryToFocusUpwards)
{
  COMPLEX_ASSERT(component);

  // if we're going up, we already know that the parent hierarchy is visible so we don't need to check
  if (!origin && !component->isStillVisible())
    return false;

  if (component == renderer->getMouseInteractions().focused)
    return true;

  if (moveFocusTo(renderer, component, FocusChange::Grabbed))
    return true;

  if (component->children)
  {
    // shallow search
    for (auto *child = component->children->previous; ; child = child->previous)
    {
      if (child != origin && moveFocusTo(renderer, child, FocusChange::Grabbed))
        return true;

      if (!child->previous->next)
        break;
    }

    // deep search
    for (auto *child = component->children->previous; ; child = child->previous)
    {
      if (child != origin)
        if (grabFocusInternal(renderer, child, nullptr, tryToFocusUpwards))
          return true;

      if (!child->previous->next)
        break;
    }
  }

  // upwards search
  // we check for origin to disallow children to perform upwards searches
  if (tryToFocusUpwards && origin && component->parent != nullptr)
    return grabFocusInternal(renderer, component->parent, component, tryToFocusUpwards);

  return false;
}

void Component::grabFocus(bool tryToFocusUpwards)
{
  // if this component isn't even visible in the hierarchy, then grabbing focus is pointless
  if (!isShowing())
    return;

  auto *renderer = getUiRelated()->renderer;
  grabFocusInternal(renderer, this, this, tryToFocusUpwards);
}

bool
Component::giveAwayFocusTo(Component *component)
{
  auto *renderer = getUiRelated()->renderer;

  if (this == renderer->getMouseInteractions().focused &&
    moveFocusTo(renderer, component, FocusChange::GivenAway))
    return true;

  return false;
}

bool
Component::isObscured(const Component *ignoreClipIncluding) const
{
  auto scissorBounds = bounds;

  bool isNotClipping = ignoreClipIncluding != nullptr;
  for (auto *parentComponent = parent; parentComponent; parentComponent = parentComponent->parent)
  {
    auto parentBounds = parentComponent->bounds;

    if (!isNotClipping)
      scissorBounds = parentBounds.withZeroOrigin().getIntersection(scissorBounds);

    scissorBounds = scissorBounds + parentBounds.getPosition();
    isNotClipping &= parentComponent != ignoreClipIncluding;

    if (scissorBounds.isEmpty())
      return true;
  }

  return scissorBounds.isEmpty();
}

bool
Component::isShowing(const Component *relativeTo) const
{
  auto *c = this;
  for (; c != relativeTo; c = c->parent)
    if (!c->isStillVisible())
      return false;

  COMPLEX_ASSERT(c == relativeTo);

  return true;
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
  auto *insertBefore = children;
  for (; index && insertBefore; (--index), (insertBefore = insertBefore->next)) { }
  addChildComponent(childToAdd, insertBefore);
}

void Component::removeChildComponent(Component *childToRemove, bool keepFocus)
{
  if (!childToRemove || childToRemove->parent != this)
    return;

  if (!keepFocus)
    clearLinkedComponents(childToRemove, true);

  childToRemove->lastParent = childToRemove->parent;
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

  clearLinkedComponents(this, false);

  while (children)
  {
    // don't remove focus because it's already done
    removeChildComponent(children, true);
  }

  COMPLEX_ASSERT(!children);
}

void Component::renderScrollbars(Graphics &g, float scrollExpandPercent)
{
  Rectangle<float> xScrollBounds{};
  Rectangle<float> yScrollBounds{};
  float xScrollBrightness = 0.0f;
  float yScrollBrightness = 0.0f;

  auto scaledPadding = scaleValueRoundInt(padding.toInt());
  Point<int> mousePosition{};
  if (componentFlags.isHovered)
  {
    auto interactions = getUiRelated()->renderer->getMouseInteractions();
    mousePosition = Point{ interactions.mouseState.x, interactions.mouseState.y } - getPositionInWindow();
  }

  auto calculateScrollWidth = [this, &mousePosition, scrollExpandPercent](Rectangle<i32> bounds,
    u16 &currentScrollWidth, float &brightness, bool isScrollClicked)
  {
    float percentExpand = (float)currentScrollWidth / (float)utils::int_max<u16>;
    if (bounds.contains(mousePosition) || isScrollClicked)
    {
      percentExpand += scrollExpandPercent;
      brightness = 0.3f;
    }
    else
      percentExpand -= scrollExpandPercent;
    percentExpand = utils::clamp(percentExpand, getValue(Skin::kScrollShrinkPercent, false, this), 1.0f);
    currentScrollWidth = (u16)(percentExpand * utils::int_max<u16>);
    return percentExpand;
  };

  if (test_flag(sizingFlags, Component::ScrollableWithBarX) && scrollableArea.w > bounds.w)
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

    float width = 0.5f * (float)scrollBounds.h * calculateScrollWidth(scrollBounds,
      scrollWidthsRatio.x, xScrollBrightness, componentFlags.isScrollbarXClicked);
    xScrollBounds = { (float)scrollBounds.x + start,
      scrollBounds.getBottom() - width - 0.25f * scrollBounds.h, length, width };
  }

  if (test_flag(sizingFlags, Component::ScrollableWithBarY) && scrollableArea.h > bounds.h)
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

    float width = 0.5f * (float)scrollBounds.w * calculateScrollWidth(scrollBounds,
      scrollWidthsRatio.y, yScrollBrightness, componentFlags.isScrollbarYClicked);
    yScrollBounds = { (float)scrollBounds.getRight() - width - 0.25f * scrollBounds.w,
      (float)scrollBounds.y + start, width, length };
  }

  if (!xScrollBounds.isEmpty())
  {
    auto c = getColour(Skin::kLightenScreen, this);
    if (xScrollBrightness > 0.0f)
      c = c.lighter(xScrollBrightness);

    fillRect(g, xScrollBounds, c, xScrollBounds.h * 0.5f);
  }

  if (!yScrollBounds.isEmpty())
  {
    auto c = getColour(Skin::kLightenScreen, this);
    if (yScrollBrightness > 0.0f)
      c = c.lighter(yScrollBrightness);

    fillRect(g, yScrollBounds, c, yScrollBounds.w * 0.5f);
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
Component::handleCommandMessage(u64 commandId, void *extraData, bool skipDefaultHandler)
{
  for (auto *handler = commandMessageHandler; handler; handler = handler->next)
    if (handler->object && handler->object(this, commandId, extraData))
      return true;

  if (!skipDefaultHandler)
  {
    // insert default handlers if necessary
    return false;
  }

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

void Component::renderChild(Graphics &g, Component *child,
  void (*renderFn)(Graphics &, Component *))
{
  if (!child->isStillVisible())
    return;

  // is the child completely outside our bounds
  if (getLocalBounds().getIntersection(child->bounds).isEmpty())
    return;

  nvgSave(g);

  auto scissorBounds = child->bounds.toFloat();

  if (child->componentFlags.halfScissor &&
    !child->componentFlags.vertical)
  {
    float scissor[4];
    nvgGetScissor(g, scissor);
    scissorBounds = { scissorBounds.x, scissor[1], scissorBounds.w, scissor[3] };
  }
  else if (child->componentFlags.halfScissor &&
    child->componentFlags.vertical)
  {
    float scissor[4];
    nvgGetScissor(g, scissor);
    scissorBounds = { scissor[0], scissorBounds.y, scissor[2], scissorBounds.h };
  }

  nvgIntersectScissor(g, scissorBounds.x, scissorBounds.y,
    scissorBounds.w, scissorBounds.h);

  nvgTranslate(g, (float)child->bounds.x, (float)child->bounds.y);

  if (renderFn)
    renderFn(g, child);
  else
  {
    bool continueRender = child->render(g);
    if (continueRender)
      child->renderChildren(g);
  }

  nvgRestore(g);
}
