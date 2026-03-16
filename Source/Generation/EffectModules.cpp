
// Created: 2021-07-27 00:30:19

#include "EffectModules.hpp"

#include "Framework/simd_math.hpp"
#include "Framework/parameter_value.hpp"
#include "Plugin/Complex.hpp"

namespace Generation
{
  using RunEffectFn = void(EffectModule *effectModule, EffectModule::EffectData *effectData, Framework::ComplexDataSource &source,
    Framework::SimdBuffer *destination, u32 binCount, float sampleRate);
  using CreateEffectFn = EffectModule::EffectData *(EffectModule *module, EffectModule::EffectData *copy);

  static EffectModule::EffectData *
  createEffectGeneric(EffectModule *module, EffectModule::EffectData *)
  {
    return anew(module->arena, EffectModule::EffectData, {});
  }

#define EFFECT_VTABLE(name, creationFunction) static void(*const vtable##name[])() = { (void (*)())creationFunction, (void (*)())run##name }
#define COMPLEX_STRUCTURE_EFFECT(nameString, idNumber, vtableArray, ...) (*new(arena->insert(arena, sizealignof(Framework::ProcessorMetadata))) \
  ProcessorMetadata{ .flags = ProcessorMetadata::ProcessorTag, .id = idNumber, .name = nameString __VA_OPT__(,) __VA_ARGS__, .vtable = vtableArray }).computeCounts()

  static Framework::ParameterValue *
  getParameter(EffectModule::EffectData *effectData, uuid id)
  {
    auto *parameter = effectData->parameters;
    for (usize i = 0; i < effectData->parameterCount; (++i), (parameter = parameter->next))
      if (parameter->object.getParameterId() == id)
        return &parameter->object;

    COMPLEX_ASSERT_FALSE("Couldn't find parameter with id: %zu", id);
    return nullptr;
  }

  namespace Utility
  {
    //static constexpr uuid id = 1759541555994;

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
    // idea: flip the phases, panning
    //
    //void run(EffectModule *effectModule, EffectModule::EffectData *effectData,
    //  Framework::ComplexDataSource &source,
    //  Framework::SimdBuffer * &destination,
    //  u32 binCount, float sampleRate) noexcept
    //{
    //}

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      auto *arena = structure.getNewArena(128);

      return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Utility", .count = 0);
    }
  }

  namespace Filter
  {
    //static constexpr uuid id = 1759541579349;

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

    void runNormal(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;


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

    void runGate(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;



    // Tilt filter
    // Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
    // Regular key tracked - regular filtering based on fundamental frequency (like dtblkfx autoharm)
    //
    // TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      EFFECT_VTABLE(Normal, createEffectGeneric);
      EFFECT_VTABLE(Gate, createEffectGeneric);

      auto *arena = structure.getNewArena(COMPLEX_KB(4));

      auto &group = COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Filter").addChildren(
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Normal", .id = Types::Normal, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Normal", Types::Normal, vtableNormal, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Gain", Normal::Gain, kMinusInfDb, kInfDb, 0.0f, 0.5f, ParameterScale::SymmetricLoudness,
                " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Cutoff", Normal::Cutoff, 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Frequency,
                " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Slope", Normal::Slope, -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic,
                "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
            )
          )
        ),
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Gate", .id = Types::Gate, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Gate", Types::Gate, vtableGate, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Input Gain", Gate::InputGain, kMinusInfDb, kInfDb, 0.0f, 0.5f, ParameterScale::SymmetricLoudness,
                " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Gain", Gate::Gain, kMinusInfDb, kInfDb, 0.0f, 0.5f, ParameterScale::SymmetricLoudness,
                " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Threshold", Gate::Threshold, -180.0f, 0.0f, -45.0f, 0.5f, ParameterScale::ReverseQuadratic,
                " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Tilt", Gate::Tilt, -24.0f, 24.0f, 0.0f, 0.5f, ParameterScale::SymmetricQuadratic,
                " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Mode", Gate::Mode,
                {
                  .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Decibels", .id = GateMode::Decibels),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Rank", .id = GateMode::Rank)),
                  .defaultOptionId = GateMode::Decibels
                }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)
            )
          )
        )
      );

      return group;
    }
  }

  namespace Dynamics
  {
    //static constexpr uuid id = 1759541589473;

    COMPLEX_ENUM(Types,
      (Contrast, 1759688533355),
      (    Clip, 1759688538437),
    );

    static constexpr float kContrastMaxPositiveValue = 4.0f;
    static constexpr float kContrastMaxNegativeValue = -0.5f;

    COMPLEX_ENUM(Contrast,
      (Depth, 1758892756432),
    );

    // Dtblkfx Contrast
    //  contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
    //
    void runContrast(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;


    COMPLEX_ENUM(Clip,
      (Threshold, 1758894269115),
    );

    // Dtblkfx Clip
    //  clip (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
    //
    void runClip(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;



    // TODO:
    //// Compressor
    //
    // Depth, Threshold, Ratio
    //

    // specops noise filter/focus, thinner
    // spectral compander, gate (threshold), clipping

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      EFFECT_VTABLE(Contrast, createEffectGeneric);
      EFFECT_VTABLE(Clip, createEffectGeneric);

      auto *arena = structure.getNewArena(COMPLEX_KB(1));

      auto &group = COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Dynamics").addChildren(
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Contrast", .id = Types::Contrast, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Contrast", Types::Contrast, vtableContrast, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Depth", Contrast::Depth, -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear,
                "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
            )
          )
        ),
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Clip", .id = Types::Clip, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Clip", Types::Clip, vtableClip, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Threshold", Clip::Threshold, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear,
                "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
            )
          )
        )
      );

      return group;
    }

  }

  namespace Phase
  {
    //static constexpr uuid id = 1759541605615;

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

    void runShift(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    // TODO:
    // phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
    // phase filter - filtering based on phase
    // mfreeformphase impl

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      EFFECT_VTABLE(Shift, createEffectGeneric);

      auto *arena = structure.getNewArena(COMPLEX_KB(4));

      auto &group = COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Phase").addChildren(
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Shift", .id = Types::Shift, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Contrast", Types::Shift, vtableShift, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Phase Shift", Shift::PhaseShift, -180.0f, 180.0f, 0.0f, 0.5f, ParameterScale::Linear,
                COMPLEX_DEGREE_SIGN_LITERAL, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Interval", Shift::Interval, 0.0f, 10.0f, 1.0f, ::powf(1.0f / 10.0f, 1.0f / 3.0f), ParameterScale::Cubic,
                " oct", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Offset", Shift::Offset, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency,
                " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Slope", Shift::Slope,
                {
                  .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Constant", .id = SlopeOptions::Constant),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Linear", .id = SlopeOptions::Linear),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Exponential", .id = SlopeOptions::Exponential)),
                  .defaultOptionId = SlopeOptions::Constant
                }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)
            )
          )
        )
      );

      return group;
    }
  }

  namespace Pitch
  {
    //static constexpr uuid id = 1759541664963;

    COMPLEX_ENUM(Types,
      (  Resample, 1759689528336),
      (ConstShift, 1759689536970),
    );

    COMPLEX_ENUM(Resample,
      (Shift, 1759183155307),
      ( Wrap, 1759183162433),
    );

    void runResample(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;


    COMPLEX_ENUM(ConstShift,
      (Shift, 1759183248900),
    );

    void runConstShift(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;

    // TODO: shift, harmonic shift, harmonic repitch

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      EFFECT_VTABLE(Resample, createEffectGeneric);
      EFFECT_VTABLE(ConstShift, createEffectGeneric);

      auto *arena = structure.getNewArena(COMPLEX_KB(1));

      auto &group = COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Pitch").addChildren(
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Resample", .id = Types::Resample, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Resample", Types::Resample, vtableResample, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Shift", Resample::Shift, -48.0f, 48.0f, 0.0f, 0.5f, ParameterScale::Linear,
                " st", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Wrap", Resample::Wrap, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Toggle,
                {}, ParameterDetails::Modulatable | ParameterDetails::Automatable)
            )
          )
        ),
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Frequency Shift", .id = Types::ConstShift, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Frequency Shift", Types::ConstShift, vtableConstShift, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Shift", Resample::Shift, -20'000.0f, 20'000.0f, 0.0f, 0.5f, ParameterScale::SymmetricCubic,
                " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
            )
          )
        )
      );

      return group;
    }

  }

  namespace Stretch
  {
    //static constexpr uuid id = 1759541679484;

    // specops geometry
  }

  namespace Warp
  {
    //static constexpr uuid id = 1759541685084;

    // vocode, harmonic match, cross/warp mix
  }

  namespace Destroy
  {
    //static constexpr uuid id = 1759541690760;

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

    void runReinterpret(EffectModule *effectModule, EffectModule::EffectData *effectData,
      Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
      u32 binCount, float sampleRate) noexcept;


    // resize, specops effects category
    // randomising bin positions
    // bin sorting - by amplitude, phase
    // TODO: freezer and glitcher classes

    static Framework::IndexedData &initialiseTypeStructure(Framework::PluginStructure &structure)
    {
      using namespace Framework;

      EFFECT_VTABLE(Reinterpret, createEffectGeneric);

      auto *arena = structure.getNewArena(COMPLEX_KB(1));

      auto &group = COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Destroy").addChildren(
        COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Reinterpret", .id = Types::Reinterpret, .flags = IndexedData::ProcessorFlag,
          .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Reinterpret", Types::Reinterpret, vtableReinterpret, .parameters =
            (
              COMPLEX_STRUCTURE_PARAMETER("Real/Imag Atten", Reinterpret::Attenuation, kMinusInfDb, kInfDb, 0.0f, 0.5f,
                ParameterScale::SymmetricLoudness, " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
              COMPLEX_STRUCTURE_PARAMETER("Mapping", Reinterpret::Mapping,
                {
                  .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "No Change", .id = ReinterpretMappingOptions::NoMapping),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Swap Real/Imaginary", .id = ReinterpretMappingOptions::SwitchRealImag),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Cartesian->Polar", .id = ReinterpretMappingOptions::CartToPolar),
                    COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Polar->Cartesian", .id = ReinterpretMappingOptions::PolarToCart)),
                  .defaultOptionId = ReinterpretMappingOptions::NoMapping
                }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable)
            )
          )
        )
      );

      return group;
    }
  }

#undef COMPLEX_STRUCTURE_GET_MEMORY
#define COMPLEX_STRUCTURE_GET_MEMORY COMPLEX_STRUCTURE_ARENA_INSERT

  ///////////////////////////////////////
  //  _   _ _   _ _ _ _   _            //
  // | | | | |_(_) (_) |_(_) ___  ___  //
  // | | | | __| | | | __| |/ _ \/ __| //
  // | |_| | |_| | | | |_| |  __/\__ \ //
  //  \___/ \__|_|_|_|\__|_|\___||___/ //
  ///////////////////////////////////////

  static void circularLoop(const auto &lambda, u32 start, u32 length, u32 binCount)
  {
    for (u32 i = 0; i < length - 1; i++)
      lambda((start + i) & (binCount - 2));

    // since we're masking to power-of-2 we skip nyquist
    // process it if passed, else continue onwards
    if ((start + length) > (binCount - 1))
      lambda(binCount - 1);
    else
      lambda(start + length);
  }

  static strict_inline simd_mask vector_call isOutsideBounds(simd_int positionIndices,
    simd_int lowBoundIndices, simd_int highBoundIndices, simd_mask isHighAboveLow)
  {
    simd_mask belowLowBounds = simd_int::lessThanSigned(positionIndices, lowBoundIndices);
    simd_mask aboveHighBounds = simd_int::greaterThanSigned(positionIndices, highBoundIndices);

    // 1st case examples: |   *  [    ]     | or
    //                    |      [    ]  *  |
    // 2nd case example:  |     ]   *  [    |
    return isHighAboveLow ^ belowLowBounds ^ aboveHighBounds;
  }

  static strict_inline simd_mask vector_call isOutsideBounds(
    simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
  {
    return isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices,
      simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices));
  }

  static strict_inline simd_mask vector_call isInsideBounds(simd_int positionIndices,
      simd_int lowBoundIndices, simd_int highBoundIndices, simd_mask isHighAboveLow)
  { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices, isHighAboveLow); }

  static strict_inline simd_mask vector_call isInsideBounds(
    simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
  { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices); }


  // first - low shifted boundary, second - high shifted boundary
  utils::pair<simd_float, simd_float>
  getShiftedBounds(EffectModule *module, EffectModule::BoundRepresentation representation,
    float sampleRate, u32 binCount) noexcept
  {
    using namespace utils;
    using namespace Framework;

    u32 FFTSize = (binCount - 1) * 2;

    simd_float lowBound = module->getParameter(EffectModule::LowBound)->getInternalValue<simd_float>(sampleRate, true);
    simd_float highBound = module->getParameter(EffectModule::HighBound)->getInternalValue<simd_float>(sampleRate, true);

    float nyquistFreq = sampleRate * 0.5f;
    float maxOctave = (float)log2(nyquistFreq / kMinFrequency);

    simd_float boundShift = module->getParameter(EffectModule::ShiftBounds)
      ->getInternalValue<simd_float>(sampleRate);
    lowBound = simd_float::clamp(lowBound + boundShift, 0.0f, 1.0f);
    highBound = simd_float::clamp(highBound + boundShift, 0.0f, 1.0f);

    switch (representation)
    {
    case EffectModule::BoundRepresentation::Normalised:
      break;
    case EffectModule::BoundRepresentation::Frequency:
      lowBound = exp2(lowBound * maxOctave);
      highBound = exp2(highBound * maxOctave);
      // snapping to 0 hz if it's below the minimum frequency
      lowBound = (lowBound & simd_float::greaterThan(lowBound, 1.0f)) * kMinFrequency;
      highBound = (highBound & simd_float::greaterThan(highBound, 1.0f)) * kMinFrequency;

      break;
    case EffectModule::BoundRepresentation::BinIndex:
      lowBound = normalisedToBin(lowBound, FFTSize, sampleRate);
      highBound = normalisedToBin(highBound, FFTSize, sampleRate);

      break;
    default:
      break;
    }

    return { lowBound, highBound };
  }

  // returns starting point, distance to end of processed/unprocessed range, and if the range is stereo
  static auto vector_call
  minimiseRange(simd_int lowIndices, simd_int highIndices, u32 binCount, bool isProcessedRange)
  {
    COMPLEX_ASSERT(utils::isPowerOfTwo(binCount - 1), "Bin count is not power-of-2 + 1, but instead %d", binCount);

    // 1. the indices in the respective bounds are different (stereo)
    // 2. all the indices in the respective bounds are the same (mono)

    struct
    {
      u32 start;
      u32 distance;
      bool isStereoRange;
    } ret{ 0, binCount, true };

    // 1.
    ret = { 0, binCount, true };

    // 2.
    if (lowIndices.allSame() && highIndices.allSame())
    {
      u32 start = lowIndices[0];
      u32 end = highIndices[0];
      u32 length = ((binCount + end - start) % binCount);

      // full/empty range if the bins overlap or span the entire range
      if (length == 0 || (start & (binCount - 2)) == (end & (binCount - 2)))
      {
        if (isProcessedRange)
          ret = { start, binCount, false };
        else
          ret = { (start + binCount) % binCount, 0, false };
      }
      else
      {
        if (isProcessedRange)
          ret = { start, length + 1, false };
        else
          ret = { (start + length + 1) % binCount, binCount - length - 1, false };
      }
    }

    return ret;
  }

  static void copyUnprocessedData(const Framework::SimdBuffer *source,
    Framework::SimdBuffer *destination, simd_int lowBoundIndices,
    simd_int highBoundIndices, u32 binCount) noexcept
  {
    COMPLEX_ASSERT(destination->size == source->size);
    auto [index, unprocessedCount, isStereo] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, false);

    usize channels = utils::min(source->channels, destination->channels);
    usize size = destination->size;
    auto rawSource = source->get();
    auto rawDestination = destination->get();

    if (isStereo)
    {
      simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
      // because procesing bounds can go past each other we assume they've done so
      // and split the algorithm in 2 regions (bottom and top)
      // if the bounds haven't passed each other then the 2nd iteration of the algorithm will just not run
      simd_int starts[] = { (highBoundIndices + 1) & ~isHighAboveLow, (highBoundIndices + 1) & isHighAboveLow };
      simd_int lengths[] = { lowBoundIndices - starts[0], (binCount - starts[1]) & isHighAboveLow };

      for (usize i = 0; i < channels; i += source->kRelativeSize)
      {
        auto sourceChannel = rawSource.offset(i * size);
        auto destinationChannel = rawDestination.offset(i * size);

        for (usize iteration = 0; iteration <= countof(starts); ++iteration)
        {
          simd_int start = starts[iteration];
          simd_int length = lengths[iteration];

          while (true)
          {
            simd_mask runNotCompleteMask = simd_mask::greaterThanSigned(length, 0);
            // if all lengths are 0 or negative then we've completed all runs
            if (runNotCompleteMask.anyMask() == 0)
              break;

            utils::scatterComplex(destinationChannel.pointer, start,
              utils::gatherComplex(sourceChannel.pointer, start), runNotCompleteMask);

            start += 1;
            length -= 1;
          }
        }
      }
    }
    // mono bounds
    else
    {
      if (!unprocessedCount)
        return;

      for (usize i = 0; i < channels; i += source->kRelativeSize)
      {
        for (usize j = 0; j < unprocessedCount - 1; j++)
        {
          auto currentIndex = (index + j) & (binCount - 2);
          rawDestination[i * size + currentIndex] = rawSource[i * size + currentIndex];
        }

        auto currentIndex = (index + unprocessedCount > binCount - 1) ?
          binCount - 1 : index + unprocessedCount - 1;

        rawDestination[i * size + currentIndex] = rawSource[i * size + currentIndex];
      }
    }
  }

  static simd_float vector_call matchPower(simd_float target, simd_float current) noexcept
  {
    simd_float result = 1.0f;
    result = result & simd_float::greaterThan(target, 0.0f);
    result = utils::merge(result, simd_float::sqrt(target / current), simd_float::greaterThan(current, 0.0f));

    result = utils::merge(result, simd_float{ 1.0f }, simd_float::greaterThan(result, 1e30f));
    result = result & simd_float::lessThanOrEqual(1e-37f, result);
    return result;
  }

  /////////////////////////////////////////////////////////////
  //           _                  _ _   _                    //
  //     /\   | |                (_) | | |                   //
  //    /  \  | | __ _  ___  _ __ _| |_| |__  _ __ ___  ___  //
  //   / /\ \ | |/ _` |/ _ \| '__| | __| '_ \| '_ ` _ \/ __| //
  //  / ____ \| | (_| | (_) | |  | | |_| | | | | | | | \__ \ //
  // /_/    \_\_|\__, |\___/|_|  |_|\__|_| |_|_| |_| |_|___/ //
  //              __/ |                                      //
  //             |___/																			 //
  /////////////////////////////////////////////////////////////

  //// Layout
  //
  // Simd values are laid out:
  //
  //        [left real     , left imaginary, right real     , right imaginary] or
  //        [left magnitude, left phase    , right magnitude, right phase    ],
  //
  // depending on the module's preferred way of handling data. (see @needsPolarData)

  //// Tips
  //
  // 1. When dealing with nyquist it's best to have a small section after your main algorithm to process it separately.
  // 2. Whenever in doubt, look at other algorithm implementations for ideas

  void Filter::runNormal(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    simd_float lowBoundNorm = effectModule->getParameter(EffectModule::LowBound)->getInternalValue<simd_float>(sampleRate, true);
    simd_float highBoundNorm = effectModule->getParameter(EffectModule::HighBound)->getInternalValue<simd_float>(sampleRate, true);
    simd_float boundShift = effectModule->getParameter(EffectModule::ShiftBounds)->getInternalValue<simd_float>(sampleRate);
    simd_float boundsDistance = modOnce(simd_float{ 1.0f } + highBoundNorm - lowBoundNorm, 1.0f);

    u32 FFTSize = (binCount - 1) * 2;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto [low, high] = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(low), toInt(high) };
    }();
    simd_mask isHighAboveLowMask = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // cutoff is described as exponential normalised value of the sample rate
    // it is dependent on the values of the low/high bounds
    simd_float cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance *
      getParameter(effectData, Filter::Normal::Cutoff)->getInternalValue<simd_float>(sampleRate, true), 1.0f, false);
    simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, FFTSize, sampleRate));

    // if mask scalars are negative/positive -> brickwall/linear slope
    // slopes are logarithmic
    simd_float slopes = getParameter(effectData, Filter::Normal::Slope)->getInternalValue<simd_float>(sampleRate) / 2.0f;
    simd_mask slopeMask = ~unsignSimd<true>(slopes);
    simd_mask slopeZeroMask = simd_float::equal(slopes, 0.0f);

    // if scalars are negative/positive, attenuate at/around cutoff
    // (gains is gain reduction in db and NOT a gain multiplier)
    simd_float gainsParameter = getParameter(effectData, Filter::Normal::Gain)->getInternalValue<simd_float>(sampleRate);
    simd_mask gainType = unsignSimd<true>(gainsParameter);

    // copy both un/processed data
    applyToThisNoMask<MathOperations::Assign>(destination, source.sourceBuffer, destination->channels, binCount);

    auto calculateDistancesFromCutoffs = [cutoffIndices, lowBoundIndices,
      cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundIndices),
      invLog2Nyquist = simd_float{ (float)(1.0 / log2(sampleRate * 0.5 / kMinFrequency)) },
      binDivisor = simd_float{ (float)(sampleRate / (FFTSize * kMinFrequency)) }](simd_int positionIndices)
    {
      // 1. both positionIndices and cutoffIndices are >= lowBound and < FFTSize_ or <= highBound and > 0
      // 2. cutoffIndices/positionIndices is >= lowBound and < FFTSize_ and
      //     positionIndices/cutoffIndices is <= highBound and > 0

      using namespace utils;

      simd_mask cutoffAbovePositions = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

      // preparing masks for 1.
      simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
      simd_mask bothAboveOrBelowLowMask = ~(positionsAboveLowMask ^ cutoffAboveLowMask);

      // preparing masks for 2.
      simd_mask positionsBelowLowBoundAndCutoffsMask = ~positionsAboveLowMask & cutoffAboveLowMask;
      simd_mask cutoffBelowLowBoundAndPositionsMask = positionsAboveLowMask & ~cutoffAboveLowMask;

      // masking for 1.
      simd_int precedingIndices  = merge(cutoffIndices  , positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
      simd_int succeedingIndices = merge(positionIndices, cutoffIndices  , bothAboveOrBelowLowMask & cutoffAbovePositions);

      // masking for 2.
      // first 2 are when cutoffs/positions are above/below lowBound
      // second 2 are when positions/cutoffs are above/below lowBound
      precedingIndices  = merge(precedingIndices , cutoffIndices  , ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
      succeedingIndices = merge(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
      precedingIndices  = merge(precedingIndices , positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );
      succeedingIndices = merge(succeedingIndices, cutoffIndices  , ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );

      auto binToNormalised = [&](simd_int bin)
      {
        return (log2(toFloat(bin) * binDivisor) * invLog2Nyquist) & simd_int::notEqual(bin, 0);
      };

      auto ratio = getDecimalPlaces(simd_float{ 1.0f } + binToNormalised(succeedingIndices) - binToNormalised(precedingIndices));
      // all i have to say is, floating point error
      return ratio & simd_int::notEqual(precedingIndices, succeedingIndices);
    };

    auto rawDestination = destination->get();
    circularLoop([&](u32 index)
      {
        // the distances are logarithmic
        // TODO: memoise cutoff distances and update only when low/high bound indices and binCount are changed
        simd_float distancesFromCutoff = calculateDistancesFromCutoffs(index);

        // calculating linear slope and brickwall, both are ratio of the gain attenuation
        // the higher tha value the more it will be affected by it
        simd_float gainRatio = merge(
          simd_float::clamp(merge(distancesFromCutoff / slopes, simd_float{ 1.0f }, slopeZeroMask), 0.0f, 1.0f),
          simd_float{ 1.0f } & simd_float::greaterThanOrEqual(distancesFromCutoff, slopes),
          slopeMask);
        simd_float gains = merge(gainsParameter * gainRatio, gainsParameter * (simd_float{ 1.0f } - gainRatio), gainType);

        // convert db reduction to amplitude multiplier
        gains = dbToAmplitude(-gains);

        rawDestination[index] = merge(rawDestination[index] * gains, rawDestination[index],
          isOutsideBounds(start, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      }, start, processedCount, binCount);
  }

  void Filter::runGate(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLowMask = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    //auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_float threshold = (float)binCount * dbToAmplitude(getParameter(effectData, Filter::Gate::Threshold)
      ->getInternalValue<simd_float>(sampleRate));
    simd_float gainParameter = getParameter(effectData, Filter::Gate::Gain)->getInternalValue<simd_float>(sampleRate);
    simd_mask gainType = ~unsignSimd<true>(gainParameter);
    gainParameter = -gainParameter;

    simd_float slope = 1.0f;
    simd_float slopeMultiplier = utils::uninitialised;
    {
      simd_float tiltParameter = getParameter(effectData, Filter::Gate::Tilt)->getInternalValue<simd_float>(sampleRate);
      simd_float decadeSlope = tiltParameter * kOctaveToDecadeConversionMult;
      float sampleHz = sampleRate / (float)(2 * binCount);
      float startDecade = ::log10f((float)kMinFrequency / sampleHz);
      float decadeCount = ::log10f(sampleRate / (2.0f * (float)kMinFrequency));
      float resolution = 1.0f / ((float)binCount - 1.0f);
      slopeMultiplier = dbToAmplitude(((decadeCount + startDecade) * decadeSlope) * resolution);
    }

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();

    for (u32 i = 0; i < binCount; ++i)
    {
      simd_float dry = rawSource[i];
      simd_mask isAboveThresholdMask = simd_float::greaterThanOrEqual(complexMagnitude(dry, true) * slope, threshold);
      // dry >= threshold && gain >= 0db ===> don't change
      // dry >= threshold && gain <  0db ===> attenuate
      // dry <  threshold && gain >= 0db ===> attenuate
      // dry <  threshold && gain <  0db ===> don't change
      simd_float gains = dbToAmplitude(gainParameter & (gainType ^ isAboveThresholdMask));

      rawDestination[i] = merge(dry * gains, dry,
        isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      slope *= slopeMultiplier;
    }
  }

  void Dynamics::runContrast(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLowMask = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // calculating contrast
    simd_float depthParameter = getParameter(effectData, Dynamics::Contrast::Depth)
      ->getInternalValue<simd_float>(sampleRate);
    simd_float contrast = depthParameter * depthParameter;
    contrast = merge(simd_float(kContrastMaxNegativeValue) * contrast,
      simd_float(kContrastMaxPositiveValue) * contrast, simd_float::greaterThanOrEqual(depthParameter, 0.0f));

    simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0f + 1.0f));
    simd_float max = exp(simd_float(80.0f) / (contrast * 2.0f + 1.0f));
    min = merge(simd_float(1e-30f), min, simd_float::greaterThan(contrast, 0.0f));
    max = merge(simd_float(1e30f), max, simd_float::greaterThan(contrast, 0.0f));

    // copy both un/processed data
    applyToThisNoMask<MathOperations::Assign>(
      destination, source.sourceBuffer, destination->channels, binCount);

    auto rawDestination = destination->get();
    simd_float inPower = 0.0f;
    circularLoop([&](u32 index)
      {
        inPower += complexMagnitude(rawDestination[index], false) &
          isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
      }, start, processedCount, binCount);

    simd_int boundDistanceCount = (modOnce(simd_int{ binCount } + highBoundIndices - lowBoundIndices, simd_int{ binCount }) + 1)
      & simd_int::notEqual(lowBoundIndices, highBoundIndices);
    simd_float inScale = matchPower(toFloat(boundDistanceCount), inPower);

    // applying gain and calculating outPower
    simd_float outPower = 0.0f;
    circularLoop([&](u32 index)
      {
        simd_float bin = inScale * rawDestination[index];
        simd_float magnitude = complexMagnitude(bin, true);

        bin &= simd_float::lessThanOrEqual(min, magnitude);
        bin = merge(bin, bin * pow(magnitude, contrast), simd_float::greaterThan(max, magnitude));

        outPower += complexMagnitude(bin, false);
        rawDestination[index] = bin;
      }, start, processedCount, binCount);

    // normalising
    simd_float outScale = matchPower(inPower, outPower);
    circularLoop([&](u32 index) { rawDestination[index] *= outScale; }, start, processedCount, binCount);
  }

  void Dynamics::runClip(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    static constexpr float kSilenceThreshold = 1e-30f;
    static constexpr float kLoudestThreshold = 1e30f;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLowMask = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    COMPLEX_ASSERT(source.sourceBuffer->size == destination->size);

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();

    // getting the min/max power in the range selected
    utils::pair<simd_float, simd_float> powerMinMax{ kLoudestThreshold, kSilenceThreshold };
    for (u32 j = 0; j < binCount; j++)
    {
      // while calculating the power min-max we can copy over data
      rawDestination[j] = rawSource[j];

      simd_float magnitude = simd_float::max(kSilenceThreshold, complexMagnitude(rawDestination[j], false));
      simd_mask isIndexOutside = isOutsideBounds(j, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
      powerMinMax.first  = merge(simd_float::min(powerMinMax.first , magnitude), powerMinMax.first , isIndexOutside);
      powerMinMax.second = merge(simd_float::max(powerMinMax.second, magnitude), powerMinMax.second, isIndexOutside);
    }

    // calculating clipping
    simd_float thresholdParameter = getParameter(effectData, Dynamics::Clip::Threshold)
      ->getInternalValue<simd_float>(sampleRate);
    simd_float threshold = exp(lerp(log(simd_float::max(powerMinMax.first, 1e-36f)),
                                    log(simd_float::max(powerMinMax.second, 1e-36f)),
                                    simd_float{ 1.0f } - thresholdParameter));
    simd_float sqrtThreshold = simd_float::sqrt(threshold);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // doing clipping
    simd_float inPower = 0.0f;
    simd_float outPower = 0.0f;
    circularLoop([&](u32 index)
      {
        simd_mask isIndexInside = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
        simd_float magnitude = complexMagnitude(rawDestination[index], false);

        rawDestination[index] = merge(rawDestination[index],
          rawDestination[index] * sqrtThreshold / simd_float::sqrt(magnitude),
          simd_float::greaterThanOrEqual(magnitude, threshold) & isIndexInside);

        inPower += magnitude & isIndexInside;
        outPower += simd_float::min(magnitude, threshold) & isIndexInside;
      }, start, processedCount, binCount);

    // normalising
    simd_float outScale = matchPower(inPower, outPower);
    circularLoop([&](u32 index)
      {
        rawDestination[index] = merge(rawDestination[index] * outScale, rawDestination[index],
          isOutsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      }, start, processedCount, binCount);
  }

  void Phase::runShift(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_float shiftIncrement = cis(kPi * (getParameter(effectData, Phase::Shift::PhaseShift)
      ->getInternalValue<simd_float>(sampleRate, true) * 2.0f - 1.0f));
    simd_float shift = shiftIncrement;
    simd_float interval = getParameter(effectData, Phase::Shift::Interval)->getInternalValue<simd_float>(sampleRate);

    auto slopeFunction = [&]() -> simd_float(*)(simd_float, simd_float)
    {
      auto [slopeId, _] = getParameter(effectData, Phase::Shift::Slope)->getInternalValue<IndexedData>(sampleRate);
      if (slopeId->id == Phase::Shift::Slope)
        return [](simd_float x, simd_float) { return x; };
      else if (slopeId->id == Phase::SlopeOptions::Linear)
      {
        // explicitly normalising because the result shoots up at some point
        return [](simd_float x, simd_float increment)
        {
          auto y = complexCartMul(x, increment);
          return y * simd_float::invSqrt(complexMagnitude(y, false));
        };
      }
      else if (slopeId->id == Phase::SlopeOptions::Exponential)
      {
        return [](simd_float x, simd_float)
        {
          auto y = complexCartMul(x, x);
          // necessary renormalisation because floating point inaccuracies cause +/- inf explosions
          return y * simd_float::invSqrt(complexMagnitude(y, false));
        };
      }

      COMPLEX_ASSERT_FALSE("Invalid slope chosen");
      return [](simd_float x, simd_float) { return x; };
    }();

    // overwrite old data
    applyToThisNoMask<MathOperations::Assign>(destination, source.sourceBuffer, destination->channels, binCount);

    auto rawDestination = destination->get();

    // if interval between bins is 0 this means every bin is affected
    if (interval == simd_float{ 0.0f })
    {
      simd_int offsetBin = toInt(normalisedToBin(getParameter(effectData, Phase::Shift::Offset)
        ->getInternalValue<simd_float>(sampleRate, true), 2 * (binCount - 1), sampleRate));

      // find the smallest offset forward and start from there
      u32 minOffset = horizontalMin(offsetBin)[0];
      u32 indexChange = (u32)clamp((i32)minOffset - (i32)start, 0, (i32)processedCount);
      processedCount -= indexChange;
      start += indexChange;

      circularLoop([&](u32 index)
        {
          simd_mask offsetMask = simd_int::greaterThanOrEqualSigned(index, offsetBin);
          simd_mask insideRangeMask = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
          rawDestination[index] = merge(rawDestination[index],
            complexCartMul(rawDestination[index], shift),
            offsetMask & insideRangeMask);

          shift = slopeFunction(shift, shiftIncrement);
        }, start, processedCount, binCount);

      return;
    }

    // otherwise the interval specifies how many octaves up the next affected bin is

    // offset is skewed towards an exp-like curve so we need to normalise it
    simd_float offsetNorm = getParameter(effectData, Phase::Shift::Offset)
      ->getInternalValue<simd_float>(sampleRate) * 2.0f / sampleRate;
    simd_float binStep = 1.0f / (float)(binCount - 1);
    simd_float logBase = log2(interval + 1.0f);
    COMPLEX_ASSERT(simd_float::lessThanOrEqual(logBase, 0.0f).anyMask() == 0);

    // if offset is 0 we need to give it a starting value based on interval
    // and shift dc component's amplitude
    {
      simd_mask zeroMask = simd_float::lessThan(offsetNorm, binStep);
      rawDestination[0] = merge(rawDestination[0], complexCartMul(rawDestination[0], shift) & kMagnitudeMask, zeroMask);
      shift = merge(shift, slopeFunction(shift, shiftIncrement), zeroMask);

      simd_float startOffset = interval * binStep;
      COMPLEX_ASSERT(simd_float::lessThanOrEqual(startOffset, 0.0f).anyMask() == 0);

      // this is derived below, the next 2 lines get the next bin after dc in case any channels started there
      simd_float multiple = simd_float::ceil(log2(binStep / startOffset) / logBase);
      startOffset *= exp2(logBase * multiple);
      offsetNorm = merge(offsetNorm, startOffset, zeroMask);
    }

    auto algorithm = [&]()
    {
      simd_int indices = toInt(simd_float::round(offsetNorm * (float)binCount));
      simd_mask indexMask = isInsideBounds(indices, lowBoundIndices, highBoundIndices, isHighAboveLow);
      simd_mask lessMask = simd_int::lessThanSigned(indices, binCount);

      // have all indices gone above nyquist?
      if (lessMask.anyMask() == 0)
        return false;

      simd_float values = gatherComplex(rawDestination.pointer, indices & lessMask);
      scatterComplex(rawDestination.pointer, indices & lessMask,
        complexCartMul(values, shift), indexMask);

      shift = merge(shift, slopeFunction(shift, shiftIncrement), indexMask);

      return true;
    };

    // if inteval < 1, then it's possible for the next while loop to make any progress,
    // so we make sure that interval * offsetNorm will yield a number at least as big as a single bin step
    {
      simd_float increment = interval * offsetNorm;
      simd_float nextBin = (simd_float::round(offsetNorm * (float)binCount) + 1.0f) / (float)binCount;
      // repeat this until all increments are bigger than a single bin step
      while (simd_float::lessThan(increment, binStep).anyMask())
      {
        if (!algorithm())
          break;

        // offsetNorm[n+1] = offsetNorm[n] + interval * offsetNorm[n]
        // offsetNorm[n+1] = offsetNorm[n] * (1 + interval)^1
        // offsetNorm[n+2] = offsetNorm[n] * (1 + interval)^2
        // offsetNorm[n+m] = offsetNorm[n] * (1 + interval)^m
        // log(offsetNorm[n+m] / offsetNorm[n]) = m * log(1 + interval)
        // log(offsetNorm[n+m] / offsetNorm[n]) / log(1 + interval) = m
        // we need to do ceil to get the first whole number of intervals
        simd_float multiple = simd_float::ceil(log2(nextBin / offsetNorm) / logBase);

        // pow(base, exponent) = exp2(log2(base) * exponent)
        offsetNorm *= exp2(logBase * multiple);
        increment = interval * offsetNorm;
        nextBin += binStep;
      }
    }

    while (true)
    {
      if (!algorithm())
        break;

      offsetNorm = simd_float::mulAdd(offsetNorm, interval, offsetNorm);
    }
  }

  void Pitch::runResample(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    static constexpr auto kNeighbourBins = 2;
    static constexpr float kMultiplierEpsilon = 1e-12f;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    simd_float shift = exp2(getParameter(effectData, Pitch::Resample::Shift)
      ->getInternalValue<simd_float>(sampleRate) / (float)kNotesPerOctave);

    bool wrapAround = getParameter(effectData, Pitch::Resample::Wrap)->getInternalValue<u32>(sampleRate);

    simd_float leakMultipliers[2 * kNeighbourBins + 1];
    simd_float phaseShift = utils::uninitialised;

    auto calculateCoefficients = [&](simd_float binFloatingPointShift) mutable
    {
      auto cycle = binFloatingPointShift * source.blockPhase;
      // modding the phase to get more accurate values, not important
      cycle -= simd_float::round(cycle * 0.5f) * 2.0f;
      phaseShift = cis(cycle * k2Pi);

      simd_float denominator = (simd_float::round(binFloatingPointShift) - binFloatingPointShift) * k2Pi;
      simd_float numerator = (simd_float{ 0.0f, 1.0f } - switchInner(cis(denominator))) ^ simd_mask{ kSignMask, 0U };

      // this might at some point in time fail and the effect will produce silence unless gain matching is enabled
      // in that case this guard has not done its job and should be increased
      simd_mask numZeroMask = simd_float::lessThan(complexMagnitude(numerator, true), kMultiplierEpsilon);

      for (i32 i = 0; i < COMPLEX_ARRAY_SIZE(leakMultipliers); ++i)
      {
        simd_float fullDenominator = denominator + k2Pi * (float)(i - kNeighbourBins);
        simd_mask denZeroMask = simd_float::lessThan(simd_float::abs(fullDenominator), kMultiplierEpsilon);
        fullDenominator = merge(fullDenominator, simd_float{ 1.0f }, denZeroMask);
        // guarding against 0.0 / 0.0
        leakMultipliers[i] = merge(numerator / fullDenominator, simd_float{ 1.0f, 0.0f }, numZeroMask & denZeroMask);
      }
    };

    destination->clear();
    copyUnprocessedData(source.sourceBuffer, destination, lowBoundIndices, highBoundIndices, binCount);

    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    if (!wrapAround)
    {
      simd_float downscaledNyquist = (float)(binCount - 1) / shift;

      // because procesing bounds can go past each other we assume they've done so
      // and split the algorithm in 2 regions (bottom and top)
      // if the bounds haven't passed each other then the 2nd iteration of the algorithm will just not run
      simd_int starts[] = { lowBoundIndices & isHighAboveLow, lowBoundIndices & ~isHighAboveLow };
      // shortening the range by however many bins get shifted out of bounds as well
      simd_int lengths[] =
      {
        (highBoundIndices + 1) - starts[0] - toInt(simd_float::max(0.0f, toFloat(highBoundIndices) - downscaledNyquist)),
        (binCount - starts[1] - toInt(simd_float::max(0.0f, (float)(binCount - 1) - downscaledNyquist))) & ~isHighAboveLow
      };

      for (usize iteration = 0; iteration < countof(starts); ++iteration)
      {
        simd_int start = starts[iteration];
        simd_int length = lengths[iteration];
        simd_float destinationIndices = shift * toFloat(start);

        while (true)
        {
          simd_mask runNotCompleteMask = simd_mask::greaterThanSigned(length, 0);
          // if all lengths are 0 or negative then we've completed all runs
          if (runNotCompleteMask.anyMask() == 0)
            break;

          calculateCoefficients(destinationIndices - toFloat(start));

          simd_float wet = complexCartMul(gatherComplex(rawSource.pointer, start) & runNotCompleteMask, phaseShift);
          simd_int destinationIndicesInt = simd_int::minUnsigned(toInt(simd_float::round(destinationIndices)), binCount - 1);

          for (i32 j = 0; j < COMPLEX_ARRAY_SIZE(leakMultipliers); ++j)
          {
            simd_int indices = destinationIndicesInt - kNeighbourBins + j;
            simd_int clampedIndices = simd_int::clampSigned(0, binCount - 1, indices);
            simd_mask inRangeMask = simd_int::equal(indices, clampedIndices);

            scatterAddComplex(rawDestination.pointer, clampedIndices,
              complexCartMul(wet, leakMultipliers[j]), inRangeMask);
          }

          length -= 1;
          start += 1;
          destinationIndices += shift;
        }
      }
    }
    else
    {
      /*simd_float destinationIndices = shift * toFloat(lowBoundIndices);
      simd_float lengths = toFloat(modOnce(binCount + highBoundIndices - lowBoundIndices, binCount, false));

      while (true)
      {
        simd_mask runCompletedMask = simd_mask::equal(getSign(lengths), kSignMask);
        // if all lengths go negative then we've completed all runs
        if (runCompletedMask.anyMask() == 0)
          break;

        simd_int destinationIndicesInt = toInt(destinationIndices);


        simd_mask outsideBoundsMask = isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow);
        simd_float wet = rawSource[i] & (~outsideBoundsMask &);

        calculateCoefficients.next();

        for (i32 j = 0; j < leakMultipliers.size(); ++j)
        {
          simd_int indices = simd_int{ (u32)((i32)i - kNeighbourBins + j) } + binShift;
          simd_int clampedIndices = simd_int::clampSigned(0, binCount - 1, indices);
          simd_mask inRangeMask = simd_int::equal(indices, clampedIndices);

          simd_float result = gatherComplex(rawDestination.pointer, clampedIndices);
          scatterComplex(rawDestination.pointer, clampedIndices,
            merge(result, result + complexCartMul(wet, leakMultipliers[j]), inRangeMask));
        }

        lengths -= shift;
        destinationIndices = modOnce(destinationIndices + shift, binCount);
      }*/
    }
  }

  void Pitch::runConstShift(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    static constexpr auto kNeighbourBins = 2;
    static constexpr float kMultiplierEpsilon = 1e-5f;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    simd_int binShift = utils::uninitialised;
    simd_float leakMultipliers[2 * kNeighbourBins + 1];
    {
      simd_float binFloatingPointShift = getParameter(effectData, Pitch::ConstShift::Shift)
        ->getInternalValue<simd_float>(sampleRate) * 2.0f * (float)(binCount - 1) / sampleRate;
      simd_float roundedShift = simd_float::round(binFloatingPointShift);
      binShift = toInt(roundedShift);

      simd_float phaseShift = utils::uninitialised;
      {
        auto cycle = binFloatingPointShift * source.blockPhase;
        // modding the phase to get more accurate values, not important
        cycle -= simd_float::round(cycle * 0.5f) * 2.0f;
        phaseShift = cis(cycle * k2Pi);
      }
      simd_float denominator = (simd_float::round(binFloatingPointShift) - binFloatingPointShift) * k2Pi;
      simd_float numerator = (simd_float{ 0.0f, 1.0f } - switchInner(cis(denominator))) ^ simd_mask{ kSignMask, 0U };

      // this might at some point in time fail and the effect will produce silence unless gain matching is enabled
      // in that case this guard has not done its job and should be increased
      simd_mask numZeroMask = simd_float::lessThan(complexMagnitude(numerator, true), kMultiplierEpsilon);

      for (i32 i = 0; i < COMPLEX_ARRAY_SIZE(leakMultipliers); ++i)
      {
        simd_float fullDenominator = denominator + k2Pi * (float)(i - kNeighbourBins);
        simd_mask denZeroMask = simd_float::lessThan(simd_float::abs(fullDenominator), kMultiplierEpsilon);
        fullDenominator = merge(fullDenominator, simd_float{ 1.0f }, denZeroMask);
        // guarding against 0.0 / 0.0
        leakMultipliers[i] = merge(numerator / fullDenominator, simd_float{ 1.0f, 0.0f }, numZeroMask & denZeroMask);
        leakMultipliers[i] = complexCartMul(leakMultipliers[i], phaseShift);
      }
    }

    destination->clear();
    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    for (u32 i = 0; i < binCount; ++i)
    {
      simd_mask outsideBoundsMask = isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow);
      rawDestination[i] += rawSource[i] & outsideBoundsMask;
      simd_float wet = rawSource[i] & ~outsideBoundsMask;

      for (u32 j = 0; j < COMPLEX_ARRAY_SIZE(leakMultipliers); ++j)
      {
        simd_int indices = simd_int{ (i - (u32)kNeighbourBins + j) } + binShift;
        simd_int clampedIndices = simd_int::clampSigned(0, binCount - 1, indices);
        simd_mask inRangeMask = simd_int::equal(indices, clampedIndices);

        scatterAddComplex(rawDestination.pointer, clampedIndices,
          complexCartMul(wet, leakMultipliers[j]), inRangeMask);
      }
    }
  }

  void Destroy::runReinterpret(EffectModule *effectModule, EffectModule::EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
    //auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_float attenuation = [&]()
    {
      auto parameter = getParameter(effectData, Destroy::Reinterpret::Attenuation)->getInternalValue<simd_float>();
      auto mergeMask = merge(kRealMask, kImaginaryMask, simd_float::greaterThan(parameter, 0.0f));
      return merge(1.0f, dbToAmplitude(parameter | simd_mask{ kSignMask }), mergeMask);
    }();

    auto mappingType = getParameter(effectData,
      Destroy::Reinterpret::Mapping)->getInternalValue<IndexedData>().first->id;

    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    auto operations = [mappingType](simd_float &one, simd_float &two)
    {
      using enum Destroy::ReinterpretMappingOptions::Value;
      switch (mappingType)
      {
      case NoMapping:
        break;
      case SwitchRealImag:
        one = switchInner(one);
        two = switchInner(two);
        break;
      case CartToPolar:
        complexCartToPolar(one, two);
        break;
      case PolarToCart:
        complexPolarToCart(one, two);
        break;
      default:
        COMPLEX_ASSERT_FALSE("Missing case");
        break;
      }
    };

    simd_float wet[2]{};
    for (u32 i = 0; i < binCount - 1; i += 2)
    {
      wet[0] = rawSource[i] * attenuation;
      wet[1] = rawSource[i + 1] * attenuation;

      operations(wet[0], wet[1]);

      rawDestination[i    ] = merge(wet[0], rawSource[i    ],
        isOutsideBounds(i    , lowBoundIndices, highBoundIndices, isHighAboveLow));
      rawDestination[i + 1] = merge(wet[1], rawSource[i + 1],
        isOutsideBounds(i + 1, lowBoundIndices, highBoundIndices, isHighAboveLow));
    }

    wet[0] = rawSource[binCount - 1] * attenuation;
    operations(wet[0], wet[1]);
    rawDestination[binCount - 1] = wet[0];
  }

  static EffectModule::EffectData *
  createEffect(Framework::ProcessorMetadata *processorMetadata, EffectModule *module,
    EffectModule::EffectData *copy = nullptr, void *jsonData = nullptr)
  {
    if (copy)
      processorMetadata = copy->metadata;

    COMPLEX_ASSERT(processorMetadata->parameters);
    COMPLEX_ASSERT(processorMetadata->vtable[0]);

    auto *createFn = (CreateEffectFn *)processorMetadata->vtable[EffectModule::CreateVtableIndex];

    EffectModule::EffectData *effectData = createFn(module, copy);
    effectData->metadata = processorMetadata;

    utils::dll<Framework::ParameterValue> *effectParameters;
    usize parameterCount;
    if (copy)
    {
      parameterCount = copy->parameterCount;
      effectParameters = module->createParameters(parameterCount, processorMetadata->parameters, copy->parameters);
    }
    else if (jsonData)
    {
      parameterCount = processorMetadata->parametersCount;
      deserialiseParametersFromJson(jsonData, processorMetadata, effectParameters, module, true);
    }
    else
    {
      parameterCount = processorMetadata->parametersCount;
      effectParameters = module->createParameters(parameterCount, processorMetadata->parameters);
    }

    effectData->parameters = effectParameters;
    effectData->parameterCount = parameterCount;

    auto previousLast = module->parameters->previous;
    module->parameters->previous = effectData->parameters->previous;
    effectData->parameters->previous = previousLast->next;
    effectData->parameters->previous->next = effectData->parameters->previous;

    return effectData;
  }

  EffectModule::EffectModule(utils::bumpArena *arena, Plugin::State *state,
    Framework::ProcessorMetadata *metadata, const EffectModule *other, void *serialisedSave) :
    BaseProcessor{ arena, state, metadata, other }
  {
    auto maxBinCount = state->getMaxBinCount();
    if (other)
    {
      if (other->buffer)
      {
        buffer = Framework::SimdBuffer::create(arena, other->buffer->channels, maxBinCount);
        Framework::applyToThisNoMask<utils::MathOperations::Assign>(buffer,
          other->buffer, buffer->channels, buffer->size);
      }

      auto [effectOption, _] = getParameter(EffectModule::ModuleType)->getInternalValue<Framework::IndexedData>();

      auto *effect = other->effects;
      for (; effect && effect->metadata->id != effectOption->processorMetadata->id; effect = effect->next) { }

      effects = createEffect(effectOption->processorMetadata, this, effect);
      currentActiveEffect.store(effects, satomi::memory_order_release);

      return;
    }

    dataBuffer = Framework::SimdBuffer::create(arena, utils::kChannelsPerInOut, maxBinCount);
    buffer = Framework::SimdBuffer::create(arena, utils::kChannelsPerInOut, maxBinCount);

    if (serialisedSave)
      deserialiseFromJson(serialisedSave);
    else
    {
      parameters = createParameters(metadata->parametersCount, metadata->parameters);
      parameterCount = metadata->parametersCount;
    }

    auto [effectOption, _] = getParameter(EffectModule::ModuleType)->getInternalValue<Framework::IndexedData>();
    effects = createEffect(effectOption->processorMetadata, this, nullptr, serialisedSave);
    currentActiveEffect.store(effects, satomi::memory_order_release);
  }

  void EffectModule::serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *>) const
  {
    auto *effect = currentActiveEffect.load(satomi::memory_order_acquire);

    auto parametersToSerialise = utils::vector<Framework::ParameterValue *>{
      localScratch, effect->parameterCount + parameterCount };

    auto parameter = parameters;
    for (usize i = 0; i < parameterCount; (++i), (parameter = parameter->next))
      parametersToSerialise.emplaceBack(&parameter->object);

    auto *effectParameter = effect->parameters;
    for (usize i = 0; i < effect->parameterCount; (++i), (effectParameter = effectParameter->next))
      parametersToSerialise.emplaceBack(&effectParameter->object);

    BaseProcessor::serialiseToJson(jsonData, parametersToSerialise);
  }

  EffectModule::EffectData *
  EffectModule::changeEffect(Framework::IndexedData *effectOption)
  {
    using namespace Framework;

    auto *currentEffect = currentActiveEffect.load(satomi::memory_order_acquire);

    if (currentEffect->metadata->id == effectOption->processorMetadata->id)
      return currentEffect;

    auto *effect = effects;
    while (true)
    {
      if (effect->metadata->id == effectOption->processorMetadata->id)
      {
        currentActiveEffect.store(effect, satomi::memory_order_release);
        return effect;
      }

      if (!effect->next)
        break;

      effect = effect->next;
    }

    auto *newEffect = createEffect(effectOption->processorMetadata, this);
    effect->next = newEffect;

    currentActiveEffect.store(newEffect, satomi::memory_order_release);

    return newEffect;
  }

  void EffectModule::processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;
    using namespace utils;

    if (!getParameter(ModuleEnabled)->getInternalValue<u32>(sampleRate))
      return;

    auto *effect = currentActiveEffect.load(satomi::memory_order_acquire);

    // getting exclusive access to data
    lockAtomic(dataBuffer->dataLock, false, true, WaitMechanism::Spin);

    ((RunEffectFn *)effect->metadata->vtable[RunVtableIndex])(this, effect, source, dataBuffer, binCount, sampleRate);

    // if the mix is 100% for all channels, we can skip mixing entirely
    simd_float wetMix = getParameter(ModuleMix)->getInternalValue<simd_float>(sampleRate);
    if (wetMix != 1.0f)
    {
      auto sourceData = source.sourceBuffer->get();
      auto destinationData = dataBuffer->get();
      simd_float dryMix = 1.0f - wetMix;
      for (u32 i = 0; i < binCount; i++)
        destinationData[i] = simd_float::mulAdd(dryMix * sourceData[i], wetMix, destinationData[i]);
    }

    // switching to being a reader and allowing other readers to participate
    // seq_cst because the following atomic could be reordered to happen prior to this one
    dataBuffer->dataLock.lock.store(1, satomi::memory_order_seq_cst);
    source.sourceBuffer->dataLock.lock.fetch_sub(1, satomi::memory_order_relaxed);

    source.sourceBuffer = dataBuffer;
  }
}

template<> Generation::BaseProcessor *
createProcessor<Generation::EffectModule>(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *copy, void *serialisedSave)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(1));
  return anew(state->processorStorage, Generation::EffectModule,
    { arena, state, metadata, (const Generation::EffectModule *)copy, serialisedSave });
}
template<> void *
initialiseTypeStructure<Generation::EffectModule>(void *, Framework::PluginStructure &structure)
{
  using namespace Framework;
  using namespace Generation;

  auto *arena = structure.getNewArena(COMPLEX_KB(2));

  ProcessorMetadata &effectModule = COMPLEX_STRUCTURE_PROCESSOR(EffectModule, "Effect Module", Generation::Processors::EffectModule);
  effectModule.flags |= ProcessorMetadata::NoParameterValidationTag;
  effectModule.parameters =
    (
      COMPLEX_STRUCTURE_PARAMETER("Module Type", EffectModule::ModuleType,
        {
          .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
            Utility::initialiseTypeStructure(structure),
            Filter::initialiseTypeStructure(structure),
            Dynamics::initialiseTypeStructure(structure),
            Phase::initialiseTypeStructure(structure),
            Pitch::initialiseTypeStructure(structure),
            Destroy::initialiseTypeStructure(structure)),
          .defaultOptionId = Filter::Types::Normal
        }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::AfterProcess),
      COMPLEX_STRUCTURE_PARAMETER("Module Enabled", EffectModule::ModuleEnabled, 0.0f, 1.0f, 1.0f, 1.0f,
        ParameterScale::Toggle, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::Realtime, Framework::printToggleValues),
      COMPLEX_STRUCTURE_PARAMETER("Module Mix", EffectModule::ModuleMix, 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("Low Bound", EffectModule::LowBound, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("High Bound", EffectModule::HighBound, 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, " hz",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("Shift Bounds", EffectModule::ShiftBounds, -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
    );

  return &effectModule.computeCounts();
}
