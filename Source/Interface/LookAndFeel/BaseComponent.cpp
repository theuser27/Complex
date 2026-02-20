
// Created: 2023-12-11 16:49:17

#include "BaseComponent.hpp"

#include "Miscellaneous.hpp"
#include "Graphics.hpp"
#include "Shaders.hpp"
#include "Plugin/Renderer.hpp"
#include "Generation/BaseProcessor.hpp"
#include "../Components/BaseControl.hpp"
#include "../Sections/MainInterface.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
  PopupSelector *getPopupSelector() { return getGui(uiRelated.renderer)->getPopupSelector(); }
  PopupDisplay *getPopupDisplay(bool primary = true) { return getGui(uiRelated.renderer)->getPopupDisplay(primary); }
  utils::bumpArena *getUIArena() { return getGui(uiRelated.renderer)->arena; }

#define GET_PRIMARY_FLAG(component, flag) ((flag) << (i32)(!!(component->componentFlags.isVertical)))
#define GET_SECONDARY_FLAG(component, flag) ((flag) << (1 - (!!(component->componentFlags.isVertical))))
#define TRANSPOSE(component, rect) ((component->componentFlags.isVertical) ? (rect).transposed() : (rect))

  static void calculateFitPrimary(Component *component, Component *children, bool isVertical)
  {
    if (!component->componentFlags.isVisible)
      return;

    if (!isVertical)
      component->bounds = {};

    auto Rectangle<i32>:: *minMember = (isVertical) ? &Rectangle<i32>::y : &Rectangle<i32>::x;
    auto Rectangle<i32>:: *maxMember = (isVertical) ? &Rectangle<i32>::h : &Rectangle<i32>::w;
    
    i32 minSize = 0;
    i32 nonPositionedMinSize = 0;

    i32 sameAsSiblingsMinSize = 0;
    utils::vector<Component *> sameAsSiblingsComponents{};

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      calculateFitPrimary(child, child->children, isVertical);

      auto childBounds = child->bounds;
      auto childPadding = child->padding.toInt();
      auto childMargin = child->margin.toInt();

      if (component->componentFlags.isVertical ^ isVertical)
      {
        minSize = utils::max(minSize, childBounds.*minMember +
          childPadding.*minMember + childPadding.*maxMember +
          childMargin.*minMember + childMargin.*maxMember);
      }
      else
      {
        if (child->placement == Placement::custom)
        {
          nonPositionedMinSize = utils::max(nonPositionedMinSize, 
            childBounds.*maxMember +
            childPadding.*minMember + childPadding.*maxMember +
            childMargin.*minMember + childMargin.*maxMember);
        }
        else
        {
          minSize += childBounds.*maxMember + 
            childPadding.*minMember + childPadding.*maxMember +
            childMargin.*minMember + childMargin.*maxMember;
        }

        auto sameAsSiblingsTest = (isVertical) ? 
          Component::SameAsSiblingsY : Component::SameAsSiblingsX;

        // taking care of children that conform to siblings' primary size
        if (test_enum(child->sizingFlags, sameAsSiblingsTest))
        {
          sameAsSiblingsComponents.emplace_back(child);
          sameAsSiblingsMinSize = utils::max(sameAsSiblingsMinSize, childBounds.*minMember);
        }
      }
    }

    for (auto *child : sameAsSiblingsComponents)
    {
      child->bounds.*maxMember = sameAsSiblingsMinSize;

      if (child->placement == Placement::custom)
      {
        auto childPadding = child->padding.toInt();
        auto childMargin = child->margin.toInt();

        nonPositionedMinSize = utils::max(nonPositionedMinSize,
          sameAsSiblingsMinSize + 
          childPadding.*minMember + childPadding.*maxMember +
          childMargin.*minMember + childMargin.*maxMember);
      }
      else
        minSize += sameAsSiblingsMinSize;
    }

    minSize = utils::max(minSize, nonPositionedMinSize);

    if (test_enum(component->sizingFlags, Component::CustomDimensions))
    {
      i32 *availableSize = (isVertical) ? &component->bounds.w : nullptr;
      auto [min, max] = component->getDimensions(component, availableSize);
      // returned bounds are scaled so we need to unscale them 
      // in order to work with the rest of the values
      component->bounds.*minMember = (i32)::ceilf(unscaleValue((float)min));
      component->bounds.*maxMember = (i32)::ceilf(unscaleValue((float)max));
    }
    else if (test_enum(component->sizingFlags, Component::Fixed))
    {
      component->bounds.*minMember = component->desiredSize.*minMember;
      component->bounds.*maxMember = component->desiredSize.*maxMember;
    }
    else
    {
      component->bounds.*minMember = utils::max(component->desiredSize.*minMember, minSize);

      bool isGrowable = (isVertical) ?
        ((component->sizingFlags & Component::ScrollableWithBarY) == Component::GrowableY) :
        ((component->sizingFlags & Component::ScrollableWithBarX) == Component::GrowableX);

      component->bounds.*maxMember = (isGrowable) ? utils::max_limit<i32> : 
        utils::max(component->bounds.*minMember, 
          utils::min(component->desiredSize.*maxMember, minSize));
    }

    // alogn the primary   axis SameAsSiblings must be zero, the parent will take care of it
    // along the secondary axis SameAsSiblings acts like Growable
    if (!isVertical)
    {
      if (test_enum(component->sizingFlags, Component::SameAsSiblingsX))
        component->bounds.w = (component->componentFlags.isVertical) ? utils::max_limit<i32> : 0;
    }
    else
    {
      if (test_enum(component->sizingFlags, Component::SameAsSiblingsY))
        component->bounds.h = (component->componentFlags.isVertical) ? 0 : utils::max_limit<i32>;
    }
  }

  static void calculateFitSecondary(Component *component, Component *children)
  {
    i32 secondaryAxis = 0;

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      calculateFitSecondary(child, child->children);

      secondaryAxis = utils::max(secondaryAxis, 
        child->bounds.y + child->padding.y + child->padding.h +
        child->margin.y + child->margin.h);
    }

    if (test_enum(component->sizingFlags, Component::CustomDimensions))
    {
      auto [min, max] = component->getDimensions(component, &component->bounds.w);
      component->bounds.y = (i32)::ceilf(unscaleValue((float)min));
      component->bounds.h = (i32)::ceilf(unscaleValue((float)max));
    }
    else if (test_enum(component->sizingFlags, Component::Fixed))
    {
      component->bounds.y = component->desiredSize.y;
      component->bounds.h = component->desiredSize.h;
    }
    else
    {
      component->bounds.y = utils::max(component->desiredSize.y, secondaryAxis);

      if (test_enum(component->sizingFlags, Component::GrowableY) &&
        !test_enum(component->sizingFlags, Component::ScrollableY))
        component->bounds.h = utils::max_limit<i32>;
      else
      {
        COMPLEX_ASSERT(secondaryAxis <= component->desiredSize.h);
        component->bounds.h = utils::min(component->desiredSize.h, secondaryAxis);
      }
    }

    // along the secondary axis SameAsSiblings acts like Growable
    if (test_enum(component->sizingFlags, Component::SameAsSiblingsY))
      component->bounds.h = utils::max_limit<i32>;
  }

  constinit utils::vector<Component *> *sortedSizesMin{};
  constinit utils::vector<Component *> *sortedSizesMax{};
  
  static void calculateGrowPrimary(Component *component, Component *children)
  {
    if (!children)
      return;

    auto &sortedMin = *sortedSizesMin;
    auto &sortedMax = *sortedSizesMax;
    sortedMin.clear();
    sortedMax.clear();

    i32 minimumPrimarySize = component->padding.x + component->padding.w;
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      minimumPrimarySize += child->bounds.x + child->margin.x + child->margin.w;

      auto iter = utils::find_if(sortedMin, [&](const Interface::Component *c)
        { return c->bounds.x > child->bounds.x; });
      sortedMin.emplace(iter, child);

      iter = utils::find_if(sortedMax, [&](const Interface::Component *c)
        { return c->bounds.w > child->bounds.w; });
      sortedMax.emplace(iter, child);
    }

    if (component->bounds.w < minimumPrimarySize &&
      test_enum(component->sizingFlags, Component::ScrollableX))
    {
      // parent is scrollable so every child is set to their preferred max sizes
      // which is already done, therefore this is a no-op
      // 
      // NB: if you have a scrollable parent and a growable child
      // along the same axis, this will fail spectacularly
      // (although does that even make sense?)

      component->scrollableArea.w = minimumPrimarySize;
    }
    else
    {
      i32 remaining = component->bounds.w - minimumPrimarySize;

      i32 smallestMaximum;
      i32 biggestMinimum;
      usize componentCount;
      usize j = 0;
      {
        usize lastJ = 0;
        usize i = sortedMin.size(), lastI = sortedMin.size();

        while (true)
        {
          // if the remaining size cannot expand all smaller components to the current minimum, exclude the biggest
          if (i > 1 &&
            (sortedMin[i - 1]->bounds.x * (i32)(i - j)) > remaining)
          {
            --i;
          }
          // if the remaining size can now expand all smaller components to the next minimum, include the next biggest
          else if (i < sortedMin.size() &&
            (sortedMin[i]->bounds.x * (i32)(i - j)) < remaining)
          {
            ++i;
          }

          i32 currentSize = sortedMin[i - 1]->bounds.x;
          if (i < sortedMin.size())
            currentSize += (remaining - (sortedMin[i]->bounds.x * (i32)(i - j)));

          // if the most constrained component's maximum gets surpassed by the current size, 
          // exclude it and update the remainder
          if (sortedMax[j]->bounds.w < currentSize)
          {
            ++j;
            if (j < sortedMax.size())
              remaining -= sortedMax[j]->bounds.w;
          }

          if (i == lastI && j == lastJ)
            break;

          lastI = i;
          lastJ = j;
        }

        biggestMinimum = sortedMin[i - 1]->bounds.x;
        smallestMaximum = sortedMax[j]->bounds.w;
        componentCount = i - j;
      }


      for (; componentCount; ++j)
      {
        auto *c = sortedMax[j];
        if (c->bounds.x <= biggestMinimum &&
          c->bounds.w >= smallestMaximum)
        {
          i32 size = remaining / (i32)(componentCount);

          auto minMax = c->desiredSize;
          c->bounds.w = utils::clamp(size, minMax.x, minMax.w);

          remaining -= c->bounds.w;
          --componentCount;
        }
      }
    }

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      // scaling the final primary size
      child->bounds.w = scaleValueRoundInt((float)child->bounds.w);
      calculateGrowPrimary(child, child->children);
    }
  }

  static void calculateGrowSecondary(Component *component, Component *children)
  {
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      auto childBounds = child->bounds;
      child->bounds.h = utils::min(child->bounds.h, component->bounds.h);
      // scaling the final secondary size
      child->bounds.h = scaleValueRoundInt((float)child->bounds.h);
    }

    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      calculateGrowSecondary(child, child->children);
    }
  }

  void calculateSizes(Component *children, Component *component)
  {
    utils::vector<Component *> sortedSizesMin_{ localScratch, 32 };
    sortedSizesMin = &sortedSizesMin_;
    utils::vector<Component *> sortedSizesMax_{ localScratch, 32 };
    sortedSizesMax = &sortedSizesMax_;

    calculateFitPrimary(component, children, false);
    calculateGrowPrimary(component, children);
    calculateFitPrimary(component, children, true);
    //calculateFitSecondary(component, children);
    calculateGrowSecondary(component, children);
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
        component->bounds.w - padding.w, component->bounds.h - padding.h };
    }

    if (test_enum(component->sizingFlags, Component::ScrollableX))
      boundsInTarget.x -= component->scrollOffset.x;
    if (test_enum(component->sizingFlags, Component::ScrollableY))
      boundsInTarget.y -= component->scrollOffset.y;

    boundsInTarget = TRANSPOSE(component, boundsInTarget);
    padding = TRANSPOSE(component, padding);

    i32 primaryUsed = padding.getRight();
    i32 groupCount = 0;
    bool isInGroup = false;
    for (auto *child = children; child; child = child->next)
    {
      if (!child->componentFlags.isVisible)
        continue;

      if (child->placement & Placement::custom)
      {
        // component has decided to take care of its own position
        customPlacement->emplace_back(child);
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
        bounds.y = boundsInTarget.y + padding.y + margin.y;
      else if (IS_SECONDARY_POSITION_FLAG_SET(child, component, Placement::right))
        bounds.y = boundsInTarget.h - padding.h - margin.h - bounds.h;
      else
        bounds.y = utils::max(boundsInTarget.y + padding.y + margin.y, 
          (boundsInTarget.h - padding.getBottom() - bounds.getBottom()) / 2);
      
      child->bounds.setPosition(TRANSPOSE(component, bounds.getPosition()));
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
          i32 offset = sizeLeftPerGroup / 2;
          boundsInTarget.x += offset;

          for (; nextChild && (nextChild->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type;
            nextChild = nextChild->next)
          {
            auto bounds = TRANSPOSE(component, nextChild->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, nextChild->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            nextChild->bounds = TRANSPOSE(component, bounds);
          }

          boundsInTarget.x += sizeLeftPerGroup - offset;
        }
        else
        {
          // inside a justify group

          usize j = 0;

          for (; nextChild && (nextChild->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type;
            (++j), (nextChild = nextChild->next)) { }

          i32 size = sizeLeftPerGroup;
          i32 offset = 0;

          for (auto *groupChild = child; j; (--j), (groupChild = groupChild->next))
          {
            boundsInTarget.x += offset;

            auto bounds = TRANSPOSE(component, groupChild->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, groupChild->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            groupChild->bounds = TRANSPOSE(component, bounds);

            // there will be a division by zero here on the last iteration,
            // but the variables don't get used after that so it's fine
            offset = size / (i32)(j - 1);
            size -= offset;
          }
        }

        --groupCount;
        primaryUsed += sizeLeftPerGroup;
        sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / groupCount;
        child = nextChild;
      }
      else
      {
        auto bounds = TRANSPOSE(component, child->bounds);
        auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());

        if (IS_PRIMARY_POSITION_FLAG_SET(child, component, Placement::left))
        {
          bounds.x = boundsInTarget.x + margin.x;
          boundsInTarget.x += bounds.w + margin.getRight();
        }
        else if (IS_PRIMARY_POSITION_FLAG_SET(child, component, Placement::right))
        {
          bounds.x = boundsInTarget.getRight() - margin.w - bounds.w;
          boundsInTarget.w -= bounds.w + margin.getRight();
        }

        child->bounds = TRANSPOSE(component, bounds);
        child = child->next;
      }
    }

    for (auto *child = children; child; child = child->next)
      calculatePositions(child->children, child);
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
      component->componentFlags.destroyOpenGl = true;
      component->render(getOpenGlContext(uiRelated.renderer));
    }
    
    auto *gui = getGui(uiRelated.renderer);
    if (auto *selector = gui->getPopupSelector(); selector->summoner == component)
    {
      if (selector->cancel)
        selector->cancel(selector);

      selector->resetState();
      component->removeChildComponent(selector);
    }
    if (auto *display = gui->getPopupDisplay(true); display->source == component)
    {
      display->componentFlags.isVisible = false;
    }
    if (auto *display = gui->getPopupDisplay(false); display->source == component)
    {
      display->componentFlags.isVisible = false;
    }

    component->deleteAllChildComponents(false);
    if (freeArena)
      utils::bumpArena::remove(component);
  }

  bool 
  Component::mouseWheelMove(const MouseEvent &event)
  {
    if ((sizingFlags & Component::ScrollableX) == 0 && event.wheelDeltaX != 0.0f)
    {
      // TODO:
      return true;
    }

    if ((sizingFlags & Component::ScrollableY) == 0 && event.wheelDeltaY != 0.0f)
    {
      // TODO:
      return true;
    }

    return false;
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
  isShowing(Component *component)
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
    if (!isShowing(component))
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

    if (component->isParentOf(focusedComponent) && isShowing(focusedComponent))
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

  void Component::addChildComponent(Component *childToAdd)
  {
    COMPLEX_ASSERT(childToAdd->parent != this);

    childToAdd->parent = this;
    childToAdd->next = nullptr;

    if (!children)
    {
      children = childToAdd;
      children->previous = childToAdd;
      return;
    }

    auto *test = children;
    for (; test; test = test->next)
      if (test->layerIndex > childToAdd->layerIndex)
        break;

    if (!test)
    {
      childToAdd->previous = children->previous;
      children->previous->next = childToAdd;
      children->previous = childToAdd;
    }
    else
    {
      if (test->previous->next)
        test->previous->next = childToAdd;
      childToAdd->previous = test->previous;
      childToAdd->next = test;
      test->previous = childToAdd;
    }
  }

  void Component::removeChildComponent(Component *childToRemove, bool keepFocus)
  {
    if (childToRemove->parent != this)
      return;

    childToRemove->parent = nullptr;
    childToRemove->next->previous = childToRemove->previous;
    if (childToRemove->previous->next)
      childToRemove->previous->next = childToRemove->next;

    if (!keepFocus)
      childToRemove->giveAwayFocus();
  }

  Component *
  Component::removeChildComponent(usize childIndexToRemove, bool keepFocus)
  {
    auto *child = children;
    for (usize i = 0; i < childIndexToRemove; ++i)
      child = child->next;

    child->parent = nullptr;
    child->next->previous = child->previous;
    if (child->previous->next)
      child->previous->next = child->next;

    if (!keepFocus)
      child->giveAwayFocus();

    return child;
  }

  void Component::removeAllChildComponents()
  {
    if (!children)
      return;

    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (auto *child = children; child; child = child->next)
    {
      child->parent = nullptr;
      child->previous->next = nullptr;
      child->previous = nullptr;
    }
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

  void Component::deleteAllChildComponents(bool freeArenas)
  {
    if (!children)
      return;

    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (auto *child = children; child; child = child->next)
      deleteComponent(child, freeArenas);
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
      u8 &currentScrollWidth, float &brightness)
    {
      float percentExpand = currentScrollWidth / 255.0f;
      if (bounds.contains(mousePosition))
      {
        percentExpand += scrollExpandPercent;
        if (componentFlags.isClicked)
          brightness = 0.3f;
      }
      else
        percentExpand -= scrollExpandPercent;
      percentExpand = utils::clamp(percentExpand, getValue(Skin::kScrollShrinkPercent, false, this), 1.0f);
      currentScrollWidth = (u8)(percentExpand * 255);
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

      float width = (float)scrollBounds.x * calculateScrollWidth(scrollBounds, scrollWidths.x, xScrollBrightness);
      xScrollBounds = { (float)scrollBounds.x + start, scrollBounds.getBottom() - width, length, width };
    }

    if (test_enum(sizingFlags, Component::ScrollableWithBarX) && scrollableArea.h > bounds.h)
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

      float width = (float)scrollBounds.y * calculateScrollWidth(scrollBounds, scrollWidths.y, yScrollBrightness);
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
    ScopedBoundsEmplace g{ openGl.parentStack, this };
    nvgTranslate(openGl.g, (float)bounds.x, (float)bounds.y);

    bool continueRender = render(openGl);
    COMPLEX_CHECK_OPENGL_ERROR();
    if (!continueRender)
    {
      nvgTranslate(openGl.g, (float)(-bounds.x), (float)(-bounds.y));
      return;
    }

    for (auto *child = children; child; child = child->next)
    {
      if (child->componentFlags.isVisible)
        child->doRender(openGl);
    }

    nvgTranslate(openGl.g, (float)(-bounds.x), (float)(-bounds.y));
  }

  void ProcessorSection::addControl(Control *control, bool addChild)
  {
    controls_.add(control->details.id, control);      

    if (addChild)
      addChildComponent(control);
  }

  void ProcessorSection::removeControl(Control *control)
  {
    removeChildComponent(control);

    controls_.erase(control->details.id);      
  }
}
