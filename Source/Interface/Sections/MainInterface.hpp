
// Created: 2022-10-10 18:02:52

#pragma once

#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Component.hpp"
#include "../Components/Control.hpp"
#include "../Components/Spectrogram.hpp"
#include "../Sections/Popups.hpp"

namespace Generation
{
  class SoundEngine;
}

namespace Interface
{
  struct EffectsLaneSection;

  struct EffectsSection final : public Component
  {
    static constexpr int kLaneSelectorHeight = 38;

    static constexpr int kLaneSelectorToLanesMargin = 8;
    static constexpr int kLaneToLaneMargin = 4;

    struct LaneSelector final : public Component
    {
      static constexpr Rectangle kScrollDimensions = { 2, 2, 2, 10 };
      static constexpr i32 kScrollRectThickness = 2;
      static constexpr i32 kLaneMiniViewRounding = 2;
      static constexpr i32 kScrollOffset = 2;
      static constexpr i32 kAutoScrollRegion = 50;

      struct AddMoreLanesButton final : public Component
      {
        AddMoreLanesButton() { componentFlags.clickable = true; }
        bool mouseDown(const MouseEvent &e) override;

        bool render(OpenGlWrapper &openGl) override;
      };

      void reinitialise();

      bool render(OpenGlWrapper &openGl) override;
      bool mouseDown(const MouseEvent &e) override { return mouseDrag(e); }
      bool mouseDrag(const MouseEvent &e) override;
      bool mouseWheelMove(const MouseEvent &e) override;



      AddMoreLanesButton addMoreLanesButton{};

      utils::sll<CommandMessages::HandleMessageFn *> laneSelectorHandler{};
    };

    struct LaneHolder : public Component
    {
      bool mouseWheelMove(const MouseEvent &e) override;

      float previousOffsetX{};
      float scrollThreshold{};
    };

    void reinitialise();

    void removeLane(EffectsLaneSection *lane);
    void setStartLaneIndex(u32 newStart, u32 newCount);
    Area<u32> checkResizing(Area<u32> newScaledSize, bool force = false);

    bool render(OpenGlWrapper &openGl) override;

    LaneSelector laneSelector{};
    LaneHolder laneHolder{};

    u32 laneNameOrdinal{};

    u32 startLaneIndex{};
    u32 visibleLaneCount = 1;
    float nextScrollPositionRatio{};
  };

  struct SoundEngineSection final : public Component
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
    Area<u32> checkResizing(Area<u32> newScaledSize, bool force = false);

    bool render(OpenGlWrapper &openGl) override;

    Generation::SoundEngine *soundEngine{};
    utils::sll<CommandMessages::HandleMessageFn *> soundEngineHandler{};

    TopBar topBar{};
    Spectrogram spectrogram{};
    EffectsSection effectsSection{};
    BottomBar bottomBar{};
  };

  Area<u32> checkResizing(Area<u32> newScaledSize, bool force = false);

  struct MainInterface final : public Component
  {
    void restartUI(Plugin::State *state);

    DrawComponent placeholderInsert{};
    PopupSelector popupSelector{};
    PopupDisplay popupDisplay1{};
    PopupDisplay popupDisplay2{};
  };
}
