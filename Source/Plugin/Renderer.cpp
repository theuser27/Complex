
// Created: 2022-12-20 19:34:54

#include "Renderer.hpp"

#include "cplug/cplug.h"

#define PUGL_NO_INCLUDE_GL_H
#include "pugl/gl.h"
#include "pugl/src/types.h"

#include "Framework/load_save.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Framework/sync_primitives.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/LookAndFeel/Shaders.hpp"
#include "Interface/LookAndFeel/Graphics.hpp"
#include "Interface/LookAndFeel/Miscellaneous.hpp"
#include "Interface/LookAndFeel/BaseComponent.hpp"
#include "Interface/Sections/MainInterface.hpp"

static auto 
unscaleDimensions(int width, int height, double currentScaling) noexcept
{
  return utils::pair{ (int)::round((double)width / currentScaling),
    (int)::round((double)height / currentScaling) };
}

static void clampScaleWidthHeight(PuglView *view, float &desiredScale, u32 &windowWidth, u32 &windowHeight)
{
  using namespace Interface;

  auto nativeHandle = puglGetNativeView(view);
  nativeHandle = nativeHandle ? nativeHandle : puglGetParent(view);

  auto info = Interface::getCurrentMonitorInfo((void *)nativeHandle);

  // the available display area on screen for the window
  Interface::Rectangle<i32> displayArea = info.totalArea;

  desiredScale = ::roundf(utils::clamp(desiredScale, kMinWindowScaleFactor, kMaxWindowScaleFactor) 
    / kWindowScaleIncrements) * kWindowScaleIncrements;

  auto clampScaleToDisplay = [&](float current, float display)
  {
    while (display > 0.0f && current * desiredScale > display)
    {
      if (desiredScale == kMinWindowScaleFactor)
        break;
      desiredScale -= kWindowScaleIncrements;
    }
  };

  float displayWidth = (float)displayArea.w;
  clampScaleToDisplay((float)windowWidth, displayWidth);

  while (desiredScale * (float)windowWidth > displayWidth)
  {
    if (windowWidth <= kMinWidth)
    {
      windowWidth = kMinWidth;
      break;
    }
    windowWidth -= kAddedWidthPerLane;
  }

  float displayHeight = (float)displayArea.h;
  clampScaleToDisplay((float)windowHeight, displayHeight);

  windowHeight = utils::clamp(windowHeight, (u32)kMinHeight, (u32)::floorf(displayHeight / desiredScale));
}

namespace Interface
{
  // deferred components that will set their own position
  constinit utils::vector<Component *> *customPlacement{};

  class Renderer
  {
  public:
    static constexpr double kMinOpenGlVersion = 1.4;
    enum TimerTypes { kTimerParameterUpdate };

    Area<u32> area{};
    float scale = 1.0f;
    bool isInitialised = false;
    bool isVisible = false;
    bool isResizing = false;
    bool hasEnteredResizeCorner = false;
    float fps = 60.0f;

    PuglWorld *world_ = nullptr;
    PuglView *view_ = nullptr;

    Plugin::ComplexPlugin &plugin;

    Skin *skinInstance;
    MainInterface *gui;
    Graphics *graphics;

    Component *mouseHoveredComponent_ = nullptr;
    Component *mouseDownComponent_ = nullptr;
    Component *focusedComponent_ = nullptr;
    Component *dragAndDropComponent_ = nullptr;

    ModifierKeys mouseButtonsDown_{};
    ModifierKeys lastKeyboardMods_{};

    Point<i32> lastMousePosition_{ 0, 0 };
    Point<i32> lastMouseDownPosition_{ 0, 0 };

    OpenGlWrapper openGl{};
    Shaders shaders;
    utils::bumpArena *arena{};



    void startUI();
    void stopUI();

    void checkBounds(u32 &newWidth, u32 &newHeight);
    void resizeChange(bool isResizing);
    void moveFocusTo(Component &component);
    //void beginDragAutoRepeat(int millisecondsBetweenCallbacks);

    MouseEvent getRelativeEvent(MouseEvent e, Component *component);

    void handleMouseMove(MouseEvent e);
    void handleMouseDown(MouseEvent e);
    void handleMouseUp(MouseEvent e);
    void handleMouseEnter(MouseEvent e);
    void handleMouseLeave(MouseEvent e);
    void handleMouseWheel(MouseEvent e);

    void teardownGl()
    {
      auto teardownComponent = [&](const auto &self, Component *c) -> void
      {
        if (c->componentFlags.isOpenGlInitialised)
        {
          c->componentFlags.destroyOpenGl = true;
          c->render(openGl);
          COMPLEX_ASSERT(!c->componentFlags.isOpenGlInitialised,
            "Didn't reset initialisation flag after opengl resource destruction");
          c->componentFlags.destroyOpenGl = false;
        }
        for (auto child : c->childComponents)
          self(self, child);
      };

      teardownComponent(teardownComponent, gui);

      openGl.shaders = nullptr;
      shaders.releaseAll();
      graphics->~Graphics();
      graphics = nullptr;
    }

    void checkFocusedComponent()
    {
      if (!focusedComponent_ || focusedComponent_->componentFlags.isVisible)
        return;

      auto *focusedComponent = focusedComponent_;
      focusedComponent_ = nullptr;
      focusedComponent->handleFocus(false, Component::FocusSetInvisible);

      auto next = focusedComponent->parent;
      while (next)
      {
        if (next->componentFlags.isVisible && next->componentFlags.wantsFocus)
        {
          next->handleFocus(true, Component::FocusSetInvisible);
          break;
        }
        next = next->parent;
      }
    }
  };


  PuglStatus
  runThread(PuglView *view, const PuglEvent *event)
  {
    auto *renderer = (Interface::Renderer *)puglGetHandle(view);

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

      // TODO:
      //double versionSupported = OpenGLShaderProgram::getLanguageVersion();
      //if (versionSupported < kMinOpenGlVersion)
      //{
      //	NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Unsupported OpenGl Version",
      //		String{ CharPointer_UTF8{ JucePlugin_Name " requires OpenGL version: " } } + String(kMinOpenGlVersion) +
      //		String("\nSupported version: ") + String(versionSupported));
      //	return false;
      //}

      renderer->openGl.shaders = &renderer->shaders;
      renderer->graphics = anew(renderer->arena, Graphics, {});
      renderer->openGl.cache = renderer->graphics;
      renderer->openGl.g = renderer->graphics->context;
      renderer->isInitialised = true;
      uiRelated.renderer = renderer;

      break;
    }
    case PUGL_CLOSE:
    {
      renderer->teardownGl();
      puglLeaveContext(view);
      renderer->isInitialised = false;
      renderer->area = {};

      break;
    }
    case PUGL_UNREALIZE:
    {
      renderer->teardownGl();
      puglLeaveContext(view);
      renderer->isInitialised = false;
      renderer->area = {};

      break;
    }
    case PUGL_CONFIGURE:
    {
      renderer->resizeChange((event->configure.style & PUGL_VIEW_STYLE_RESIZING) != 0);
      renderer->area = { event->configure.width, event->configure.height };

      break;
    }
    case PUGL_DPI_CHANGE:
      //setScaleFactor(event->dpiChange.newScaleFactor);
      break;

    case PUGL_EXPOSE:
    {
      renderer->gui->desiredSize = { { (i32)renderer->area.w, (i32)renderer->area.h, 
        (i32)renderer->area.w, (i32)renderer->area.h } };

      utils::vector<Component *> customPlacement_{ localScratch, 8 };
      customPlacement = &customPlacement_;

      calculateSizes(renderer->gui->childComponents, renderer->gui);
      renderer->gui->bounds.setPosition({});
      calculatePositions(renderer->gui->childComponents, renderer->gui, renderer->gui->bounds);

      for (auto *c : *customPlacement)
        c->handleCommandMessage(Component::HandleCustomPosition);
      customPlacement->clear();

      renderer->checkFocusedComponent();

      renderer->openGl.parentStack.emplace_back(renderer->gui, renderer->gui->bounds, true);
      renderer->openGl.parentStack.emplace_back(ScopedBoundsEmplace::doNotAddFlag);
      renderer->gui->doRender(renderer->openGl);

      // calling swapBuffers inside the critical section in case 
      // we're resizing because a glFinish is necessary in order to
      // not get frame tearing/overlap with previous frames
      // https://community.khronos.org/t/swapbuffers-and-synchronization/107667/5
      puglSwapBuffers(view);
      if (renderer->isResizing)
        glFinish();
      
      break;
    }
    case PUGL_UPDATE: break;
    case PUGL_LOOP_ENTER: break;
    case PUGL_LOOP_LEAVE: break;
    case PUGL_KEY_PRESS: break;
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
      e.x = (int)::round(event->crossing.x);
      e.y = (int)::round(event->crossing.y);
      e.mods |= ((event->crossing.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->crossing.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->crossing.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_.flags;
      e.mouseDownPosition = renderer->lastMouseDownPosition_;

      if (event->type == PUGL_POINTER_IN)
        renderer->handleMouseEnter(COMPLEX_MOVE(e));
      else
        renderer->handleMouseLeave(COMPLEX_MOVE(e));

      renderer->lastMousePosition_ = { e.x, e.y };
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
      e.x = (int)::round(event->button.x);
      e.y = (int)::round(event->button.y);
      e.mods |= ((event->button.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->button.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->button.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = { e.x, e.y };

      if (event->type == PUGL_BUTTON_PRESS)
      {
        renderer->handleMouseDown(COMPLEX_MOVE(e));
        renderer->lastMouseDownPosition_ = { e.x, e.y };
      }
      else
        renderer->handleMouseUp(COMPLEX_MOVE(e));

      renderer->lastMousePosition_ = { e.x, e.y };
      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;

      break;
    }
    case PUGL_MOTION:
    {
      MouseEvent e;
      e.x = (int)::round(event->motion.x);
      e.y = (int)::round(event->motion.y);
      e.mods |= ((event->motion.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->motion.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->motion.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = { e.x, e.y };

      renderer->handleMouseMove(COMPLEX_MOVE(e));

      renderer->lastMousePosition_ = { e.x, e.y };
      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;

      break;
    }
    case PUGL_SCROLL:
    {
      MouseEvent e;
      e.x = (int)::round(event->scroll.x);
      e.y = (int)::round(event->scroll.y);
      e.mods |= ((event->scroll.state & PUGL_MOD_SHIFT) != 0) ? ModifierKeys::shiftModifier : 0;
      e.mods |= ((event->scroll.state & PUGL_MOD_CTRL) != 0) ? ModifierKeys::ctrlModifier : 0;
      e.mods |= ((event->scroll.state & PUGL_MOD_ALT) != 0) ? ModifierKeys::altModifier : 0;
      e.mods |= renderer->mouseButtonsDown_;
      e.mouseDownPosition = renderer->lastMouseDownPosition_;
      e.wheelDeltaX = (float)event->scroll.dx;
      e.wheelDeltaY = (float)event->scroll.dy;

      renderer->handleMouseWheel(COMPLEX_MOVE(e));

      renderer->lastKeyboardMods_ = e.mods & ModifierKeys::allKeyboardModifiers;
      renderer->lastMousePosition_ = { e.x, e.y };

      break;
    }
    case PUGL_TIMER:
    {
      if (event->timer.id == Renderer::kTimerParameterUpdate)
      {
        auto state = renderer->plugin.state_;
        for (usize i = 0; i < state->parameterBridges.size(); ++i)
          state->parameterBridges[i].updateUIParameter();
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

  Renderer *
  createRenderer(Plugin::ComplexPlugin &plugin)
  {
    auto *arena = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(1));

    auto *renderer = anew(arena, Renderer, { .plugin = plugin, 
      .shaders = { arena }, .arena = arena });
    uiRelated.renderer = renderer;
    renderer->skinInstance = anew(arena, Skin, {});
    uiRelated.skin = renderer->skinInstance;
    renderer->gui = anew(arena, MainInterface, {});

    return renderer;
  }

  void destroyRenderer(Renderer *renderer)
  {
    customPlacement = {};
    renderer->gui->~MainInterface();
    utils::bumpArena::destroy(renderer->arena);
  }

  Plugin::ComplexPlugin &getPlugin(Renderer *renderer) { return renderer->plugin; }
  MainInterface *getGui(Renderer *renderer) { return renderer->gui; }
  Skin *getSkin(Renderer *renderer) { return renderer->skinInstance; }
  PuglView *getPuglView(Renderer *renderer) { return renderer->view_; }
  OpenGlWrapper &getOpenGlContext(Renderer *renderer) { return renderer->openGl; }
  Area<u32> getUISize(Renderer *renderer) { return renderer->area; }

  bool 
  setUISize(Renderer *renderer, u32 width, u32 height)
  {
    return renderer->plugin.hostContext->requestResize(renderer->plugin.hostContext, width, height);
  }

  void setMouseCursor(Renderer *renderer, MouseCursorTypes cursorType)
  {
    if (cursorType <= MouseCursorTypes::AllScroll)
      puglSetCursor(renderer->view_, (PuglCursor)cursorType);
  }

  void setFocusedComponent(Renderer *renderer, Component *component)
  {
    renderer->focusedComponent_ = component;
  }

  void moveFocusTo(Renderer *renderer, Component &component)
  {
    renderer->moveFocusTo(component);
  }

  MouseInteractions 
  getMouseInteractions(Renderer *renderer)
  {
    return MouseInteractions
    { 
      .hovered = renderer->mouseHoveredComponent_,
      .clicked = renderer->mouseDownComponent_,
      .focused = renderer->focusedComponent_,
      .mouseState = 
      {
        .x = renderer->lastMousePosition_.x, 
        .y = renderer->lastMousePosition_.y,
        .mouseDownPosition = renderer->lastMouseDownPosition_,
        .mods = renderer->mouseButtonsDown_ | renderer->lastKeyboardMods_
      }
    };
  }

  void Renderer::startUI()
  {
    if (view_ != nullptr)
      return;
    
    // Create world and view
    world_ = puglNewWorld(PUGL_MODULE, 0);
    view_ = puglNewView(world_);
    
    // load *some* kind of size until we get a window to check if we stretch outside the screen
    Framework::LoadSave::getWindowSizeScale(area.w, area.h, scale);

    // Set up world and view
    puglSetWorldString(world_, PUGL_CLASS_NAME, "ComplexAudioPlugin");
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
    puglSetHandle(view_, this);
    puglSetEventFunc(view_, runThread);

    puglStartTimer(view_, kTimerParameterUpdate, 1.0 / kParameterUpdateIntervalHz);
  }

  void Renderer::stopUI()
  {
    if (view_ == nullptr)
      return;

    puglStopTimer(view_, kTimerParameterUpdate);

    puglUnrealize(view_);
    puglFreeView(view_);
    puglFreeWorld(world_);

    view_ = nullptr;
    world_ = nullptr;
    isVisible = false;
  }

  void Renderer::resizeChange(bool resizeChange)
  {
    if (isResizing && !resizeChange)
    {
      auto [unscaledWidth, unscaledHeight] = unscaleDimensions(area.w, area.h, scale);
      Framework::LoadSave::saveWindowSizeScale(unscaledWidth, unscaledHeight, scale);
    }

    isResizing = resizeChange;
  }


  void Renderer::checkBounds(u32 &newWidth, u32 &newHeight)
  {
    newWidth = utils::max(newWidth, (u32)kMinWidth);
    newHeight = utils::max(newHeight, (u32)kMinHeight);

    // TODO: make resizing snap horizontally to the width of individual lanes 
  }

  void Renderer::moveFocusTo(Component &component)
  {
    if (focusedComponent_ &&
      focusedComponent_->handleFocus(false, Component::FocusMoved))
    {
      focusedComponent_ = &component;
      component.handleFocus(true, Component::FocusMoved);
    }
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

  void Renderer::handleMouseMove(MouseEvent e)
  {
    if (lastMousePosition_ == Point{ e.x, e.y })
      return;

    Component *newHoveredComponent = nullptr;
    auto eventFunc = &Component::mouseMove;

    if (mouseDownComponent_)
    {
      newHoveredComponent = gui->getComponentAt(e.x, e.y);
      bool hoverStealsMouseEvent = mouseDownComponent_ && newHoveredComponent &&
        mouseDownComponent_ != newHoveredComponent && newHoveredComponent->componentFlags.stealsMouseEvents;

      if (!hoverStealsMouseEvent)
      {
        mouseDownComponent_->mouseDrag(getRelativeEvent(e, mouseDownComponent_));
        return;
      }

      eventFunc = &Component::mouseDrag;
    }

    if (!newHoveredComponent)
      newHoveredComponent = gui->getComponentAt(e.x, e.y);

    if (newHoveredComponent != mouseHoveredComponent_)
    {
      if (mouseHoveredComponent_)
      {
        mouseHoveredComponent_->componentFlags.isHovered = false;
        mouseHoveredComponent_->mouseExit(getRelativeEvent(e, mouseHoveredComponent_));
      }
      if (newHoveredComponent)
      {
        newHoveredComponent->componentFlags.isHovered = true;
        newHoveredComponent->mouseEnter(getRelativeEvent(e, newHoveredComponent));
      }

      mouseHoveredComponent_ = newHoveredComponent;
    }

    if (mouseHoveredComponent_)
    {
      auto relativeEvent = getRelativeEvent(e, mouseHoveredComponent_);
      (mouseHoveredComponent_->*eventFunc)(relativeEvent);
    }
  }

  void Renderer::handleMouseDown(MouseEvent e)
  {
    handleMouseMove(e);

    mouseDownComponent_ = gui->getComponentAt(e.x, e.y, true);
    auto *newKeyboardFocusComponent = mouseDownComponent_;
    while (newKeyboardFocusComponent && !newKeyboardFocusComponent->componentFlags.wantsFocus)
      newKeyboardFocusComponent = newKeyboardFocusComponent->parent;

    if (focusedComponent_ && newKeyboardFocusComponent != focusedComponent_ &&
      // does the old focused component allow losing focus
      focusedComponent_->handleFocus(false, Component::FocusClick))
    {
      focusedComponent_ = newKeyboardFocusComponent;
      // does the newly focused component exist and allow gaining focus
      if (focusedComponent_ && !focusedComponent_->handleFocus(true, Component::FocusClick))
        focusedComponent_ = nullptr;
    }

    bool success = false;
    while (mouseDownComponent_)
    {
      mouseDownComponent_->componentFlags.isClicked = true;
      success = mouseDownComponent_->mouseDown(getRelativeEvent(e, mouseDownComponent_));
      if (success)
        break;
      mouseDownComponent_->componentFlags.isClicked = false;

      mouseDownComponent_ = mouseDownComponent_->parent;
      while (mouseDownComponent_ && !mouseDownComponent_->componentFlags.clickable)
        mouseDownComponent_ = mouseDownComponent_->parent;
    }
  }

  void Renderer::handleMouseUp(MouseEvent e)
  {
    handleMouseMove(e);

    bool exited = mouseHoveredComponent_ != mouseDownComponent_;

    if (mouseDownComponent_)
    {
      auto event = getRelativeEvent(e, mouseDownComponent_);

      auto *oldMouseDownComponent = mouseDownComponent_;
      mouseDownComponent_ = nullptr;

      if (exited && mouseHoveredComponent_ && mouseHoveredComponent_->componentFlags.stealsMouseEvents)
      {
        event.originalComponent = oldMouseDownComponent;
        mouseHoveredComponent_->mouseUp(event);
        oldMouseDownComponent->componentFlags.isClicked = false;
      }
      else
        oldMouseDownComponent->mouseUp(event);

      oldMouseDownComponent->componentFlags.isClicked = false;
      if (exited)
        oldMouseDownComponent->mouseExit(event);
    }

    if (exited && mouseHoveredComponent_)
    {
      mouseHoveredComponent_->componentFlags.isHovered = true;
      mouseHoveredComponent_->mouseEnter(getRelativeEvent(e, mouseHoveredComponent_));
    }
  }

  void Renderer::handleMouseEnter([[maybe_unused]] MouseEvent e)
  {
    // nothing necessary for now, pugl calls mouse move immediately after this
  }

  void Renderer::handleMouseLeave(MouseEvent e)
  {
    if (mouseHoveredComponent_)
    {
      auto mouseHoveredComponent = mouseHoveredComponent_;
      mouseHoveredComponent_ = nullptr;

      mouseHoveredComponent->mouseExit(getRelativeEvent(COMPLEX_MOVE(e), mouseHoveredComponent));
      mouseHoveredComponent->componentFlags.isHovered = false;
    }
  }

  void Renderer::handleMouseWheel(MouseEvent e)
  {
    if (!mouseDownComponent_)
      mouseHoveredComponent_->mouseWheelMove(e);
  }
}

extern "C"
{
  void *cplug_createGUI(void *userPlugin)
  {
    auto &renderer = ((Plugin::ComplexPlugin *)userPlugin)->getRenderer();
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
      puglSetParent(renderer->view_, (PuglNativeView)nullptr);
      puglUnrealize(renderer->view_);
    }

    if (newParent)
    {
      puglSetParent(renderer->view_, (PuglNativeView)newParent);
      [[maybe_unused]] PuglStatus status = puglRealize(renderer->view_);
      COMPLEX_ASSERT(status == PUGL_SUCCESS);
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
        u32 windowWidth, windowHeight;
        Framework::LoadSave::getWindowSizeScale(windowWidth, windowHeight, renderer->scale);

        clampScaleWidthHeight(renderer->view_, renderer->scale, windowWidth, windowHeight);
        Framework::LoadSave::saveWindowSizeScale(windowWidth, windowHeight, renderer->scale);
      
        renderer->plugin.hostContext->requestResize(renderer->plugin.hostContext, (u32)windowWidth, (u32)windowHeight);
      }
      renderer->isVisible = visible;
      //puglSetSizeHint(renderer->view_, PUGL_DEFAULT_SIZE, (u32)windowWidth, (u32)windowHeight);
    }
    else
    {
      auto [unscaledWidth, unscaledHeight] = unscaleDimensions(renderer->area.w, renderer->area.h, renderer->scale);
      Framework::LoadSave::saveWindowSizeScale(unscaledWidth, unscaledHeight, renderer->scale);

      puglHide(renderer->view_);
    }
  }

  void cplug_setScaleFactor(void *userGUI, float scale)
  {
    (void)userGUI;
    (void)scale;
    //auto *renderer = (Interface::Renderer *)userGUI;
    //renderer->scale = scale;

    //if (renderer->)
    //clampScaleWidthHeight(renderer->view_, renderer->scale, renderer->area.w, renderer->area.h);

    //puglSetSizeHint(renderer->view_, PUGL_CURRENT_SIZE,
    //  (u32)::round(renderer->area.w * scale), (u32)::round(renderer->area.h * scale));

    //Framework::LoadSave::saveWindowSizeScale(renderer->area.w, renderer->area.h, scale);
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
    ((Interface::Renderer *)userGUI)->checkBounds(*width, *height);
  }
}
