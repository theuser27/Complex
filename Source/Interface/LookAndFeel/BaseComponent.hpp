/*
  ==============================================================================

    BaseComponent.hpp
    Created: 11 Dec 2023 4:49:17pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/utils.hpp"
#include "Framework/sync_primitives.hpp"
#include "gui_utils.hpp"
#include "Miscellaneous.hpp"
#include "Skin.hpp"

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

  void calculateSizes(utils::span<Component *> children, Component *component);
  void calculatePositions(utils::span<Component *> children,
    Component *component, Rectangle<i32> boundsInComponent = {});

  PopupDisplay *getPopupDisplay(bool primary);
  PopupSelector *getPopupSelector();
  utils::bumpArena *getUIArena();

  void deleteComponent(Component *component);

  class Component
  {
  public:
    enum CommandMessages : u64
    {
      HandleCustomPosition = 1,
    };

    enum SizingFlags : u16
    {
      None = 0,

      // the size of the component will whatever is set in desiredSize
      Fixed = 1 << 0,

      // maximum size will optimistically grow as the parent grows
      GrowableX = 1 << 1,
      GrowableY = 1 << 2,

      // maximum size will be matched with siblings
      // unless minimum size is bigger
      SameAsSiblingsX = 1 << 3,
      SameAsSiblingsY = 1 << 4,
      
      // the size along the scrollable direction will be unconstrained
      // and will always fit the children
      ScrollableX = 1 << 5,
      ScrollableY = 1 << 6,

      // adds an extra scrollbar to the off-axis' size
      HasScrollbarX = 1 << 7,
      HasScrollbarY = 1 << 8,

      ScrollableWithBarX = ScrollableX | HasScrollbarX,
      ScrollableWithBarY = ScrollableY | HasScrollbarY,

      ScrollIsPartOfPadding = 1 << 9,

      // will use desiredSize.getTextDimensions to calculate dimensions
      HasText = 1 << 10,
      // controls the children stack direction
      IsVertical = 1 << 11,
    };

    // all mouse events return true/false if they have/have not consumed the event

    virtual bool mouseMove([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseEnter([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseExit([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseDown([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseDrag([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseUp([[maybe_unused]] const MouseEvent &event) { return false; }
    virtual bool mouseWheelMove(const MouseEvent &event);

    Rectangle<i32> getLocalBounds() const noexcept { return bounds.withZeroOrigin(); }
    Point<i32> getPosition() const noexcept { return bounds.getPosition(); }
    Point<i32> 
    getPositionInWindow() const noexcept
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
    getRelativePoint(const Component *source,
      Point<i32> pointRelativeToSource = {}) const noexcept
    {
      if (source->parent == parent)
        return pointRelativeToSource;

      Point otherPosition = pointRelativeToSource;
      if (source)
        otherPosition += source->getPositionInWindow();
      Point position = getPositionInWindow();
      return { otherPosition.x - position.x, otherPosition.y - position.y };
    }
    Rectangle<i32> 
    getRelativeArea(const Component *source,
      Rectangle<i32> areaRelativeToSource = {}) const noexcept
    {
      return Rectangle{ getRelativePoint(source, areaRelativeToSource.getPosition()),
        areaRelativeToSource.w, areaRelativeToSource.y };
    }

    bool contains(Point<i32> parentPoint) { return bounds.contains(parentPoint); }
    bool contains(Point<float> parentPoint) { return contains(parentPoint.toInt()); }

    Component *getComponentAt(i32 x, i32 y);
    Component *getComponentAt(Point<i32> position) { return getComponentAt(position.x, position.y); }

    bool 
    isParentOf(const Component *possibleChild) const noexcept
    {
      while (possibleChild != nullptr)
      {
        possibleChild = possibleChild->parent;
        if (possibleChild == this)
          return true;
      }
      return false;
    }
    void addChildComponent(Component *child, i8 layerIndex = 0);
    // keepFocus will let the UI system continue focus on childToRemove
    // but you MUST NOT delete the object after removal,
    // otherwise the UI system will be reading into "freed" arena memory
    void removeChildComponent(Component *childToRemove, bool keepFocus = false);
    Component *removeChildComponent(usize childIndexToRemove, bool keepFocus = false);
    void removeAllChildComponents();

    // this one removes and frees component from arena
    void deleteChildComponent(Component *childToDelete);
    void deleteChildComponent(usize childIndexToDelete);
    void deleteAllChildComponents();

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

    virtual void handleCommandMessage([[maybe_unused]] u64 commandId, [[maybe_unused]] utils::whatever extraData = {}) { }

    void doRender(OpenGlWrapper &openGl);
    void renderScrollbars(OpenGlWrapper &openGl, float scrollHoverIncrement);
    virtual bool render([[maybe_unused]] OpenGlWrapper &openGl) { return true; }

    utils::bumpArena *arena{};

    Rectangle<i32> previousBounds{};
    Rectangle<i32> bounds{};
    Rectangle<u16> addedHitbox{};

    Component *parent = nullptr;
    utils::vector<Component *> childComponents{};
    i8 layerIndex = 0;
    Skin::SectionOverride skinOverride = Skin::kUseParentOverride;
    struct
    {
      // feature flags
      bool clickable : 1 = false;
      bool clickableChildren : 1 = true;
      bool wantsFocus : 1 = false;
      bool focusOnMouseClick : 1 = false;
      bool stealsMouseEvents : 1 = false;

      // state flags
      bool isVisible : 1 = true;
      bool isHovered : 1 = false;
      bool isClicked : 1 = false;
      bool isOpenGlInitialised : 1 = false;
      bool destroyOpenGl : 1 = false;
      bool hasRenderedFeatures : 1 = false;
      bool hasSummonnedPopupSelector : 1 = false;
      RenderFlag renderState : 2 = RenderFlag::Dirty;
    } componentFlags{};
    
    Range<u8> scrollWidths{};

    SizingFlags sizingFlags = None;

    // margin in parent
    Rectangle<i16> margin{};
    // padding for children
    Rectangle<u8> padding{};
    Placement placement{};
    union
    {
      Rectangle<i32> minMax{};
      // Text
      // returns primary/secondary axis min and max size if availablePrimarySize ==/!= nullptr
      Range<i32> (*getTextDimensions)(Component *c, i32 *availablePrimarySize);
    } desiredSize{ { 0, 0, utils::max_limit<i32>, utils::max_limit<i32> } };

    Point<i32> scrollOffset{};
    Area<i32> scrollableArea{};
  };

  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, |, u16)
  COMPLEX_DEFINE_ENUM_OPERATION(Component::SizingFlags, &, u16)


  class BaseControl;
  class BaseSection : public Component, public ControlListener
  {
  public:
    void controlValueChanged(BaseControl *) override { }

    void addControl(BaseControl *control, bool addChild = true);
    void removeControl(BaseControl *control);
    BaseControl *getControl(uuid id) { return controls_.find(id)->second; }

    utils::vector_map<uuid, BaseControl *> controls_{};
  };

  class PowerButton;
  class ProcessorSection : public BaseSection
  {
  public:
    static constexpr int kDefaultActivatorSize = 12;

    ProcessorSection(Generation::BaseProcessor *processor) : processor{ processor } { }

    void setActivator(PowerButton *activator);

    Generation::BaseProcessor *processor = nullptr;
    PowerButton *activator = nullptr;
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
        vector.emplace_back(component, bounds, true);
    }

    ~ScopedBoundsEmplace() noexcept { if (shouldAdd_) vector_.pop_back(); }
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
