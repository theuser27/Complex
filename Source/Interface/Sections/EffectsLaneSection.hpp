/*
  ==============================================================================

    EffectsLaneSection.hpp
    Created: 3 Feb 2023 6:42:55pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Components/OpenGlImage.hpp"
#include "../Components/OpenGlQuad.hpp"
#include "../Components/ScrollBar.hpp"
#include "../Components/Viewport.hpp"
#include "BaseSection.hpp"

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
  class EffectsLaneSection;

  class EffectsContainer final : public BaseSection
  {
  public:
    EffectsContainer();
    ~EffectsContainer() override;

    void renderOpenGlComponents(OpenGlWrapper &openGl) override
    {
      ScopedBoundsEmplace b1{ openGl.parentStack, this, clipBounds_ };
      ScopedBoundsEmplace b2{ openGl.parentStack, this, getBoundsSafe().withPosition(scrollOffset_) };
      openGl.parentStack.emplace_back(ScopedBoundsEmplace::doNotAddFlag);
      BaseSection::renderOpenGlComponents(openGl);
    }

    void setLane(EffectsLaneSection *lane) noexcept { lane_ = lane; }

    void setClipBounds(juce::Rectangle<int> bounds) noexcept { clipBounds_ = bounds; }
    void setScrollOffset(Point<int> offset) noexcept { scrollOffset_ = offset; }
  private:
    EffectsLaneSection *lane_ = nullptr;
    utils::shared_value<juce::Rectangle<int>> clipBounds_{};
    utils::shared_value<Point<int>> scrollOffset_{};

    utils::up<OptionsButton> addModulesButton_;

    friend class EffectsLaneSection;
  };

  class EffectsLaneSection final : public ProcessorSection, public OpenGlScrollBarListener, 
    OpenGlViewportListener, public Generation::BaseProcessorListener
  {
  public:
    static constexpr int kLeftEdgePadding = 12;
    static constexpr int kRightEdgePadding = 8;

    static constexpr int kGainMatchButtonDimensions = 10;

    static constexpr int kInsideRouding = 4;

    EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *state, String name = {});
    ~EffectsLaneSection() override;
    utils::up<EffectsLaneSection> createCopy();

    void resized() override;
    void renderOpenGlComponents(OpenGlWrapper &openGl) override
    {
      BaseSection::renderOpenGlComponents(openGl);
    }

    void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

    juce::Rectangle<int> getPowerButtonBounds() const noexcept override
    {
      auto widthHeight = scaleValueRoundInt(kDefaultActivatorSize);
      return { getWidth() - scaleValueRoundInt(kRightEdgePadding) - widthHeight,
        utils::centerAxis(widthHeight, scaleValueRoundInt(kEffectsLaneTopBarHeight)),
        widthHeight, widthHeight };
    }

    void scrollBarMoved(ScrollBar *, double rangeStart) override
    { viewport_.setViewPosition(Point{ 0, (int)rangeStart }); }

    void visibleAreaChanged(int, int newY, int, int) override
    {
      setScrollBarRange();
      scrollBar_.setCurrentRange(newY, viewport_.getHeight());
      container_.setScrollOffset(Point{ 0, -newY });
    }
    void setScrollBarRange()
    {
      scrollBar_.setRangeLimits(0.0, container_.getHeight());
      scrollBar_.setCurrentRange(scrollBar_.getCurrentRangeStart(), 
        viewport_.getHeight(), dontSendNotification);
    }

    int scrollLane(const MouseEvent &e, const MouseWheelDetails &wheel)
    {
      auto start = scrollBar_.getCurrentRangeStart();
      viewport_.mouseWheelMove(e, wheel);
      return (int)std::round(scrollBar_.getCurrentRangeStart() - start);
    }

    void insertedSubProcessor(size_t index, Generation::BaseProcessor &newSubProcessor) override;
    void deletedSubProcessor(size_t index, Generation::BaseProcessor &deletedSubProcessor) override;

    void insertModule(size_t index, std::string_view newModuleType);
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
    size_t getIndexFromScreenPositionIgnoringSelf(Point<int> point,
      const EffectModuleSection *moduleSection) const noexcept;

    void setLaneName(String newName);
    void addListener(EffectsLaneListener *listener) { laneListeners_.push_back(listener); }

  private:
    Viewport viewport_{};
    EffectsContainer container_{};

    OpenGlQuad outerRectangle_{ Shaders::kRoundedRectangleFragment };
    OpenGlQuad innerRectangle_{ Shaders::kRoundedRectangleFragment };
    PlainTextComponent laneTitle_;
    ScrollBar scrollBar_{ true };
    std::vector<utils::up<EffectModuleSection>> effectModules_;

    utils::up<PowerButton> laneActivator_;
    utils::up<RadioButton> gainMatchingButton_;
    utils::up<TextSelector> inputSelector_;
    utils::up<TextSelector> outputSelector_;

    Generation::EffectsLane *effectsLane_ = nullptr;
    EffectsStateSection *parentState_ = nullptr;

    std::vector<EffectsLaneListener *> laneListeners_{};
  };
}
