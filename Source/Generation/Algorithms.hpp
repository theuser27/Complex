
// Created: 2026-04-29 05:57:53

#pragma once

#include "Framework/stl_utils.hpp"

extern "C" typedef struct NSVGimage NSVGimage;

namespace utils
{
  struct bumpArena;
}

namespace Framework
{
  struct Buffer;
  struct IndexedData;
  struct ProcessorMetadata;
  struct PluginStructure;
  class ParameterValue;
  struct ComplexDataSource;
  struct SimdBuffer;
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
  class EffectModule;

  struct EffectData
  {
    using CreateEffectFn = EffectData *(EffectModule *module, EffectData *copy);
    using RunEffectFn = void(EffectModule *effectModule, EffectData *effectData, Framework::ComplexDataSource &source,
      Framework::SimdBuffer *destination, u32 binCount, float sampleRate);
    using CreateUIFn = utils::span<Interface::Control *>(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    enum EffectVtableIndices { CreateVtableIndex, RunVtableIndex, CreateUIVtableIndex, VtableIndexCount };

    EffectData *next{};

    Framework::ProcessorMetadata *metadata{};
    Framework::ParameterValue *parameters{};
    usize parameterCount{};
    NSVGimage *(*createEffectIcon)(Interface::Graphics &g){};
  };

  namespace Utility
  {
    inline constexpr uuid id = 1759541555994;

    //// Parameters
    //
    // 5. channel pan (stereo) - [-1.0f, 1.0f]
    // 6. flip phases (stereo) - [0, 1]
    // 7. reverse spectrum bins (stereo) - [0, 1]
    //
    // freeze input
    // record and play back input
    // nyquist gate (some values of destroy/reinterpret cause massive spikes)
    //
    // TODO:
    // idea: mix 2 input signals (left and right/right and left channels)
    // idea: flip the phases, panning, LR <-> MS conversion
    // idea: gain matching instead of it being a lane option
    //       gain match based on a set db value
    // 
    //void run(EffectModule *effectModule, EffectModule::EffectData *effectData,
    //  Framework::ComplexDataSource &source,
    //  Framework::SimdBuffer * &destination,
    //  u32 binCount, float sampleRate) noexcept
    //{
    //}

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }

  namespace Filter
  {
    inline constexpr uuid id = 1759541579349;

    COMPLEX_ENUM(Types,
      (Normal, 1758738064349),
      (  Gate, 1758738078894),
    );

    // Normal - normal filtering
    //  gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
    //    lowers the loudness at/around the cutoff point for negative/positive values
    //    *parameter values are interpreted linearly, so the control needs to have an exponential slope
    //
    //  cutoff (stereo) - range[0.0f, 1.0f]; controls where the filtering starts
    //    at 0.0f/1.0f it's at the low/high boundary
    //    *parameter values are interpreted linearly, so the control needs to have an exponential slope
    //
    //  slope (stereo) - range[-1.0f, 1.0f]; controls the slope transition
    //    at 0.0f it stretches from the cutoff to the frequency boundaries
    //    at 1.0f only the center bin is left unaffected
    //
    COMPLEX_ENUM(Normal,
      (  Gain, 1758735249666),
      (Cutoff, 1758735281753),
      ( Slope, 1758735286827),
    );

    void runNormal(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUINormal(utils::bumpArena *arena, 
      Interface::EffectModuleSection *section, EffectData *effectData);

    // Gate - frequency gating
    //  threshold (stereo) - range[0%, 100%]; threshold is relative to the loudest bin in the spectrum
    //
    //  gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
    //    positive/negative values lower the loudness of the bins below/above the threshold
    //
    //  tilt (stereo) - range[-100%, +100%]; tilt relative to the loudest bin in the spectrum
    //    at min/max values the left/right-most part of the masked spectrum will have no threshold
    //
    //  delta - instead of the absolute loudness it uses the averaged loudness change from the 2 neighbouring bins
    //
    COMPLEX_ENUM(Gate,
      (InputGain, 1758738170767),
      (     Gain, 1758738189871),
      (Threshold, 1758738202274),
      (     Tilt, 1758738216078),
      (     Mode, 1758738231926),
    );

    COMPLEX_ENUM(GateMode,
      (Decibels, 1759681582236),
      (    Rank, 1759681594091),
    );

    void runGate(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIGate(utils::bumpArena *arena, 
      Interface::EffectModuleSection *section, EffectData *effectData);


    // Tilt filter
    // Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
    // Regular key tracked - regular filtering based on fundamental frequency (like dtblkfx autoharm)
    //
    // TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }

  namespace Dynamics
  {
    inline constexpr uuid id = 1759541589473;

    COMPLEX_ENUM(Types,
      (Contrast, 1759688533355),
      (    Clip, 1759688538437),
    );

    inline constexpr float kContrastMaxPositiveValue = 4.0f;
    inline constexpr float kContrastMaxNegativeValue = -0.5f;

    COMPLEX_ENUM(Contrast,
      (Depth, 1758892756432),
    );

    // Dtblkfx Contrast
    //  contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
    //
    void runContrast(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIContrast(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    COMPLEX_ENUM(Clip,
      (Threshold, 1758894269115),
    );

    // Dtblkfx Clip
    //  clip (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
    //
    void runClip(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIClip(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    // TODO:
    //// Compressor
    //
    // Depth, Threshold, Ratio
    //

    // specops noise filter/focus, thinner
    // spectral compander, gate (threshold), clipping

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }

  namespace Phase
  {
    inline constexpr uuid id = 1759541605615;

    COMPLEX_ENUM(Types,
      (Shift, 1759688567995),
    );

    COMPLEX_ENUM(Shift,
      (PhaseShift, 1758894408955),
      (  Interval, 1758894436267),
      (    Offset, 1758894440849),
      (     Slope, 1758894444857),
    );

    COMPLEX_ENUM(SlopeOptions,
      (   Constant, 1758897431173),
      (     Linear, 1758897457186),
      (Exponential, 1758897461956),
    );

    void runShift(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;
    
    utils::span<Interface::Control *> createUIShift(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    // TODO:
    // phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
    // phase filter - filtering based on phase
    // mfreeformphase impl

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }

  namespace Pitch
  {
    inline constexpr uuid id = 1759541664963;

    COMPLEX_ENUM(Types,
      (      Resample, 1759689528336),
      (FrequencyShift, 1759689536970),
    );

    COMPLEX_ENUM(Resample,
      (Shift, 1759183155307),
      ( Wrap, 1759183162433),
    );

    void runResample(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIResample(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);


    COMPLEX_ENUM(FrequencyShift,
      (Shift, 1759183248900),
    );

    void runFrequencyShift(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIFrequencyShift(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    // TODO: shift, harmonic shift, harmonic repitch

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }

  namespace Stretch
  {
    inline constexpr uuid id = 1759541679484;

    // specops geometry
  }

  namespace Warp
  {
    inline constexpr uuid id = 1759541685084;

    // vocode, harmonic match, cross/warp mix
  }

  namespace Destroy
  {
    inline constexpr uuid id = 1759541690760;

    COMPLEX_ENUM(Types,
      (Reinterpret, 1759689938250),
    );

    COMPLEX_ENUM(Reinterpret,
      (Attenuation, 1759183439582),
      (    Mapping, 1759183450466),
    );

    COMPLEX_ENUM(ReinterpretMappingOptions,
      (     NoMapping, 1759183512424),
      (SwitchRealImag, 1759183528061),
      (   CartToPolar, 1759183533232),
      (   PolarToCart, 1759183546859)
    );

    void runReinterpret(EffectModule *effectModule, EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    utils::span<Interface::Control *> createUIReinterpret(utils::bumpArena *arena,
      Interface::EffectModuleSection *section, EffectData *effectData);

    // resize, specops effects category
    // randomising bin positions
    // bin sorting - by amplitude, phase
    // TODO: freezer and glitcher classes

    Framework::IndexedData *initialiseTypeStructure(Framework::PluginStructure &structure);
  }
}
