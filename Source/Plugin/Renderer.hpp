/*
  ==============================================================================

    Renderer.hpp
    Created: 20 Dec 2022 19:34:54
    Author:  theuser27

  ==============================================================================
*/

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

  Renderer *createRenderer(Plugin::ComplexPlugin &plugin);
  void destroyRenderer(Renderer *renderer);

  Plugin::ComplexPlugin &getPlugin(Renderer *renderer);
  MainInterface *getGui(Renderer *renderer);
  Skin *getSkin(Renderer *renderer);
  PuglView *getPuglView(Renderer *renderer);

  OpenGlWrapper &getOpenGlContext(Renderer *renderer);

  Area<u32> getUISize(Renderer *renderer);
  bool setUISize(Renderer *renderer, u32 width, u32 height);

  void setMouseCursor(Renderer *renderer, MouseCursorTypes cursorType);

  void setFocusedComponent(Renderer *renderer, Component *component);
  void moveFocusTo(Renderer *renderer, Component &component);

  struct MouseInteractions
  {
    Component *hovered = nullptr;
    Component *clicked = nullptr;
    Component *focused = nullptr;

    MouseEvent mouseState{};
  };

  MouseInteractions getMouseInteractions(Renderer *renderer);
}
