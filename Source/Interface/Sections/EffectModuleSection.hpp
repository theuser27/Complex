
// Created: 2023-02-14 02:29:16

#pragma once

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "../Components/DraggableComponent.hpp"
#include "../Components/Control.hpp"

namespace Generation
{
  class EffectModule;
}

namespace Interface
{
  class EmptySlider;
  class SpectralMaskComponent;
  struct EffectsLaneSection;

  struct EffectModuleSection final : public Component
  {
    static constexpr int kTopMenuHeight = 28;
    static constexpr int kDraggableSectionWidth = 36;
    static constexpr int kIconSize = 16;
    static constexpr int kIconToTextSelectorMargin = 4;
    static constexpr int kDelimiterWidth = 1;
    static constexpr int kPowerButtonMargin = 4;
    static constexpr int kLabelToNumberBoxMargin = 4;
    static constexpr int kPowerButtonPadding = 8;

    static constexpr int kOuterPixelRounding = 8;
    static constexpr int kInnerPixelRounding = 3;

    bool render(Graphics &g) override;

    void reinitialise();
    void destroy();
    void restartEffectUI();

    bool mouseDown(const MouseEvent &e) override;

    Generation::EffectModule *effectModule{};
    EffectsLaneSection *laneSection{};
    utils::bumpArena *effectArena{};
    utils::span<Control *> effectControls{};

    struct SpectralMaskComponent final : public PinBoundsBox
    {
      struct EmptySlider final : public PinSlider
      {
        EmptySlider();

        bool mouseDown(const MouseEvent &e) override;
        bool render(Graphics &) override { return false; }

      } shiftBounds{};

      SpectralMaskComponent();
      bool render(Graphics &g) override;

    } maskComponent{};

    struct EffectHolder : public Component
    {
      struct Header : public Component
      {
        void reinitialise();

        DraggableComponent draggableBox{};
        DrawComponent effectTypeIcon{};
        TextSelector effectTypeSelector{};
        Numberbox mixNumberBox{};
        PowerButton moduleActivator{};
        NSVGimage *icon{};

      } header{};

      void reinitialise();

      bool render(Graphics &g) override;

    } effectHolder{};
  };
}
