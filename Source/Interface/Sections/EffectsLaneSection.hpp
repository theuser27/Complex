
// Created: 2023-02-03 18:42:55

#pragma once

#include "../LookAndFeel/Component.hpp"
#include "../Components/Control.hpp"

namespace Generation
{
  class EffectsLane;
}

namespace Interface
{
  struct SoundEngineSection;
  struct EffectModuleSection;

  struct EffectsLaneSection final : public Component
  {
    static constexpr int kAutoScrollRegion = 100;

    static constexpr int kLeftEdgePadding = 12;
    static constexpr int kRightEdgePadding = 8;

    static constexpr int kGainMatchButtonDimensions = 10;

    static constexpr int kInsideRouding = 4;

    static constexpr float kTimeout = 0.1f;
    static constexpr float kBorderRounding = 8.0f;

    void reinitialise();
    void destroy();

    bool render(OpenGlWrapper &openGl) override;

    Generation::EffectsLane *effectsLane{};
    SoundEngineSection *soundEngineSection{};

    Component header{};
    Component footer{};

    struct ModuleHolder : public Component
    {
      bool mouseEnter(const MouseEvent &e) override;
      bool mouseExit(const MouseEvent &e) override;
      bool mouseMove(const MouseEvent &e) override;
      bool mouseDown(const MouseEvent &e) override;
      bool render(OpenGlWrapper &openGl) override;

      double enterHoverTime{};
      bool hasEnteredHover{};
      float animationValues[1]{};
      Generation::Processor *hoveredBeforeModule{};
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

    utils::sll<CommandMessages::HandleMessageFn *> laneHandler{};
  };
}
