
// Created: 2021-10-02 20:53:05

#pragma once

#include "Processor.hpp"

#include "Framework/simd_buffer.hpp"
#include "Algorithms.hpp"

extern "C" typedef struct NSVGimage NSVGimage;

namespace Framework
{
  struct Buffer;
}

namespace Plugin
{
  struct State;
}

namespace Interface
{
  class Graphics;
  class Control;
  struct EffectModuleSection;
}

namespace Generation
{
  // class for the whole FX unit
  class EffectModule final : public Processor
  {
  public:
    COMPLEX_ENUM_LOCAL(Parameters,
      (ModuleEnabled, 1758553237829),
      (   ModuleType, 1758553260932),
      (    ModuleMix, 1758553272065),
      (     LowBound, 1758553297900),
      (    HighBound, 1758553309533),
      (  ShiftBounds, 1758553325233),
    )

    enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

    using Processor::Processor;

    void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept;

    EffectData *changeEffect(const Framework::IndexedData *effectOption);

    EffectData *effects{};
    satomi::atomic<EffectData *> currentEffect{};
  };

  static_assert(utils::is_trivially_destructible_v<EffectModule>);

  class EffectsLane final : public Processor
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
    enum class Status : u32 { Finished, Ready, Running, Stopped };

    using Processor::Processor;

    Framework::ComplexDataSource laneDataSource;

    satomi::atomic<u32> currentEffectIndex = 0;

    satomi::atomic<Status> status = Status::Finished;
    satomi::atomic<f4> volumeScale{};
  };

  static_assert(utils::is_trivially_destructible_v<EffectsLane>);
}

void *initialiseTypeStructureEffectModule(void *metadata, Framework::PluginStructure &structure);
void *initialiseTypeStructureEffectsLane(void *metadata, Framework::PluginStructure &structure);
