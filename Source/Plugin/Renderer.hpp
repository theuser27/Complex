
// Created: 2022-12-20 19:34:54

#pragma once

#include "pugl/pugl.h"
#include "Interface/LookAndFeel/gui_utils.hpp"

extern "C"
{
  //typedef struct PuglViewImpl PuglView;
  //union PuglEvent;
  union CplugEvent;
}

namespace satomi
{
  template<typename T>
  class atomic;
}

namespace Plugin
{
  struct ComplexPlugin;
}

namespace Interface
{
  class Component;
  class MainInterface;
  class Skin;
  struct OpenGlWrapper;

  class Renderer;

  using PersistentCallback = void(Component *component);

  Renderer *createRenderer(Plugin::ComplexPlugin &plugin);
  void destroyRenderer(Renderer *renderer);

  Plugin::ComplexPlugin &getPlugin(Renderer *renderer);
  MainInterface *getGui(Renderer *renderer);
  Skin *getSkin(Renderer *renderer);
  PuglView *getPuglView(Renderer *renderer);

  OpenGlWrapper &getOpenGlContext(Renderer *renderer);

  Area<u32> getUISize(Renderer *renderer);
  bool setUISize(Renderer *renderer, u32 width, u32 height);
  void setUIScale(Renderer *renderer, float pluginScale);

  void setMouseCursor(Renderer *renderer, MouseCursorTypes cursorType);

  void registerCallback(Renderer *renderer, Component *component, PersistentCallback *callback);
  void deregisterCallback(Renderer *renderer, Component *component);

  void setHoveredComponent(Renderer *renderer, Component *component);
  void setClickedComponent(Renderer *renderer, Component *component);
  void setFocusedComponent(Renderer *renderer, Component *component);
  void moveFocusTo(Renderer *renderer, Component *component);

  struct MouseInteractions
  {
    Component *hovered = nullptr;
    Component *clicked = nullptr;
    Component *focused = nullptr;

    MouseEvent mouseState{};
  };

  MouseInteractions getMouseInteractions(Renderer *renderer);
}
