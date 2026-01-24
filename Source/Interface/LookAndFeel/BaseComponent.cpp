/*
  ==============================================================================

    BaseComponent.cpp
    Created: 11 Dec 2023 4:49:17pm
    Author:  theuser27

  ==============================================================================
*/

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
  PopupDisplay *getPopupDisplay(bool primary) { return getGui(uiRelated.renderer)->getPopupDisplay(primary); }
  utils::bumpArena *getUIArena() { return getGui(uiRelated.renderer)->arena; }

#define GET_PRIMARY_FLAG(component, flag) ((flag) << (i32)(!!(component->sizingFlags & Component::IsVertical)))
#define GET_SECONDARY_FLAG(component, flag) ((flag) << (1 - (!!(component->sizingFlags & Component::IsVertical))))
#define IS_PRIMARY_FLAG_SET(component, flag) (component->sizingFlags & GET_PRIMARY_FLAG(component, flag))
#define IS_SECONDARY_FLAG_SET(component, flag) (component->sizingFlags & GET_SECONDARY_FLAG(component, flag))
#define TRANSPOSE(component, rect) ((component->sizingFlags & Component::IsVertical) ? (rect).transposed() : (rect))

  static void calculateFitPrimary(Component *component, utils::span<Component *> children)
  {
    component->previousBounds = component->bounds;
    component->bounds = {};

    i32 primaryAxis = 0;
    i32 nonPositionedMinSize = 0;

    i32 sameAsSiblingsMinSize = 0;
    utils::vector<Component *> sameAsSiblingsComponents{};

    for (auto *child : children)
    {
      calculateFitPrimary(child, child->childComponents);

      auto padding = TRANSPOSE(component, child->padding);
      auto margin = TRANSPOSE(component, child->margin);

      if (child->placement == Placement::custom)
      {
        nonPositionedMinSize = utils::max(nonPositionedMinSize, 
          child->bounds.w + padding.x + padding.w + margin.x + margin.w);
      }
      else
      {
        primaryAxis += child->bounds.w + padding.x + padding.w + margin.x + margin.w;
      }

      // taking care of children that conform to siblings' primary size
      if (IS_PRIMARY_FLAG_SET(child, Component::SameAsSiblingsX))
      {
        sameAsSiblingsComponents.emplace_back(child);
        sameAsSiblingsMinSize = utils::max(sameAsSiblingsMinSize, child->bounds.x);
      }
    }

    for (auto *child : sameAsSiblingsComponents)
    {
      child->bounds.w = sameAsSiblingsMinSize;
      if (child->placement == Placement::custom)
      {
        auto padding = TRANSPOSE(component, child->padding);
        auto margin = TRANSPOSE(component, child->margin);
        nonPositionedMinSize = utils::max(nonPositionedMinSize,
          child->bounds.w + padding.x + padding.w + margin.x + margin.w);
      }
      else
        primaryAxis += child->bounds.w;
    }

    primaryAxis = utils::max(primaryAxis, nonPositionedMinSize);

    if (component->sizingFlags & Component::HasText)
    {
      auto [min, max] = component->desiredSize.getTextDimensions(component, nullptr);
      // caching bounds for future use
      component->bounds.x = min;
      component->bounds.w = max;
    }
    else
    {
      auto minMax = TRANSPOSE(component, component->desiredSize.minMax);
      component->bounds.x = utils::max(minMax.x, primaryAxis);

      if (IS_PRIMARY_FLAG_SET(component, Component::GrowableX))
        component->bounds.w = utils::max_limit<i32>;
      else
        component->bounds.w = utils::max(minMax.w, primaryAxis);
    }

    // reset our max size, the parent will take care of it
    if (IS_PRIMARY_FLAG_SET(component, Component::SameAsSiblingsX))
      component->bounds.w = 0;

    if (IS_SECONDARY_FLAG_SET(component, Component::HasScrollbarX) && 
      (component->sizingFlags & Component::ScrollIsPartOfPadding) == 0)
    {
      component->bounds.x += 2 * component->scrollWidths.max;
      if (component->bounds.w < utils::max_limit<i32>)
        component->bounds.w += 2 * component->scrollWidths.max;
    }
  }

  static void calculateFitSecondary(Component *component, utils::span<Component *> children)
  {
    i32 secondaryAxis = 0;

    for (auto *child : children)
    {
      calculateFitSecondary(child, child->childComponents);

      auto padding = TRANSPOSE(component, child->padding);
      auto margin = TRANSPOSE(component, child->margin);

      secondaryAxis = utils::max(secondaryAxis, 
        child->bounds.h + padding.y + padding.h + margin.y + margin.h);
    }

    if (component->sizingFlags & Component::HasText)
    {
      auto range = component->desiredSize.getTextDimensions(component, &component->bounds.w);
      component->bounds.y = utils::max(component->bounds.y, range.min);
      component->bounds.h = utils::max(component->bounds.h, range.max);
    }
    else
    {
      auto minMax = TRANSPOSE(component, component->desiredSize.minMax);
      component->bounds.y = utils::max(minMax.y, secondaryAxis);

      if (IS_SECONDARY_FLAG_SET(component, Component::GrowableX))
        component->bounds.h = utils::max_limit<i32>;
      else
      {
        COMPLEX_ASSERT(secondaryAxis <= minMax.h);
        component->bounds.h = utils::max(minMax.h, utils::max(component->bounds.h, secondaryAxis));
      }
    }

    // along the secondary axis SameAsSiblings acts like Growable
    if (IS_SECONDARY_FLAG_SET(component, Component::SameAsSiblingsX))
      component->bounds.h = utils::max_limit<i32>;

    if (IS_PRIMARY_FLAG_SET(component, Component::HasScrollbarX) &&
      (component->sizingFlags & Component::ScrollIsPartOfPadding) == 0)
    {
      component->bounds.y += 2 * component->scrollWidths.max;
      if (component->bounds.h < utils::max_limit<i32>)
        component->bounds.h += 2 * component->scrollWidths.max;
    }
  }

  static void calculateGrowPrimary(Component *component, utils::span<Component *> children)
  {
    if (children.size() == 0)
      return;

    auto sortedMin = utils::vector<Component *>{ localScratch, children.size() };
    auto sortedMax = utils::vector<Component *>{ localScratch, children.size() };

    auto padding = TRANSPOSE(component, component->padding);
    i32 minimumPrimarySize = padding.x + padding.w;
    for (auto &child : children)
    {
      auto margin = TRANSPOSE(component, child->margin);
      minimumPrimarySize += child->bounds.x + margin.x + margin.w;

      auto iter = utils::find_if(sortedMin, [&](const Interface::Component *c)
        { return c->bounds.x > child->bounds.x; });
      sortedMin.emplace(iter, child);

      iter = utils::find_if(sortedMax, [&](const Interface::Component *c)
        { return (c->bounds.w) > (child->bounds.w); });
      sortedMax.emplace(iter, child);
    }

    if (component->bounds.w < minimumPrimarySize &&
      IS_PRIMARY_FLAG_SET(component, Component::ScrollableX))
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
        if (sortedMax[j]->bounds.x <= biggestMinimum &&
          sortedMax[j]->bounds.w >= smallestMaximum)
        {
          i32 size = remaining / (i32)(componentCount);

          auto minMax = children[j]->desiredSize.minMax;
          sortedMax[j]->bounds.w = utils::clamp(size, minMax.x, minMax.w);

          remaining -= sortedMax[j]->bounds.w;
          --componentCount;
        }
      }
    }
    
    // free memory so the next recursive call can reuse it
    sortedMax.~vector();
    sortedMin.~vector();

    for (auto *child : children)
    {
      // scaling the final primary size
      child->bounds.w = scaleValueRoundInt((float)child->bounds.w);
      calculateGrowPrimary(child, child->childComponents);
    }
  }

  static void calculateGrowSecondary(Component *component, utils::span<Component *> children)
  {
    for (auto &child : children)
    {
      child->bounds.h = utils::min(child->bounds.h, component->bounds.h);
      // scaling the final secondary size
      child->bounds.h = scaleValueRoundInt((float)child->bounds.h);
    }

    if (component->sizingFlags & Component::IsVertical)
      COMPLEX_SWAP(component->bounds.w, component->bounds.h);

    for (auto *child : children)
      calculateGrowSecondary(child, child->childComponents);
  }

  void calculateSizes(utils::span<Component *> children, Component *component)
  {
    if (!component->componentFlags.isVisible)
      return;

    calculateFitPrimary(component, children);
    calculateGrowPrimary(component, children);
    calculateFitSecondary(component, children);
    calculateGrowSecondary(component, children);
  }

  extern utils::vector<Component *> *customPlacement;

  void calculatePositions(utils::span<Component *> children, 
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

    if (component->sizingFlags & Component::ScrollableX)
      boundsInTarget.x -= component->scrollOffset.x;
    if (component->sizingFlags & Component::ScrollableY)
      boundsInTarget.y -= component->scrollOffset.y;

    boundsInTarget = TRANSPOSE(component, boundsInTarget);
    padding = TRANSPOSE(component, padding);

    i32 primaryUsed = padding.getRight();
    i32 groupCount = 0;
    bool isInGroup = false;
    for (usize i = 0; i < children.size(); ++i)
    {
      auto &child = children[i];
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
      if (IS_SECONDARY_FLAG_SET(child, Placement::left))
        bounds.y = boundsInTarget.y + padding.y + margin.y;
      else if (IS_SECONDARY_FLAG_SET(child, Placement::right))
        bounds.y = boundsInTarget.h - padding.h - margin.h - bounds.h;
      else
        bounds.y = utils::max(boundsInTarget.y + padding.y + margin.y, 
          (boundsInTarget.h - padding.getBottom() - bounds.getBottom()) / 2);
      
      child->bounds.setPosition(TRANSPOSE(component, bounds.getPosition()));
    }

    groupCount = utils::max(1, groupCount);
    i32 sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / groupCount;

    for (usize i = 0; i < children.size();)
    {
      // skip if custom
      if (children[i]->placement & Placement::custom)
      {
        ++i;
        continue;
      }

      if (auto type = children[i]->placement & GET_PRIMARY_FLAG(component, Placement::justifyX);
        type == GET_PRIMARY_FLAG(component, Placement::justifyX) || type == 0)
      {
        usize j = 0;
        if (type == 0)
        {
          i32 offset = sizeLeftPerGroup / 2;
          boundsInTarget.x += offset;

          for (; (i + j) < children.size() && 
            (children[i + j]->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type; 
            ++j)
          {
            auto &child = children[i + j];
            auto bounds = TRANSPOSE(component, child->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            child->bounds = TRANSPOSE(component, bounds);
          }

          boundsInTarget.x += sizeLeftPerGroup - offset;
        }
        else
        {
          for (; (i + j) < children.size() && 
            (children[i + j]->placement & GET_PRIMARY_FLAG(component, Placement::justifyX)) == type; 
            ++j) { }

          i32 size = sizeLeftPerGroup;
          i32 offset = 0;

          for (usize k = 0; k < j; ++k)
          {
            boundsInTarget.x += offset;

            auto &child = children[i + k];
            auto bounds = TRANSPOSE(component, child->bounds);
            auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());

            bounds.x = boundsInTarget.x + margin.x;
            boundsInTarget.x += bounds.w + margin.getRight();

            child->bounds = TRANSPOSE(component, bounds);

            // there will be a division by zero here
            offset = size / (i32)(j - k - 1);
            size -= offset;
          }
        }

        --groupCount;
        primaryUsed += sizeLeftPerGroup;
        sizeLeftPerGroup = (boundsInTarget.w - primaryUsed) / groupCount;
        i += j;
      }
      else
      {
        auto &child = children[i];
        auto bounds = TRANSPOSE(component, child->bounds);
        auto margin = scaleValueRoundInt(TRANSPOSE(component, child->margin).toInt());

        if (IS_PRIMARY_FLAG_SET(child, Placement::left))
        {
          bounds.x = boundsInTarget.x + margin.x;
          boundsInTarget.x += bounds.w + margin.getRight();
        }
        else if (IS_PRIMARY_FLAG_SET(child, Placement::right))
        {
          bounds.x = boundsInTarget.getRight() - margin.w - bounds.w;
          boundsInTarget.w -= bounds.w + margin.getRight();
        }

        child->bounds = TRANSPOSE(component, bounds);
        ++i;
      }
    }

    for (auto &child : children)
      calculatePositions(child->childComponents, child);
  }

#undef TRANSPOSE
#undef IS_SECONDARY_FLAG_SET
#undef IS_PRIMARY_FLAG_SET
#undef GET_SECONDARY_FLAG
#undef GET_PRIMARY_FLAG

  void deleteComponent(Component *component)
  {
    if (component->componentFlags.isOpenGlInitialised)
    {
      component->componentFlags.destroyOpenGl = true;
      component->render(getOpenGlContext(uiRelated.renderer));
    }
    if (component->componentFlags.hasSummonnedPopupSelector)
    {
      auto *selector = getPopupSelector();
      if (selector->cancel)
        selector->cancel(selector);
      //selector->resetState();
    }
    component->deleteAllChildComponents();
    utils::bumpArena::remove(component);
  }

  Component *Component::getComponentAt(i32 x, i32 y)
  {
    if (componentFlags.clickableChildren)
    {
      for (usize i = childComponents.size(); i > 0; --i)
      {
        auto *child = childComponents[i - 1];
        if (child->componentFlags.isVisible && child->contains(Point{ x, y }))
        {
          auto *result = child->getComponentAt(x - child->bounds.x, y - child->bounds.y);
          if (result)
            return result;
        }
      }
    }

    if (!componentFlags.clickable)
      return this;
    return nullptr;
  }

  bool Component::hasFocus(bool trueIfChildIsFocused) const
  {
    auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused;
    if (!trueIfChildIsFocused && this != focusedComponent)
      return false;

    return isParentOf(focusedComponent);
  }

  static bool isShowing(Component *component)
  {
    while (component)
    {
      if (!component->componentFlags.isVisible)
        return false;

      component = component->parent;
    }

    return true;
  }

  bool grabFocusInternal(Component *component, Component *origin = nullptr)
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

    // shallow search
    for (usize i = component->childComponents.size(); i > 0; --i)
    {
      auto *child = component->childComponents[i - 1];
      if (component->componentFlags.wantsFocus && child != origin)
      {
        setFocusedComponent(uiRelated.renderer, child);
        if (focusedComponent)
          focusedComponent->handleFocus(false, Component::FocusGrabbed);

        child->handleFocus(true, Component::FocusGrabbed);
        return true;
      }
    }

    // deep search
    for (usize i = component->childComponents.size(); i > 0; --i)
    {
      auto *child = component->childComponents[i - 1];
      if (child != origin)
        if (grabFocusInternal(child))
          return true;
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

  void Component::addChildComponent(Component *child, i8 layer)
  {
    COMPLEX_ASSERT(utils::find(childComponents, child) == childComponents.end());

    auto iter = utils::find_if(childComponents, 
      [&](Component *element) { return element->layerIndex > layer; });
    childComponents.emplace(iter, child);

    child->parent = this;
    child->layerIndex = layer;
  }

  void Component::removeChildComponent(Component *childToRemove, bool keepFocus)
  {
    auto iter = utils::find(childComponents, childToRemove);
    if (iter == childComponents.end())
      return;

    childToRemove->parent = nullptr;
    childComponents.erase(iter);

    if (!keepFocus)
      childToRemove->giveAwayFocus();
  }

  Component *Component::removeChildComponent(usize childIndexToRemove, bool keepFocus)
  {
    COMPLEX_ASSERT(childIndexToRemove < childComponents.size());

    auto child = childComponents[childIndexToRemove];
    child->parent = nullptr;
    childComponents.erase(childComponents.begin() + childIndexToRemove);

    if (!keepFocus)
      child->giveAwayFocus();

    return child;
  }

  void Component::removeAllChildComponents()
  {
    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (usize i = childComponents.size(); i > 0; --i)
    {
      childComponents[i - 1]->parent = nullptr;
      childComponents.erase(childComponents.begin() + i - 1);
    }
  }

  void Component::deleteChildComponent(Component *childToDelete)
  {
    removeChildComponent(childToDelete);
    deleteComponent(childToDelete);
  }

  void Component::deleteChildComponent(usize childIndexToDelete)
  {
    auto childToDelete = removeChildComponent(childIndexToDelete);
    deleteComponent(childToDelete);
  }

  void Component::deleteAllChildComponents()
  {
    if (auto *focusedComponent = getMouseInteractions(uiRelated.renderer).focused)
      if (isParentOf(focusedComponent))
        focusedComponent->giveAwayFocus();

    for (usize i = childComponents.size(); i > 0; --i)
      deleteComponent(childComponents[i - 1]);
    
    childComponents.clear();
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

    if ((sizingFlags & Component::HasScrollbarX) && scrollableArea.w > bounds.w)
    {
      Rectangle<i32> scrollBounds{ scaledPadding.x, bounds.getBottom() - scaledPadding.h,
        bounds.w - scaledPadding.x - scaledPadding.w, scaledPadding.h };

      float length = utils::max((float)bounds.w / (float)scrollableArea.w, 0.1f) * (float)scrollBounds.w;
      float start = ((float)scrollOffset.x / (float)scrollableArea.w) * (float)scrollBounds.w;

      if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      {
        auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
        scrollBounds = scrollBounds.withShift(0, scrollWidth).withHeight(scrollWidth);
      }

      float width = (float)scrollBounds.x * calculateScrollWidth(scrollBounds, scrollWidths.x, xScrollBrightness);
      xScrollBounds = { (float)scrollBounds.x + start, scrollBounds.getBottom() - width, length, width };
    }

    if ((sizingFlags & Component::HasScrollbarY) && scrollableArea.h > bounds.h)
    {
      Rectangle<i32> scrollBounds{ bounds.getRight() - scaledPadding.w, scaledPadding.y,
        scaledPadding.w, bounds.h - scaledPadding.y - scaledPadding.h };

      float length = utils::max((float)bounds.h / (float)scrollableArea.h, 0.1f) * (float)scrollBounds.h;
      float start = ((float)scrollOffset.y / (float)scrollableArea.h) * (float)scrollBounds.h;

      if ((sizingFlags & Component::ScrollIsPartOfPadding) == 0)
      {
        auto scrollWidth = scaleValueRoundInt(getValue(Skin::kScrollHitboxWidth, false, this));
        scrollBounds = scrollBounds.withShift(scrollWidth, 0).withWidth(scrollWidth);
      }

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
      return;

    for (auto *child : childComponents)
    {
      if (child->componentFlags.isVisible)
        child->doRender(openGl);
    }

    nvgTranslate(openGl.g, (float)(-bounds.x), (float)(-bounds.y));
  }

  void BaseSection::addControl(BaseControl *control, bool addChild)
  {
    controls_.add(control->details.id, control);      

    control->addListener(this);

    if (addChild)
      addChildComponent(control);
  }

  void BaseSection::removeControl(BaseControl *control)
  {
    removeChildComponent(control);
    control->removeListener(this);

    controls_.erase(control->details.id);      
  }

  void ProcessorSection::setActivator(PowerButton *newActivator)
  {
    if (newActivator)
      newActivator->removeListener(this);

    activator = newActivator;

    if (newActivator)
    {
      newActivator->addListener(this);
      controlValueChanged(newActivator);
    }
  }
}
