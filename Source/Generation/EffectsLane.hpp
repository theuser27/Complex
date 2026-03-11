
// Created: 2021-10-02 20:53:05

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

  class EffectsLane final : public BaseProcessor
  {
  public:
    COMPLEX_ENUM_LOCAL(Parameters,
      ( LaneEnabled, 1758157321265),
      (       Input, 1758157334428),
      (      Output, 1758157341650),
      (GainMatching, 1758157353552),
    )
    COMPLEX_ENUM_LOCAL(InputOptions,
      (     InputOptionsMain, 1758157430681),
      (InputOptionsSidechain, 1758157438261),
      (     InputOptionsLane, 1758157445897),
    )
    COMPLEX_ENUM_LOCAL(OutputOptions,
      (     OutputOptionsMain, 1758157487088),
      (OutputOptionsSidechain, 1758157490890),
      (     OutputOptionsNone, 1758157494872),
    )

    // Finished - finished all processing
    // Ready - ready for a thread to begin work
    // Running - processing is running
    // Stopped - temporarily stopped to wait for data from another lane
    enum class LaneStatus : u32 { Finished, Ready, Running, Stopped };

    EffectsLane(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, const EffectsLane *other, void *serialisedSave);

    Interface::Component *createUI() override;
    void reset() override
    {
      BaseProcessor::reset();
      status.store(LaneStatus::Finished, satomi::memory_order_release);
      currentEffectIndex.store(0, satomi::memory_order_release);
    }

    Framework::ComplexDataSource laneDataSource;

    //// Parameters
    //
    // 1. lane enabled
    // 2. input index
    // 3. output index
    // 4. gain match

    satomi::atomic<u32> currentEffectIndex = 0;

    satomi::atomic<LaneStatus> status = LaneStatus::Finished;
    satomi::atomic<simd_float> volumeScale{};

    friend class SoundEngine;
  };

  static_assert(utils::is_trivially_destructible_v<EffectsLane>);
}

extern template Generation::BaseProcessor *createProcessor<Generation::EffectsLane>(Plugin::State *, Framework::ProcessorMetadata *, const void *, void *);
extern template void *initialiseTypeStructure<Generation::EffectsLane>(void *metadata, Framework::PluginStructure &structure);
