
// Created: 2023-12-11 16:49:17

#pragma once

#include "Framework/utils.hpp"
#include "Framework/memory.hpp"
#include "ui_constants.hpp"
#include "gui_utils.hpp"
#include "Skin.hpp"
#include "Graphics.hpp"

namespace Generation
{
  class Processor;
}

namespace Interface
{
  class Component;
  struct OpenGlWrapper;
  class PopupDisplay;
  class PopupSelector;
  struct PopupItem;

  // Basic UI thread loop:
  // 1. Perform any modifications from the UI
  // 2. Calculate sizes and positions
  // 3. Do rendering

  void calculateSizes(Component *children, Component *component);
  void calculatePositions(Component *children,
    Component *component, Rectangle<i32> boundsInComponent = {});
  void animatePosition(Component *component, bool wasParentResized);

  PopupDisplay *getPopupDisplay(bool primary);
  PopupSelector *getPopupSelector();
  utils::bumpArena *getUIArena();

  void deleteComponent(Component *component, bool freeArena = true);

  void checkScrollClick(Component *component, const MouseEvent &e);
  void dragScroll(Component *component, const MouseEvent &e);
  bool offsetScroll(Component *component, float deltaX, float deltaY, bool switchDirections);

  namespace CommandMessages
  {
    enum CommandMessage : u64
    {
      HandleAutoscroll = 1,
      HandleProcessorInsertion,
      HandleReinitialisation,
    };

    using HandleMessageFn = bool(Component *self, u64 commandId, void *extraData);

    struct Autoscroll
    {
      Point<i32> position{};
      bool handleX{};
      bool handleY{};
    };

    struct ProcessorInsertion
    {
      Point<i32> position{};
      Generation::Processor *processor{};
      Component *placeholder{};
      u32 index{};
      bool useIndex{};
      bool isMovingUpX{};
      bool isMovingUpY{};
    };

    bool handleProcessorInsertion(Generation::Processor *parent, 
      Component *parentComponent, ProcessorInsertion *metadata, Placement placement);

    void tryProcessorInsertion(Component *parentComponent, ProcessorInsertion info);
  }

  class Component
  {
  public:
    enum SizingFlags : u16
    {
      None = 0,

      // the size of the component will be whatever is set in desiredSize.min
      FixedX = 1 << 0,
      FixedY = 1 << 1,

      // maximum size will optimistically grow as the parent grows
      GrowableX = 1 << 2,
      GrowableY = 1 << 3,

      // maximum size will be matched with siblings
      // unless minimum size is bigger
      SameAsSiblingsX = 1 << 4,
      SameAsSiblingsY = 1 << 5,
      
      // the size along the scrollable direction will be unconstrained
      // and will always fit the children
      ScrollableX = 1 << 6,
      ScrollableY = 1 << 7,

      // adds an extra scrollbar to the off-axis' size
      ScrollbarX = 1 << 8,
      ScrollbarY = 1 << 9,

      ScrollableWithBarX = ScrollableX | ScrollbarX,
      ScrollableWithBarY = ScrollableY | ScrollbarY,

      // the max size will be determined by clamp(childrenMinSize, desiredSize.min, desiredSize.max)
      SnapToMinX = 1 << 10,
      SnapToMinY = 1 << 11,
    };

    // all mouse events return true/false if they have/have not consumed the event

    virtual bool mouseMove([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseEnter([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseExit([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseDown(const MouseEvent &event);
    virtual bool mouseDrag(const MouseEvent &event);
    virtual bool mouseUp([[maybe_unused]] const MouseEvent &event) { return true; }
    virtual bool mouseWheelMove(const MouseEvent &event);

    Rectangle<i32> getLocalBounds() const { return bounds.withZeroOrigin(); }
    Point<i32> getPosition() const { return bounds.getPosition(); }
    Point<i32> getPositionInWindow() const;

    Point<i32> getRelativePoint(const Component *source, Point<i32> pointRelativeToSource = {}) const;
    Rectangle<i32> 
    getRelativeArea(const Component *source,
      Rectangle<i32> areaRelativeToSource = {}) const
    {
      areaRelativeToSource = (areaRelativeToSource.isEmpty()) ? 
        Rectangle{ 0, 0, source->bounds.w, source->bounds.h } : areaRelativeToSource;
      auto [x, y] = getRelativePoint(source, areaRelativeToSource.getPosition());
      return Rectangle{ x, y, areaRelativeToSource.w, areaRelativeToSource.h };
    }

    bool contains(Point<i32> parentPoint) const { return getLocalBounds().contains(parentPoint); }
    bool contains(Point<float> parentPoint) const { return contains(parentPoint.toInt()); }

    Component *getComponentAt(i32 x, i32 y, bool onlyClickable = false, 
      bool recursive = true, Component *startingAt = nullptr);

    bool 
    isParentOf(const Component *possibleChild) const
    {
      while (possibleChild != nullptr)
      {
        possibleChild = possibleChild->parent;
        if (possibleChild == this)
          return true;
      }
      return false;
    }
    bool 
    isStillVisible()
    {
      if (componentFlags.isVisible)
        return true;

      return (!componentFlags.animateFadeAway) ? false :
        fadeawayRatio != utils::max_limit<decltype(fadeawayRatio)>;
    }
    bool isObscured(const Component *ignoreClipIncluding = nullptr) const;
    void addChildComponent(Component *childToAdd, Component *insertBefore = nullptr);
    void addChildComponent(Component *childToAdd, usize index);
    // keepFocus will let the UI system continue focus on childToRemove
    // but you MUST NOT delete the object after removal,
    // otherwise the UI system will be reading into "freed" arena memory
    void removeChildComponent(Component *childToRemove, bool keepFocus = false);
    Component *removeChildComponent(usize childIndexToRemove, bool keepFocus = false);
    void removeAllChildComponents();

    // this one removes and frees component from arena
    void deleteChildComponent(Component *childToDelete, bool freeArena = true);
    void deleteChildComponent(usize childIndexToDelete, bool freeArena = true);
    void deleteAllChildComponents(bool freeChildArenas = true);

    bool hasFocus(bool trueIfChildIsFocused) const;
    void grabFocus();
    bool giveAwayFocusTo(Component *component = nullptr);

    // needs wantsFocus=1
    enum FocusChange : u8
    {                     //                                                     focus transfer direction
      FocusClick,         // focus changed by a click (clickable=1),          initiator <--  indirect target
      FocusGrabbed,       // focus manually grabbed by (grabFocus()),         initiator <--  indirect target
      FocusGivenAway,     // focus willingly given away (giveAwayFocusTo()),  initiator --> no/direct target
      FocusSetInvisible,  // focus changed due to invisibility (isVisible=0), initiator -->  indirect target
    };
    virtual bool handleFocus([[maybe_unused]] bool hasFocus, 
      [[maybe_unused]] FocusChange focusChange, 
      [[maybe_unused]] Component *correspondent) { return true; }

    virtual bool keyPressed([[maybe_unused]] const KeyPress &key) { return false; }

    void addCommandMessageHandler(utils::sll<CommandMessages::HandleMessageFn *> &handler,
      utils::sll<CommandMessages::HandleMessageFn *> *insertBefore = nullptr);
    void removeCommandMessageHandler(utils::sll<CommandMessages::HandleMessageFn *> &handler);
    bool handleCommandMessage(u64 commandId, void *extraData = {}, bool useFallbackHandler = true);
    static bool componentHandleCommandMessage(Component *c, u64 commandId, void *extraData);

    Skin::Override getSkinOverride() const;

    void doRenderChildren(OpenGlWrapper &openGl);
    void doRender(OpenGlWrapper &openGl);
    void renderScrollbars(OpenGlWrapper &openGl, float scrollHoverIncrement);
    virtual bool render([[maybe_unused]] OpenGlWrapper &openGl) { return true; }

    utils::bumpArena *arena{};

    Rectangle<i32> bounds{};

    Component *parent{};
    Component *children{};
    Component *previous{};
    Component *next{};

    utils::sll<CommandMessages::HandleMessageFn *> *commandMessageHandler{};

    //i8 layerIndex = 0;
    Skin::Override skinOverride = Skin::kUseParentOverride;
    struct
    {
      // feature flags
      bool clickable : 1 = false;
      bool clickableChildren : 1 = true;
      bool vertical : 1 = false;                  // controls the children stack direction
      bool animateMovement : 1 = false;
      bool animateFadeAway : 1 = false;
      bool alwaysOnTop : 1 = false;
      bool acceptsOrphanMouseEvents : 1 = false;  // if component can accept mouseUp without mouseDown
                                                  //  happens if another component gives up on handling mouseUp

      // state flags
      bool isVisible : 1 = true;
      bool isHovered : 1 = false;
      bool isClicked : 1 = false;
      bool isOpenGlInitialised : 1 = false;
      bool isScrollbarXClicked : 1 = false;
      bool isScrollbarYClicked : 1 = false;
      bool isPositionSet : 1 = true;              // to aid components with custom placement 
                                                  //  depending on other components' position
    } componentFlags{};
    

    Placement placement{};
    SizingFlags sizingFlags = None;

    // 
    Point<i8> autoScrollIncrements{};

    // margin in parent
    Rectangle<i16> margin{};
    // padding for children
    Rectangle<u16> padding{};
    Rectangle<i32> desiredSize{ 0, 0, utils::max_limit<i32>, utils::max_limit<i32> };

    // returns width/height min and max sizes depending on isCalculatingVertical
    // can return -1 to use the calculations in from the underlying algorithm
    Range<i32> (*overrideSize)(Component *c, bool isCalculatingVertical){};
    // return false if the component couldn't be positioned, 
    // so that it can be pushed at the end of the queue
    bool (*overridePosition)(Component *c){};

    Point<float> scrollOffset{};
    Area<i32> scrollableArea{};

    // animation related
    Rectangle<i32> lastBounds{};

    // support for animated positions
    Point<i32> nextPosition{};
    Point<i32> previousPosition = invalidPosition;
    u16 distanceToNextPositionRatio{};

    // support for animated shrinking and alpha fade when made invisible
    // until it reaches utils::max_limit<u16> the component is in a grace period 
    // where it will continue to have its size and position calculated 
    // @see isStillVisible()
    u16 fadeawayRatio{};

    // support for animated scroll shrinking when not/hovered
    Range<u16> scrollWidthsRatio{};
  };

  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, |, u16)
  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, &, u16)


  struct DrawComponent final : public Component
  {
    bool (*draw)(OpenGlWrapper &openGl, Component *reference, 
      Component *self, Point<i32> relativePoint) = nullptr;
    Component *reference = nullptr;

    bool 
    render(OpenGlWrapper &openGl)
    {
      auto relativePoint = getRelativePoint(reference);
      return draw(openGl, reference, this, relativePoint);
    }
  };

  inline bool preOrderTreeTraversal(Component *tree, const auto &lambda, 
    bool forward = true, bool includeParent = false)
  {
    if (includeParent && lambda(tree))
      return true;

    if (forward)
    {
      for (auto *child = tree->children; child; child = child->next)
        if (lambda(child))
          return true;
    }
    else if (tree->children)
    {
      for (auto *child = tree->children->previous; ; child = child->previous)
      {
        if (lambda(child))
          return true;

        if (!child->previous->next)
          break;
      }
    }

    for (auto *child = tree->children; child; child = child->next)
      if (preOrderTreeTraversal(child, lambda))
        return true;

    return false;
  }

  inline bool 
  preOrderTreeTraversal(Component *tree, const auto &lambda, Point<i32> at,
    bool onlyClickable = true, Component *startingAt = nullptr, bool includeParent = false)
  {
    if (includeParent && lambda(tree))
      return true;

    Component *originalStartingAt = startingAt;

    Component *child{};
    while ((child = tree->getComponentAt(at.x, at.y, onlyClickable, false, startingAt)))
    {
      // if the algorithm resorts to the parent itself, we're done
      if (child == tree)
        break;

      if (lambda(child))
        return true;

      startingAt = child;
      // if the algorithm reaches the first child it will loop back around, so we're done
      if (child == tree->children)
        break;
    }

    startingAt = originalStartingAt;

    while ((child = tree->getComponentAt(at.x, at.y, false, false, startingAt)))
    {
      // if the algorithm resorts to the parent itself, we're done
      if (child == tree)
        break;

      if (preOrderTreeTraversal(child, lambda, at - child->getPosition(), onlyClickable))
        return true;

      startingAt = child;
      // if the algorithm reaches the first child it will loop back around, so we're done
      if (child == tree->children)
        break;
    }
    
    return false;
  }

  inline bool 
  dfsUpwardTreeTraversal(Component *tree, const auto &lambda, 
    Point<i32> at, bool onlyClickable = true, Component *startingAt = nullptr)
  {
    Component *deepestComponent{};
    while (deepestComponent = tree->getComponentAt(at.x, at.y, onlyClickable, true, startingAt))
    {
      while (deepestComponent->parent != tree)
      {
        if (lambda(deepestComponent))
          return true;

        deepestComponent = deepestComponent->parent;
      }

      startingAt = tree->getComponentAt(at.x, at.y, false, false, startingAt);
    }

    return false;
  }

  class ScopedBoundsEmplace
  {
  public:
    static constexpr auto doNotAddFlag = ViewportChange{ nullptr, {}, true };
    static constexpr auto doNotClipFlag = ViewportChange{ nullptr, {}, false };

    ScopedBoundsEmplace(utils::vector<ViewportChange> &vector, Component *component) :
      ScopedBoundsEmplace{ vector, component, component->bounds } { }

    ScopedBoundsEmplace(utils::vector<ViewportChange> &vector,
      Component *component, Rectangle<int> bounds) : vector_(vector)
    {
      shouldAdd_ = vector.back() != doNotAddFlag;
      if (!shouldAdd_)
        vector.erase(vector.end() - 1);
      else if (vector.back() == doNotClipFlag)
        vector.back() = { component, bounds, false };
      else
        vector.emplaceBack(component, bounds, true);
    }

    ~ScopedBoundsEmplace() noexcept { if (shouldAdd_) vector_.popBack(); }
  private:
    utils::vector<ViewportChange> &vector_;
    bool shouldAdd_;
  };

  class ScopedIgnoreClip
  {
  public:
    ScopedIgnoreClip(utils::vector<ViewportChange> &vector,
      const Component *ignoreClipIncluding) : vector_(vector)
    {
      COMPLEX_ASSERT(vector.back() != ScopedBoundsEmplace::doNotClipFlag);
      COMPLEX_ASSERT(vector.back() != ScopedBoundsEmplace::doNotAddFlag);
        
      if (ignoreClipIncluding == nullptr)
      {
        i_ = vector.size();
        return;
      }

      for (i_ = vector.size() - 1; i_ > 0; --i_)
      {
        vector[i_].isClipping = false;
        if (vector[i_].component == ignoreClipIncluding)
          break;
      }
    }

    ~ScopedIgnoreClip() noexcept
    {
      for (; i_ < vector_.size(); ++i_)
        vector_[i_].isClipping = true;
    }
  private:
    utils::vector<ViewportChange> &vector_;
    usize i_;
  };
}
