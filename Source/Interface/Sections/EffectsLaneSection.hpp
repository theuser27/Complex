/*
  ==============================================================================

    EffectsLaneSection.hpp
    Created: 3 Feb 2023 6:42:55pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Components/ScrollBar.hpp"
#include "../LookAndFeel/BaseComponent.hpp"

namespace Generation
{
  class EffectsLane;
}

namespace Interface
{
  class PlainTextComponent;
  class OptionsButton;
  class RadioButton;
  class EffectsStateSection;
  class EffectModuleSection;

  class EffectsLaneSection final : public ProcessorSection
  {
  public:
    static constexpr int kLeftEdgePadding = 12;
    static constexpr int kRightEdgePadding = 8;

    static constexpr int kGainMatchButtonDimensions = 10;

    static constexpr int kInsideRouding = 4;

    EffectsLaneSection(Generation::EffectsLane *effectsLane, 
      EffectsStateSection *state, std::string name = {});
    ~EffectsLaneSection() noexcept override;
    utils::up<EffectsLaneSection> createCopy();

    bool render(OpenGlWrapper &openGl) override;

    void resized() override;
    bool mouseWheelMove(const MouseEvent &e) override;

    void setScrollBarRange()
    {
      scrollBar_.setRangeLimits(0.0, container_.bounds.h);
      scrollBar_.setCurrentRange(scrollBar_.getCurrentRangeStart(), 
        bounds.h, dontSendNotification);
    }

    int scrollLane(const MouseEvent &e)
    {
      auto start = scrollBar_.getCurrentRangeStart();
      mouseWheelMove(e);
      return (int)::round(scrollBar_.getCurrentRangeStart() - start);
    }

    //void insertedSubProcessor(size_t index, Generation::BaseProcessor &newSubProcessor) override;
    //void deletedSubProcessor(size_t index, Generation::BaseProcessor &deletedSubProcessor) override;
    //void movedSubProcessor(Generation::BaseProcessor &subProcessor, Generation::BaseProcessor &sourceProcessor,
    //  usize sourceIndex, Generation::BaseProcessor &destinationProcessor, usize destinationIndex) override;

    void insertModule(size_t index, utils::string_view newModuleType);
    utils::up<EffectModuleSection> deleteModule(const EffectModuleSection *instance, bool createUpdate = true);

    void setEffectPositions();

    size_t getNumModules() const noexcept { return effectModules_.size(); }
    std::optional<size_t> getModuleIndex(EffectModuleSection *effectModuleSection) const
    {
      auto iter = std::ranges::find_if(effectModules_, [effectModuleSection](const auto &pointer)
        { return effectModuleSection == pointer.get(); });
      return (iter != effectModules_.end()) ?
        (size_t)(iter - effectModules_.begin()) : std::optional<size_t>{};
    }

    // needs a point local to the EffectsLaneSection
    usize getIndexFromScreenPositionIgnoringSelf(Rectangle<int> point,
      const EffectModuleSection *moduleSection) const noexcept;

    void setLaneName(std::string newName);

    Component container_{};

    ScrollBar scrollBar_{ true };
    std::vector<utils::up<EffectModuleSection>> effectModules_;

    utils::up<PowerButton> laneActivator_;
    utils::up<RadioButton> gainMatchingButton_;
    utils::up<TextSelector> inputSelector_;
    utils::up<TextSelector> outputSelector_;

    utils::up<OptionsButton> addModulesButton_;

    Generation::EffectsLane *effectsLane_ = nullptr;
    EffectsStateSection *parentState_ = nullptr;
  };
}
