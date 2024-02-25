/*
	==============================================================================

		OpenGlScrollBar.h
		Created: 15 Dec 2023 9:19:13pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Interface/LookAndFeel/Miscellaneous.h"
#include "OpenGlMultiQuad.h"

namespace Interface
{
	class OpenGlScrollBar;
	class OpenGlViewport;

	class OpenGlScrollQuad final : public OpenGlMultiQuad
	{
	public:
		static constexpr float kHoverChange = 0.2f;

		OpenGlScrollQuad() : OpenGlMultiQuad(1, Shaders::kRoundedRectangleFragment, typeid(OpenGlScrollQuad).name())
		{
			setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
			animator_.setHoverIncrement(kHoverChange);
		}

		void render(OpenGlWrapper &openGl, bool animate) override;

		void setShrinkLeft(bool shrinkLeft) { shrinkLeft_ = shrinkLeft; }
		void setScrollBar(OpenGlScrollBar *scrollBar) { scrollBar_ = scrollBar; }

	private:
		OpenGlScrollBar *scrollBar_ = nullptr;
		shared_value<bool> shrinkLeft_ = false;
		float hoverAmount_ = -1.0f;

		friend class OpenGlScrollBar;
	};

	// scrollbar with base implementation taken from juce's
	class OpenGlScrollBar final : public BaseComponent, public AsyncUpdater
	{
	public:
		OpenGlScrollBar(bool isVertical);
		~OpenGlScrollBar() override = default;

		void resized() override;
		void mouseEnter(const MouseEvent &) override;
		void mouseDown(const MouseEvent &) override;
		void mouseDrag(const MouseEvent &) override;
		void mouseUp(const MouseEvent &) override;
		void mouseExit(const MouseEvent &) override;
		void mouseWheelMove(const MouseEvent &, const MouseWheelDetails &) override;
		bool keyPressed(const KeyPress &) override;
		void setVisible(bool) override;

		bool isVertical() const noexcept { return isVertical_; }
		bool autoHides() const noexcept { return autohides; }

		void setOrientation(bool shouldBeVertical);
		void setAutoHide(bool shouldHideWhenFullRange);

		//==============================================================================
		/** Sets the minimum and maximum values that the bar will move between.

				The bar's thumb will always be constrained so that the entire thumb lies
				within this range.

				@param newRangeLimit    the new range.
				@param notification     whether to send a notification of the change to listeners.
																A notification will only be sent if the range has changed.

				@see setCurrentRange
		*/
		void setRangeLimits(Range<double> newRangeLimit,
			NotificationType notification = sendNotificationAsync);

		/** Sets the minimum and maximum values that the bar will move between.

				The bar's thumb will always be constrained so that the entire thumb lies
				within this range.

				@param minimum         the new range minimum.
				@param maximum         the new range maximum.
				@param notification    whether to send a notification of the change to listeners.
															 A notification will only be sent if the range has changed.

				@see setCurrentRange
		*/
		void setRangeLimits(double minimum, double maximum,
			NotificationType notification = sendNotificationAsync);

		/** Returns the current limits on the thumb position.
				@see setRangeLimits
		*/
		Range<double> getRangeLimit() const noexcept { return totalRange; }

		/** Returns the lower value that the thumb can be set to.

				This is the value set by setRangeLimits().
		*/
		double getMinimumRangeLimit() const noexcept { return totalRange.get().getStart(); }

		/** Returns the upper value that the thumb can be set to.

				This is the value set by setRangeLimits().
		*/
		double getMaximumRangeLimit() const noexcept { return totalRange.get().getEnd(); }

		//==============================================================================
		/** Changes the position of the scrollbar's 'thumb'.

				This sets both the position and size of the thumb - to just set the position without
				changing the size, you can use setCurrentRangeStart().

				If this method call actually changes the scrollbar's position, it will trigger an
				asynchronous call to ScrollBar::Listener::scrollBarMoved() for all the listeners that
				are registered.

				The notification parameter can be used to optionally send or inhibit a callback to
				any scrollbar listeners.

				@returns true if the range was changed, or false if nothing was changed.
				@see getCurrentRange. setCurrentRangeStart
		*/
		bool setCurrentRange(Range<double> newRange,
			NotificationType notification = sendNotificationAsync);

		/** Changes the position of the scrollbar's 'thumb'.

				This sets both the position and size of the thumb - to just set the position without
				changing the size, you can use setCurrentRangeStart().

				@param newStart     the top (or left) of the thumb, in the range
														getMinimumRangeLimit() <= newStart <= getMaximumRangeLimit(). If the
														value is beyond these limits, it will be clipped.
				@param newSize      the size of the thumb, such that
														getMinimumRangeLimit() <= newStart + newSize <= getMaximumRangeLimit(). If the
														size is beyond these limits, it will be clipped.
				@param notification specifies if and how a callback should be made to any listeners
														if the range actually changes
				@see setCurrentRangeStart, getCurrentRangeStart, getCurrentRangeSize
		*/
		void setCurrentRange(double newStart, double newSize,
			NotificationType notification = sendNotificationAsync);

		/** Moves the bar's thumb position.

				This will move the thumb position without changing the thumb size. Note
				that the maximum thumb start position is (getMaximumRangeLimit() - getCurrentRangeSize()).

				If this method call actually changes the scrollbar's position, it will trigger an
				asynchronous call to ScrollBar::Listener::scrollBarMoved() for all the listeners that
				are registered.

				@see setCurrentRange
		*/
		void setCurrentRangeStart(double newStart,
			NotificationType notification = sendNotificationAsync);

		/** Returns the current thumb range.
				@see getCurrentRange, setCurrentRange
		*/
		Range<double> getCurrentRange() const noexcept { return visibleRange; }

		/** Returns the position of the top of the thumb.
				@see getCurrentRange, setCurrentRangeStart
		*/
		double getCurrentRangeStart() const noexcept { return visibleRange.get().getStart(); }

		/** Returns the current size of the thumb.
				@see getCurrentRange, setCurrentRange
		*/
		double getCurrentRangeSize() const noexcept { return visibleRange.get().getLength(); }

		//==============================================================================
		/** Sets the amount by which the up and down buttons will move the bar.

				The value here is in terms of the total range, and is added or subtracted
				from the thumb position when the user clicks an up/down (or left/right) button.
		*/
		void setSingleStepSize(double newSingleStepSize) noexcept { singleStepSize = newSingleStepSize; }

		/** Returns the current step size.
				@see setSingleStepSize
		*/
		double getSingleStepSize() const noexcept { return singleStepSize; }

		/** Moves the scrollbar by a number of single-steps.

				This will move the bar by a multiple of its single-step interval (as
				specified using the setSingleStepSize() method).

				A positive value here will move the bar down or to the right, a negative
				value moves it up or to the left.

				@param howManySteps    the number of steps to move the scrollbar
				@param notification    whether to send a notification of the change to listeners.
															 A notification will only be sent if the position has changed.

				@returns true if the scrollbar's position actually changed.
		*/
		bool moveScrollbarInSteps(int howManySteps,
			NotificationType notification = sendNotificationAsync);

		/** Moves the scroll bar up or down in pages.

				This will move the bar by a multiple of its current thumb size, effectively
				doing a page-up or down.

				A positive value here will move the bar down or to the right, a negative
				value moves it up or to the left.

				@param howManyPages    the number of pages to move the scrollbar
				@param notification    whether to send a notification of the change to listeners.
															 A notification will only be sent if the position has changed.

				@returns true if the scrollbar's position actually changed.
		*/
		bool moveScrollbarInPages(int howManyPages,
			NotificationType notification = sendNotificationAsync);

		/** Scrolls to the top (or left).
				This is the same as calling setCurrentRangeStart (getMinimumRangeLimit());

				@param notification    whether to send a notification of the change to listeners.
															 A notification will only be sent if the position has changed.

				@returns true if the scrollbar's position actually changed.
		*/
		bool scrollToTop(NotificationType notification = sendNotificationAsync);

		/** Scrolls to the bottom (or right).
				This is the same as calling setCurrentRangeStart (getMaximumRangeLimit() - getCurrentRangeSize());

				@param notification    whether to send a notification of the change to listeners.
															 A notification will only be sent if the position has changed.

				@returns true if the scrollbar's position actually changed.
		*/
		bool scrollToBottom(NotificationType notification = sendNotificationAsync);

		auto getGlComponent() { return bar_; }

		void setColor(Colour color) { color_ = color; bar_->setColor(color); }
		void setShrinkLeft(bool shrink_left) { bar_->setShrinkLeft(shrink_left); }
		void setViewport(OpenGlViewport *viewport) { viewport_ = viewport; }

		void addListener(OpenGlScrollBarListener *listener) { listeners.add(listener); }
		void removeListener(OpenGlScrollBarListener *listener) { listeners.remove(listener); }
	private:
		//==============================================================================
		shared_value<Range<double>> totalRange{ Range{ 0.0, 1.0} }, visibleRange{ Range{ 0.0, 1.0} };
		double singleStepSize = 0.1, dragStartRange = 0;
		int thumbAreaSize = 0, thumbStart = 0, thumbSize = 0;
		int dragStartMousePos = 0, lastMousePos = 0;
		bool isVertical_, isDraggingThumb = false, autohides = true, userVisibilityFlag = false;
		ListenerList<OpenGlScrollBarListener> listeners;

		Colour color_;
		OpenGlViewport *viewport_ = nullptr;
		gl_ptr<OpenGlScrollQuad> bar_;

		void handleAsyncUpdate() override;
		void updateThumbPosition();
		bool getVisibility() const noexcept;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlScrollBar)
	};
}
