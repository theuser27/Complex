
// Created: 2022-10-10 18:02:52

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"
#include "Plugin/Renderer.hpp"

extern "C" void cplug_checkSize(void *userGUI, u32 *width, u32 *height);

namespace Interface
{
  class NumberBox;
  class TextSelector;
  class Spectrogram;
  class EffectsStateSection;
  class PopupDisplay;
  class PopupSelector;

  class MainInterface final : public Component
  {
  public:
    MainInterface();
    ~MainInterface();

    bool render(OpenGlWrapper &openGl) override;

    //void controlValueChanged(BaseControl *control) override;

    PopupSelector *getPopupSelector() { return nullptr;/* popupSelector_.get();*/ }
    PopupDisplay *getPopupDisplay(bool primary = true)
    { (void)primary; return nullptr; /*(primary) ? popupDisplay1_.get() : popupDisplay2_.get();*/ }

    void reinstantiateUI();

  private:
    class ResizeCorner : public Component
    {
    public:
      Area<u32> areaAtMouseDown{};

      ResizeCorner()
      {
        static constexpr i32 resizeCornerWidth = 10;
        static constexpr i32 resizeCornerHeight = 10;

        desiredSize.minMax = { resizeCornerWidth, resizeCornerHeight,
          resizeCornerWidth, resizeCornerHeight };

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

      bool mouseEnter(const MouseEvent &) override
      {
        Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::UpLeftDownRightResize);
        return true;
      }
      bool mouseExit(const MouseEvent &) override
      {
        Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
        return true;
      }
      bool mouseDown(const MouseEvent &) override
      {
        areaAtMouseDown = getUISize(uiRelated.renderer);
        return true;
      }
      bool mouseDrag(const MouseEvent &event) override
      {
        auto offset = event.getOffsetFromDragStart();
        u32 w = areaAtMouseDown.w + offset.x;
        u32 h = areaAtMouseDown.h + offset.y;
        cplug_checkSize(uiRelated.renderer, &w, &h);
        setUISize(uiRelated.renderer, w, h);
        return true;
      }
    } resizeCorner;

    //utils::up<EffectsStateSection> effectsStateSection_;
    //utils::up<PopupSelector> popupSelector_;
    //utils::up<PopupDisplay> popupDisplay1_;
    //utils::up<PopupDisplay> popupDisplay2_;

    //Component topBar{};
    //utils::up<NumberBox> mixNumberBox_;
    //utils::up<NumberBox> gainNumberBox_;

    //utils::up<Spectrogram> spectrogram_;

    //Component bottomBar{};
    //utils::up<NumberBox> blockSizeNumberBox_;
    //utils::up<NumberBox> overlapNumberBox_;
    //utils::up<TextSelector> windowTypeSelector_;
    //utils::up<NumberBox> windowAlphaNumberBox_;

    Colour backgroundColour_{};
  };
}
