
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

    void reinitialise();
    void destroy();

    bool render(OpenGlWrapper &openGl) override;

    Generation::EffectsLane *effectsLane{};
    SoundEngineSection *soundEngineSection{};

    Component header{};
    Component footer{};

    DrawComponent moduleHolder{};

    TextEditor laneTitle{};
    PowerButton laneActivator{};
    RadioButton gainMatchingButton{};
    Label gainMatchingButtonLabel{};
    TextSelector inputSelector{};
    TextSelector outputSelector{};

    struct AddModulesButton : public Component
    {
      static constexpr float kPlusRelativeSize = 7;
      static constexpr float kBorderRounding = 8.0f;

      bool mouseEnter(const MouseEvent &e) override;
      bool mouseExit(const MouseEvent &e) override;
      bool mouseDown(const MouseEvent &e) override;
      bool render(OpenGlWrapper &openGl) override;

      float animationValues[1]{};
      bool isDropdownOpen{};
      EffectsLaneSection *laneSection{};

    } addModulesButton{};

    utils::sll<CommandMessages::HandleMessageFn *> laneHandler{};
  };
}
