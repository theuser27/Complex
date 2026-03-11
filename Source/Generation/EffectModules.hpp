
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
      (ModuleEnabled, 1758553237829),
      (   ModuleType, 1758553260932),
      (    ModuleMix, 1758553272065),
      (     LowBound, 1758553297900),
      (    HighBound, 1758553309533),
      (  ShiftBounds, 1758553325233),
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

    EffectModule(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, const EffectModule *other, void *serialisedSave);

    Interface::Component *createUI() override { return nullptr; }

    void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept;

    // Inherited via BaseProcessor
    // this method exists only to accomodate loading from save files
    void serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise = {}) const override;

    EffectData *
    changeEffect(Framework::IndexedData *effectOption);

    Framework::SimdBuffer *buffer{};
    EffectData *effects{};
    satomi::atomic<EffectData *> currentActiveEffect{};
  };

  static_assert(utils::is_trivially_destructible_v<EffectModule>);
}

extern template Generation::BaseProcessor *createProcessor<Generation::EffectModule>(Plugin::State *, Framework::ProcessorMetadata *, const void *, void *);
extern template void *initialiseTypeStructure<Generation::EffectModule>(void *metadata, Framework::PluginStructure &structure);
