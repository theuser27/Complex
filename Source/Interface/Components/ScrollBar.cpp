/*
  ==============================================================================

    ScrollBar.cpp
    Created: 15 Dec 2023 9:19:13pm
    Author:  theuser27

  ==============================================================================
*/

#include "ScrollBar.hpp"

#include "Viewport.hpp"

namespace Interface
{
  ScrollBar::ScrollBar(bool isVertical) : isVertical_(isVertical)
  {
    bar_.setScrollBar(this);
    addOpenGlComponent(&bar_);
  }

  ScrollBar::~ScrollBar() = default;

  //==============================================================================
  void ScrollBar::setRangeLimits(Range<double> newRangeLimit, NotificationType notification)
  {
    if (totalRange != newRangeLimit)
    {
      totalRange = newRangeLimit;
      setCurrentRange(visibleRange, notification);
      updateThumbPosition();
    }
  }

  void ScrollBar::setRangeLimits(double newMinimum, double newMaximum, NotificationType notification)
  {
    jassert(newMaximum >= newMinimum); // these can't be the wrong way round!
    setRangeLimits(Range<double>(newMinimum, newMaximum), notification);
  }

  bool ScrollBar::setCurrentRange(Range<double> newRange, NotificationType notification)
  {
    auto constrainedRange = totalRange.get().constrainRange(newRange);

    if (visibleRange != constrainedRange)
    {
      visibleRange = constrainedRange;

      updateThumbPosition();

      if (notification != dontSendNotification)
        triggerAsyncUpdate();

      if (notification == sendNotificationSync)
        handleUpdateNowIfNeeded();

      return true;
    }

    return false;
  }

  void ScrollBar::setCurrentRange(double newStart, double newSize, NotificationType notification)
  {
    setCurrentRange(Range<double>(newStart, newStart + newSize), notification);
  }

  void ScrollBar::setCurrentRangeStart(double newStart, NotificationType notification)
  {
    setCurrentRange(visibleRange.get().movedToStartAt(newStart), notification);
  }

  bool ScrollBar::moveScrollbarInSteps(int howManySteps, NotificationType notification)
  {
    return setCurrentRange(visibleRange.get() + howManySteps * singleStepSize, notification);
  }

  bool ScrollBar::moveScrollbarInPages(int howManyPages, NotificationType notification)
  {
    return setCurrentRange(visibleRange.get() + howManyPages * visibleRange.get().getLength(), notification);
  }

  bool ScrollBar::scrollToTop(NotificationType notification)
  {
    return setCurrentRange(visibleRange.get().movedToStartAt(getMinimumRangeLimit()), notification);
  }

  bool ScrollBar::scrollToBottom(NotificationType notification)
  {
    return setCurrentRange(visibleRange.get().movedToEndAt(getMaximumRangeLimit()), notification);
  }

  void ScrollBar::setColor(Colour color) { color_ = color; bar_.setColor(color); }
  void ScrollBar::setShrinkLeft(bool shrink_left) { bar_.setShrinkLeft(shrink_left); }

  //==============================================================================
  void ScrollBar::updateThumbPosition()
  {
    int minimumScrollBarThumbSize = std::min(getWidth(), getHeight()) * 2;

    int newThumbSize = roundToInt((totalRange.get().getLength() <= 0) ? thumbAreaSize :
      (visibleRange.get().getLength() * thumbAreaSize) / totalRange.get().getLength());

    if (newThumbSize < minimumScrollBarThumbSize)
      newThumbSize = jmin(minimumScrollBarThumbSize, thumbAreaSize - 1);

    if (newThumbSize > thumbAreaSize)
      newThumbSize = thumbAreaSize;
    thumbSize = newThumbSize;

    if (totalRange.get().getLength() > visibleRange.get().getLength())
      thumbStart = roundToInt((visibleRange.get().getStart() - totalRange.get().getStart()) *
        (thumbAreaSize - newThumbSize) / (totalRange.get().getLength() - visibleRange.get().getLength()));

    BaseComponent::setVisible(getVisibility());
  }

  void ScrollBar::setOrientation(bool shouldBeVertical)
  {
    if (isVertical_ != shouldBeVertical)
    {
      isVertical_ = shouldBeVertical;
      updateThumbPosition();
    }
  }

  void ScrollBar::setAutoHide(bool shouldHideWhenFullRange)
  {
    autohides = shouldHideWhenFullRange;
    updateThumbPosition();
  }

  //==============================================================================

  void ScrollBar::resized()
  {
    thumbAreaSize = isVertical_ ? getHeight() : getWidth();

    updateThumbPosition();

    bar_.setBounds(renderInsets_.getRight(), renderInsets_.getTop(), 
      getWidth() - renderInsets_.getLeftAndRight(), getHeight() - renderInsets_.getTopAndBottom());
    bar_.setRounding((float)getWidth() * 0.25f);
  }

  void ScrollBar::mouseEnter(const MouseEvent &)
  {
    bar_.getAnimator().setIsHovered(true);
  }

  void ScrollBar::mouseDown(const MouseEvent &e)
  {
    isDraggingThumb = false;
    lastMousePos = isVertical_ ? e.y : e.x;
    dragStartMousePos = lastMousePos;

    if (dragStartMousePos < thumbStart || dragStartMousePos >= thumbStart + thumbSize)
    {
      double normalised = (double)dragStartMousePos / (double)thumbAreaSize;
      setCurrentRangeStart(normalised * totalRange.get().getLength());
      isDraggingThumb = true;
    }
    else
    {
      isDraggingThumb = (thumbAreaSize > (std::min(getWidth(), getHeight()) * 2))
        && (thumbAreaSize > thumbSize);
    }

    dragStartRange = visibleRange.get().getStart();
    bar_.setColor(color_.overlaidWith(color_));
  }

  void ScrollBar::mouseDrag(const MouseEvent &e)
  {
    auto mousePos = isVertical_ ? e.y : e.x;

    if (isDraggingThumb && lastMousePos != mousePos && thumbAreaSize > thumbSize)
    {
      auto deltaPixels = mousePos - dragStartMousePos;

      setCurrentRangeStart(dragStartRange + deltaPixels * 
        (totalRange.get().getLength() - visibleRange.get().getLength()) / (thumbAreaSize - thumbSize));
    }

    lastMousePos = mousePos;
  }

  void ScrollBar::mouseUp(const MouseEvent &)
  {
    isDraggingThumb = false;
    bar_.setColor(color_);
  }

  void ScrollBar::mouseExit(const MouseEvent &)
  {
    bar_.getAnimator().setIsHovered(false);
  }

  void ScrollBar::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    if (viewport_)
      viewport_->mouseWheelMove(e, wheel);
    else
      BaseComponent::mouseWheelMove(e, wheel);
  }

  void ScrollBar::handleAsyncUpdate()
  {
    auto start = visibleRange.get().getStart(); // (need to use a temp variable for VC7 compatibility)
    listeners.call([this, start](OpenGlScrollBarListener &l) { l.scrollBarMoved(this, start); });
  }

  bool ScrollBar::keyPressed(const KeyPress &key)
  {
    if (isVisible())
    {
      if (key == KeyPress::upKey || key == KeyPress::leftKey)    return moveScrollbarInSteps(-1);
      if (key == KeyPress::downKey || key == KeyPress::rightKey) return moveScrollbarInSteps(1);
      if (key == KeyPress::pageUpKey)                            return moveScrollbarInPages(-1);
      if (key == KeyPress::pageDownKey)                          return moveScrollbarInPages(1);
      if (key == KeyPress::homeKey)                              return scrollToTop();
      if (key == KeyPress::endKey)                               return scrollToBottom();
    }

    return false;
  }

  void ScrollBar::setVisible(bool shouldBeVisible)
  {
    if (userVisibilityFlag != shouldBeVisible)
    {
      userVisibilityFlag = shouldBeVisible;
      BaseComponent::setVisible(getVisibility());
    }
  }

  bool ScrollBar::getVisibility() const noexcept
  {
    if (!userVisibilityFlag)
      return false;

    return (!autohides) || (totalRange.get().getLength() > visibleRange.get().getLength()
      && visibleRange.get().getLength() > 0.0);
  }
}
