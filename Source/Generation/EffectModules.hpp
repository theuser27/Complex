
// Created: 2021-07-27 00:30:19

#pragma once

#include "Framework/constants.hpp"
#include "Framework/simd_buffer.hpp"
#include "BaseProcessor.hpp"

namespace Generation
{
  // class for the whole FX unit
  class EffectModule final : public BaseProcessor
  {
  public:
    COMPLEX_ENUM_LOCAL(Parameters,
      (ModuleEnabled, 1758553237829855100),
      (   ModuleType, 1758553260932768000),
      (    ModuleMix, 1758553272065877400),
      (     LowBound, 1758553297900505900),
      (    HighBound, 1758553309533931300),
      (  ShiftBounds, 1758553325233040900),
    )

    enum EffectVtableIndices { CreateVtableIndex, RunVtableIndex, VtableIndexCount };
    enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

    struct EffectData
    {
      EffectData *next{};

      Framework::ProcessorMetadata *metadata{};
      utils::dll<Framework::ParameterValue> *parameters{};
      usize parameterCount{};
    };

    EffectModule(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept;
    EffectModule(const EffectModule &other, utils::bumpArena *arena) noexcept;

    void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept;

    // Inherited via BaseProcessor
    // this method exists only to accomodate loading from save files
    void serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise = {}) const override;
    void deserialiseFromJson(void *jsonData) override;
    void initialiseParameters() override;

    EffectData *
    changeEffect(Framework::IndexedData *effectOption);

    Framework::SimdBuffer *buffer{};
    EffectData *effects{};
    satomi::atomic<EffectData *> currentActiveEffect{};
  };

  static_assert(utils::is_trivially_destructible_v<EffectModule>);
}

extern template Generation::EffectModule *createProcessor<>(Plugin::State *, Framework::ProcessorMetadata *, const void *);
extern template void *initialiseTypeStructure<Generation::EffectModule>(void *metadata, Framework::PluginStructure &structure);
