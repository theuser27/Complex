/*
  ==============================================================================

    Viewport.cpp
    Created: 17 Dec 2023 4:37:11pm
    Author:  theuser27

  ==============================================================================
*/

#include "Viewport.hpp"

#include "ScrollBar.hpp"

namespace Interface
{
  static bool viewportWouldScrollOnEvent(const Viewport *vp, const MouseInputSource &src) noexcept
  {
    if (vp != nullptr)
    {
      switch (vp->getScrollOnDragMode())
      {
      case Viewport::ScrollOnDragMode::all:           return true;
      case Viewport::ScrollOnDragMode::nonHover:      return !src.canHover();
      case Viewport::ScrollOnDragMode::never:         return false;
      }
    }

    return false;
  }

  //==============================================================================
  Viewport::Viewport(const String &name) : BaseComponent(name)
  {
    // content holder is used to clip the contents so they don't overlap the scrollbars
    addAndMakeVisible(contentHolder);
    contentHolder.setInterceptsMouseClicks(false, true);
    contentHolder.addMouseListener(this, true);

    scrollBarThickness = 8;

    setInterceptsMouseClicks(false, true);
    setWantsKeyboardFocus(true);

    recreateScrollbars();
  }

  Viewport::~Viewport()
  {
    contentHolder.removeMouseListener(this);
    Desktop::getInstance().removeGlobalMouseListener(this);
    deleteOrRemoveContentComp();
  }

  //==============================================================================
  void Viewport::deleteOrRemoveContentComp()
  {
    if (contentComp != nullptr)
    {
      contentComp->removeComponentListener(this);

      if (deleteContent)
      {
        // This sets the content comp to a null pointer before deleting the old one, in case
        // anything tries to use the old one while it's in mid-deletion..
        auto component = contentComp.get();
        contentComp = nullptr;
        delete component;
      }
      else
      {
        contentHolder.removeChildComponent(contentComp);
        contentComp = nullptr;
      }
    }
  }

  void Viewport::setViewedComponent(BaseComponent *const newViewedComponent, const bool deleteComponentWhenNoLongerNeeded)
  {
    if (contentComp.get() != newViewedComponent)
    {
      deleteOrRemoveContentComp();
      contentComp = newViewedComponent;
      deleteContent = deleteComponentWhenNoLongerNeeded;

      if (contentComp != nullptr)
      {
        contentHolder.addAndMakeVisible(contentComp);
        setViewPosition(Point<int>());
        contentComp->addComponentListener(this);
      }

      updateVisibleArea();
    }
  }

  void Viewport::recreateScrollbars()
  {
    verticalScrollBar.reset();
    horizontalScrollBar.reset();

    verticalScrollBar = utils::up<ScrollBar>::create(true);
    verticalScrollBar->setViewport(this);
    horizontalScrollBar = utils::up<ScrollBar>::create(false);
    horizontalScrollBar->setViewport(this);

    addChildComponent(verticalScrollBar.get());
    addChildComponent(horizontalScrollBar.get());

    getVerticalScrollBar().addListener(this);
    getHorizontalScrollBar().addListener(this);

    resized();
  }

  bool Viewport::canScrollVertically() const noexcept { return contentComp->getY() < 0 || contentComp->getBottom() > getHeight(); }
  bool Viewport::canScrollHorizontally() const noexcept { return contentComp->getX() < 0 || contentComp->getRight() > getWidth(); }

  Point<int> Viewport::viewportPosToCompPos(Point<int> pos) const
  {
    jassert(contentComp != nullptr);

    auto contentBounds = contentHolder.getLocalArea(contentComp.get(), contentComp->getLocalBounds());

    Point<int> p(jmax(jmin(0, contentHolder.getWidth() - contentBounds.getWidth()), jmin(0, -(pos.x))),
      jmax(jmin(0, contentHolder.getHeight() - contentBounds.getHeight()), jmin(0, -(pos.y))));

    return p;
  }

  void Viewport::setViewPosition(const int xPixelsOffset, const int yPixelsOffset)
  {
    setViewPosition({ xPixelsOffset, yPixelsOffset });
  }

  void Viewport::setViewPosition(Point<int> newPosition)
  {
    if (contentComp != nullptr)
      contentComp->setTopLeftPosition(viewportPosToCompPos(newPosition));
  }

  void Viewport::setViewPositionProportionately(const double x, const double y)
  {
    if (contentComp != nullptr)
      setViewPosition(jmax(0, roundToInt(x * (contentComp->getWidth() - getWidth()))),
        jmax(0, roundToInt(y * (contentComp->getHeight() - getHeight()))));
  }

  bool Viewport::autoScroll(const int mouseX, const int mouseY, const int activeBorderThickness, const int maximumSpeed)
  {
    if (contentComp != nullptr)
    {
      int dx = 0, dy = 0;

      if (getHorizontalScrollBar().isVisible() || canScrollHorizontally())
      {
        if (mouseX < activeBorderThickness)
          dx = activeBorderThickness - mouseX;
        else if (mouseX >= contentHolder.getWidth() - activeBorderThickness)
          dx = (contentHolder.getWidth() - activeBorderThickness) - mouseX;

        if (dx < 0)
          dx = jmax(dx, -maximumSpeed, contentHolder.getWidth() - contentComp->getRight());
        else
          dx = jmin(dx, maximumSpeed, -contentComp->getX());
      }

      if (getVerticalScrollBar().isVisible() || canScrollVertically())
      {
        if (mouseY < activeBorderThickness)
          dy = activeBorderThickness - mouseY;
        else if (mouseY >= contentHolder.getHeight() - activeBorderThickness)
          dy = (contentHolder.getHeight() - activeBorderThickness) - mouseY;

        if (dy < 0)
          dy = jmax(dy, -maximumSpeed, contentHolder.getHeight() - contentComp->getBottom());
        else
          dy = jmin(dy, maximumSpeed, -contentComp->getY());
      }

      if (dx != 0 || dy != 0)
      {
        contentComp->setTopLeftPosition(contentComp->getX() + dx,
          contentComp->getY() + dy);

        return true;
      }
    }

    return false;
  }

  void Viewport::componentMovedOrResized(Component &, bool, bool)
  {
    updateVisibleArea();
  }

  void Viewport::resized()
  {
    updateVisibleArea();
  }

  //==============================================================================
  void Viewport::updateVisibleArea()
  {
    if (!isVisible())
      return;

    auto scrollbarWidth = getScrollBarThickness();
    const bool canShowAnyBars = getWidth() > scrollbarWidth && getHeight() > scrollbarWidth;
    const bool canShowHBar = showHScrollbar && canShowAnyBars;
    const bool canShowVBar = showVScrollbar && canShowAnyBars;

    bool hBarVisible = false, vBarVisible = false;
    juce::Rectangle<int> contentArea;

    for (int i = 3; --i >= 0;)
    {
      hBarVisible = canShowHBar && !getHorizontalScrollBar().autoHides();
      vBarVisible = canShowVBar && !getVerticalScrollBar().autoHides();
      contentArea = getLocalBounds();

      if (contentComp != nullptr && !contentArea.contains(contentComp->getBounds()))
      {
        hBarVisible = canShowHBar && (hBarVisible || contentComp->getX() < 0 || contentComp->getRight() > contentArea.getWidth());
        vBarVisible = canShowVBar && (vBarVisible || contentComp->getY() < 0 || contentComp->getBottom() > contentArea.getHeight());

        if (vBarVisible)
          contentArea.setWidth(getWidth() - scrollbarWidth);

        if (hBarVisible)
          contentArea.setHeight(getHeight() - scrollbarWidth);

        if (!contentArea.contains(contentComp->getBounds()))
        {
          hBarVisible = canShowHBar && (hBarVisible || contentComp->getRight() > contentArea.getWidth());
          vBarVisible = canShowVBar && (vBarVisible || contentComp->getBottom() > contentArea.getHeight());
        }
      }

      if (vBarVisible)  contentArea.setWidth(getWidth() - scrollbarWidth);
      if (hBarVisible)  contentArea.setHeight(getHeight() - scrollbarWidth);

      if (!vScrollbarRight && vBarVisible)
        contentArea.setX(scrollbarWidth);

      if (!hScrollbarBottom && hBarVisible)
        contentArea.setY(scrollbarWidth);

      if (contentComp == nullptr)
      {
        contentHolder.setBounds(contentArea);
        break;
      }

      auto oldContentBounds = contentComp->getBounds();
      contentHolder.setBounds(contentArea);

      // If the content has changed its size, that might affect our scrollbars, so go round again and re-calculate..
      if (oldContentBounds == contentComp->getBounds())
        break;
    }

    juce::Rectangle<int> contentBounds;

    if (auto cc = contentComp.get())
      contentBounds = contentHolder.getLocalArea(cc, cc->getLocalBounds());

    auto visibleOrigin = -contentBounds.getPosition();

    auto &hbar = getHorizontalScrollBar();
    auto &vbar = getVerticalScrollBar();

    hbar.setBounds(contentArea.getX(), hScrollbarBottom ? contentArea.getHeight() : 0, contentArea.getWidth(), scrollbarWidth);
    hbar.setRangeLimits(0.0, contentBounds.getWidth());
    hbar.setCurrentRange(visibleOrigin.x, contentArea.getWidth());
    hbar.setSingleStepSize(singleStepX);

    if (canShowHBar && !hBarVisible)
      visibleOrigin.setX(0);

    vbar.setBounds(vScrollbarRight ? contentArea.getWidth() : 0, contentArea.getY(), scrollbarWidth, contentArea.getHeight());
    vbar.setRangeLimits(0.0, contentBounds.getHeight());
    vbar.setCurrentRange(visibleOrigin.y, contentArea.getHeight());
    vbar.setSingleStepSize(singleStepY);

    if (canShowVBar && !vBarVisible)
      visibleOrigin.setY(0);

    // Force the visibility *after* setting the ranges to avoid flicker caused by edge conditions in the numbers.
    hbar.setVisible(hBarVisible);
    vbar.setVisible(vBarVisible);

    if (contentComp != nullptr)
    {
      auto newContentCompPos = viewportPosToCompPos(visibleOrigin);

      if (contentComp->getBounds().getPosition() != newContentCompPos)
      {
        contentComp->setTopLeftPosition(newContentCompPos);  // (this will re-entrantly call updateVisibleArea again)
        return;
      }
    }

    const juce::Rectangle<int> visibleArea(visibleOrigin.x, visibleOrigin.y,
      jmin(contentBounds.getWidth() - visibleOrigin.x, contentArea.getWidth()),
      jmin(contentBounds.getHeight() - visibleOrigin.y, contentArea.getHeight()));

    if (lastVisibleArea != visibleArea)
    {
      lastVisibleArea = visibleArea;
      for (auto *listener : listeners)
        listener->visibleAreaChanged(visibleArea.getX(), visibleArea.getY(), visibleArea.getWidth(), visibleArea.getHeight());
    }

    hbar.handleUpdateNowIfNeeded();
    vbar.handleUpdateNowIfNeeded();
  }

  //==============================================================================
  void Viewport::setSingleStepSizes(const int stepX, const int stepY)
  {
    if (singleStepX != stepX || singleStepY != stepY)
    {
      singleStepX = stepX;
      singleStepY = stepY;
      updateVisibleArea();
    }
  }

  void Viewport::setScrollBarsShown(const bool showVerticalScrollbarIfNeeded,
    const bool showHorizontalScrollbarIfNeeded,
    const bool allowVerticalScrollingWithoutScrollbar,
    const bool allowHorizontalScrollingWithoutScrollbar)
  {
    allowScrollingWithoutScrollbarV = allowVerticalScrollingWithoutScrollbar;
    allowScrollingWithoutScrollbarH = allowHorizontalScrollingWithoutScrollbar;

    if (showVScrollbar != showVerticalScrollbarIfNeeded
      || showHScrollbar != showHorizontalScrollbarIfNeeded)
    {
      showVScrollbar = showVerticalScrollbarIfNeeded;
      showHScrollbar = showHorizontalScrollbarIfNeeded;
      updateVisibleArea();
    }
  }

  void Viewport::setScrollBarThickness(const int thickness)
  {
    COMPLEX_ASSERT(thickness > 0);

    if (scrollBarThickness != thickness)
    {
      scrollBarThickness = thickness;
      updateVisibleArea();
    }
  }

  void Viewport::scrollBarMoved(ScrollBar *scrollBarThatHasMoved, double newRangeStart)
  {
    auto newRangeStartInt = roundToInt(newRangeStart);

    if (scrollBarThatHasMoved == horizontalScrollBar.get())
    {
      setViewPosition(newRangeStartInt, getViewPositionY());
    }
    else if (scrollBarThatHasMoved == verticalScrollBar.get())
    {
      setViewPosition(getViewPositionX(), newRangeStartInt);
    }
  }

  void Viewport::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    // it's possible that viewports might be nested
    if (!needsToRedirectMouse(e))
      useMouseWheelMoveIfNeeded(e, wheel);
    else if (!redirectMouse(RedirectMouseWheel, e, &wheel))
      BaseComponent::mouseWheelMove(e, wheel);
  }

  void Viewport::mouseDown(const MouseEvent &e)
  {
    if (!isGlobalMouseListener && viewportWouldScrollOnEvent(this, e.source))
    {
      // switch to a global mouse listener so we still receive mouseUp events
      // if the original event component is deleted
      contentHolder.removeMouseListener(this);
      Desktop::getInstance().addGlobalMouseListener(this);

      isGlobalMouseListener = true;

      scrollSource = e.source;
    }
  }

  void Viewport::mouseDrag(const MouseEvent &e)
  {
    if (e.source != scrollSource)
      return;

    auto totalOffset = e.getEventRelativeTo(this).getOffsetFromDragStart();

    if (!isDragging && totalOffset.toFloat().getDistanceFromOrigin() > 8.0f && viewportWouldScrollOnEvent(this, e.source))
      isDragging = true;

    if (isDragging)
      setViewPosition(getViewPosition() - totalOffset);
  }

  void Viewport::mouseUp(const MouseEvent &e)
  {
    if (!isGlobalMouseListener || e.source != scrollSource)
      return;

    isDragging = false;

    contentHolder.addMouseListener(this, true);
    Desktop::getInstance().removeGlobalMouseListener(this);

    isGlobalMouseListener = false;
  }

  static int rescaleMouseWheelDistance(float distance, int singleStepSize) noexcept
  {
    if (distance == 0.0f)
      return 0;

    distance *= 14.0f * (float)singleStepSize;

    return roundToInt(distance < 0 ? jmin(distance, -1.0f)
      : jmax(distance, 1.0f));
  }

  bool Viewport::useMouseWheelMoveIfNeeded(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    if (e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown())
      return false;

    const bool canScrollVert = (allowScrollingWithoutScrollbarV || getVerticalScrollBar().isVisible());
    const bool canScrollHorz = (allowScrollingWithoutScrollbarH || getHorizontalScrollBar().isVisible());

    if (!canScrollHorz && !canScrollVert)
      return false;

    auto deltaX = rescaleMouseWheelDistance(wheel.deltaX, singleStepX);
    auto deltaY = rescaleMouseWheelDistance(wheel.deltaY, singleStepY);

    auto pos = getViewPosition();

    if (deltaX != 0 && deltaY != 0 && canScrollHorz && canScrollVert)
    {
      pos.x -= deltaX;
      pos.y -= deltaY;
    }
    else if (canScrollHorz && (deltaX != 0 || e.mods.isShiftDown() || !canScrollVert))
    {
      pos.x -= deltaX != 0 ? deltaX : deltaY;
    }
    else if (canScrollVert && deltaY != 0)
    {
      pos.y -= deltaY;
    }

    if (pos != getViewPosition())
    {
      setViewPosition(pos);
      return true;
    }

    return false;
  }

  static bool isUpDownKeyPress(const KeyPress &key)
  {
    return key == KeyPress::upKey
      || key == KeyPress::downKey
      || key == KeyPress::pageUpKey
      || key == KeyPress::pageDownKey
      || key == KeyPress::homeKey
      || key == KeyPress::endKey;
  }

  static bool isLeftRightKeyPress(const KeyPress &key)
  {
    return key == KeyPress::leftKey
      || key == KeyPress::rightKey;
  }

  bool Viewport::keyPressed(const KeyPress &key)
  {
    const bool isUpDownKey = isUpDownKeyPress(key);

    if (getVerticalScrollBar().isVisible() && isUpDownKey)
      return getVerticalScrollBar().keyPressed(key);

    const bool isLeftRightKey = isLeftRightKeyPress(key);

    if (getHorizontalScrollBar().isVisible() && (isUpDownKey || isLeftRightKey))
      return getHorizontalScrollBar().keyPressed(key);

    return false;
  }

  void Viewport::setScrollBarPosition(bool verticalScrollbarOnRight,
    bool horizontalScrollbarAtBottom)
  {
    vScrollbarRight = verticalScrollbarOnRight;
    hScrollbarBottom = horizontalScrollbarAtBottom;

    resized();
  }
}
