
// Created: 2022-10-10 18:02:52

#pragma once

#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../Components/BaseControl.hpp"
#include "../Sections/Popups.hpp"

extern "C" void cplug_checkSize(void *userGUI, u32 *width, u32 *height);

namespace Interface
{
  class NumberBox;
  class TextSelector;
  class Spectrogram;
  class EffectsStateSection;
  class PopupDisplay;
  class PopupSelector;

  struct TopBar final : public Component
  {
    void reinitialise();

    Label gainLabel{};
    Numberbox gain{};
    Component gainGroup{};

    Label mixLabel{};
    Numberbox mix{};
    Component mixGroup{};
  };

  struct BottomBar final : public Component
  {
    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;

    Label blockSizeLabel{};
    Numberbox blockSize{};
    Component blockSizeGroup{};

    Label overlapLabel{};
    Numberbox overlap{};
    Component overlapGroup{};

    Label windowLabel{};
    Numberbox windowAlpha{};
    TextSelector window{};
    Component windowGroup{};
  };

  class ResizeCorner final : public Component
  {
  public:
    static constexpr i32 kWidth = 15;
    static constexpr i32 kHeight = 15;

    Area<u32> areaAtMouseDown{};

    ResizeCorner()
    {
      componentFlags.clickable = true;
      desiredSize = { kWidth, kHeight, kWidth, kHeight };

      placement = Placement::custom;
    }

    void handleCommandMessage(u64 commandId, [[maybe_unused]] utils::whatever extraData) override
    {
      switch (commandId)
      {
      case HandleCustomPosition:
        bounds = bounds.withPosition(parent->bounds.w - bounds.w, parent->bounds.h - bounds.h);
        break;
      }
    }

    bool
    mouseEnter(const MouseEvent &) override
    {
      Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::UpLeftDownRightResize);
      return true;
    }
    bool
    mouseExit(const MouseEvent &) override
    {
      Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
      return true;
    }
    bool
    mouseDown(const MouseEvent &) override
    {
      areaAtMouseDown = getUISize(uiRelated.renderer);
      return true;
    }
    bool
    mouseDrag(const MouseEvent &event) override
    {
      auto offset = event.getOffsetFromDragStart();
      u32 w = areaAtMouseDown.w + offset.x;
      u32 h = areaAtMouseDown.h + offset.y;
      cplug_checkSize(uiRelated.renderer, &w, &h);
      setUISize(uiRelated.renderer, w, h);
      return true;
    }
  };

  class MainInterface final : public Component
  {
  public:
    MainInterface();
    ~MainInterface();

    bool render(OpenGlWrapper &openGl) override;

    //void controlValueChanged(Control *control) override;

    PopupSelector *getPopupSelector() { return &popupSelector; }
    PopupDisplay *
    getPopupDisplay(bool primary = true)
    { return (primary) ? &popupDisplay1 : &popupDisplay2; }

    void reinstantiateUI();

    TopBar topBar{};
    BottomBar bottomBar{};
    ResizeCorner resizeCorner{};

    //utils::up<EffectsStateSection> effectsStateSection_;
    PopupSelector popupSelector{};
    PopupDisplay popupDisplay1{};
    PopupDisplay popupDisplay2{};

    //utils::up<Spectrogram> spectrogram_;
  };
}
