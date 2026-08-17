
// Created: 2022-12-20 19:34:54

#include "Plugin/Complex.hpp"

#include "Third Party/cplug/cplug.h"
#include "Third Party/glad/glad.h"
#include "Third Party/xhl/xhl_files.h"

#define PUGL_NO_INCLUDE_GL_H
#include "Third Party/pugl/gl.h"

#include "Framework/load_save.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Interface/LookAndFeel/Graphics.hpp"
#include "Interface/LookAndFeel/Component.hpp"
#include "Interface/Sections/MainInterface.hpp"

static auto
unscaleDimensions(u32 width, u32 height, double currentScaling) noexcept
{
  return utils::pair{ (u32)::round((double)width / currentScaling),
    (u32)::round((double)height / currentScaling) };
}

// static void clampScaleWidthHeight(PuglView *view, float &desiredScale, u32 &windowWidth, u32 &windowHeight)
// {
//   using namespace Interface;

//   auto nativeHandle = puglGetNativeView(view);
//   nativeHandle = nativeHandle ? nativeHandle : puglGetParent(view);

//   auto info = Interface::getCurrentMonitorInfo((void *)nativeHandle);

//   // the available display area on screen for the window
//   Interface::Rectangle<i32> displayArea = info.totalArea;

//   desiredScale = ::roundf(utils::clamp(desiredScale, kMinWindowScaleFactor, kMaxWindowScaleFactor)
//     / kWindowScaleIncrements) * kWindowScaleIncrements;

//   auto clampScaleToDisplay = [&](float current, float display)
//   {
//     while (display > 0.0f && current * desiredScale > display)
//     {
//       if (desiredScale == kMinWindowScaleFactor)
//         break;
//       desiredScale -= kWindowScaleIncrements;
//     }
//   };

//   float displayWidth = (float)displayArea.w;
//   clampScaleToDisplay((float)windowWidth, displayWidth);

//   while (desiredScale * (float)windowWidth > displayWidth)
//   {
//     if (windowWidth <= kMinWidth)
//     {
//       windowWidth = kMinWidth;
//       break;
//     }
//     windowWidth -= kAddedWidthPerLane;
//   }

//   float displayHeight = (float)displayArea.h;
//   clampScaleToDisplay((float)windowHeight, displayHeight);

//   windowHeight = utils::clamp(windowHeight, (u32)kMinHeight, (u32)::floorf(displayHeight / desiredScale));
// }

namespace utils
{
  void initialiseHotreload(utils::Dylib *hotreload);
}

namespace Interface
{
  // deferred components that will set their own position
  constinit utils::vector<Component *> *customPlacement{};
  constinit utils::vector<Component *> *sortedSizesMin{};
  constinit utils::vector<Component *> *sortedSizesMax{};

  void teardownGl(Renderer *renderer)
  {
    renderer->generalData.g->~Graphics();
    renderer->generalData.g = nullptr;
  }

  void computeMultiClick(Renderer *renderer, double currentTime, MouseEvent &e, bool isClicking)
  {
    if (isClicking)
    {
      if (currentTime - renderer->lastMouseClickTime < Renderer::kMultiClickTimeout &&
        renderer->lastMouseDownPosition_ == Point{ e.x, e.y })
        ++renderer->numberOfClicks;
      else
        renderer->numberOfClicks = 1;
      renderer->lastMouseClickTime = currentTime;
    }
    else if (currentTime - renderer->lastMouseClickTime >= Renderer::kMultiClickTimeout)
      renderer->numberOfClicks = 0;

    e.numberOfClicks = renderer->numberOfClicks;
  }

  // constinit thread_local PuglView *lastView{};
  
  PuglStatus
  runThread(PuglView *view, const PuglEvent *event)
  {
    // puglChangeContext(lastView, view);
    // lastView = view;

    auto *renderer = (Interface::Renderer *)puglGetHandle(view);

    getUiRelated() = &renderer->generalData;
    defer { getUiRelated() = nullptr; };

    COMPLEX_ASSERT(renderer->view_ == view);

    renderer->plugin.rescanLatency();

    if (!renderer->isInitialised || renderer->area.w == 0 || renderer->area.h == 0)
    {
      // until we're initialised and have a size we can't do anything else
      if ((!renderer->isInitialised && event->type != PUGL_REALIZE) &&
        ((renderer->area.w == 0 || renderer->area.h == 0) && event->type != PUGL_CONFIGURE))
      {
        COMPLEX_ASSERT_FALSE("UI initialisation: %d, UI size: { w: %d, h: %d }",
          renderer->isInitialised, (int)renderer->area.w, (int)renderer->area.h);
        return PUGL_FAILURE;
      }
    }

    switch (event->type)
    {
    case PUGL_REALIZE:
    {
      puglEnterContext(view);
      if (!gladLoadGLLoader((GLADloadproc)&puglGetProcAddress))
      {
        COMPLEX_ASSERT_FALSE("Failed to load OpenGL functions\n");
        return PUGL_FAILURE;
      }

      renderer->generalData.g = anew(renderer->arena, Graphics, {});
      renderer->isInitialised = true;

      break;
    }
    case PUGL_CLOSE:
    {
      teardownGl(renderer);
      // puglLeaveContext(view);
      // lastView = nullptr;
      renderer->isInitialised = false;
      puglStopTimer(view, Renderer::kTimerRefreshRate);

      break;
    }
    case PUGL_UNREALIZE:
    {
      teardownGl(renderer);
      // puglLeaveContext(view);
      // lastView = nullptr;
      renderer->isInitialised = false;
      puglStopTimer(view, Renderer::kTimerRefreshRate);

      auto [unscaledWidth, unscaledHeight] = unscaleDimensions(renderer->area.w, renderer->area.h, renderer->generalData.scale);
      Framework::LoadSave::saveWindowSizeScale(unscaledWidth, unscaledHeight, renderer->pluginScale);

      break;
    }
    case PUGL_CONFIGURE:
    {
      renderer->resizeChange((event->configure.style & PUGL_VIEW_STYLE_RESIZING) != 0);
      renderer->area = { event->configure.width, event->configure.height };

      renderer->renderLoop(view);

      break;
    }
    case PUGL_DPI_CHANGE:
      //setScaleFactor(event->dpiChange.newScaleFactor);
      break;

    case PUGL_EXPOSE:
      //renderer->renderLoop(view);
      break;

    case PUGL_UPDATE: break;
    case PUGL_LOOP_ENTER: break;
    case PUGL_LOOP_LEAVE: break;
    case PUGL_KEY_PRESS:
    {
      KeyPress k{};
      k.mods |= ((event->key.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      k.mods |= ((event->key.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      k.mods |= ((event->key.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      k.mods |= renderer->mouseButtonsDown_.flags;

      k.keyCode = event->key.key;
      renderer->handleKeyPress(k);

      break;
    }
    case PUGL_FOCUS_OUT:
    {
      if (renderer->mouseDownComponent_)
      {
        MouseEvent e{};
        e.x = renderer->lastMousePosition_.x;
        e.y = renderer->lastMousePosition_.y;
        e.mouseDownPosition = renderer->lastMouseDownPosition_;
        e.mods = renderer->lastKeyboardMods_;
        e = renderer->getRelativeEvent(e, renderer->mouseDownComponent_);

        renderer->mouseDownComponent_->mouseExit(e);
        renderer->mouseDownComponent_ = nullptr;
      }

      break;
    }
    case PUGL_POINTER_IN:
    case PUGL_POINTER_OUT:
    {
      MouseEvent e;
      e.x = (i32)::round(event->crossing.x);
      e.y = (i32)::round(event->crossing.y);
      e.mods |= ((event->crossing.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->crossing.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->crossing.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_.flags;
      e.mouseDownPosition = renderer->lastMouseDownPosition_;
      computeMultiClick(renderer, event->crossing.time, e, false);

      if (event->type == PUGL_POINTER_IN)
        renderer->handleMouseEnter(COMPLEX_MOVE(e));
      else
        renderer->handleMouseLeave(COMPLEX_MOVE(e));

      renderer->lastMousePosition_ = { e.x, e.y };
      if (event->type == PUGL_POINTER_OUT)
        renderer->lastMousePosition_ = { -1, -1 };
      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;

      break;
    }
    case PUGL_BUTTON_PRESS:
    case PUGL_BUTTON_RELEASE:
    {
      i32 flag = 0;
      switch (event->button.button)
      {
      case 0: flag = ModifierKeys::leftButtonModifier; break;
      case 1:	flag = ModifierKeys::rightButtonModifier; break;
      case 2:	flag = ModifierKeys::middleButtonModifier; break;
      case 4:	flag = ModifierKeys::forwardButtonModifier; break;
      case 3:	flag = ModifierKeys::backwardButtonModifier; break;
      }

      if (event->type == PUGL_BUTTON_PRESS)
        renderer->mouseButtonsDown_ = renderer->mouseButtonsDown_.withFlags(flag);
      else
        renderer->mouseButtonsDown_ = renderer->mouseButtonsDown_.withoutFlags(flag);

      MouseEvent e;
      e.x = (i32)::round(event->button.x);
      e.y = (i32)::round(event->button.y);
      e.mods |= test_flag(event->button.state, PUGL_MOD_SHIFT) ? ModifierKeys::shiftModifier : 0;
      e.mods |= test_flag(event->button.state, PUGL_MOD_CTRL) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= test_flag(event->button.state, PUGL_MOD_ALT) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = { e.x, e.y };
      e.directionX = (i8)utils::clamp(e.x - renderer->lastMousePosition_.x, -1, 1);
      e.directionY = (i8)utils::clamp(e.y - renderer->lastMousePosition_.y, -1, 1);

      if (event->type == PUGL_BUTTON_PRESS)
      {
        computeMultiClick(renderer, event->button.time, e, true);
        renderer->handleMouseDown(COMPLEX_MOVE(e));
        renderer->lastMouseDownPosition_ = { e.x, e.y };
      }
      else
      {
        computeMultiClick(renderer, event->button.time, e, false);
        renderer->handleMouseUp(COMPLEX_MOVE(e));
      }

      renderer->lastMousePosition_ = { e.x, e.y };
      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;

      break;
    }
    case PUGL_MOTION:
    {
      MouseEvent e;
      e.x = (i32)::round(event->motion.x);
      e.y = (i32)::round(event->motion.y);
      e.mods |= test_flag(event->motion.state, PUGL_MOD_SHIFT) ? ModifierKeys::shiftModifier : 0;
      e.mods |= test_flag(event->motion.state, PUGL_MOD_CTRL) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= test_flag(event->motion.state, PUGL_MOD_ALT) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = renderer->lastMouseDownPosition_;
      e.directionX = (i8)utils::clamp(e.x - renderer->lastMousePosition_.x, -1, 1);
      e.directionY = (i8)utils::clamp(e.y - renderer->lastMousePosition_.y, -1, 1);

      computeMultiClick(renderer, event->motion.time, e, false);
      renderer->handleMouseMove(COMPLEX_MOVE(e));

      renderer->lastMousePosition_ = { e.x, e.y };
      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;

      break;
    }
    case PUGL_SCROLL:
    {
      MouseEvent e;
      e.x = (i32)::round(event->scroll.x);
      e.y = (i32)::round(event->scroll.y);
      e.mods |= ((event->scroll.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->scroll.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->scroll.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = renderer->lastMouseDownPosition_;
      e.directionX = (i8)utils::clamp(e.x - renderer->lastMousePosition_.x, -1, 1);
      e.directionY = (i8)utils::clamp(e.y - renderer->lastMousePosition_.y, -1, 1);
      e.wheelDeltaX = (float)event->scroll.dx;
      e.wheelDeltaY = (float)event->scroll.dy;

      computeMultiClick(renderer, event->scroll.time, e, false);
      renderer->handleMouseWheel(COMPLEX_MOVE(e));

      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;
      renderer->lastMousePosition_ = { e.x, e.y };

      break;
    }
    case PUGL_TIMER:
    {
      if (event->timer.id == Renderer::kTimerRefreshRate)
      {
        renderer->renderLoop(view);
      }

      break;
    }
    case PUGL_CLIENT:
      break;

    default:
      break;
    }

    return PUGL_SUCCESS;
  }

  void hotreloadCallback(XFILES_WATCH_TYPE type, const char *path, void *udata)
  {
    switch (type)
    {
    case XFILES_WATCH_CREATED:
    {
      auto s = utils::string_view{ path, utils::getStringSize(path) };
      // filter out non-DLL files
      if (s.rfind(".dll") == utils::string_view::npos)
        return;

      auto newLib = utils::sp<utils::Dylib>::create(path);
      if (!newLib->handle)
        return;

      utils::initialiseHotreload(newLib.get());
      executableStaticData.structure.loadedDynamicLibs.emplaceBack(newLib);

      auto *renderer = (Renderer *)udata;
      auto g = renderer->plugin.acquireProcessingLock();
      auto *state = renderer->plugin.state_.get();
      state->pluginStructure.loadedDynamicLibs.emplaceBack(COMPLEX_MOVE(newLib));

      // invalidate cached symbols
      state->cachedHotreloadSymbols.data.clear();
    } break;
    case XFILES_WATCH_DELETED:
    case XFILES_WATCH_MODIFIED:
    default:
      break;
    }
  }

  Renderer::Renderer(Plugin::ComplexPlugin &plugin) : plugin{ plugin }, generalData{ plugin }
  {
    arena = utils::bumpArena::createNested(plugin.arena, COMPLEX_MB(1));
    callbacks.data = { arena };

  #ifdef COMPLEX_HOTRELOAD_DIR
    watchFileContext = xfiles_watch_create(COMPLEX_HOTRELOAD_DIR, this, hotreloadCallback);
  #endif
    generalData.renderer = this;
    generalData.skin = anew(arena, Skin, {});
  }

  Renderer::~Renderer()
  {
    customPlacement = {};
    gui->~MainInterface();
  #ifdef COMPLEX_HOTRELOAD_DIR
    xfiles_watch_destroy(watchFileContext);
  #endif
    utils::bumpArena::destroy(arena);
  }

  void Renderer::resetGui(MainInterface *newGui)
  {
    if (gui)
    {
      dragAndDropComponent_ = focusedComponent_ =
        mouseDownComponent_ = mouseHoveredComponent_ = nullptr;
      callbacks.data.clear();
    }

    gui = newGui;
  }

  bool
  Renderer::setUISize(u32 width, u32 height)
  {
    return plugin.hostContext->requestResize(plugin.hostContext, width, height);
  }

  void Renderer::setUIScale(float newPluginScale)
  {
    pluginScale = newPluginScale;
    recalculateScale();
  }

  void Renderer::setMouseCursor(MouseCursorTypes cursorType)
  {
    if (cursorType <= MouseCursorTypes::AllScroll)
      puglSetCursor(view_, (PuglCursor)cursorType);
  }

  void Renderer::setHoveredComponent(Component *component)
  {
    mouseHoveredComponent_ = component;
  }

  void Renderer::setClickedComponent(Component *component)
  {
    if (mouseDownComponent_)
    {
      mouseDownComponent_->componentFlags.isClicked = false;
      mouseDownComponent_->componentFlags.isScrollbarYClicked = false;
      mouseDownComponent_->componentFlags.isScrollbarXClicked = false;
    }
    mouseDownComponent_ = component;
    if (mouseDownComponent_)
      mouseDownComponent_->componentFlags.isClicked = true;
  }

  void Renderer::setFocusedComponent(Component *component)
  {
    focusedComponent_ = component;
  }


  MouseInteractions
  Renderer::getMouseInteractions()
  {
    return MouseInteractions
    {
      .hovered = mouseHoveredComponent_,
      .clicked = mouseDownComponent_,
      .focused = focusedComponent_,
      .mouseState =
      {
        .x = lastMousePosition_.x,
        .y = lastMousePosition_.y,
        .mouseDownPosition = lastMouseDownPosition_,
        .mods = mouseButtonsDown_ | lastKeyboardMods_
      }
    };
  }

  void Renderer::startUI()
  {
    if (view_ != nullptr)
      return;

    getUiRelated() = &generalData;

    // Create world and view
    auto world = puglNewWorld(PUGL_MODULE, 0);
    view_ = puglNewView(world);

    // load *some* kind of size until we get a window to check if we stretch outside the screen
    Framework::LoadSave::getWindowSizeScale(area.w, area.h, pluginScale);

    // Set up world and view
    puglSetWorldString(world, PUGL_CLASS_NAME, "ComplexAudioPlugin");
    puglSetViewString(view_, PUGL_WINDOW_TITLE, "Complex");
    // set placeholder sizes for now and calculate the actual ones when we have a window
    puglSetSizeHint(view_, PUGL_DEFAULT_SIZE, kMinWidth, kMinHeight);

    puglSetBackend(view_, puglGlBackend());
    puglSetViewHint(view_, PUGL_CONTEXT_API, PUGL_OPENGL_API);
    puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MAJOR, 3);
    puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MINOR, 3);
    puglSetViewHint(view_, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
    puglSetViewHint(view_, PUGL_CONTEXT_DEBUG, PUGL_TRUE);
    puglSetViewHint(view_, PUGL_RESIZABLE, PUGL_TRUE);
    puglSetViewHint(view_, PUGL_SAMPLES, 1);
    puglSetViewHint(view_, PUGL_DOUBLE_BUFFER, PUGL_TRUE);
    puglSetViewHint(view_, PUGL_SWAP_INTERVAL, 1);
    puglSetViewHint(view_, PUGL_DARK_FRAME, PUGL_TRUE);
    //puglSetViewHint(view_, PUGL_REFRESH_RATE, 60);
    puglSetHandle(view_, this);
    puglSetEventFunc(view_, runThread);
  }

  void Renderer::stopUI()
  {
    if (view_ == nullptr)
      return;

    getUiRelated() = &generalData;

    puglUnrealize(view_);

    auto world = view_->world;
    puglFreeView(view_);
    puglFreeWorld(world);

    view_ = nullptr;
    isVisible = false;
  }

  void Renderer::resizeChange(bool resizeChange)
  {
    //if (isResizing && !resizeChange)
    //{
    //  auto [unscaledWidth, unscaledHeight] = unscaleDimensions(area.w, area.h, pluginScale);
    //  Framework::LoadSave::saveWindowSizeScale(unscaledWidth, unscaledHeight, pluginScale);
    //}

    isResizing = resizeChange;
  }


  void Renderer::moveFocusTo(Component *component)
  {
    if ((!focusedComponent_ || focusedComponent_->handleFocus(false, Component::FocusGivenAway, component)) &&
      (!component || component->handleFocus(true, Component::FocusGivenAway, focusedComponent_)))
    {
      focusedComponent_ = component;
    }
  }

  utils::span<const byte> 
  Renderer::getClipboard()
  {
    usize size;
    auto *data = (const byte *)puglGetClipboard(getUiRelated()->renderer->view_, 0, &size);
    return { data, size };
  }

  void Renderer::setClipboard(utils::span<const byte> data)
  {
    puglSetClipboard(getUiRelated()->renderer->view_, nullptr, data.data(), data.size());
  }

  MouseEvent
  Renderer::getRelativeEvent(MouseEvent e, Component *component)
  {
    auto componentPosition = component->getPositionInWindow();
    e.x -= componentPosition.x;
    e.y -= componentPosition.y;
    e.mouseDownPosition = lastMouseDownPosition_ - componentPosition;
    e.eventComponent = component;
    e.originalComponent = component;
    return e;
  }

  void Renderer::refreshComponentUnderMouse(MouseEvent e, bool forceMouseMove)
  {
    // we don't want to take away mouse focus from clicked components
    if (mouseDownComponent_)
      return;

    // because of the above condition we can assume nothing is clicked from here on

    Component *newHoveredComponent = gui->getComponentAt(e.x, e.y, true);
    forceMouseMove |= newHoveredComponent != mouseHoveredComponent_;

    if (newHoveredComponent != mouseHoveredComponent_)
    {
      if (mouseHoveredComponent_)
      {
        mouseHoveredComponent_->mouseExit(getRelativeEvent(e, mouseHoveredComponent_));
        mouseHoveredComponent_->componentFlags.isHovered = false;
      }
      if (newHoveredComponent)
      {
        if (!newHoveredComponent->componentFlags.isHovered)
          newHoveredComponent->mouseEnter(getRelativeEvent(e, newHoveredComponent));
        newHoveredComponent->componentFlags.isHovered = true;
      }

      mouseHoveredComponent_ = newHoveredComponent;
    }

    if (mouseHoveredComponent_ && forceMouseMove)
    {
      auto relativeEvent = getRelativeEvent(e, mouseHoveredComponent_);
      mouseHoveredComponent_->mouseMove(relativeEvent);
    }
  }

  void Renderer::handleMouseMove(MouseEvent e)
  {
    // things might disappear without having moved the mouse pointer
    // therefore we need to check hovered component
    if (lastMousePosition_ == Point{ e.x, e.y } &&
      (mouseHoveredComponent_ && mouseHoveredComponent_->isShowing()))
      return;

    if (mouseDownComponent_)
    {
      auto relativeEvent = getRelativeEvent(e, mouseDownComponent_);
      mouseDownComponent_->componentFlags.isHovered =
        mouseDownComponent_->contains(Point{ relativeEvent.x, relativeEvent.y });
      if (mouseDownComponent_->mouseDrag(relativeEvent))
        return;
    }

    refreshComponentUnderMouse(e);
  }

  void Renderer::handleMouseDown(MouseEvent e)
  {
    handleMouseMove(e);

    // TODO: handle more than 1 button being pressed

    mouseDownComponent_ = gui->getComponentAt(e.x, e.y, true);

    if (focusedComponent_ && mouseDownComponent_ != focusedComponent_ &&
      // does the old focused component allow losing focus
      focusedComponent_->handleFocus(false, Component::FocusClick, mouseDownComponent_))
    {
      auto oldFocused = focusedComponent_;
      focusedComponent_ = mouseDownComponent_;
      // does the newly clicked component exist and allow gaining focus
      if (mouseDownComponent_ && !mouseDownComponent_->handleFocus(true, Component::FocusClick, oldFocused))
        if (focusedComponent_ == mouseDownComponent_)
          focusedComponent_ = nullptr;
    }

    bool success = false;
    while (mouseDownComponent_)
    {
      mouseDownComponent_->componentFlags.isClicked = true;
      success = mouseDownComponent_->mouseDown(getRelativeEvent(e, mouseDownComponent_));
      // note: mouseDownComponent_ might not be the same as it was before the call
      //       because setClickedComponent() might have been called
      if (success)
        break;
      mouseDownComponent_->componentFlags.isClicked = false;

      mouseDownComponent_ = mouseDownComponent_->parent;
      while (mouseDownComponent_ && !mouseDownComponent_->componentFlags.clickable)
        mouseDownComponent_ = mouseDownComponent_->parent;
    }

    // if the component has decided to not handle further mouse events
    // this makes the upcoming mouse events orphaned,
    // which can be handled by other components if acceptsOrphanMouseEvents == true
    if (success && (!mouseDownComponent_ || !mouseDownComponent_->componentFlags.isClicked))
    {
      if (mouseDownComponent_)
      {
        mouseDownComponent_->componentFlags.isScrollbarYClicked = false;
        mouseDownComponent_->componentFlags.isScrollbarXClicked = false;
      }

      mouseDownComponent_ = nullptr;
      isHandlingOrphanedMouseEvents = true;
    }
  }

  void Renderer::handleMouseUp(MouseEvent e)
  {
    handleMouseMove(e);

    // TODO: handle more than 1 button being pressed

    bool exited = mouseHoveredComponent_ != mouseDownComponent_;

    if (mouseDownComponent_)
    {
      auto event = getRelativeEvent(e, mouseDownComponent_);

      auto *oldMouseDownComponent = mouseDownComponent_;

      //if (oldMouseDownComponent->contains(Point{ event.x, event.y }))
      {
        if (!oldMouseDownComponent->mouseUp(event))
          return;
      }

      mouseDownComponent_ = nullptr;

      oldMouseDownComponent->componentFlags.isClicked = false;
      oldMouseDownComponent->componentFlags.isScrollbarYClicked = false;
      oldMouseDownComponent->componentFlags.isScrollbarXClicked = false;
      if (exited)
        oldMouseDownComponent->mouseExit(event);
    }

    if (exited && mouseHoveredComponent_)
    {
      if (!mouseHoveredComponent_->componentFlags.isHovered && !mouseHoveredComponent_->componentFlags.isClicked)
        mouseHoveredComponent_->mouseEnter(getRelativeEvent(e, mouseHoveredComponent_));
      mouseHoveredComponent_->componentFlags.isHovered = true;
      if (isHandlingOrphanedMouseEvents && mouseHoveredComponent_->componentFlags.acceptsOrphanMouseEvents)
      {
        isHandlingOrphanedMouseEvents = false;
        mouseHoveredComponent_->mouseUp(getRelativeEvent(e, mouseHoveredComponent_));
      }
    }
  }

  void Renderer::handleMouseEnter([[maybe_unused]] MouseEvent e)
  {
    // nothing necessary for now, pugl calls mouse move immediately after this
  }

  void Renderer::handleMouseLeave(MouseEvent e)
  {
    // this function is expected to be called while nothing is clicked

    if (mouseHoveredComponent_)
    {
      auto mouseHoveredComponent = mouseHoveredComponent_;
      mouseHoveredComponent_ = nullptr;

      if (mouseHoveredComponent->componentFlags.isHovered)
        mouseHoveredComponent->mouseExit(getRelativeEvent(COMPLEX_MOVE(e), mouseHoveredComponent));
      mouseHoveredComponent->componentFlags.isHovered = false;
    }
  }

  void Renderer::handleMouseWheel(MouseEvent e)
  {
    handleMouseMove(e);

    utils::vector<Component *> stack{ getLocalScratch(), 32 };
    auto *c = mouseHoveredComponent_;
    while (c)
    {
      stack.emplaceBack(c);
      c = c->parent;
    }

    while (!stack.empty())
    {
      if (stack.back()->mouseWheelMove(getRelativeEvent(e, stack.back())))
        break;
      stack.popBack();
    }

    //if (!mouseDownComponent_ && mouseHoveredComponent_)
    //  mouseHoveredComponent_->mouseWheelMove(e);
  }

  void Renderer::handleKeyPress(KeyPress k)
  {
    for (auto f = focusedComponent_; f; f = f->parent)
      if (focusedComponent_->keyPressed(k))
        return;

    // so that no keyboard input is missed
    gui->keyPressed(k);
  }
  
  void Renderer::recalculateScale(bool forceResize)
  {
    auto newEffectiveScale = getEffectiveScale();
    if (newEffectiveScale == generalData.scale && !forceResize)
      return;

    area = { (u32)::roundf((float)area.w * newEffectiveScale / generalData.scale),
      (u32)::roundf((float)area.h * newEffectiveScale / generalData.scale) };

    generalData.scale = newEffectiveScale;
    getUiRelated() = &generalData;
    defer { getUiRelated() = nullptr; };
    // the UI needs to be sized before we can proceed with checking
    if (isInitialised && !gui->bounds.isEmpty())
      doSizingAndPositioning();

    area = checkResizing(area, true);

    plugin.hostContext->requestResize(plugin.hostContext, area.w, area.h);
  }
  
  void Renderer::checkFocusedComponent()
  {
    if (!focusedComponent_ || focusedComponent_->componentFlags.isVisible)
      return;

    auto next = focusedComponent_->parent;
    while (next)
    {
      // both parties must agree to take over/relinquish focus from/to the other
      if (next->componentFlags.isVisible &&
        focusedComponent_->handleFocus(false, Component::FocusSetInvisible, next) &&
        next->handleFocus(true, Component::FocusSetInvisible, focusedComponent_))
      {
        focusedComponent_ = next;
        return;
      }
      next = next->parent;
    }

    focusedComponent_ = nullptr;
  }
  
  void Renderer::doSizingAndPositioning()
  {
    auto [unscaledWidth, unscaledHeight] = unscaleDimensions(area.w, area.h, generalData.scale);
    gui->desiredSize = { (i32)unscaledWidth, (i32)unscaledHeight, (i32)unscaledWidth, (i32)unscaledHeight };

    for (auto &[c, callback] : callbacks.data)
      callback(c);

    utils::vector<Component *> customPlacement_{ getLocalScratch(), 32 };
    customPlacement = &customPlacement_;
    utils::vector<Component *> sortedSizesMin_{ getLocalScratch(), 32 };
    sortedSizesMin = &sortedSizesMin_;
    utils::vector<Component *> sortedSizesMax_{ getLocalScratch(), 32 };
    sortedSizesMax = &sortedSizesMax_;

    calculateSizes(gui->children, gui);
    gui->bounds.x = 0;
    gui->bounds.y = 0;
    calculatePositions(gui->children, gui, gui->bounds);

    // looping until all conflicts are resolved
    // BEWARE of circular dependencies
    while (!customPlacement->empty())
    {
      auto c = customPlacement->front();
      customPlacement->popFront();
      c->componentFlags.isPositionSet = c->overridePosition(c);
      if (!c->componentFlags.isPositionSet)
        customPlacement->emplaceBack(c);
    }
    customPlacement->clear();
  }
  
  void Renderer::renderLoop(PuglView *view)
  {
  #ifdef COMPLEX_HOTRELOAD_DIR
    if (watchFileContext)
      xfiles_watch_flush(watchFileContext);
  #endif

    ++numberOfFrames;
    if (numberOfFrames == 200)
    {
      utils::shrinkWorkingSet();
    }

    auto newRenderTime = puglGetTime(puglGetWorld(view));
    generalData.deltaTime = (float)(newRenderTime - generalData.steadyTime);
    generalData.steadyTime = newRenderTime;
    graph.updateGraph((float)generalData.deltaTime);

    auto state = plugin.state_;
    for (usize i = 0; i < state->parameterBridges.size(); ++i)
      state->parameterBridges[i].updateUIParameter();

    doSizingAndPositioning();

    refreshComponentUnderMouse(getMouseInteractions().mouseState, false);
    checkFocusedComponent();

    // Reset viewport
    glViewport(0, 0, area.w, area.h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(*generalData.g, (float)area.w, (float)area.h, getUiRelated()->scale);

    gui->doRender(*generalData.g);

    if (renderDebugFps)
    {
      nvgReset(*generalData.g);
      auto text = utils::floatToString(getLocalScratch(), 1.0f / graph.getGraphAverage(), 2);
      renderText(text, FontId::DDinType, scaleValue({ 4.0f, 4.0f, 24.0f, 24.0f }).toInt().toFloat(),
        *generalData.g, Colours::white, Placement::left);
    }

    nvgEndFrame(*generalData.g);

    // calling swapBuffers inside the critical section in case
    // we're resizing because a glFinish is necessary in order to
    // not get frame tearing/overlap with previous frames
    // https://community.khronos.org/t/swapbuffers-and-synchronization/107667/5
    puglSwapBuffers(view);
    if (isResizing)
      glFinish();

    u32 a, b;
    Framework::LoadSave::getWindowSizeScale(a, b, pluginScale);
    recalculateScale();
  }
}

extern "C"
{
  void *cplug_createGUI(void *userPlugin)
  {
    auto &renderer = ((Plugin::ComplexPlugin *)userPlugin)->renderer;
    renderer.startUI();
    return &renderer;
  }

  void cplug_destroyGUI(void *userGUI)
  {
    ((Interface::Renderer *)userGUI)->stopUI();
  }

  void cplug_setParent(void *userGUI, void *newParent)
  {
    auto renderer = (Interface::Renderer *)userGUI;
    if (!renderer->view_)
      return;

    if (puglGetParent(renderer->view_))
    {
      puglStopTimer(renderer->view_, Interface::Renderer::kTimerRefreshRate);
      puglSetParent(renderer->view_, (PuglNativeView)nullptr);
      puglUnrealize(renderer->view_);
    }

    if (newParent)
    {
      puglSetParent(renderer->view_, (PuglNativeView)newParent);
      [[maybe_unused]] PuglStatus status = puglRealize(renderer->view_);
      COMPLEX_ASSERT(status == PUGL_SUCCESS);
      puglStartTimer(renderer->view_, Interface::Renderer::kTimerRefreshRate, 1.0 / renderer->fps);

      auto monitorInfo = Interface::getCurrentMonitorInfo(newParent);
      renderer->monitorScale = monitorInfo.dpiScale;

      Framework::LoadSave::getWindowSizeScale(renderer->area.w, renderer->area.h, renderer->pluginScale);
      renderer->generalData.scale = 1.0f;
      renderer->recalculateScale(true);

      //clampScaleWidthHeight(renderer->view_, renderer->effectiveScale, windowWidth, windowHeight);

      //renderer->plugin.hostContext->requestResize(renderer->plugin.hostContext, (u32)windowWidth, (u32)windowHeight);
    }
  }

  void cplug_setVisible(void *userGUI, bool visible)
  {
    auto *renderer = (Interface::Renderer *)userGUI;
    if (!renderer->view_ || !renderer->view_->parent)
      return;

    if (visible)
    {
      puglShow(renderer->view_, PUGL_SHOW_RAISE);

      // calculate actual sizes when the window first appears
      if (!renderer->isVisible)
      {

      }
      renderer->isVisible = visible;
      //puglSetSizeHint(renderer->view_, PUGL_DEFAULT_SIZE, (u32)windowWidth, (u32)windowHeight);
    }
    else
    {
      //auto [unscaledWidth, unscaledHeight] = unscaleDimensions(renderer->area.w, renderer->area.h, renderer->pluginScale);
      //Framework::LoadSave::saveWindowSizeScale(unscaledWidth, unscaledHeight, renderer->pluginScale);

      puglHide(renderer->view_);
    }
  }

  void cplug_setScaleFactor(void *userGUI, float scale)
  {
    auto *renderer = (Interface::Renderer *)userGUI;
    renderer->monitorScale = scale;
    renderer->recalculateScale();
  }
  void cplug_getSize(void *userGUI, uint32_t *width, uint32_t *height)
  {
    auto *renderer = (Interface::Renderer *)userGUI;
    *width = (u32)renderer->area.w;
    *height = (u32)renderer->area.h;
  }
  bool cplug_setSize(void *userGUI, uint32_t width, uint32_t height)
  {
    auto *renderer = (Interface::Renderer *)userGUI;
    puglSetSizeHint(renderer->view_, PUGL_CURRENT_SIZE, width, height);

    return true;
  }
  void cplug_checkSize(void *userGUI, uint32_t *width, uint32_t *height)
  {
    auto renderer = (Interface::Renderer *)userGUI;
    Interface::getUiRelated() = &renderer->generalData;
    defer { Interface::getUiRelated() = nullptr; };
    auto [w, h] = Interface::checkResizing({ *width, *height });
    *width = w;
    *height = h;
  }
}
