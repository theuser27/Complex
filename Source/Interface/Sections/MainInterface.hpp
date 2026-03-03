
// Created: 2022-10-10 18:02:52

#pragma once

#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../Components/BaseControl.hpp"
#include "../Components/Spectrogram.hpp"
#include "../Sections/Popups.hpp"

extern "C" void cplug_checkSize(void *userGUI, u32 *width, u32 *height);

namespace Interface
{
  class Spectrogram;

  struct InvisibleHoverComponent : public Component
  {
    InvisibleHoverComponent();

    bool render(OpenGlWrapper &openGl) override;

    Component *source{};
  };

  struct LaneSelector : public Component
  {

    u32 firstVisibleLaneIndex{};
    u32 lastFirstVisibleLaneIndex{};
  };

  struct EffectsStateSection : public ProcessorSection
  {
    static constexpr int kLaneSelectorHeight = 38;

    static constexpr int kLaneSelectorToLanesMargin = 8;
    static constexpr int kLaneToLaneMargin = 4;

    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;

    bool handleCommandMessage(u64 commandId, utils::whatever<64> extraData) override;

    InvisibleHoverComponent invisibleHover{};
    LaneSelector laneSelector{};
    Component laneHolder{};

  private:
    float scrollXDistanceRatio_{};
  };

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

    ResizeCorner();

    bool handleCommandMessage(u64 commandId, utils::whatever<64> extraData) override;

    bool mouseEnter(const MouseEvent &) override;
    bool mouseExit(const MouseEvent &) override;
    bool mouseDown(const MouseEvent &) override;
    bool mouseDrag(const MouseEvent &event) override;

    Area<u32> areaAtMouseDown{};
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

    void reinitialise();

    TopBar topBar{};
    Spectrogram spectrogram{};
    EffectsStateSection effectsStateSection{};
    BottomBar bottomBar{};
    ResizeCorner resizeCorner{};

    PopupSelector popupSelector{};
    PopupDisplay popupDisplay1{};
    PopupDisplay popupDisplay2{};
  };
}
