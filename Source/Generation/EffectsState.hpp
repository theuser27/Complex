/*
  ==============================================================================

    EffectsState.hpp
    Created: 2 Oct 2021 20:53:05
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "BaseProcessor.hpp"

#include "Framework/sync_primitives.hpp"
#include "Framework/simd_buffer.hpp"

namespace Framework
{
  struct Buffer;
}

namespace Plugin
{
  struct State;
}

namespace Generation
{
  class EffectModule;
  class EffectsState;

  class EffectsLane final : public BaseProcessor
  {
  public:
    COMPLEX_ENUM_LOCAL(Parameters,
      ( LaneEnabled, 1758157321265403900),
      (       Input, 1758157334428822300),
      (      Output, 1758157341650409000),
      (GainMatching, 1758157353552984900),
    )
    COMPLEX_ENUM_LOCAL(InputOptions,
      (     InputOptionsMain, 1758157430681746300),
      (InputOptionsSidechain, 1758157438261920000),
      (     InputOptionsLane, 1758157445897557300),
    )
    COMPLEX_ENUM_LOCAL(OutputOptions,
      (     OutputOptionsMain, 1758157487088872000),
      (OutputOptionsSidechain, 1758157490890168800),
      (     OutputOptionsNone, 1758157494872876200),
    )

    EffectsLane(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept;
    EffectsLane(const EffectsLane &other, utils::bumpArena *arena) noexcept : BaseProcessor{ other, arena } { }

    void initialise() noexcept override
    {
      BaseProcessor::initialise();
      status_.store(LaneStatus::Finished, satomi::memory_order_release);
      currentEffectIndex_.store(0, satomi::memory_order_release);
    }

    // Inherited via BaseProcessor
    bool insertSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true) override;
    BaseProcessor &deleteSubProcessor(usize index, bool callListeners = true) noexcept override;
    void initialiseParameters() override;

    Framework::ComplexDataSource laneDataSource_;

    //// Parameters
    // 
    // 1. lane enabled
    // 2. input index
    // 3. output index
    // 4. gain match

    satomi::atomic<u32> currentEffectIndex_ = 0;

    // Finished - finished all processing
    // Ready - ready for a thread to begin work
    // Running - processing is running
    // Stopped - temporarily stopped to wait for data from another lane
    enum class LaneStatus : u32 { Finished, Ready, Running, Stopped };

    satomi::atomic<LaneStatus> status_ = LaneStatus::Finished;
    utils::shared_value<simd_float> volumeScale_{};

    friend class EffectsState;
  };

  static_assert(utils::is_trivially_destructible_v<EffectsLane>);

  class EffectsState final : public BaseProcessor
  {
  public:
    // data link between modules in different lanes
    struct EffectsModuleLink
    {
      bool checkForFeedback();

      utils::pair<u32, u32> sourceIndex, destinationIndex;
    };

    EffectsState(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept;

    EffectsState(const EffectsState &) = delete;

    // Inherited via BaseProcessor
    bool insertSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true) override;
    BaseProcessor &deleteSubProcessor(usize index, bool callListeners = true) override;
    void initialiseParameters() override;

    void writeInputData(const Framework::Buffer &inputBuffer) noexcept;
    void processLanes() noexcept;
    void sumLanesAndWriteOutput(Framework::Buffer &outputBuffer) noexcept;

    utils::span<bool> getUsedInputChannels() noexcept;
    utils::span<bool> getUsedOutputChannels() noexcept;

    const Framework::SimdBuffer *getOutputBuffer() const noexcept { return outputBuffer_; }

    // current number of FFT bins (real-imaginary pairs)
    u32 binCount = 0;
    float sampleRate = 0.0f;
    u32 blockPosition = 0;
    float blockPhase = 0.0f;
    
    void checkUsage();
    
    void distributeWork() const noexcept;
    void processIndividualLanes(EffectsLane *lane) const noexcept;

    // if an input/output isn't used there's no need to process it at all
    utils::span<bool> usedInputChannels_{};
    utils::span<bool> usedOutputChannels_{};
    utils::span<float> outputScaleMultipliers_{};

    Framework::SimdBuffer *outputBuffer_{};

    satomi::atomic<bool> shouldWorkersProcess_ = false;
  };

  static_assert(utils::is_trivially_destructible_v<EffectsState>);
}

extern template Generation::EffectsLane *createProcessor<>(Plugin::State *, Framework::ProcessorMetadata *, const void *);
extern template void *initialiseTypeStructure<Generation::EffectsLane>(void *metadata, Framework::PluginStructure &structure);

extern template Generation::EffectsState *createProcessor<>(Plugin::State *, Framework::ProcessorMetadata *, const void *);
extern template void *initialiseTypeStructure<Generation::EffectsState>(void *metadata, Framework::PluginStructure &structure);
