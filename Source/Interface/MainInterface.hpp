
// Created: 2022-10-10 18:02:52

#pragma once

#include "Component.hpp"
#include "Control.hpp"
#include "Spectrogram.hpp"
#include "Popups.hpp"

class SoundEngine;

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

      bool render(Graphics &g) override;
    };

    void reinitialise();

    bool render(Graphics &g) override;
    bool mouseDown(const MouseEvent &e) override { return mouseDrag(e); }
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &e) override;

    void selectViewedLane(const MouseEvent &e);

    AddMoreLanesButton addMoreLanesButton{};

    utils::sll<CommandMessages::HandleMessageFn *> laneSelectorHandler{};
  };

  struct LaneHolder : public Component
  {
    bool mouseWheelMove(const MouseEvent &e) override;
    bool render(Graphics &g) override;

    float previousOffsetX{};
    float scrollThreshold{};
  };

  void reinitialise();

  void removeLane(EffectsLaneSection *lane);
  void setStartLaneIndex(u32 newStart, u32 newCount);
  Area<u32> checkResizing(Area<u32> newScaledSize, bool force = false);

  bool render(Graphics &g) override;

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
    struct FpsGraph final : public Component
    {
      float values[128]{};
      u32 head{};
      u32 validValues{};
      float cachedValue{};

      FpsGraph()
      {
        overrideSize = [](Component *c, bool isCalculatingVertical)
        {
          auto *self = (FpsGraph *)c;
          if (!isCalculatingVertical)
          {
            self->updateGraph(getUiRelated()->deltaTime);
            self->cachedValue = 1.0f / self->getGraphAverage();
          }

          char string[16];
          usize size = utils::floatToString(self->cachedValue, string, countof(string), 2);

          return calculateSingleLineTextSize(c, isCalculatingVertical,
            FontId::DDinType, { string, size });
        };
      }

      float
      getGraphAverage()
      {
        float avg = 0;
        for (usize i = 0; i < validValues; i++)
          avg += values[i];

        return avg / (float)validValues;
      }

      void updateGraph(float frameTime)
      {
        head = (head + 1) % countof(values);
        validValues = utils::min(validValues + 1, (u32)countof(values));
        values[head] = frameTime;
      }

      bool
      render(Graphics &g) override
      {
        char string[16];
        usize size = utils::floatToString(cachedValue, string, countof(string), 2);
        renderText({ string, size }, FontId::DDinType, getLocalBounds().toFloat(),
          g, Colours::white, Placement::left);

        return true;
      }
    };

    struct UndoRedoButton final : public Component
    {
      static constexpr int kAddedMargin = 4;
      static constexpr int kDimensions = 10;
      static constexpr float kHoverIncrement = 0.1f;

      UndoRedoButton()
      {
        componentFlags.clickable = true;
        padding = { kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
        desiredSize = { kDimensions, kDimensions, kDimensions, kDimensions };
      }

      bool mouseDown(const MouseEvent &) override { return true; }
      bool mouseUp(const MouseEvent &event) override;
      bool render(Graphics &g) override;

      bool isRedo = false;
      float rectRoundingRatio = 0.25f;
      float animationValues[1]{};
    };

    void reinitialise();

    FpsGraph fpsGraph{};

    Component undoRedoGroup{};
    UndoRedoButton undoButton{};
    UndoRedoButton redoButton{};

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

    bool render(Graphics &g) override;

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

  bool render(Graphics &g) override;

  SoundEngine *soundEngine{};
  utils::sll<CommandMessages::HandleMessageFn *> soundEngineHandler{};
  bool showFps = true;

  TopBar topBar{};
  Spectrogram spectrogram{};
  EffectsSection effectsSection{};
  BottomBar bottomBar{};
};

Area<u32> checkResizing(Area<u32> newScaledSize, bool force = false);

struct MainInterface final : public Component
{
  static void restartUI(State *state);

  bool keyPressed(const KeyPress &key) override;

  DrawComponent placeholderInsert{};
  PopupSelector popupSelector{};
  PopupDisplay popupDisplay1{};
  PopupDisplay popupDisplay2{};
};
