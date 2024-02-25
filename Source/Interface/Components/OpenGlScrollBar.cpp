/*
	==============================================================================

		OpenGlScrollBar.cpp
		Created: 15 Dec 2023 9:19:13pm
		Author:  theuser27

	==============================================================================
*/

#include "OpenGlScrollBar.h"
#include "OpenGlViewport.h"

namespace Interface
{
	void OpenGlScrollQuad::render(OpenGlWrapper &openGl, bool animate)
	{
		float lastHover = hoverAmount_;
		animator_.tick();

		if (!scrollBar_->isVisibleSafe())
			return;

		hoverAmount_ = animator_.getValue(Animator::Hover);

		if (lastHover != hoverAmount_)
		{
			if (shrinkLeft_)
				setQuadHorizontal(0, -1.0f, 1.0f + hoverAmount_);
			else
				setQuadHorizontal(0, 0.0f - hoverAmount_, 1.0f + hoverAmount_);
		}

		Range<double> range = scrollBar_->getCurrentRange();
		Range<double> totalRange = scrollBar_->getRangeLimit();
		double startRatio = (range.getStart() - totalRange.getStart()) / totalRange.getLength();
		double endRatio = (range.getEnd() - totalRange.getStart()) / totalRange.getLength();
		setQuadVertical(0, 1.0f - 2.0f * (float)endRatio, 2.0f * (float)(endRatio - startRatio));

		OpenGlMultiQuad::render(openGl, animate);
	}

	//==============================================================================
	OpenGlScrollBar::OpenGlScrollBar(bool isVertical) : isVertical_(isVertical)
	{
		bar_ = makeOpenGlComponent<OpenGlScrollQuad>();
		bar_->setTargetComponent(this);
		addAndMakeVisible(bar_.get());
		bar_->setScrollBar(this);
	}

	//==============================================================================
	void OpenGlScrollBar::setRangeLimits(Range<double> newRangeLimit, NotificationType notification)
	{
		if (totalRange != newRangeLimit)
		{
			totalRange = newRangeLimit;
			setCurrentRange(visibleRange, notification);
			updateThumbPosition();
		}
	}

	void OpenGlScrollBar::setRangeLimits(double newMinimum, double newMaximum, NotificationType notification)
	{
		jassert(newMaximum >= newMinimum); // these can't be the wrong way round!
		setRangeLimits(Range<double>(newMinimum, newMaximum), notification);
	}

	bool OpenGlScrollBar::setCurrentRange(Range<double> newRange, NotificationType notification)
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

	void OpenGlScrollBar::setCurrentRange(double newStart, double newSize, NotificationType notification)
	{
		setCurrentRange(Range<double>(newStart, newStart + newSize), notification);
	}

	void OpenGlScrollBar::setCurrentRangeStart(double newStart, NotificationType notification)
	{
		setCurrentRange(visibleRange.get().movedToStartAt(newStart), notification);
	}

	bool OpenGlScrollBar::moveScrollbarInSteps(int howManySteps, NotificationType notification)
	{
		return setCurrentRange(visibleRange.get() + howManySteps * singleStepSize, notification);
	}

	bool OpenGlScrollBar::moveScrollbarInPages(int howManyPages, NotificationType notification)
	{
		return setCurrentRange(visibleRange.get() + howManyPages * visibleRange.get().getLength(), notification);
	}

	bool OpenGlScrollBar::scrollToTop(NotificationType notification)
	{
		return setCurrentRange(visibleRange.get().movedToStartAt(getMinimumRangeLimit()), notification);
	}

	bool OpenGlScrollBar::scrollToBottom(NotificationType notification)
	{
		return setCurrentRange(visibleRange.get().movedToEndAt(getMaximumRangeLimit()), notification);
	}

	//==============================================================================
	void OpenGlScrollBar::updateThumbPosition()
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

	void OpenGlScrollBar::setOrientation(bool shouldBeVertical)
	{
		if (isVertical_ != shouldBeVertical)
		{
			isVertical_ = shouldBeVertical;
			updateThumbPosition();
		}
	}

	void OpenGlScrollBar::setAutoHide(bool shouldHideWhenFullRange)
	{
		autohides = shouldHideWhenFullRange;
		updateThumbPosition();
	}

	//==============================================================================

	void OpenGlScrollBar::resized()
	{
		thumbAreaSize = isVertical_ ? getHeight() : getWidth();

		updateThumbPosition();

		bar_->setBounds(getLocalBounds());
		bar_->setRounding((float)getWidth() * 0.25f);
	}

	void OpenGlScrollBar::mouseEnter(const MouseEvent &)
	{
		bar_->getAnimator().setIsHovered(true);
	}

	void OpenGlScrollBar::mouseDown(const MouseEvent &e)
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
		bar_->setColor(color_.overlaidWith(color_));
	}

	void OpenGlScrollBar::mouseDrag(const MouseEvent &e)
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

	void OpenGlScrollBar::mouseUp(const MouseEvent &)
	{
		isDraggingThumb = false;
		bar_->setColor(color_);
	}

	void OpenGlScrollBar::mouseExit(const MouseEvent &)
	{
		bar_->getAnimator().setIsHovered(false);
	}

	void OpenGlScrollBar::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
	{
		if (viewport_)
			viewport_->mouseWheelMove(e, wheel);
		else
			BaseComponent::mouseWheelMove(e, wheel);
	}

	void OpenGlScrollBar::handleAsyncUpdate()
	{
		auto start = visibleRange.get().getStart(); // (need to use a temp variable for VC7 compatibility)
		listeners.call([this, start](OpenGlScrollBarListener &l) { l.scrollBarMoved(this, start); });
	}

	bool OpenGlScrollBar::keyPressed(const KeyPress &key)
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

	void OpenGlScrollBar::setVisible(bool shouldBeVisible)
	{
		if (userVisibilityFlag != shouldBeVisible)
		{
			userVisibilityFlag = shouldBeVisible;
			BaseComponent::setVisible(getVisibility());
		}
	}

	bool OpenGlScrollBar::getVisibility() const noexcept
	{
		if (!userVisibilityFlag)
			return false;

		return (!autohides) || (totalRange.get().getLength() > visibleRange.get().getLength()
			&& visibleRange.get().getLength() > 0.0);
	}
}
