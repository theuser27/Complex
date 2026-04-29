
// Created: 2021-10-02 20:53:05

#pragma once

#include "Processor.hpp"

#include "Framework/simd_buffer.hpp"
#include "Interface/LookAndFeel/Skin.hpp"

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

    struct EffectData
    {
      EffectData *next{};

      Framework::ProcessorMetadata *metadata{};
      Framework::ParameterValue *parameters{};
      usize parameterCount{};
      NSVGimage *(*createEffectIcon)(Interface::Graphics &g) { };
    };

    using CreateUIFn = utils::span<Interface::Control *>(utils::bumpArena *arena, 
      Interface::EffectModuleSection *section, EffectData *effectData);

    enum EffectVtableIndices { CreateVtableIndex, RunVtableIndex, CreateUIVtableIndex, VtableIndexCount };
    enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };


    EffectModule(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, const EffectModule *other, void *serialisedSave);

    Interface::Component *createUI() override;

    void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept;

    // this method exists only to accomodate loading from save files
    void serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise = {}) const override;

    EffectData *changeEffect(const Framework::IndexedData *effectOption);

    Framework::SimdBuffer *buffer{};
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
    enum class LaneStatus : u32 { Finished, Ready, Running, Stopped };

    EffectsLane(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, const EffectsLane *other, void *serialisedSave);

    Interface::Component *createUI() override;
    void reset() override
    {
      Processor::reset();
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

extern template Generation::Processor *createProcessor<Generation::EffectModule>(Plugin::State *, Framework::ProcessorMetadata *, const void *, void *);
extern template void *initialiseTypeStructure<Generation::EffectModule>(void *metadata, Framework::PluginStructure &structure);

extern template Generation::Processor *createProcessor<Generation::EffectsLane>(Plugin::State *, Framework::ProcessorMetadata *, const void *, void *);
extern template void *initialiseTypeStructure<Generation::EffectsLane>(void *metadata, Framework::PluginStructure &structure);
