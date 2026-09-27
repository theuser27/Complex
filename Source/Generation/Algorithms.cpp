
// Created: 2021-07-27 00:30:19

#include "Algorithms.hpp"

#include "Framework/simd_math.hpp"
#include "Framework/parameters.hpp"
#include "Plugin/Complex.hpp"
#include "Effects.hpp"
#include "Interface/LookAndFeel/Skin.hpp"
#include "Data/BinaryData.hpp"
#include "Third Party/nanovg/nanosvg.h"

namespace Interface
{
  NSVGimage *parseSVG(utils::Pool *arena,
    const void *data, usize dataSize);
}

namespace Generation
{
  static EffectData *
  createEffectGeneric(EffectModule *module, EffectData *)
  {
    return anew(module->arena, EffectData, {});
  }

  // unary plus is so that it converts lambdas to function pointers (while also accepting raw functions as well)
#define EFFECT_VTABLE(name, creationFunction, createUIFunction) \
  static constexpr EffectVtable vtable##name = { .createEffect = creationFunction, .runEffect = run##name, .createUI = createUIFunction }
#define COMPLEX_STRUCTURE_EFFECT(nameString, idNumber, vtableStruct, skinOverride, ...) (*anew(arena, Framework::ProcessorMetadata, \
  { .flags = ProcessorMetadata::ProcessorTag, .userFlags = skinOverride, .id = idNumber, .name = nameString __VA_OPT__(,) __VA_ARGS__, .vtable = &vtableStruct })).computeCounts()

  static Framework::Parameter *
  getParameter(EffectData *effectData, uuid id)
  {
    auto *parameter = effectData->parameters;
    for (usize i = 0; i < effectData->parameterCount; (++i), (parameter = parameter->next))
      if (parameter->details.id == id)
        return parameter;

    COMPLEX_ASSERT_FALSE("Couldn't find parameter with id: %zu", id);
    return nullptr;
  }

  Framework::IndexedData *
  Utility::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    auto *arena = structure.getNewArena(128);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Utility", .id = id, .valueCount = 0);
  }

  Framework::IndexedData *
  Filter::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Normal, createEffectGeneric, createUINormal);
    EFFECT_VTABLE(Gate, createEffectGeneric, createUIGate);

    auto *arena = structure.getNewArena(COMPLEX_KB(40));
    auto *parsedSVG = Interface::parseSVG(arena, BinaryData::Icon_Filter_svg, BinaryData::Icon_Filter_svgSize);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Filter", .id = id, .flags = IndexedData::SVGData, .svgData = parsedSVG)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Normal", .id = Types::Normal, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Normal", Types::Normal, vtableNormal, Interface::Skin::kFilterModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Gain", Normal::Gain, { kMinusInfDb, kInfDb, 0.5f }, ParameterScale::SymmetricLoudness,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Cutoff", Normal::Cutoff, { 0.0f, 1.0f, 0.5f }, ParameterScale::Frequency,
              " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Slope", Normal::Slope, { -1.0f, 1.0f, 0.75f }, ParameterScale::SymmetricQuadratic,
              "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
          )
        )
      ),
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Gate", .id = Types::Gate, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Gate", Types::Gate, vtableGate, Interface::Skin::kFilterModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Input Gain", Gate::InputGain, { kMinusInfDb, kInfDb, 0.5f }, ParameterScale::SymmetricLoudness,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Gain", Gate::Gain, { kMinusInfDb, kInfDb, 0.5f }, ParameterScale::SymmetricLoudness,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Threshold", Gate::Threshold, { -180.0f, 0.0f, 0.5f }, ParameterScale::ReverseQuadratic,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Tilt", Gate::Tilt, { -24.0f, 24.0f, 0.5f }, ParameterScale::SymmetricQuadratic,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Mode", Gate::Mode,
              {
                .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Decibels", .id = GateMode::Decibels),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Rank", .id = GateMode::Rank) }}),
                .defaultOptionId = GateMode::Decibels
              }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)
          )
        )
      )
    }});
  }

  Framework::IndexedData *
  Dynamics::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Contrast, createEffectGeneric, createUIContrast);
    EFFECT_VTABLE(Clip, createEffectGeneric, createUIClip);

    auto *arena = structure.getNewArena(COMPLEX_KB(2));
    auto *parsedSVG = Interface::parseSVG(arena, BinaryData::Icon_Dynamics_svg, BinaryData::Icon_Dynamics_svgSize);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Dynamics", .id = id, .flags = IndexedData::SVGData, .svgData = parsedSVG)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Contrast", .id = Types::Contrast, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Contrast", Types::Contrast, vtableContrast, Interface::Skin::kDynamicsModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Depth", Contrast::Depth, { -1.0f, 1.0f, 0.5f }, ParameterScale::Linear,
              "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Range", Contrast::Range, { 0.0f, 1.0f, 1.0f }, ParameterScale::Cubic,
              "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Tilt", Contrast::Tilt, { -24.0f, 24.0f, 0.5f }, ParameterScale::SymmetricQuadratic,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
          )
        )
      ),
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Clip", .id = Types::Clip, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Clip", Types::Clip, vtableClip, Interface::Skin::kDynamicsModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Threshold", Clip::Threshold, { 0.0f, 1.0f, 0.0f }, ParameterScale::Linear,
              "%", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Tilt", Clip::Tilt, { -24.0f, 24.0f, 0.5f }, ParameterScale::SymmetricQuadratic,
              " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
          )
        )
      )
    }});
  }

  Framework::IndexedData *
  Phase::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Shift, createEffectGeneric, createUIShift);

    auto *arena = structure.getNewArena(COMPLEX_KB(4));
    auto *parsedSVG = Interface::parseSVG(arena, BinaryData::Icon_Phase_svg, BinaryData::Icon_Phase_svgSize);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Phase", .id = id, .flags = IndexedData::SVGData, .svgData = parsedSVG)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Shift", .id = Types::Shift, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Contrast", Types::Shift, vtableShift, Interface::Skin::kPhaseModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Phase Shift", Shift::PhaseShift, { -180.0f, 180.0f, 0.5f }, ParameterScale::Linear,
              COMPLEX_DEGREE_SIGN_LITERAL, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Interval", Shift::Interval, { 0.0f, 10.0f, ::powf(1.0f / 10.0f, 1.0f / 3.0f) }, ParameterScale::Cubic,
              " oct", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Offset", Shift::Offset, { 0.0f, 1.0f, 0.0f }, ParameterScale::Frequency,
              " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Slope", Shift::Slope,
              {
                .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Constant", .id = SlopeOptions::Constant),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Linear", .id = SlopeOptions::Linear),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Exponential", .id = SlopeOptions::Exponential) }}),
                .defaultOptionId = SlopeOptions::Constant
              }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)
          )
        )
      )
    }});
  }

  Framework::IndexedData *
  Pitch::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Resample, createEffectGeneric, createUIResample);
    EFFECT_VTABLE(FrequencyShift, createEffectGeneric, createUIFrequencyShift);

    auto *arena = structure.getNewArena(COMPLEX_KB(2));
    auto *parsedSVG = Interface::parseSVG(arena, BinaryData::Icon_Pitch_svg, BinaryData::Icon_Pitch_svgSize);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Pitch", .id = id, .flags = IndexedData::SVGData, .svgData = parsedSVG)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Resample", .id = Types::Resample, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Resample", Types::Resample, vtableResample, Interface::Skin::kPitchModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Shift", Resample::Shift, { -48.0f, 48.0f, 0.5f }, ParameterScale::Linear,
              " st", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Wrap", Resample::Wrap, { 0.0f, 1.0f, 0.0f }, ParameterScale::Toggle,
              {}, ParameterDetails::Modulatable | ParameterDetails::Automatable)
          )
        )
      ),
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Frequency Shift", .id = Types::FrequencyShift, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Frequency Shift", Types::FrequencyShift, vtableFrequencyShift, Interface::Skin::kPitchModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Shift", FrequencyShift::Shift, { -20'000.0f, 20'000.0f, 0.5f }, ParameterScale::SymmetricCubic,
              " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
          )
        )
      )
    }});
  }

  namespace Freeze
  {
    EffectData *
    createEffectRolling(EffectModule *module, EffectData *copy)
    {
      auto *rollingData = anew(module->arena, RollingData, {});
      auto maxBinCount = module->state->getMaxBinCount();

      rollingData->freezeBuffer = Framework::SimdBuffer::create(module->arena,
        kChannelsPerInOut, maxBinCount, copy != nullptr);

      if (copy)
        valcpy(rollingData->freezeBuffer->data,
          ((RollingData *)copy)->freezeBuffer->data, rollingData->freezeBuffer->size);

      return (EffectData *)rollingData;
    }
  }

  Framework::IndexedData *
  Freeze::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Rolling, createEffectRolling, createUIRolling);

    auto *arena = structure.getNewArena(COMPLEX_KB(1));

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Freeze", .id = id)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Rolling", .id = Types::Rolling, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Rolling", Types::Rolling, vtableRolling, Interface::Skin::kPitchModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Rate", Rolling::Rate, { -4.0f, 4.0f, 0.5f }, ParameterScale::SymmetricCubic,
              "x", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Shift", Rolling::Shift, { 0.0f, 1.0f, 0.0f }, ParameterScale::Frequency,
              " hz", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
          )
        )
      )
    }});
  }

  Framework::IndexedData *
  Destroy::initialiseTypeStructure(Framework::PluginStructure &structure)
  {
    using namespace Framework;

    EFFECT_VTABLE(Reinterpret, createEffectGeneric, createUIReinterpret);

    auto *arena = structure.getNewArena(COMPLEX_KB(1));
    auto *parsedSVG = Interface::parseSVG(arena, BinaryData::Icon_Destroy_svg, BinaryData::Icon_Destroy_svgSize);

    return COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Destroy", .id = id, .flags = IndexedData::SVGData, .svgData = parsedSVG)->addChildren({{
      COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Reinterpret", .id = Types::Reinterpret, .flags = IndexedData::ProcessorFlag,
        .processorMetadata = COMPLEX_STRUCTURE_EFFECT("Reinterpret", Types::Reinterpret, vtableReinterpret, Interface::Skin::kDestroyModule, .parameters =
          (
            COMPLEX_STRUCTURE_PARAMETER("Real/Imag Atten", Reinterpret::Attenuation, { kMinusInfDb, kInfDb, 0.5f },
              ParameterScale::SymmetricLoudness, " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
            COMPLEX_STRUCTURE_PARAMETER("Transform", Reinterpret::Transform,
              {
                .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "No Transform", .id = ReinterpretTransformOptions::NoTransform),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Swap Real/Imaginary", .id = ReinterpretTransformOptions::SwitchRealImag),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Cartesian => Polar", .id = ReinterpretTransformOptions::CartToPolar),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Polar => Cartesian", .id = ReinterpretTransformOptions::PolarToCart),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Duplicate Real", .id = ReinterpretTransformOptions::CopyReal),
                  COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Duplicate Imaginary", .id = ReinterpretTransformOptions::CopyImag) }}),
                .defaultOptionId = ReinterpretTransformOptions::NoTransform
              }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable)
          )
        )
      )
    }});
  }


  ///////////////////////////////////////
  //  _   _ _   _ _ _ _   _            //
  // | | | | |_(_) (_) |_(_) ___  ___  //
  // | | | | __| | | | __| |/ _ \/ __| //
  // | |_| | |_| | | | |_| |  __/\__ \ //
  //  \___/ \__|_|_|_|\__|_|\___||___/ //
  ///////////////////////////////////////

  static void circularLoop(const auto &lambda, u32 start, u32 length, u32 binCount)
  {
    COMPLEX_ASSERT(utils::isPowerOfTwo(binCount - 1));

    for (u32 i = 0; i < length - 1; i++)
      lambda((start + i) & (binCount - 2));

    // since we're masking to power-of-2 we skip nyquist
    // process it if passed, else continue onwards
    if ((start + length) > (binCount - 1))
      lambda(binCount - 1);
    else
      lambda(start + length);
  }

  static i4mask vectorcall
  isOutsideBounds(i4 positionIndices,
    i4 lowBoundIndices, i4 highBoundIndices, i4mask isHighAboveLow)
  {
    i4mask belowLowBounds = i4::lessThanSigned(positionIndices, lowBoundIndices);
    i4mask aboveHighBounds = i4::greaterThanSigned(positionIndices, highBoundIndices);

    // 1st case examples: |   *  [    ]     | or
    //                    |      [    ]  *  |
    // 2nd case example:  |     ]   *  [    |
    return (belowLowBounds | aboveHighBounds) & ((belowLowBounds & aboveHighBounds) ^ isHighAboveLow);
  }

  static forceinline i4mask vectorcall isOutsideBounds(
    i4 positionIndices, i4 lowBoundIndices, i4 highBoundIndices)
  {
    return isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices,
      i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices));
  }

  static forceinline i4mask vectorcall isInsideBounds(i4 positionIndices,
      i4 lowBoundIndices, i4 highBoundIndices, i4mask isHighAboveLow)
  { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices, isHighAboveLow); }

  static forceinline i4mask vectorcall isInsideBounds(
    i4 positionIndices, i4 lowBoundIndices, i4 highBoundIndices)
  { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices); }


  // first - low shifted boundary, second - high shifted boundary
  utils::pair<f4, f4>
  getShiftedBounds(EffectModule *module, EffectModule::BoundRepresentation representation,
    float sampleRate, u32 binCount) noexcept
  {
    using namespace utils;
    using namespace Framework;

    u32 FFTSize = (binCount - 1) * 2;

    f4 lowBound = module->getParameter(EffectModule::LowBound)->getInternalValue<f4>(sampleRate, true);
    f4 highBound = module->getParameter(EffectModule::HighBound)->getInternalValue<f4>(sampleRate, true);

    float nyquistFreq = sampleRate * 0.5f;
    float maxOctave = (float)log2(nyquistFreq / kMinFrequency);

    f4 boundShift = module->getParameter(EffectModule::ShiftBounds)
      ->getInternalValue<f4>(sampleRate);
    lowBound = f4::clamp(lowBound + boundShift, 0.0f, 1.0f);
    highBound = f4::clamp(highBound + boundShift, 0.0f, 1.0f);

    switch (representation)
    {
    case EffectModule::BoundRepresentation::Normalised:
      break;
    case EffectModule::BoundRepresentation::Frequency:
      lowBound = exp2(lowBound * maxOctave);
      highBound = exp2(highBound * maxOctave);
      // snapping to 0 hz if it's below the minimum frequency
      lowBound = (lowBound & f4::greaterThan(lowBound, 1.0f)) * kMinFrequency;
      highBound = (highBound & f4::greaterThan(highBound, 1.0f)) * kMinFrequency;

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
  static auto vectorcall
  minimiseRange(i4 lowIndices, i4 highIndices, u32 binCount, bool isProcessedRange)
  {
    COMPLEX_ASSERT(utils::isPowerOfTwo(binCount - 1), "Bin count is not power-of-2 + 1, but instead %d", binCount);

    // 1. the indices in the respective bounds are different (stereo)
    // 2. all the indices in the respective bounds are the same (mono)

    struct
    {
      u32 start;
      u32 distance;
      bool isStereoRange;
    } ret;

    // 1.
    ret = { 0, binCount, true };

    // 2.
    if (i4::allSame(lowIndices) && i4::allSame(highIndices))
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
    Framework::SimdBuffer *destination, i4 lowBoundIndices,
    i4 highBoundIndices, u32 binCount) noexcept
  {
    COMPLEX_ASSERT(destination->size == source->size);
    auto [index, unprocessedCount, isStereo] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, false);

    usize channels = utils::min(source->channels, destination->channels);
    usize size = destination->size;
    auto rawSource = source->get();
    auto rawDestination = destination->get();

    if (isStereo)
    {
      i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
      // because procesing bounds can go past each other we assume they've done so
      // and split the algorithm in 2 regions (bottom and top)
      // if the bounds haven't passed each other then the 2nd iteration of the algorithm will just not run
      i4 starts[] = { (highBoundIndices + 1) & ~isHighAboveLow, (highBoundIndices + 1) & isHighAboveLow };
      i4 lengths[] = { lowBoundIndices - starts[0], (binCount - starts[1]) & isHighAboveLow };

      for (usize i = 0; i < channels; i += source->kRelativeSize)
      {
        auto sourceChannel = rawSource.offset(i * size);
        auto destinationChannel = rawDestination.offset(i * size);

        for (usize iteration = 0; iteration <= countof(starts); ++iteration)
        {
          i4 start = starts[iteration];
          i4 length = lengths[iteration];

          while (true)
          {
            i4mask runNotCompleteMask = i4mask::greaterThanSigned(length, 0);
            // if all lengths are 0 or negative then we've completed all runs
            if (i4mask::anyMask(runNotCompleteMask) == 0)
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

  static f4 vectorcall
  matchPower(f4 target, f4 current)
  {
    f4 result = 1.0f;
    result = result & f4::greaterThan(target, 0.0f);
    result = utils::merge(result, f4::sqrt(target / current), f4::greaterThan(current, 0.0f));

    result = utils::merge(result, f4{ 1.0f }, f4::greaterThan(result, 1e30f));
    result = result & f4::lessThanOrEqual(1e-37f, result);
    return result;
  }

  static f4 vectorcall
  getTiltSlopeMultiplier(f4 tiltDb, float sampleRate,
    u32 binCount, float minFrequency = (float)kMinFrequency)
  {
    f4 decadeSlope = tiltDb * kOctaveToDecadeConversionMult;
    float sampleHz = sampleRate / (float)(2 * binCount);
    float startDecade = ::log10f(minFrequency / sampleHz);
    float decadeCount = ::log10f(sampleRate / (2.0f * minFrequency));
    float resolution = 1.0f / ((float)binCount - 1.0f);
    return dbToAmplitude(((decadeCount + startDecade) * decadeSlope) * resolution);
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
  // 3. If no output/no unprocessed ranges or only dry/wet signal can be heard, be sure to check masking logic

  // placeholder processing while developing
#define PASSTHROUGH_PROCESS(outputBuffer, inputBuffer, binCount) do { \
  auto rawSource = inputBuffer->get();                                \
  auto rawDestination = destination->get();                           \
  for (u32 _i = 0; _i < binCount; _i++)                               \
    rawDestination[_i] = rawSource[_i]; } while(false)

  void Filter::runNormal(EffectModule *effectModule, EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    f4 lowBoundNorm = effectModule->getParameter(EffectModule::LowBound)->getInternalValue<f4>(sampleRate, true);
    f4 highBoundNorm = effectModule->getParameter(EffectModule::HighBound)->getInternalValue<f4>(sampleRate, true);
    f4 boundShift = effectModule->getParameter(EffectModule::ShiftBounds)->getInternalValue<f4>(sampleRate);
    f4 boundsDistance = modOnce(f4{ 1.0f } + highBoundNorm - lowBoundNorm, 1.0f);

    u32 FFTSize = (binCount - 1) * 2;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto [low, high] = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(low), toInt(high) };
    }();
    i4mask isHighAboveLowMask = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // cutoff is described as exponential normalised value of the sample rate
    // it is dependent on the values of the low/high bounds
    f4 cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance *
      getParameter(effectData, Filter::Normal::Cutoff)->getInternalValue<f4>(sampleRate, true), 1.0f, false);
    i4 cutoffIndices = toInt(normalisedToBin(cutoffNorm, FFTSize, sampleRate));

    // if mask scalars are negative/positive -> brickwall/linear slope
    // slopes are logarithmic
    f4 slopes = getParameter(effectData, Filter::Normal::Slope)->getInternalValue<f4>(sampleRate) / 2.0f;
    i4mask slopeMask = ~unsignSimd<true>(slopes);
    i4mask slopeZeroMask = f4::equal(slopes, 0.0f);

    // if scalars are negative/positive, attenuate at/around cutoff
    // (gains is gain reduction in db and NOT a gain multiplier)
    f4 gainsParameter = getParameter(effectData, Filter::Normal::Gain)->getInternalValue<f4>(sampleRate);
    i4mask gainType = unsignSimd<true>(gainsParameter);

    // copy both un/processed data
    applyToThisNoMask<MathOperations::Assign>(destination, source.sourceBuffer, destination->channels, binCount);

    auto calculateDistancesFromCutoffs = [cutoffIndices, lowBoundIndices,
      cutoffAboveLowMask = i4mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundIndices),
      invLog2Nyquist = f4{ (float)(1.0 / ::log2(sampleRate * 0.5 / kMinFrequency)) },
      binDivisor = f4{ (float)(sampleRate / (FFTSize * kMinFrequency)) }](i4 positionIndices)
    {
      // 1. both positionIndices and cutoffIndices are >= lowBound and < FFTSize_ or <= highBound and > 0
      // 2. cutoffIndices/positionIndices is >= lowBound and < FFTSize_ and
      //     positionIndices/cutoffIndices is <= highBound and > 0

      using namespace utils;

      i4mask cutoffAbovePositions = i4::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

      // preparing masks for 1.
      i4mask positionsAboveLowMask = i4::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
      i4mask bothAboveOrBelowLowMask = ~(positionsAboveLowMask ^ cutoffAboveLowMask);

      // preparing masks for 2.
      i4mask positionsBelowLowBoundAndCutoffsMask = ~positionsAboveLowMask & cutoffAboveLowMask;
      i4mask cutoffBelowLowBoundAndPositionsMask = positionsAboveLowMask & ~cutoffAboveLowMask;

      // masking for 1.
      i4 precedingIndices  = merge(cutoffIndices  , positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
      i4 succeedingIndices = merge(positionIndices, cutoffIndices  , bothAboveOrBelowLowMask & cutoffAbovePositions);

      // masking for 2.
      // first 2 are when cutoffs/positions are above/below lowBound
      // second 2 are when positions/cutoffs are above/below lowBound
      precedingIndices  = merge(precedingIndices , cutoffIndices  , ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
      succeedingIndices = merge(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
      precedingIndices  = merge(precedingIndices , positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );
      succeedingIndices = merge(succeedingIndices, cutoffIndices  , ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );

      auto binToNormalised = [&](i4 bin)
      {
        return (log2(toFloat(bin) * binDivisor) * invLog2Nyquist) & i4::notEqual(bin, 0);
      };

      auto ratio = getDecimalPlaces(f4{ 1.0f } + binToNormalised(succeedingIndices) - binToNormalised(precedingIndices));
      // all i have to say is, floating point error
      return ratio & i4::notEqual(precedingIndices, succeedingIndices);
    };

    auto rawDestination = destination->get();
    circularLoop([&](u32 index)
      {
        // the distances are logarithmic
        // TODO: memoise cutoff distances and update only when low/high bound indices and binCount are changed
        f4 distancesFromCutoff = calculateDistancesFromCutoffs(index);

        // calculating linear slope and brickwall, both are ratio of the gain attenuation
        // the higher tha value the more it will be affected by it
        f4 gainRatio = merge(
          f4::clamp(merge(distancesFromCutoff / slopes, f4{ 1.0f }, slopeZeroMask), 0.0f, 1.0f),
          f4{ 1.0f } & f4::greaterThanOrEqual(distancesFromCutoff, slopes),
          slopeMask);
        f4 gains = merge(gainsParameter * gainRatio, gainsParameter * (f4{ 1.0f } - gainRatio), gainType);

        // convert db reduction to amplitude multiplier
        gains = dbToAmplitude(-gains);

        rawDestination[index] = merge(rawDestination[index] * gains, rawDestination[index],
          isOutsideBounds(start, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      }, start, processedCount, binCount);
  }

  void Filter::runGate(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLowMask = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    //auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    f4 threshold = (float)binCount * dbToAmplitude(getParameter(effectData, Filter::Gate::Threshold)
      ->getInternalValue<f4>(sampleRate));
    f4 gainParameter = getParameter(effectData, Filter::Gate::Gain)->getInternalValue<f4>(sampleRate);
    i4mask gainType = ~unsignSimd<true>(gainParameter);
    gainParameter = -gainParameter;

    f4 slope = 1.0f;
    f4 slopeMultiplier = getTiltSlopeMultiplier(
      getParameter(effectData, Filter::Gate::Tilt)->getInternalValue<f4>(sampleRate),
      sampleRate, binCount);

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();

    for (u32 i = 0; i < binCount; ++i)
    {
      f4 dry = rawSource[i];
      i4mask isAboveThresholdMask = f4::greaterThanOrEqual(complexMagnitude(dry, true) * slope, threshold);
      // dry >= threshold && gain >= 0db ===> don't change
      // dry >= threshold && gain <  0db ===> attenuate
      // dry <  threshold && gain >= 0db ===> attenuate
      // dry <  threshold && gain <  0db ===> don't change
      f4 gains = dbToAmplitude(gainParameter & (gainType ^ isAboveThresholdMask));

      rawDestination[i] = merge(dry * gains, dry,
        isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      slope *= slopeMultiplier;
    }
  }

  void Dynamics::runContrast(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLowMask = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    f4 slopeMultiplier = getTiltSlopeMultiplier(
      getParameter(effectData, Dynamics::Contrast::Tilt)->getInternalValue<f4>(sampleRate),
      sampleRate, binCount);

    auto rawSource = source.sourceBuffer->get();
    auto rawScratch = source.scratchBuffer->get();
    auto rawDestination = destination->get();

    // starting point is the weighted average
    //
    // weights should be determined based on human perception (e^(-index) or 2^(-index)?)
    // because we're basing things on human perception
    // we can take a subset of the entire spectrum to compute the average
    // skip some high frequency content entirely, which is where the bulk of the bins are
    // if we want XY% coverage we need to take the first
    // `(kMinFrequency * (sampleRate / (2 * kMinFrequency))^XY) / (sampleRate / (2 * binCount))` number of bins
    // then weighted avg = sum(magnitude[i] * weight[i]) / sum(weight[i])
    // where the sum of all the weights is 1 / (log(base) * base^x)
    //
    // range is percentage of the all bins (determined by parameter)
    // only the bins in that range participate

    f4 maxMagnitude = 0.0f;
    f4 minMagnitude = kFloatInf;
    f4 avg = 0.0f;
    f4 slope = 1.0f;
    // 2^(-i)
    f4 weight = 1.0f;
    for (u32 i = 0; i < binCount; ++i)
    {
      // copy both un/processed data
      rawDestination[i] = rawSource[i];
      i4mask isInsideBoundsMask = isInsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
      rawScratch[i] = reinterpretToFloat(isInsideBoundsMask);
      f4 magnitude = slope * complexMagnitude(rawSource[i], true);
      avg = merge(avg, avg + magnitude, isInsideBoundsMask);
      maxMagnitude = merge(maxMagnitude, magnitude, f4::greaterThan(magnitude, maxMagnitude) & isInsideBoundsMask);
      minMagnitude = merge(minMagnitude, magnitude, f4::lessThan(magnitude, minMagnitude) & isInsideBoundsMask);

      weight *= 0.5f;
      slope *= slopeMultiplier;
    }

    {
      // integral(0, inf, 2^(-i)), sum of all the weights
      static constexpr f4 kWeightSum = 1.0f / const_math::log(2.0f);
      avg /= kWeightSum * (float)binCount;

      f4 avgDb = utils::amplitudeToDb(avg);
      f4 minDb = utils::amplitudeToDb(minMagnitude);
      f4 maxDb = utils::amplitudeToDb(maxMagnitude);

      COMPLEX_ASSERT(i4mask::anyMask(f4::greaterThan(avgDb, maxDb)) == 0);
      COMPLEX_ASSERT(i4mask::anyMask(f4::lessThan(avgDb, minDb)) == 0);

      f4 rangeParameter = getParameter(effectData, Dynamics::Contrast::Range)->getInternalValue<f4>(sampleRate);
      f4 dbRange = (maxDb - minDb) * rangeParameter;
      f4 newMinDb = f4::max(minDb, avgDb - dbRange * 0.5f);
      maxDb = f4::min(newMinDb + dbRange, maxDb);
      minDb = maxDb - dbRange;

      minMagnitude = utils::dbToAmplitude(minDb);
      maxMagnitude = utils::dbToAmplitude(maxDb);

      // squaring the magnitudes because we use squared norms in the processing below
      minMagnitude *= minMagnitude;
      maxMagnitude *= maxMagnitude;
    }

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);
    slope = utils::pow(slopeMultiplier, (float)start);

    f4 inPower = 0.0f;
    circularLoop([&](u32 index)
      {
        f4 magnitude = slope * complexMagnitude(rawDestination[index], false);
        rawScratch[index] &= f4::greaterThanOrEqual(magnitude, minMagnitude) &
          f4::lessThanOrEqual(magnitude, maxMagnitude);
        inPower += magnitude & rawScratch[index];
        slope *= slopeMultiplier;
      }, start, processedCount, binCount);

    // calculating contrast
    f4 depthParameter = getParameter(effectData, Dynamics::Contrast::Depth)
      ->getInternalValue<f4>(sampleRate);
    f4 contrast = depthParameter * depthParameter;
    contrast = merge(kContrastMaxNegativeValue * contrast,
      kContrastMaxPositiveValue * contrast,
      f4::greaterThanOrEqual(depthParameter, 0.0f));

    f4 min = exp(-80.0f / (contrast * 2.0f + 1.0f));
    f4 max = exp( 80.0f / (contrast * 2.0f + 1.0f));
    min = merge(1e-30f, min, f4::greaterThan(contrast, 0.0f));
    max = merge(1e+30f, max, f4::greaterThan(contrast, 0.0f));

    i4 boundDistanceCount = (modOnce(binCount + highBoundIndices - lowBoundIndices, binCount) + 1)
      & i4::notEqual(lowBoundIndices, highBoundIndices);
    f4 inScale = matchPower(toFloat(boundDistanceCount), inPower);

    slope = utils::pow(slopeMultiplier, (float)start);

    // applying gain and calculating outPower
    f4 outPower = 0.0f;
    circularLoop([&](u32 index)
      {
        f4 bin = slope * inScale * rawDestination[index];
        f4 magnitude = complexMagnitude(bin, true);

        bin &= f4::lessThanOrEqual(min, magnitude);
        bin = merge(bin, bin * pow(magnitude, contrast), f4::greaterThan(max, magnitude));

        outPower += complexMagnitude(bin, false) & reinterpretToInt(rawScratch[index]);
        rawDestination[index] = merge(rawDestination[index], bin / slope, reinterpretToInt(rawScratch[index]));
        slope *= slopeMultiplier;
      }, start, processedCount, binCount);

    // normalising
    f4 outScale = matchPower(inPower, outPower);
    circularLoop([&](u32 index)
      {
        rawDestination[index] *= merge(1.0f, outScale, reinterpretToInt(rawScratch[index]));
      }, start, processedCount, binCount);
  }

  void Dynamics::runClip(EffectModule *effectModule, EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    static constexpr float kSilenceThreshold = 1e-30f;
    static constexpr float kLoudestThreshold = 1e+30f;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    i4mask isHighAboveLowMask = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    COMPLEX_ASSERT(source.sourceBuffer->size == destination->size);

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();

    // getting the min/max power in the range selected
    utils::pair<f4, f4> powerMinMax{ kLoudestThreshold, kSilenceThreshold };
    for (u32 j = 0; j < binCount; j++)
    {
      // while calculating the power min-max we can copy over data
      rawDestination[j] = rawSource[j];

      f4 magnitude = complexMagnitude(rawDestination[j], false);
      i4mask isIndexOutside = isOutsideBounds(j, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
      powerMinMax.first  = merge(f4::min(powerMinMax.first , magnitude), powerMinMax.first , isIndexOutside);
      powerMinMax.second = merge(f4::max(powerMinMax.second, magnitude), powerMinMax.second, isIndexOutside);
    }

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // calculating clipping
    f4 thresholdParameter = getParameter(effectData, Dynamics::Clip::Threshold)
      ->getInternalValue<f4>(sampleRate);
    thresholdParameter = thresholdParameter * thresholdParameter * thresholdParameter;
    f4 threshold = exp(lerp(log(f4::max(powerMinMax.first, 1e-36f)),
                                    log(f4::max(powerMinMax.second, 1e-36f)),
                                    f4{ 1.0f } - thresholdParameter));
    f4 slopeMultiplier = getTiltSlopeMultiplier(
      -getParameter(effectData, Dynamics::Clip::Tilt)->getInternalValue<f4>(sampleRate),
      sampleRate, binCount);
    // reset slope to the start of the covered range
    threshold *= utils::pow(slopeMultiplier, (float)start);

    // doing clipping
    f4 inPower = 0.0f;
    f4 outPower = 0.0f;
    circularLoop([&](u32 index)
      {
        i4mask isIndexInside = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
        f4 magnitude = complexMagnitude(rawDestination[index], false);

        // 0/0 and >0/0 masking, prevents NaN and Inf respectively
        f4 rescale = f4::sqrt(threshold / magnitude) &
          f4::notEqual(threshold, 0.0f) & f4::notEqual(magnitude, 0.0f);
        rawDestination[index] *= merge(1.0f, rescale,
          f4::greaterThanOrEqual(magnitude, threshold) & isIndexInside);

        inPower += magnitude & isIndexInside;
        outPower += f4::min(magnitude, threshold) & isIndexInside;
        threshold *= slopeMultiplier;
      }, start, processedCount, binCount);

    // normalising
    f4 outScale = matchPower(inPower, outPower);
    outScale = merge(outScale, 1.0f, f4::isNan(outScale));
    circularLoop([&](u32 index)
      {
        rawDestination[index] = merge(rawDestination[index] * outScale, rawDestination[index],
          isOutsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      }, start, processedCount, binCount);
  }

  // TODO: broken version but makes cool artifacts
  /*void Dynamics::runClip(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLowMask = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    COMPLEX_ASSERT(source.sourceBuffer->size == destination->size);

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();

    // getting the min/max power in the range selected
    utils::pair<f4, f4> powerMinMax{ kLoudestThreshold, kSilenceThreshold };
    f4 slope = 1.0f;
    f4 slopeMultiplier = getTiltSlopeMultiplier(
      getParameter(effectData, Dynamics::Clip::Tilt)->getInternalValue<f4>(sampleRate),
      sampleRate, binCount);

    for (u32 j = 0; j < binCount; j++)
    {
      // while calculating the power min-max we can copy over data
      rawDestination[j] = rawSource[j];

      f4 magnitude = slope * complexMagnitude(rawDestination[j], false);
      i4mask isIndexOutside = isOutsideBounds(j, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
      powerMinMax.first  = merge(f4::min(powerMinMax.first , magnitude), powerMinMax.first , isIndexOutside);
      powerMinMax.second = merge(f4::max(powerMinMax.second, magnitude), powerMinMax.second, isIndexOutside);
      slope *= slopeMultiplier;
    }

    // calculating clipping
    f4 thresholdParameter = getParameter(effectData, Dynamics::Clip::Threshold)
      ->getInternalValue<f4>(sampleRate);
    thresholdParameter = thresholdParameter * thresholdParameter * thresholdParameter;
    f4 threshold = exp(lerp(log(f4::max(powerMinMax.first, 1e-36f)),
                                    log(f4::max(powerMinMax.second, 1e-36f)),
                                    f4{ 1.0f } - thresholdParameter));
    f4 sqrtThreshold = f4::sqrt(threshold);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // reset slope to the start of the covered range
    slope = utils::pow(slopeMultiplier, (float)start);

    // doing clipping
    f4 inPower = 0.0f;
    f4 outPower = 0.0f;
    circularLoop([&](u32 index)
      {
        i4mask isIndexInside = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask);
        f4 magnitude = slope * complexMagnitude(rawDestination[index], false);

        rawDestination[index] = merge(rawDestination[index],
          rawDestination[index] * sqrtThreshold / f4::sqrt(magnitude),
          f4::greaterThanOrEqual(magnitude, threshold) & isIndexInside);

        inPower += magnitude & isIndexInside;
        outPower += f4::min(magnitude, threshold) & isIndexInside;
        slope *= slopeMultiplier;
      }, start, processedCount, binCount);

    // normalising
    f4 outScale = matchPower(inPower, outPower);
    circularLoop([&](u32 index)
      {
        auto result = rawDestination[index] * outScale;
        rawDestination[index] = merge(result / outScale, rawDestination[index],
          isOutsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLowMask));
      }, start, processedCount, binCount);
  }*/

  void Phase::runShift(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    f4 shiftIncrement = cis(kPi * (getParameter(effectData, Phase::Shift::PhaseShift)
      ->getInternalValue<f4>(sampleRate, true) * 2.0f - 1.0f));
    f4 shift = shiftIncrement;
    f4 interval = getParameter(effectData, Phase::Shift::Interval)->getInternalValue<f4>(sampleRate);

    auto slopeFunction = [&]() -> f4(*)(f4, f4)
    {
      auto [slopeId, _] = getParameter(effectData, Phase::Shift::Slope)->getInternalValue<IndexedData>(sampleRate);
      if (slopeId->id == Phase::SlopeOptions::Constant)
        return [](f4 x, f4) { return x; };
      else if (slopeId->id == Phase::SlopeOptions::Linear)
      {
        // explicitly normalising because the result shoots up at some point
        return [](f4 x, f4 increment)
        {
          auto y = complexCartMul(x, increment);
          return y * f4::invSqrt(complexMagnitude(y, false));
        };
      }
      else if (slopeId->id == Phase::SlopeOptions::Exponential)
      {
        return [](f4 x, f4)
        {
          auto y = complexCartMul(x, x);
          // necessary renormalisation because floating point inaccuracies cause +/- inf explosions
          return y * f4::invSqrt(complexMagnitude(y, false));
        };
      }

      COMPLEX_ASSERT_FALSE("Invalid slope chosen");
      return [](f4 x, f4) { return x; };
    }();

    // overwrite old data
    applyToThisNoMask<MathOperations::Assign>(destination, source.sourceBuffer, destination->channels, binCount);

    auto rawDestination = destination->get();

    // if interval between bins is 0 this means every bin is affected
    if (f4::allEqual(interval, 0.0f))
    {
      i4 offsetBin = toInt(normalisedToBin(getParameter(effectData, Phase::Shift::Offset)
        ->getInternalValue<f4>(sampleRate, true), 2 * (binCount - 1), sampleRate));

      // find the smallest offset forward and start from there
      u32 minOffset = horizontalMin(offsetBin)[0];
      u32 indexChange = (u32)clamp((i32)minOffset - (i32)start, 0, (i32)processedCount);
      processedCount -= indexChange;
      start += indexChange;

      circularLoop([&](u32 index)
        {
          i4mask offsetMask = i4::greaterThanOrEqualSigned(index, offsetBin);
          i4mask insideRangeMask = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
          rawDestination[index] = merge(rawDestination[index],
            complexCartMul(rawDestination[index], shift),
            offsetMask & insideRangeMask);

          shift = slopeFunction(shift, shiftIncrement);
        }, start, processedCount, binCount);

      return;
    }

    // otherwise the interval specifies how many octaves up the next affected bin is

    // offset is skewed towards an exp-like curve so we need to normalise it
    f4 offsetNorm = getParameter(effectData, Phase::Shift::Offset)
      ->getInternalValue<f4>(sampleRate) * 2.0f / sampleRate;
    f4 binStep = 1.0f / (float)(binCount - 1);
    f4 log2Base = log2(interval + 1.0f);
    COMPLEX_ASSERT(i4mask::anyMask(f4::lessThanOrEqual(log2Base, 0.0f)) == 0);

    // if offset is 0 we need to give it a starting value based on interval
    // and shift dc component's amplitude
    {
      i4mask zeroMask = f4::lessThan(offsetNorm, binStep);
      rawDestination[0] = merge(rawDestination[0], complexCartMul(rawDestination[0], shift) & kMagnitudeMask, zeroMask);
      shift = merge(shift, slopeFunction(shift, shiftIncrement), zeroMask);

      f4 startOffset = interval * binStep;
      COMPLEX_ASSERT(i4mask::anyMask(f4::lessThanOrEqual(startOffset, 0.0f)) == 0);

      // this is derived below, the next 2 lines get the next bin after dc in case any channels started there
      f4 multiple = f4::ceil(log2(binStep / startOffset) / log2Base);
      startOffset *= exp2(log2Base * multiple);
      offsetNorm = merge(offsetNorm, startOffset, zeroMask);
    }

    auto algorithm = [&]()
    {
      i4 indices = toInt(f4::round(offsetNorm * (float)binCount));
      i4mask indexMask = isInsideBounds(indices, lowBoundIndices, highBoundIndices, isHighAboveLow);
      i4mask lessMask = i4::lessThanSigned(indices, binCount);

      // have all indices gone above nyquist?
      if (i4mask::anyMask(lessMask) == 0)
        return false;

      f4 values = gatherComplex(rawDestination.pointer, indices & lessMask);
      scatterComplex(rawDestination.pointer, indices & lessMask,
        complexCartMul(values, shift), indexMask);

      shift = merge(shift, slopeFunction(shift, shiftIncrement), indexMask);

      return true;
    };

    // if inteval < 1, then it's possible for the next while loop to make any progress,
    // so we make sure that interval * offsetNorm will yield a number at least as big as a single bin step
    {
      f4 increment = interval * offsetNorm;
      f4 nextBin = (f4::round(offsetNorm * (float)binCount) + 1.0f) / (float)binCount;
      // repeat this until all increments are bigger than a single bin step
      while (i4mask::anyMask(f4::lessThan(increment, binStep)))
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
        f4 multiple = f4::ceil(log2(nextBin / offsetNorm) / log2Base);

        // pow(base, exponent) = exp2(log2(base) * exponent)
        offsetNorm *= exp2(log2Base * multiple);
        increment = interval * offsetNorm;
        nextBin += binStep;
      }
    }

    while (true)
    {
      if (!algorithm())
        break;

      offsetNorm = f4::mulAdd(offsetNorm, interval, offsetNorm);
    }
  }

  void Pitch::runResample(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    f4 shift = exp2(getParameter(effectData, Pitch::Resample::Shift)
      ->getInternalValue<f4>(sampleRate) / (float)kNotesPerOctave);

    bool wrapAround = getParameter(effectData, Pitch::Resample::Wrap)->getInternalValue<u32>(sampleRate);

    f4 leakMultipliers[2 * kNeighbourBins + 1];
    f4 phaseShift{};

    auto calculateCoefficients = [&](f4 binFloatingPointShift) mutable
    {
      auto cycle = binFloatingPointShift * source.blockPhase;
      // modding the phase to get more accurate values, not important
      cycle -= f4::round(cycle * 0.5f) * 2.0f;
      phaseShift = cis(cycle * k2Pi);

      f4 denominator = (f4::round(binFloatingPointShift) - binFloatingPointShift) * k2Pi;
      f4 numerator = (f4{ 0.0f, 1.0f } - switchInner(cis(denominator))) ^ i4mask{ kSignMask, 0U };

      // this might at some point in time fail and the effect will produce silence unless gain matching is enabled
      // in that case this guard has not done its job and should be increased
      i4mask numZeroMask = f4::lessThan(complexMagnitude(numerator, true), kMultiplierEpsilon);

      for (i32 i = 0; i < (i32)countof(leakMultipliers); ++i)
      {
        f4 fullDenominator = denominator + k2Pi * (float)(i - kNeighbourBins);
        i4mask denZeroMask = f4::lessThan(f4::abs(fullDenominator), kMultiplierEpsilon);
        fullDenominator = merge(fullDenominator, f4{ 1.0f }, denZeroMask);
        // guarding against 0.0 / 0.0
        leakMultipliers[i] = merge(numerator / fullDenominator, f4{ 1.0f, 0.0f }, numZeroMask & denZeroMask);
      }
    };

    destination->clear();
    copyUnprocessedData(source.sourceBuffer, destination, lowBoundIndices, highBoundIndices, binCount);

    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    if (!wrapAround)
    {
      f4 downscaledNyquist = (float)(binCount - 1) / shift;

      // because procesing bounds can go past each other we assume they've done so
      // and split the algorithm in 2 regions (bottom and top)
      // if the bounds haven't passed each other then the 2nd iteration of the algorithm will just not run
      i4 starts[] = { lowBoundIndices & isHighAboveLow, lowBoundIndices & ~isHighAboveLow };
      // shortening the range by however many bins get shifted out of bounds as well
      i4 lengths[] =
      {
        (highBoundIndices + 1) - starts[0] - toInt(f4::max(0.0f, toFloat(highBoundIndices) - downscaledNyquist)),
        (binCount - starts[1] - toInt(f4::max(0.0f, (float)(binCount - 1) - downscaledNyquist))) & ~isHighAboveLow
      };

      for (usize iteration = 0; iteration < countof(starts); ++iteration)
      {
        i4 start = starts[iteration];
        i4 length = lengths[iteration];
        f4 destinationIndices = shift * toFloat(start);

        while (true)
        {
          i4mask runNotCompleteMask = i4mask::greaterThanSigned(length, 0);
          // if all lengths are 0 or negative then we've completed all runs
          if (i4mask::anyMask(runNotCompleteMask) == 0)
            break;

          calculateCoefficients(destinationIndices - toFloat(start));

          f4 wet = complexCartMul(gatherComplex(rawSource.pointer, start) & runNotCompleteMask, phaseShift);
          i4 destinationIndicesInt = i4::minUnsigned(toInt(f4::round(destinationIndices)), binCount - 1);

          for (i32 j = 0; j < (i32)countof(leakMultipliers); ++j)
          {
            i4 indices = destinationIndicesInt - kNeighbourBins + j;
            i4 clampedIndices = i4::clampSigned(0, binCount - 1, indices);
            i4mask inRangeMask = i4::equal(indices, clampedIndices);

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
      /*f4 destinationIndices = shift * toFloat(lowBoundIndices);
      f4 lengths = toFloat(modOnce(binCount + highBoundIndices - lowBoundIndices, binCount, false));

      while (true)
      {
        i4mask runCompletedMask = i4mask::equal(getSign(lengths), kSignMask);
        // if all lengths go negative then we've completed all runs
        if (runCompletedMask.anyMask() == 0)
          break;

        i4 destinationIndicesInt = toInt(destinationIndices);


        i4mask outsideBoundsMask = isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow);
        f4 wet = rawSource[i] & (~outsideBoundsMask &);

        calculateCoefficients.next();

        for (i32 j = 0; j < leakMultipliers.size(); ++j)
        {
          i4 indices = i4{ (u32)((i32)i - kNeighbourBins + j) } + binShift;
          i4 clampedIndices = i4::clampSigned(0, binCount - 1, indices);
          i4mask inRangeMask = i4::equal(indices, clampedIndices);

          f4 result = gatherComplex(rawDestination.pointer, clampedIndices);
          scatterComplex(rawDestination.pointer, clampedIndices,
            merge(result, result + complexCartMul(wet, leakMultipliers[j]), inRangeMask));
        }

        lengths -= shift;
        destinationIndices = modOnce(destinationIndices + shift, binCount);
      }*/
    }
  }

  void Pitch::runFrequencyShift(EffectModule *effectModule, EffectData *effectData,
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
    i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    i4 binShift{};
    f4 leakMultipliers[2 * kNeighbourBins + 1];
    {
      f4 binFloatingPointShift = getParameter(effectData, Pitch::FrequencyShift::Shift)
        ->getInternalValue<f4>(sampleRate) * 2.0f * (float)(binCount - 1) / sampleRate;
      f4 roundedShift = f4::round(binFloatingPointShift);
      binShift = toInt(roundedShift);

      f4 phaseShift{};
      {
        auto cycle = binFloatingPointShift * source.blockPhase;
        // modding the phase to get more accurate values, not important
        cycle -= f4::round(cycle * 0.5f) * 2.0f;
        phaseShift = cis(cycle * k2Pi);
      }
      f4 denominator = (f4::round(binFloatingPointShift) - binFloatingPointShift) * k2Pi;
      f4 numerator = (f4{ 0.0f, 1.0f } - switchInner(cis(denominator))) ^ i4mask{ kSignMask, 0U };

      // this might at some point in time fail and the effect will produce silence unless gain matching is enabled
      // in that case this guard has not done its job and should be increased
      i4mask numZeroMask = f4::lessThan(complexMagnitude(numerator, true), kMultiplierEpsilon);

      for (i32 i = 0; i < (i32)countof(leakMultipliers); ++i)
      {
        f4 fullDenominator = denominator + k2Pi * (float)(i - kNeighbourBins);
        i4mask denZeroMask = f4::lessThan(f4::abs(fullDenominator), kMultiplierEpsilon);
        fullDenominator = merge(fullDenominator, f4{ 1.0f }, denZeroMask);
        // guarding against 0.0 / 0.0
        leakMultipliers[i] = merge(numerator / fullDenominator, f4{ 1.0f, 0.0f }, numZeroMask & denZeroMask);
        leakMultipliers[i] = complexCartMul(leakMultipliers[i], phaseShift);
      }
    }

    destination->clear();
    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    for (u32 i = 0; i < binCount; ++i)
    {
      i4mask outsideBoundsMask = isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow);
      rawDestination[i] += rawSource[i] & outsideBoundsMask;
      f4 wet = rawSource[i] & ~outsideBoundsMask;

      for (u32 j = 0; j < countof(leakMultipliers); ++j)
      {
        i4 indices = i4{ (i - (u32)kNeighbourBins + j) } + binShift;
        i4 clampedIndices = i4::clampSigned(0, binCount - 1, indices);
        i4mask inRangeMask = i4::equal(indices, clampedIndices);

        scatterAddComplex(rawDestination.pointer, clampedIndices,
          complexCartMul(wet, leakMultipliers[j]), inRangeMask);
      }
    }
  }

  void Freeze::runRolling(EffectModule *effectModule, EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination, 
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    // TODO: linear vs frequency slope

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    auto rawSource = source.sourceBuffer->get();
    auto rawDestination = destination->get();
    auto *rollingData = (RollingData *)effectData;
    auto rawFreezeBuffer = rollingData->freezeBuffer->get();

    // copy over data we don't have
    if (rollingData->lastBinCount < binCount)
      for (usize i = rollingData->lastBinCount; i < binCount; ++i)
        rawFreezeBuffer[i] = rawSource[i];

    f4 mod = (float)binCount;
    f4 rate = getParameter(effectData, Freeze::Rolling::Rate)->getInternalValue<f4>();
    f4 offset = rate * (float)(source.blockPosition - rollingData->lastBlockPosition);
    offset = f4::min(mod, offset);

    f4 oldPosition = f4::min(rollingData->lastPosition, mod - 1.0f);
    f4 newPosition = modOnce(mod + modOnce(oldPosition + offset, mod), mod);

    i4 start, end;
    {
      i4mask switchBounds = f4::lessThan(rate, 0.0f);
      start = toInt(f4::round(oldPosition));
      end = toInt(f4::round(newPosition));
      auto temp = merge(start, end, switchBounds);
      end = merge(end, start, switchBounds);
      start = temp;
    }
    i4mask isHighAboveLow2 = i4::greaterThanSigned(end, start);

    for (u32 i = 0; i < binCount; ++i)
    {
      f4 wet = merge(rawSource[i], rawFreezeBuffer[i], isOutsideBounds(i, start, end, isHighAboveLow2));
      rawFreezeBuffer[i] = wet;
      rawDestination[i] = merge(wet, rawSource[i],
        isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow));
    }
    
    rollingData->lastPosition = newPosition;
    rollingData->lastBinCount = binCount;
    rollingData->lastBlockPosition = source.blockPosition;
  }

  hotreloadable void Destroy::runReinterpret(EffectModule *effectModule, EffectData *effectData,
    Framework::ComplexDataSource &source, Framework::SimdBuffer *destination,
    u32 binCount, float sampleRate) noexcept
  {
    using namespace utils;
    using namespace Framework;

    COMPLEX_HOTRELOAD_CHECK(effectModule->state, runReinterpret, effectModule,
      effectData, source, destination, binCount, sampleRate);

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(effectModule, EffectModule::BoundRepresentation::BinIndex, sampleRate, binCount);
      return utils::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    i4mask isHighAboveLow = i4::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
    
    f4 attenuation = [&]()
    {
      auto parameter = getParameter(effectData, Destroy::Reinterpret::Attenuation)->getInternalValue<f4>();
      auto mergeMask = merge(kRealMask, kImaginaryMask, f4::greaterThan(parameter, 0.0f));
      return merge(1.0f, dbToAmplitude(parameter | i4mask{ kSignMask }), mergeMask);
    }();

    auto mappingType = getParameter(effectData,
      Destroy::Reinterpret::Transform)->getInternalValue<IndexedData>().first->id;

    auto rawDestination = destination->get();
    auto rawSource = source.sourceBuffer->get();

    auto operations = [mappingType](f4 &one, f4 &two)
    {
      using enum ReinterpretTransformOptions::Value;
      switch (mappingType)
      {
      case NoTransform:
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
      case CopyReal:
        one = copyFromEven(one);
        two = copyFromEven(two);
        break;
      case CopyImag:
        one = copyFromOdd(one);
        two = copyFromOdd(two);
        break;
      default:
        COMPLEX_ASSERT_FALSE("Missing case");
        break;
      }
    };

    f4 wet[2]{};
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

    if (mappingType != ReinterpretTransformOptions::NoTransform)
    {
      wet[0] = rawSource[0] * attenuation;
      wet[1] = rawSource[binCount - 1] * attenuation;
      rawDestination[0] = merge(wet[0], rawSource[0],
        isOutsideBounds(0, lowBoundIndices, highBoundIndices, isHighAboveLow));
      rawDestination[binCount - 1] = merge(wet[1], rawSource[binCount - 1],
        isOutsideBounds(binCount - 1, lowBoundIndices, highBoundIndices, isHighAboveLow));
    }
  }

#undef PASSTHROUGH_PROCESS
}
