
// Created: 2022-10-10 18:02:52

#pragma once

#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../Components/BaseControl.hpp"
#include "../Components/Spectrogram.hpp"
#include "../Sections/Popups.hpp"

namespace Generation
{
  class SoundEngine;
}

namespace Interface
{
  class EffectsLaneSection;

  struct InvisibleHoverComponent final : public Component
  {
    InvisibleHoverComponent();

    bool render(OpenGlWrapper &openGl) override;

    Component *source{};
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

  struct SoundEngineSection : public Component
  {
    struct TopBar final : public Component
    {
      void reinitialise();

      SoundEngineSection *mainSection{};

      Label gainLabel{};
      Numberbox gain{};
      Component gainGroup{};

      Label mixLabel{};
      Numberbox mix{};
      Component mixGroup{};
    };

    struct EffectsSection final : public Component
    {
      struct LaneSelector : public Component
      {
        bool render(OpenGlWrapper &openGl) override;

        u32 firstVisibleLaneIndex{};
        u32 lastFirstVisibleLaneIndex{};
      };

      static constexpr int kLaneSelectorHeight = 38;

      static constexpr int kLaneSelectorToLanesMargin = 8;
      static constexpr int kLaneToLaneMargin = 4;

      void reinitialise();

      bool render(OpenGlWrapper &openGl) override;

      LaneSelector laneSelector{};
      Component laneHolder{};

    private:
      float scrollXDistanceRatio_{};
    };

    struct BottomBar final : public Component
    {
      void reinitialise();

      bool render(OpenGlWrapper &openGl) override;

      SoundEngineSection *mainSection{};

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

    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;
    bool handleCommandMessage(u64 commandId, utils::whatever<64> extraData) override;

    Generation::SoundEngine *soundEngine{};

    TopBar topBar{};
    Spectrogram spectrogram{};
    EffectsSection effectsSection{};
    BottomBar bottomBar{};
    InvisibleHoverComponent invisibleHover{};
  };

  class MainInterface final : public Component
  {
  public:
    MainInterface();
    ~MainInterface();

    void restartUI();

    //ResizeCorner resizeCorner{};

    PopupSelector popupSelector{};
    PopupDisplay popupDisplay1{};
    PopupDisplay popupDisplay2{};
  };
}
