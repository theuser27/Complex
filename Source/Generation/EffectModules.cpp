/*
  ==============================================================================

    EffectModules.cpp
    Created: 27 Jul 2021 12:30:19am
    Author:  theuser27

  ==============================================================================
*/

#include "EffectModules.hpp"

#include <algorithm>

#include "Framework/simd_math.hpp"
#include "Framework/parameter_value.hpp"
#include "Plugin/ProcessorTree.hpp"

namespace Generation
{
  template<typename Type>
  void fillAndSetParameters(BaseEffect &effect)
  {
    using namespace Framework;

    static constexpr auto indexedData = []()
    {
      constexpr auto nameIdPairs = Type::template enum_names_and_ids_filter<kGetActiveAlgoPredicate, true>(true);
      std::array<IndexedData, nameIdPairs.size()> data{};
      for (usize i = 0; i < data.size(); i++)
      {
        data[i].id = nameIdPairs[i].second;
        data[i].displayName = nameIdPairs[i].first;
      }
      return data;
    }();

    effect.createProcessorParameters(effect.parameters_);

    auto *parameter = effect.getParameter(Processors::BaseEffect::Algorithm::id().value());
    auto details = parameter->getParameterDetails();
    details.indexedData = indexedData;
    details.maxValue = (float)(indexedData.size() - 1);
    parameter->setParameterDetails(details);
  }

  template<nested_enum::NestedEnum E>
  E getEffectAlgorithm(BaseEffect &effect)
  {
    static constexpr auto algoId = Framework::Processors::BaseEffect::Algorithm::id().value();
    return E::enum_value_by_id(effect.getParameter(algoId)->getInternalValue<std::string_view>().first->id).value();
  }


  void BaseEffect::initialiseParameters()
  {
    if (!parameters_.empty())
      createProcessorParameters(parameters_);
    else
      createProcessorParameters(Framework::Processors::BaseEffect::
        enum_ids_filter<Framework::kGetParameterPredicate, true>());
  }

  template<typename T, usize ... Ns>
  static constexpr auto concatenateArrays(const std::array<T, Ns> ... arrays)
  {
    if constexpr (sizeof...(arrays) == 0)
    {
      auto empty = std::array<T, 0>{};
      return empty;
    }
    else
    {
      constexpr usize N = (Ns + ...);
      std::array<T, N> total;
      usize index = 0;

      auto iterate = [&index, &total](const auto &current)
      {
        for (usize i = 0; i < current.size(); i++)
          total[index + i] = current[i];

        index += current.size();
      };

      (iterate(arrays), ...);

      return total;
    }
  }

#define DEFINE_PARAMETER_INIT(x) 																																\
  x##Effect::x##Effect(Plugin::ProcessorTree *processorTree) : BaseEffect{ processorTree,				\
    Framework::Processors::BaseEffect::x::id().value() }																		    \
  {																																															\
    using namespace Framework;																																	\
    static constexpr auto allParameters = []<typename ... Ts>(																	\
      const std::tuple<std::type_identity<Ts>...> &)																						\
    {																																														\
      return concatenateArrays(																													        \
        Processors::BaseEffect::enum_ids_filter<kGetParameterPredicate, true>(),						    \
        Ts::template enum_ids_filter<kGetParameterPredicate, true>()...);												\
    }(Processors::BaseEffect::x::template enum_subtypes_filter<kGetActiveAlgoPredicate>());	    \
    parameters_ = allParameters;																																\
  }																																															\
  void x##Effect::initialiseParameters()																												\
  { fillAndSetParameters<Framework::Processors::BaseEffect::x::type>(*this); }

  DEFINE_PARAMETER_INIT(Utility)
  DEFINE_PARAMETER_INIT(Filter)
  DEFINE_PARAMETER_INIT(Dynamics)
  DEFINE_PARAMETER_INIT(Phase)
  DEFINE_PARAMETER_INIT(Pitch)
  DEFINE_PARAMETER_INIT(Stretch)
  DEFINE_PARAMETER_INIT(Warp)
  DEFINE_PARAMETER_INIT(Destroy)

#undef DEFINE_PARAMETER_INIT

  ///////////////////////////////////////////////
  ///////////////////////////////////////////////
  ////////////////// Utilities //////////////////
  ///////////////////////////////////////////////
  ///////////////////////////////////////////////

  auto BaseEffect::getShiftedBounds(BoundRepresentation representation,
    float sampleRate, u32 binCount) const noexcept -> std::pair<simd_float, simd_float>
  {
    using namespace utils;
    using namespace Framework;

    u32 FFTSize = (binCount - 1) * 2;
    simd_float lowBound = getParameter(Processors::BaseEffect::LowBound::id().value())->getInternalValue<simd_float>(sampleRate, true);
    simd_float highBound = getParameter(Processors::BaseEffect::HighBound::id().value())->getInternalValue<simd_float>(sampleRate, true);
    float nyquistFreq = sampleRate * 0.5f;
    float maxOctave = (float)std::log2(nyquistFreq / kMinFrequency);

    simd_float boundShift = getParameter(Processors::BaseEffect::ShiftBounds::id().value())
      ->getInternalValue<simd_float>(sampleRate);
    lowBound = simd_float::clamp(lowBound + boundShift, 0.0f, 1.0f);
    highBound = simd_float::clamp(highBound + boundShift, 0.0f, 1.0f);

    switch (representation)
    {
    case BoundRepresentation::Normalised:
      break;
    case BoundRepresentation::Frequency:
      lowBound = exp2(lowBound * maxOctave);
      highBound = exp2(highBound * maxOctave);
      // snapping to 0 hz if it's below the minimum frequency
      lowBound = (lowBound & simd_float::greaterThan(lowBound, 1.0f)) * kMinFrequency;
      highBound = (highBound & simd_float::greaterThan(highBound, 1.0f)) * kMinFrequency;

      break;
    case BoundRepresentation::BinIndex:
      lowBound = normalisedToBin(lowBound, FFTSize, sampleRate);
      highBound = normalisedToBin(highBound, FFTSize, sampleRate);

      break;
    default:
      break;
    }

    return { lowBound, highBound };
  }

  auto vector_call BaseEffect::minimiseRange(simd_int lowIndices, 
    simd_int highIndices, u32 binCount, bool isProcessedRange) -> std::tuple<u32, u32, bool>
  {
    COMPLEX_ASSERT((binCount & (binCount - 2)) == 1, "Bin count is not power-of-2 + 1, but instead %d", binCount);

    // 1. all the indices in the respective bounds are the same (mono)
    // 2. the indices in the respective bounds are different (stereo)

    // 1.
    if (utils::areAllElementsSame(lowIndices) && utils::areAllElementsSame(highIndices))
    {
      auto start = lowIndices[0];
      auto end = highIndices[0];
      auto length = ((binCount + end - start) % binCount);

      // full/empty range if the bins overlap or span the entire range
      if (length == 0 || (start & (binCount - 2)) == (end & (binCount - 2)))
      {
        if (isProcessedRange)
          return { start, binCount, false };
        else
          return { (start + binCount) % binCount, 0, false };
      }
      else
      {
        if (isProcessedRange)
          return { start, length + 1, false };
        else
          return { (start + length + 1) % binCount, binCount - length - 1, false };
      }
    }

    // 2.
    // trying to rationalise what parts to cover is proving too complicated
    return { 0, binCount, true };
  }

  void BaseEffect::copyUnprocessedData(Framework::SimdBufferView<Framework::complex<float>, simd_float> source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, simd_int lowBoundIndices,
    simd_int highBoundIndices, u32 binCount) noexcept
  {
    COMPLEX_ASSERT(destination.getSize() == source.getSize());
    auto [index, unprocessedCount, isStereo] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, false);

    usize channels = std::min(source.getChannels(), destination.getChannels());
    usize size = destination.getSize();
    auto rawSource = source.get();
    auto rawDestination = destination.get();

    // this is not possible, so we'll take it to mean that we have stereo bounds
    if (isStereo)
    {
      simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
      for (usize i = 0; i < channels; i += source.getRelativeSize())
      {
        index = 0;
        for (; index < unprocessedCount; ++index)
          rawDestination[i * size + index] = utils::merge(rawDestination[i * size + index], rawSource[i * size + index],
            isOutsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow));
      }
    }
    // mono bounds
    else
    {
      for (usize i = 0; i < channels; i += source.getRelativeSize())
      {
        for (usize j = 0; j < unprocessedCount - 1; j++)
        {
          auto currentIndex = (index + j) & (binCount - 2);
          rawDestination[i * size + currentIndex] = rawSource[i * size + currentIndex];
        }

        if (index + unprocessedCount > binCount - 1)
          rawDestination[i * size + binCount - 1] = rawSource[i * size + binCount - 1];
        else
          rawDestination[i * size + index + unprocessedCount] = rawSource[i * size + index + unprocessedCount];

      }
    }
  }

  strict_inline simd_float vector_call FilterEffect::getDistancesFromCutoffs(simd_int positionIndices,
    simd_int cutoffIndices, simd_int lowBoundIndices, u32 FFTSize, float sampleRate)
  {
    // 1. both positionIndices and cutoffIndices are >= lowBound and < FFTSize_ or <= highBound and > 0
    // 2. cutoffIndices/positionIndices is >= lowBound and < FFTSize_ and
    //     positionIndices/cutoffIndices is <= highBound and > 0

    using namespace utils;

    simd_mask cutoffAbovePositions = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

    // preparing masks for 1.
    simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
    simd_mask cutoffAboveLowMask    = simd_mask::greaterThanOrEqualSigned(cutoffIndices,   lowBoundIndices);
    simd_mask bothAboveOrBelowLowMask = ~(positionsAboveLowMask ^ cutoffAboveLowMask);

    // preparing masks for 2.
    simd_mask positionsBelowLowBoundAndCutoffsMask = ~positionsAboveLowMask & cutoffAboveLowMask;
    simd_mask cutoffBelowLowBoundAndPositionsMask = positionsAboveLowMask & ~cutoffAboveLowMask;

    // masking for 1.
    simd_int precedingIndices  = merge(cutoffIndices, positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
    simd_int succeedingIndices = merge(positionIndices, cutoffIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);

    // masking for 2.
    // first 2 are when cutoffs/positions are above/below lowBound
    // second 2 are when positions/cutoffs are above/below lowBound
    precedingIndices  = merge(precedingIndices,  cutoffIndices,   ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
    succeedingIndices = merge(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
    precedingIndices  = merge(precedingIndices,  positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );
    succeedingIndices = merge(succeedingIndices, cutoffIndices,   ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask );

    simd_float precedingIndicesRatios = binToNormalised(toFloat(precedingIndices), FFTSize, sampleRate);
    simd_float succeedingIndicesRatios = binToNormalised(toFloat(succeedingIndices), FFTSize, sampleRate);

    // all i have to say is, floating point error
    simd_mask notEqual = simd_int::notEqual(precedingIndices, succeedingIndices);
    return getDecimalPlaces(simd_float{ 1.0f } + ((succeedingIndicesRatios - precedingIndicesRatios) & notEqual));
  }

  simd_float vector_call DynamicsEffect::matchPower(simd_float target, simd_float current) noexcept
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

  void FilterEffect::runNormal(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
    u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    using BaseParameters = Processors::BaseEffect::type;
    using Parameters = Processors::BaseEffect::Filter::Normal::type;

    simd_float lowBoundNorm = getParameter(BaseParameters::LowBound::id().value())->getInternalValue<simd_float>(sampleRate, true);
    simd_float highBoundNorm = getParameter(BaseParameters::HighBound::id().value())->getInternalValue<simd_float>(sampleRate, true);
    simd_float boundShift = getParameter(BaseParameters::ShiftBounds::id().value())->getInternalValue<simd_float>(sampleRate);
    simd_float boundsDistance = modOnce(simd_float{ 1.0f } + highBoundNorm - lowBoundNorm, 1.0f);

    u32 FFTSize = (binCount - 1) * 2;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto [low, high] = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(low), toInt(high) };
    }();

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // cutoff is described as exponential normalised value of the sample rate
    // it is dependent on the values of the low/high bounds
    simd_float cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance * 
      getParameter(Parameters::Cutoff::id().value())->getInternalValue<simd_float>(sampleRate, true), 1.0f, false);
    simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, FFTSize, sampleRate));

    // if mask scalars are negative/positive -> brickwall/linear slope
    // slopes are logarithmic
    simd_float slopes = getParameter(Parameters::Slope::id().value())->getInternalValue<simd_float>(sampleRate) / 2.0f;
    simd_mask slopeMask = unsignSimd<true>(slopes);
    simd_mask slopeZeroMask = simd_float::equal(slopes, 0.0f);

    // if scalars are negative/positive, attenuate at/around cutoff
    // (gains is gain reduction in db and NOT a gain multiplier)
    simd_float gainsParameter = getParameter(Parameters::Gain::id().value())->getInternalValue<simd_float>(sampleRate);
    simd_mask gainType = unsignSimd<true>(gainsParameter);
    simd_mask gainMask = (source.dataType == ComplexDataSource::Cartesian) ? simd_mask{ kFullMask } : kMagnitudeMask;

    // copy both un/processed data
    SimdBuffer<complex<float>, simd_float>::applyToThisNoMask<MathOperations::Assign>(
      destination, source.sourceBuffer, destination.getChannels(), binCount, 0, 0, 0, 0);

    auto rawDestination = destination.get();
    circularLoop([&](u32 index)
      {
        // the distances are logarithmic
        // TODO: memoise cutoff distances and update only when low/high bound indices and binCount are changed
        simd_float distancesFromCutoff = getDistancesFromCutoffs(index,
          cutoffIndices, lowBoundIndices, FFTSize, sampleRate);

        // calculating linear slope and brickwall, both are ratio of the gain attenuation
        // the higher tha value the more it will be affected by it
        simd_float gainRatio = merge(
          simd_float::clamp(merge(distancesFromCutoff / slopes, simd_float{ 1.0f }, slopeZeroMask), 0.0f, 1.0f),
          simd_float{ 1.0f } & simd_float::greaterThanOrEqual(distancesFromCutoff, slopes),
          ~slopeMask);
        simd_float gains = merge(gainsParameter * gainRatio, gainsParameter * (simd_float{ 1.0f } - gainRatio), gainType);

        // convert db reduction to amplitude multiplier
        gains = dbToAmplitude(-gains & gainMask);

        rawDestination[index] = merge(rawDestination[index] * gains, rawDestination[index], 
          isOutsideBounds(start, lowBoundIndices, highBoundIndices));
      }, start, processedCount, binCount);
  }

  void FilterEffect::runPhase(Framework::ComplexDataSource &,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto [low, high] = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(low), toInt(high) };
    }();

    // minimising the bins to iterate on
    auto [index, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // if scalars are negative/positive, attenuate phases in/outside the range
    // (gains is gain reduction in db and NOT a gain multiplier)
    simd_float gains = getParameter(Processors::BaseEffect::Filter::Phase::Gain::id().value())->getInternalValue<simd_float>(sampleRate);
    simd_mask gainMask = unsignSimd<true>(gains);

    simd_float lowPhaseBound = getParameter(Processors::BaseEffect::Filter::Phase::LowPhaseBound::id().value())
      ->getInternalValue<simd_float>(sampleRate);
    simd_float highPhaseBound = getParameter(Processors::BaseEffect::Filter::Phase::HighPhaseBound::id().value())
      ->getInternalValue<simd_float>(sampleRate);

    for (u32 i = 0; i < processedCount; i += 2)
    {
      //u32 oneIndex = (index + i) & (binCount - 1);
      //u32 twoIndex = (index + i + 1) & (binCount - 1);
      //simd_float one = source.sourceBuffer.readSimdValueAt(0, oneIndex);
      //simd_float two = source.sourceBuffer.readSimdValueAt(0, twoIndex);

      //simd_float phases = complexPhase({ one, two });

    }
  }

  void FilterEffect::run(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
    u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;
    switch (getEffectAlgorithm<Processors::BaseEffect::Filter::type>(*this))
    {
    case Processors::BaseEffect::Filter::Normal:
      runNormal(source, destination, binCount, sampleRate);

      break;
    case Processors::BaseEffect::Filter::Regular:
    default:
      break;
    }
  }

  void DynamicsEffect::runContrast(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    using Parameters = Processors::BaseEffect::Dynamics::Contrast::type;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    // calculating contrast
    simd_float depthParameter = getParameter(Parameters::Depth::id().value())
      ->getInternalValue<simd_float>(sampleRate);
    simd_float contrast = depthParameter * depthParameter;
    contrast = merge(simd_float(kContrastMaxNegativeValue) * contrast, 
      simd_float(kContrastMaxPositiveValue) * contrast, simd_float::greaterThanOrEqual(depthParameter, 0.0f));

    simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0f + 1.0f));
    simd_float max = exp(simd_float(80.0f) / (contrast * 2.0f + 1.0f));
    min = merge(simd_float(1e-30f), min, simd_float::greaterThan(contrast, 0.0f));
    max = merge(simd_float(1e30f), max, simd_float::greaterThan(contrast, 0.0f));

    // copy both un/processed data
    SimdBuffer<complex<float>, simd_float>::applyToThisNoMask<MathOperations::Assign>(
      destination, source.sourceBuffer, destination.getChannels(), binCount, 0, 0, 0, 0);

    auto rawDestination = destination.get();
    simd_float inPower = 0.0f;
    if (source.dataType == ComplexDataSource::Cartesian)
      circularLoop([&](u32 index)
        {
          inPower += complexMagnitude(rawDestination[index], false) & 
            isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
        }, start, processedCount, binCount);
    else
    {
      circularLoop([&](u32 index) 
        {
          simd_float bin = rawDestination[index] & isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
          inPower = simd_float::mulAdd(inPower, bin, bin);
        }, start, processedCount, binCount);
    }
    
    simd_int boundDistanceCount = (modOnce(simd_int{ binCount } + highBoundIndices - lowBoundIndices, simd_int{ binCount }) + 1)
      & simd_int::notEqual(lowBoundIndices, highBoundIndices);
    simd_float inScale = matchPower(toFloat(boundDistanceCount), inPower);

    // applying gain and calculating outPower
    simd_float outPower = 0.0f;
    if (source.dataType == ComplexDataSource::Cartesian)
    {
      circularLoop([&](u32 index)
        {
          simd_float bin = inScale * rawDestination[index];
          simd_float magnitude = complexMagnitude(bin, true);

          bin &= simd_float::lessThanOrEqual(min, magnitude);
          bin = merge(bin, bin * pow(magnitude, contrast), simd_float::greaterThan(max, magnitude));

          outPower += complexMagnitude(bin, false);
          rawDestination[index] = bin;
        }, start, processedCount, binCount);
    }
    else
    {
      circularLoop([&](u32 index)
        {
          simd_float bin = inScale * rawDestination[index];

          bin &= simd_float::lessThanOrEqual(min, bin);
          bin = merge(bin, bin * pow(bin, contrast), simd_float::greaterThan(max, bin));

          outPower += bin * bin;
          rawDestination[index] = merge(bin, rawDestination[index], kPhaseMask);
        }, start, processedCount, binCount);
    }

    // normalising 
    simd_float outScale = matchPower(inPower, outPower);
    if (source.dataType == ComplexDataSource::Polar) 
      outScale = merge(outScale, simd_float{ 1.0f }, kPhaseMask);
    circularLoop([&](u32 index) { rawDestination[index] *= outScale; }, start, processedCount, binCount);
  }

  void DynamicsEffect::runClip(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;
    using Parameters = Processors::BaseEffect::Dynamics::Clip::type;

    static constexpr float kSilenceThreshold = 1e-30f;
    static constexpr float kLoudestThreshold = 1e30f;

    // getting the boundaries in terms of bin position
    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    COMPLEX_ASSERT(source.sourceBuffer.getSize() == destination.getSize());

    auto rawSource = source.sourceBuffer.get();
    auto rawDestination = destination.get();

    // getting the min/max power in the range selected
    std::pair<simd_float, simd_float> powerMinMax{ kLoudestThreshold, kSilenceThreshold };
    for (u32 j = 0; j < binCount; j++)
    {
      // while calculating the power min-max we can copy over data
      rawDestination[j] = rawSource[j];

      simd_float magnitude = simd_float::max(kSilenceThreshold, complexMagnitude(rawDestination[j], false));
      simd_mask isIndexOutside = isOutsideBounds(j, lowBoundIndices, highBoundIndices, isHighAboveLow);
      powerMinMax.first  = merge(simd_float::min(powerMinMax.first , magnitude), powerMinMax.first , isIndexOutside);
      powerMinMax.second = merge(simd_float::max(powerMinMax.second, magnitude), powerMinMax.second, isIndexOutside);
    }

    // calculating clipping
    simd_float thresholdParameter = getParameter(Parameters::Threshold::id().value())
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
        simd_mask isIndexInside = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
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
          isOutsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow));
      }, start, processedCount, binCount);
  }

  void DynamicsEffect::run(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;

    switch (getEffectAlgorithm<Processors::BaseEffect::Dynamics::type>(*this))
    {
    case Processors::BaseEffect::Dynamics::Contrast:
      // based on dtblkfx contrast
      runContrast(source, destination, binCount, sampleRate);
      break;
    case Processors::BaseEffect::Dynamics::Clip:
      // based on dtblkfx clip
      runClip(source, destination, binCount, sampleRate);
      break;
    default:
      break;
    }
  }

  void PhaseEffect::runShift(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    using Parameters = Processors::BaseEffect::Phase::Shift::type;
    static constexpr simd_mask kPhaseLeftMask = utils::array{ 0U, kFullMask, 0U, 0U };
    static constexpr simd_mask kPhaseRightMask = utils::array{ 0U, 0U, 0U, kFullMask };

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_float shift = (getParameter(Parameters::PhaseShift::id().value())
      ->getInternalValue<simd_float>(sampleRate, true) * 2.0f - 1.0f) * kPi;
    simd_float interval = getParameter(Parameters::Interval::id().value())->getInternalValue<simd_float>(sampleRate);
    
    u32 shiftSlope = getParameter(Parameters::Slope::id().value())->getInternalValue<u32>(sampleRate);
    simd_float (*slopeFunction)(simd_float);
    if (shiftSlope == 0)
      slopeFunction = [](simd_float x) { return x; };
    else
      slopeFunction = [](simd_float x) { return modOnceSymmetric(x + x, kPi); };

    // overwrite old data
    SimdBuffer<complex<float>, simd_float>::applyToThisNoMask<MathOperations::Assign>(
      destination, source.sourceBuffer, destination.getChannels(), binCount);

    auto rawDestination = destination.get();

    // if interval between bins is 0 this means every bin is affected
    if (completelyEqual(interval, 0.0f))
    {
      simd_int offsetBin = toInt(normalisedToBin(getParameter(Parameters::Offset::id().value())
        ->getInternalValue<simd_float>(sampleRate, true), 2 * (binCount - 1), sampleRate));

      // find the smallest offset forward and start from there
      u32 minOffset = horizontalMin(offsetBin)[0];
      i32 indexChange = clamp((i32)minOffset - (i32)start, 0, (i32)processedCount);
      processedCount -= indexChange;
      start += indexChange;

      circularLoop([&](u32 index)
        {
          simd_mask offsetMask = simd_int::greaterThanOrEqualSigned(index, offsetBin);
          simd_mask insideRangeMask = isInsideBounds(index, lowBoundIndices, highBoundIndices, isHighAboveLow);
          rawDestination[index] = merge(rawDestination[index], 
            modOnceSymmetric(rawDestination[index] + shift, kPi),
            kPhaseMask & offsetMask & insideRangeMask);

          shift = slopeFunction(shift);
        }, start, processedCount, binCount);

      return;
    }

    // otherwise the interval specifies how many octaves up the next affected bin is
    
    // offset is skewed towards an exp-like curve so we need to normalise it
    simd_float offsetNorm = getParameter(Parameters::Offset::id().value())
      ->getInternalValue<simd_float>(sampleRate) * 2.0f / sampleRate;
    simd_float binStep = 1.0f / (float)(binCount - 1);
    simd_float logBase = log2(interval + 1.0f);
    COMPLEX_ASSERT(simd_float::lessThanOrEqual(logBase, 0.0f).anyMask() == 0);

    // if offset is 0 we need to give it a starting value based on interval
    // and shift dc component's amplitude
    {
      simd_mask zeroMask = simd_float::lessThan(offsetNorm, binStep);
      simd_float modifiedShift = (-simd_float::abs(shift / kPi) + 0.5f) * 2.0f;
      rawDestination[0] = (rawDestination[0] * modifiedShift) & kMagnitudeMask;

      shift = merge(shift, slopeFunction(shift), zeroMask);

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
        modOnceSymmetric(values + shift, kPi), indexMask & kPhaseMask);

      shift = merge(shift, slopeFunction(shift), indexMask);

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

  void PhaseEffect::run(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;
    switch (getEffectAlgorithm<Processors::BaseEffect::Phase::type>(*this))
    {
    case Processors::BaseEffect::Phase::Shift:
      runShift(source, destination, binCount, sampleRate);
      break;
    default:
      BaseEffect::run(source, destination, binCount, sampleRate);
      break;
    }
  }

  void PitchEffect::runResample(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();

    // minimising the bins to iterate on
    auto [start, numBins, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_float shift = exp2(getParameter(Processors::BaseEffect::Pitch::Resample::Shift::id().value())
      ->getInternalValue<simd_float>(sampleRate) / (float)kNotesPerOctave);

    simd_int processedIndex = toInt(shift * (float)start);

    copyUnprocessedData(source.sourceBuffer, destination, lowBoundIndices, highBoundIndices, binCount);
  }

  void PitchEffect::runConstShift(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
  {
    using namespace utils;
    using namespace Framework;

    static constexpr auto kNeighbourBins = 2;

    auto [lowBoundIndices, highBoundIndices] = [&]()
    {
      auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount);
      return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
    }();
    simd_mask isHighAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);

    // minimising the bins to iterate on
    auto [start, processedCount, _] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

    simd_int binShift;
    simd_float phaseShift;
    std::array<simd_float, 2 * kNeighbourBins + 1> neighbourMultipliers;
    {
      simd_float binFloatingPointShift = getParameter(Processors::BaseEffect::Pitch::ConstShift::Shift::id().value())
        ->getInternalValue<simd_float>(sampleRate) * 2.0f * (float)(binCount - 1) / sampleRate;
      simd_float roundedShift = simd_float::round(binFloatingPointShift);
      binShift = toInt(roundedShift);

      for (usize i = 0; i < kChannelsPerInOut; ++i)
      {
        phaseShift.set(i * 2, std::cosf(binFloatingPointShift[i * 2] * (k2Pi * source.normalisedPhase)));
        phaseShift.set(i * 2 + 1, std::sinf(binFloatingPointShift[i * 2] * (k2Pi * source.normalisedPhase)));
      }

      simd_float binFractionalShift = roundedShift - binFloatingPointShift;
      simd_float cosMultiplier, sinMultiplier;
      for (usize i = 0; i < simd_float::size; i += 2)
      {
        auto cos = std::cosf(binFractionalShift[i] * k2Pi);
        cosMultiplier.set(i, cos);
        cosMultiplier.set(i + 1, cos);
        auto sin = std::sinf(binFractionalShift[i] * k2Pi);
        sinMultiplier.set(i, sin);
        sinMultiplier.set(i + 1, sin);
      }

      simd_float numerator = merge(sinMultiplier, simd_float{ 1.0f } - cosMultiplier, kPhaseMask);
      simd_mask numZeroMask = simd_float::lessThan(simd_float::abs(numerator), kEpsilon);
      for (i32 i = 0; i < neighbourMultipliers.size(); ++i)
      {
        simd_float denominator = (binFractionalShift - kNeighbourBins + (float)i) * k2Pi;
        simd_mask denZeroMask = simd_float::lessThan(simd_float::abs(denominator), kEpsilon);
        denominator = merge(denominator, simd_float{ 1.0f }, denZeroMask);
        neighbourMultipliers[i] = merge(numerator / denominator, simd_float{ 1.0f }, numZeroMask & denZeroMask);
      }
    }

    destination.clear();
    auto rawDestination = destination.get();
    auto rawSource = source.sourceBuffer.get();

    for (u32 i = 0; i < binCount; ++i)
    {
      simd_mask outsideBoundsMask = isOutsideBounds(i, lowBoundIndices, highBoundIndices, isHighAboveLow);
      rawDestination[i] += rawSource[i] & outsideBoundsMask;
      simd_float wet = complexCartMul(rawSource[i], phaseShift) & ~outsideBoundsMask;

      for (i32 j = 0; j < neighbourMultipliers.size(); ++j)
      {
        simd_int indices = simd_int{ (u32)((i32)i - kNeighbourBins + j) } + binShift;
        simd_int clampedIndices = simd_int::clampSigned(0, binCount - 1, indices);
        simd_mask inRangeMask = simd_int::equal(indices, clampedIndices);

        for (usize k = 0; k < kChannelMasks.size(); ++k)
        {
          u32 channelIndex = clampedIndices[k * 2];
          rawDestination[channelIndex] = merge(rawDestination[channelIndex],
            simd_float::mulAdd(rawDestination[channelIndex], wet, neighbourMultipliers[j]),
            inRangeMask & kChannelMasks[k]);
        }
      }
    }
  }

  void PitchEffect::run(Framework::ComplexDataSource &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;
    switch (getEffectAlgorithm<Processors::BaseEffect::Pitch::type>(*this))
    {
    case Processors::BaseEffect::Pitch::Resample:
      runResample(source, destination, binCount, sampleRate);
      break;
    case Processors::BaseEffect::Pitch::ConstShift:
      runConstShift(source, destination, binCount, sampleRate);
      break;
    default:
      BaseEffect::run(source, destination, binCount, sampleRate);
      break;
    }
  }

  static constexpr auto effectIds = Framework::Processors::BaseEffect::enum_ids_filter<Framework::kGetActiveEffectPredicate, true>();

  EffectModule::EffectModule(Plugin::ProcessorTree *processorTree) noexcept :
    BaseProcessor{ processorTree, Framework::Processors::EffectModule::id().value() }
  {
    using namespace Framework;

    auto maxBinCount = processorTree->getMaxBinCount();
    dataBuffer_.reserve(kChannelsPerInOut, maxBinCount);
    buffer_.reserve(kChannelsPerInOut, maxBinCount);
  }

  EffectModule::~EffectModule()
  {
    for (auto *effect : effects_)
      if (effect && (BaseProcessor *)effect != getSubProcessor(0))
        getProcessorTree()->deleteProcessor(effect->getProcessorId());
  }

  void EffectModule::insertSubProcessor(usize, BaseProcessor &newSubProcessor) noexcept
  {
    COMPLEX_ASSERT(subProcessors_.size() == 0);
    usize effectIndex = (usize)(std::ranges::find(effectIds, newSubProcessor.getProcessorType()) - effectIds.begin());
    COMPLEX_ASSERT(effectIndex < effectIds.size() && "Unknown effect being inserted into EffectModule");

    newSubProcessor.setParentProcessorId(processorId_);
    subProcessors_.emplace_back(&newSubProcessor);
    effects_[effectIndex] = utils::as<BaseEffect>(&newSubProcessor);

    auto *parameter = getParameter(Framework::Processors::EffectModule::ModuleType::id().value());
    parameter->updateNormalisedValue((float)unscaleValue((double)effectIndex, parameter->getParameterDetails()));
    parameter->updateValue(kDefaultSampleRate);
  }

  BaseProcessor &EffectModule::updateSubProcessor(usize index, BaseProcessor &newSubProcessor) noexcept
  {
    COMPLEX_ASSERT(std::ranges::find(effectIds, newSubProcessor.getProcessorType()) != effectIds.end()
      && "Unknown effect being inserted into EffectModule");

    auto *replacedEffect = subProcessors_[0];
    newSubProcessor.setParentProcessorId(processorId_);
    subProcessors_[0] = &newSubProcessor;

    for (auto *listener : listeners_)
      listener->updatedSubProcessor(index, *replacedEffect, newSubProcessor);

    return *replacedEffect;
  }

  BaseEffect *EffectModule::changeEffect(std::string_view effectType)
  {
    if (subProcessors_[0]->getProcessorType() == effectType)
      return utils::as<BaseEffect>(subProcessors_[0]);
    
    usize newEffectIndex = std::ranges::find(effectIds, effectType) - effectIds.begin();
    BaseEffect *effect;
    if (effects_[newEffectIndex])
      effect = effects_[newEffectIndex];
    else
    {
      [&]<typename ... Ts>(const std::tuple<std::type_identity<Ts>...> &)
      {
        std::ignore = ((Ts::id().value() == effectType && (effect = new typename Ts::linked_type{ processorTree_ }, true)) || ...);
      }(Framework::Processors::BaseEffect::enum_subtypes_filter<Framework::kGetActiveEffectPredicate>());

      COMPLEX_ASSERT(effect && "Unknown effect type was provided");

      effect->initialiseParameters();
      effects_[newEffectIndex] = effect;
    }

    getProcessorTree()->executeOutsideProcessing([&]()
      {
        updateSubProcessor(0, *effect);

        auto *parameter = getParameter(Framework::Processors::EffectModule::ModuleType::id().value());
        parameter->updateNormalisedValue((float)unscaleValue((double)newEffectIndex, parameter->getParameterDetails()));
        parameter->updateValue(kDefaultSampleRate);
      });
    
    return effect;
  }

  void EffectModule::processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept
  {
    using namespace Framework;
    using namespace utils;

    if (!getParameter(Processors::EffectModule::ModuleEnabled::id().value())->getInternalValue<u32>(sampleRate))
      return;

    auto *effect = as<BaseEffect>(subProcessors_[0]);

    if (auto neededType = effect->neededDataType(); neededType != ComplexDataSource::Both && source.dataType != neededType)
    {
      if (neededType == ComplexDataSource::Polar)
      {
        convertBuffer<complexCartToPolar>(source.sourceBuffer, source.conversionBuffer, binCount);
        source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);
        source.dataType = ComplexDataSource::Polar;
      }
      else
      {
        convertBuffer<complexPolarToCart>(source.sourceBuffer, source.conversionBuffer, binCount);
        source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);
        source.dataType = ComplexDataSource::Cartesian;
      }
      
      source.sourceBuffer = source.conversionBuffer;
    }

    // getting exclusive access to data
    lockAtomic(dataBuffer_.getLock(), true, WaitMechanism::Spin);

    effect->run(source, dataBuffer_, binCount, sampleRate);

    // if the mix is 100% for all channels, we can skip mixing entirely
    simd_float wetMix = getParameter(Processors::EffectModule::ModuleMix::id().value())->getInternalValue<simd_float>(sampleRate);
    if (!completelyEqual(wetMix, 1.0f))
    {
      auto sourceData = source.sourceBuffer.get();
      auto destinationData = dataBuffer_.get();
      simd_float dryMix = simd_float(1.0f) - wetMix;
      for (u32 i = 0; i < binCount; i++)
        destinationData[i] = simd_float::mulAdd(dryMix * sourceData[i], wetMix, destinationData[i]);
    }

    // switching to being a reader and allowing other readers to participate
    // seq_cst because the following atomic could be reordered prior to this one
    dataBuffer_.getLock().lock.store(1, std::memory_order_seq_cst);
    if (source.sourceBuffer != source.conversionBuffer)
      source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);

    source.sourceBuffer = dataBuffer_;
  }
}