/*
  ==============================================================================

    EffectsStateSection.hpp
    Created: 3 Feb 2023 6:41:53pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Components/DraggableComponent.hpp"
#include "../Components/LaneSelector.hpp"

namespace Generation
{
  class EffectsState;
}

namespace Interface
{
  class EffectModuleSection;
  class EffectsLaneSection;

  class EffectsStateSection final : public ProcessorSection, public DraggableComponent::Listener
  {
  public:
    static constexpr int kLaneSelectorHeight = 38;

    static constexpr int kLaneSelectorToLanesMargin = 8;
    static constexpr int kLaneToLaneMargin = 4;

    EffectsStateSection(Generation::EffectsState &state);
    ~EffectsStateSection() override;

    void resized() override;

    // Inherited via DraggableComponent::Listener
    EffectModuleSection *prepareToMove(EffectModuleSection *movedModule, const MouseEvent &e, bool isCopying = false) override;
    void draggingComponent(EffectModuleSection *component, const MouseEvent &e) override;
    void releaseComponent(EffectModuleSection *movedModule, const MouseEvent &e) override;
    Point<int> mouseWheelWhileDragging(EffectModuleSection *movedModule, const MouseEvent &e) override;

    void createLane(EffectsLaneSection *laneToCopy = nullptr);
    void deleteLane(EffectsLaneSection *laneToDelete);
    std::optional<usize> getLaneIndex(EffectsLaneSection *lane) noexcept;

  private:
    std::vector<utils::up<EffectsLaneSection>> lanes_;

    LaneSelector laneSelector_{};
    u32 laneViewStartIndex_ = 0;

    utils::up<EffectModuleSection> movedModuleCopy_;
    utils::pair<usize, usize> dragStartIndices_{};

    Generation::EffectsState &state_;
  };
}
