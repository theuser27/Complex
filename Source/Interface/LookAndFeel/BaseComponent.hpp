
// Created: 2023-12-11 16:49:17

#pragma once

#include "Framework/utils.hpp"
#include "Framework/sync_primitives.hpp"
#include "gui_utils.hpp"
#include "ui_constants.hpp"
#include "Skin.hpp"

namespace Generation
{
  class BaseProcessor;
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

  enum class RenderFlag : u8
  {
    // skip rendering on component
    NoWork, 
    // render and update flag to NoWork
    Dirty, 
    // render every frame
    Realtime
  };

  void calculateSizes(Component *children, Component *component);
  void calculatePositions(Component *children,
    Component *component, Rectangle<i32> boundsInComponent = {});

  utils::pair<i32, i32> getScrollOffsets(const MouseEvent &e,
    float singleStepX = 16, float singleStepY = 16);

  PopupDisplay *getPopupDisplay(bool primary);
  PopupSelector *getPopupSelector();
  utils::bumpArena *getUIArena();

  void deleteComponent(Component *component, bool freeArena = true);

  namespace CommandMessages
  {
    struct ProcessorInsertion
    {
      Point<i32> position{};
      Generation::BaseProcessor *processor{};
      u32 index{};
      bool useIndex{};
      bool insertPlaceholder{};
    };

    bool handleProcessorInsertion(Generation::BaseProcessor *parent, Component *parentComponent,
      ProcessorInsertion *metadata, Component *substituteInsert = nullptr);
    bool tryProcessorInsert(Generation::BaseProcessor *processorToInsert, 
      Component *treeToInsertInto, ProcessorInsertion &data);
  }

  class Component
  {
  public:
    enum CommandMessage : u64
    {
      HandleCustomPosition = 1,
      HandleProcessorInsertion,
      HandleReinitialisation,
    };

    enum SizingFlags : u16
    {
      None = 0,

      // the size of the component will be whatever is set in desiredSize
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
    };

    // all mouse events return true/false if they have/have not consumed the event

    virtual bool mouseMove([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseEnter([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseExit([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseDown([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseDrag([[maybe_unused]] const MouseEvent &event) { return false; }
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
        Rectangle{ source->bounds.w, source->bounds.h } : areaRelativeToSource;
      return Rectangle{ getRelativePoint(source, areaRelativeToSource.getPosition()),
        areaRelativeToSource.w, areaRelativeToSource.y };
    }

    bool contains(Point<i32> parentPoint) const { return bounds.contains(parentPoint); }
    bool contains(Point<float> parentPoint) const { return contains(parentPoint.toInt()); }

    Component *getComponentAt(i32 x, i32 y, bool onlyClickable = false);
    Component *
    getComponentAt(Point<i32> position, bool onlyClickable = false) 
    { return getComponentAt(position.x, position.y, onlyClickable); }

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
    void giveAwayFocus();

    // needs wantsFocus=1
    enum FocusChange
    {
      FocusClick,         // focus changed by a click (focusOnMouseClick=1),  initiator <-- indirect target
      FocusGrabbed,       // focus manually grabbed by (grabFocus()),         initiator <-- indirect target
      FocusGivenAway,     // focus willingly given away (giveAwayFocus()),    initiator --> indirect target
      FocusMoved,         // focus manually moved to (moveFocusTo()),         initiator -->   direct target
      FocusSetInvisible,  // focus changed due to invisibility (isVisible=0), initiator --> indirect target
    };
    virtual bool handleFocus([[maybe_unused]] bool hasFocus, [[maybe_unused]] FocusChange focusChange) { return true; }

    virtual bool keyPressed([[maybe_unused]] const KeyPress &key) { return false; }
    virtual bool modifierKeysChanged([[maybe_unused]] const ModifierKeys &modifiers) { return false; }

    virtual bool handleCommandMessage([[maybe_unused]] u64 commandId, 
      [[maybe_unused]] utils::whatever<64> extraData = {}) { return false; }

    void doRender(OpenGlWrapper &openGl);
    void renderScrollbars(OpenGlWrapper &openGl, float scrollHoverIncrement);
    virtual bool render([[maybe_unused]] OpenGlWrapper &openGl) { return true; }

    utils::bumpArena *arena{};

    Rectangle<i32> bounds{};

    Component *parent = nullptr;
    Component *children = nullptr;
    Component *previous = nullptr;
    Component *next = nullptr;

    //i8 layerIndex = 0;
    Skin::Override skinOverride = Skin::kUseParentOverride;
    struct
    {
      // feature flags
      bool clickable : 1 = false;
      bool clickableChildren : 1 = true;
      bool wantsFocus : 1 = false;
      bool focusOnMouseClick : 1 = false;
      bool acceptsOrphanedMouseEvents : 1 = false;
      bool vertical : 1 = false;                    // controls the children stack direction
      bool animateMovement : 1 = false;
      bool animateFadeAway : 1 = false;
      bool alwaysOnTop : 1 = false;

      // state flags
      bool isVisible : 1 = true;
      bool isHovered : 1 = false;
      bool isClicked : 1 = false;
      bool isOpenGlInitialised : 1 = false;
      bool isDestroyingOpenGl : 1 = false;
      RenderFlag renderState : 2 = RenderFlag::Dirty;
      bool hasRenderedFeatures : 1 = false;
    } componentFlags{};
    

    Placement placement{};
    SizingFlags sizingFlags = None;

    // margin in parent
    Rectangle<i16> margin{};
    // padding for children
    Rectangle<u16> padding{};
    Rectangle<i32> desiredSize{ 0, 0, utils::max_limit<i32>, utils::max_limit<i32> };

    // returns width/height min and max sizes depending on isCalculatingVertical
    // can return -1 to use the calculations in from the underlying algorithm
    Range<i32> (*overrideDimensions)(Component *c, bool isCalculatingVertical);

    Point<i32> scrollOffset{};
    Area<i32> scrollableArea{};

    // animation related
    Rectangle<i32> lastBounds{};

    // support for animated positions
    Point<i32> nextPosition{};
    Point<i32> previousPosition{ -1, -1 };
    u16 distanceToNextPositionRatio = 0;

    // support for animated shrinking and alpha fade when made invisible
    // until it reaches utils::max_limit<u16> the component is in a grace period 
    // where it will continue to have its size and position calculated 
    // @see isStillVisible
    u16 fadeawayRatio = 0;

    // support for animated scroll shrinking when not/hovered
    Range<u16> scrollWidthsRatio{};
  };

  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, |, u16)
  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, &, u16)


  struct DrawComponent final : public Component
  {
    void (*draw)(OpenGlWrapper &openGl, Component *reference, Component *self) = nullptr;
    Component *reference = nullptr;

    bool 
    render(OpenGlWrapper &openGl)
    {
      draw(openGl, reference, this);
      return true;
    }
  };

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
