
// Created: 2023-02-03 18:42:55

#pragma once

#include "../LookAndFeel/Component.hpp"
#include "../Components/Control.hpp"
#include "../Components/DraggableComponent.hpp"

class EffectsLane;

struct SoundEngineSection;
struct EffectModuleSection;

struct EffectsLaneSection final : public Component
{
  enum RenderStage { All, Background, Modules, Highlight };

  static constexpr int kAutoScrollRegion = 100;

  static constexpr int kLeftEdgePadding = 12;
  static constexpr int kRightEdgePadding = 8;

  static constexpr int kGainMatchButtonDimensions = 10;

  static constexpr int kInsideRouding = 4;

  static constexpr float kTimeout = 0.05f;
  static constexpr float kBorderRounding = 8.0f;

  void reinitialise();
  void destroy();

  bool render(Graphics &g) override;

  EffectsLane *effectsLane{};

  Component header{};
  Component footer{};

  struct ModuleHolder : public Component
  {
    bool mouseEnter(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &e) override;
    bool mouseMove(const MouseEvent &e) override;
    bool mouseDown(const MouseEvent &e) override;
    bool render(Graphics &g) override;

    double enterHoverTime{};
    bool hasEnteredHover{};
    float animationValues[1]{};
    Processor *hoveredBeforeModule{};
    usize hoveredBeforeModuleIndex{};
    MouseEvent lastMouseMove{};

  } moduleHolder{};

  TextEditor laneTitle{};
  PowerButton laneActivator{};
  RadioButton gainMatchingButton{};
  Label gainMatchingButtonLabel{};
  TextSelector inputSelector{};
  TextSelector outputSelector{};

  bool isDropdownOpen{};
  RenderStage renderStage = All;

  utils::sll<CommandMessages::HandleMessageFn *> laneHandler{};

  struct LaneMiniView : public DraggableComponent
  {
    static constexpr int kMinWidth = 50;
    static constexpr int kMinHeight = 24;

    LaneMiniView();

    bool mouseDown(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool render(Graphics &g) override;
  };

  LaneMiniView miniView{};
};
