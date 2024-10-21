/*
  ==============================================================================

    Viewport.hpp
    Created: 17 Dec 2023 4:37:10pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Interface/LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  class Viewport final : public BaseComponent, private juce::ComponentListener, private OpenGlScrollBarListener
  {
  public:
    //==============================================================================
    /** Creates a Viewport.

        The viewport is initially empty - use the setViewedComponent() method to
        add a child component for it to manage.
    */
    explicit Viewport(const juce::String &componentName = juce::String());

    /** Destructor. */
    ~Viewport() override;

    //==============================================================================
    /** Sets the component that this viewport will contain and scroll around.

        This will add the given component to this Viewport and position it at (0, 0).

        (Don't add or remove any child components directly using the normal
        Component::addChildComponent() methods).

        @param newViewedComponent   the component to add to this viewport, or null to remove
                                    the current component.
        @param deleteComponentWhenNoLongerNeeded    if true, the component will be deleted
                                    automatically when the viewport is deleted or when a different
                                    component is added. If false, the caller must manage the lifetime
                                    of the component
        @see getViewedComponent
    */
    void setViewedComponent(BaseComponent *newViewedComponent,
      bool deleteComponentWhenNoLongerNeeded = true);

    /** Returns the component that's currently being used inside the Viewport.

        @see setViewedComponent
    */
    BaseComponent *getViewedComponent() const noexcept { return contentComp.get(); }

    //==============================================================================
    /** Changes the position of the viewed component.

        The inner component will be moved so that the pixel at the top left of
        the viewport will be the pixel at position (xPixelsOffset, yPixelsOffset)
        within the inner component.

        This will update the scrollbars and might cause a call to visibleAreaChanged().

        @see getViewPositionX, getViewPositionY, setViewPositionProportionately
    */
    void setViewPosition(int xPixelsOffset, int yPixelsOffset);

    /** Changes the position of the viewed component.

        The inner component will be moved so that the pixel at the top left of
        the viewport will be the pixel at the specified coordinates within the
        inner component.

        This will update the scrollbars and might cause a call to visibleAreaChanged().

        @see getViewPositionX, getViewPositionY, setViewPositionProportionately
    */
    void setViewPosition(juce::Point<int> newPosition);

    /** Changes the view position as a proportion of the distance it can move.

        The values here are from 0.0 to 1.0 - where (0, 0) would put the
        visible area in the top-left, and (1, 1) would put it as far down and
        to the right as it's possible to go whilst keeping the child component
        on-screen.
    */
    void setViewPositionProportionately(double proportionX, double proportionY);

    /** If the specified position is at the edges of the viewport, this method scrolls
        the viewport to bring that position nearer to the centre.

        Call this if you're dragging an object inside a viewport and want to make it scroll
        when the user approaches an edge. You might also find Component::beginDragAutoRepeat()
        useful when auto-scrolling.

        @param mouseX       the x position, relative to the Viewport's top-left
        @param mouseY       the y position, relative to the Viewport's top-left
        @param distanceFromEdge     specifies how close to an edge the position needs to be
                            before the viewport should scroll in that direction
        @param maximumSpeed the maximum number of pixels that the viewport is allowed
                            to scroll by.
        @returns            true if the viewport was scrolled
    */
    bool autoScroll(int mouseX, int mouseY, int distanceFromEdge, int maximumSpeed);

    /** Returns the position within the child component of the top-left of its visible area. */
    juce::Point<int> getViewPosition() const noexcept { return lastVisibleArea.getPosition(); }

    /** Returns the visible area of the child component, relative to its top-left */
    juce::Rectangle<int> getViewArea() const noexcept { return lastVisibleArea; }

    /** Returns the position within the child component of the top-left of its visible area.
        @see getViewWidth, setViewPosition
    */
    int getViewPositionX() const noexcept { return lastVisibleArea.getX(); }

    /** Returns the position within the child component of the top-left of its visible area.
        @see getViewHeight, setViewPosition
    */
    int getViewPositionY() const noexcept { return lastVisibleArea.getY(); }

    /** Returns the width of the visible area of the child component.

        This may be less than the width of this Viewport if there's a vertical scrollbar
        or if the child component is itself smaller.
    */
    int getViewWidth() const noexcept { return lastVisibleArea.getWidth(); }

    /** Returns the height of the visible area of the child component.

        This may be less than the height of this Viewport if there's a horizontal scrollbar
        or if the child component is itself smaller.
    */
    int getViewHeight() const noexcept { return lastVisibleArea.getHeight(); }

    /** Returns the width available within this component for the contents.

        This will be the width of the viewport component minus the width of a
        vertical scrollbar (if visible).
    */
    int getMaximumVisibleWidth() const { return contentHolder.getWidth(); }

    /** Returns the height available within this component for the contents.

        This will be the height of the viewport component minus the space taken up
        by a horizontal scrollbar (if visible).
    */
    int getMaximumVisibleHeight() const { return contentHolder.getHeight(); }

    //==============================================================================
    /** Turns scrollbars on or off.

        If set to false, the scrollbars won't ever appear. When true (the default)
        they will appear only when needed.

        The allowVerticalScrollingWithoutScrollbar parameters allow you to enable
        mouse-wheel scrolling even when there the scrollbars are hidden. When the
        scrollbars are visible, these parameters are ignored.
    */
    void setScrollBarsShown(bool showVerticalScrollbarIfNeeded,
      bool showHorizontalScrollbarIfNeeded,
      bool allowVerticalScrollingWithoutScrollbar = false,
      bool allowHorizontalScrollingWithoutScrollbar = false);

    /** Changes where the scroll bars are positioned

        If verticalScrollbarOnRight is set to true, then the vertical scrollbar will
        appear on the right side of the view port's content (this is the default),
        otherwise it will be on the left side of the content.

        If horizontalScrollbarAtBottom is set to true, then the horizontal scrollbar
        will appear at the bottom of the view port's content (this is the default),
        otherwise it will be at the top.
    */
    void setScrollBarPosition(bool verticalScrollbarOnRight,
      bool horizontalScrollbarAtBottom);

    /** True if the vertical scrollbar will appear on the right side of the content */
    bool isVerticalScrollbarOnTheRight() const noexcept { return vScrollbarRight; }

    /** True if the horizontal scrollbar will appear at the bottom of the content */
    bool isHorizontalScrollbarAtBottom() const noexcept { return hScrollbarBottom; }

    /** True if the vertical scrollbar is enabled.
        @see setScrollBarsShown
    */
    bool isVerticalScrollBarShown() const noexcept { return showVScrollbar; }

    /** True if the horizontal scrollbar is enabled.
        @see setScrollBarsShown
    */
    bool isHorizontalScrollBarShown() const noexcept { return showHScrollbar; }

    /** Changes the width of the scrollbars.
        If this isn't specified, the default width from the LookAndFeel class will be used.
        @see LookAndFeel::getDefaultScrollbarWidth
    */
    void setScrollBarThickness(int thickness);

    /** Returns the thickness of the scrollbars.
        @see setScrollBarThickness
    */
    int getScrollBarThickness() const { return scrollBarThickness; }

    /** Changes the distance that a single-step click on a scrollbar button
        will move the viewport.
    */
    void setSingleStepSizes(int stepX, int stepY);

    /** Returns a reference to the scrollbar component being used.
        Handy if you need to customise the bar somehow.
    */
    ScrollBar &getVerticalScrollBar() noexcept { return *verticalScrollBar; }

    /** Returns a reference to the scrollbar component being used.
        Handy if you need to customise the bar somehow.
    */
    ScrollBar &getHorizontalScrollBar() noexcept { return *horizontalScrollBar; }

    /** Re-instantiates the scrollbars, which is only really useful if you've overridden createScrollBarComponent(). */
    void recreateScrollbars();

    /** True if there's any off-screen content that could be scrolled vertically,
        or false if everything is currently visible.
    */
    bool canScrollVertically() const noexcept;

    /** True if there's any off-screen content that could be scrolled horizontally,
        or false if everything is currently visible.
    */
    bool canScrollHorizontally() const noexcept;

    enum class ScrollOnDragMode
    {
      never,          /**< Dragging will never scroll the viewport. */
      nonHover,       /**< Dragging will only scroll the viewport if the input source cannot hover. */
      all             /**< Dragging will always scroll the viewport. */
    };

    /** Sets the current scroll-on-drag mode. The default is ScrollOnDragMode::nonHover.

        If your viewport contains a Component that you don't want to receive mouse events when the
        user is drag-scrolling, you can disable this with the Component::setViewportIgnoreDragFlag()
        method.
    */
    void setScrollOnDragMode(ScrollOnDragMode mode) { scrollOnDragMode = mode; }

    /** Returns the current scroll-on-drag mode. */
    ScrollOnDragMode getScrollOnDragMode() const { return scrollOnDragMode; }

    /** Returns true if the user is currently dragging-to-scroll.
        @see setScrollOnDragEnabled
    */
    bool isCurrentlyScrollingOnDrag() const noexcept { return isDragging; }

    void addListener(OpenGlViewportListener *listener) { listeners.emplace_back(listener); }
    void removeListener(OpenGlViewportListener *listener) { std::erase(listeners, listener); }

    //==============================================================================
    /** @internal */
    void resized() override;
    /** @internal */
    void scrollBarMoved(ScrollBar *, double newRangeStart) override;
    /** @internal */
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    /** @internal */
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    /** @internal */
    bool keyPressed(const juce::KeyPress &) override;
    /** @internal */
    void componentMovedOrResized(Component &, bool wasMoved, bool wasResized) override;
    /** @internal */
    bool useMouseWheelMoveIfNeeded(const juce::MouseEvent &, const juce::MouseWheelDetails &);

  private:
    utils::up<ScrollBar> verticalScrollBar, horizontalScrollBar;
    BaseComponent contentHolder;
    juce::WeakReference<BaseComponent> contentComp;
    juce::Rectangle<int> lastVisibleArea;
    int scrollBarThickness = 0;
    int singleStepX = 16, singleStepY = 16;
    ScrollOnDragMode scrollOnDragMode = ScrollOnDragMode::nonHover;
    bool showHScrollbar = true, showVScrollbar = true, deleteContent = true;
    bool allowScrollingWithoutScrollbarV = false, allowScrollingWithoutScrollbarH = false;
    bool vScrollbarRight = true, hScrollbarBottom = true;

    bool isDragging = false;
    bool isGlobalMouseListener = false;
    juce::MouseInputSource scrollSource = juce::Desktop::getInstance().getMainMouseSource();

    std::vector<OpenGlViewportListener *> listeners{};

    juce::Point<int> viewportPosToCompPos(juce::Point<int>) const;

    void updateVisibleArea();
    void deleteOrRemoveContentComp();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Viewport)
  };
}
