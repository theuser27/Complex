/*
	==============================================================================

		EffectModules.cpp
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#include "EffectModules.h"

#include <algorithm>
#include "Framework/spectral_support_functions.h"
#include "Framework/parameter_value.h"

namespace Generation
{
	template<typename Type>
	void fillAndSetParameters(baseEffect &effect)
	{
		using namespace Framework;

		auto *parameter = effect.getParameter(BaseProcessors::BaseEffect::Algorithm::name());
		auto details = parameter->getParameterDetails();
		details.stringLookup = Parameters::getEffectModesStrings(Type::value());
		parameter->setParameterDetails(details);

		nested_enum::recurse_over_types<[]<auto Self, typename ... Ts>(baseEffect &effect)
			{
				(effect.createProcessorParameters(Ts::template enum_names<nested_enum::OuterNodes>()), ...);
			}>(Type::template enum_subtypes<nested_enum::InnerNodes>(), effect);
	}

	template<nested_enum::NestedEnum E>
	E getEffectAlgorithm(baseEffect &effect)
	{ return E::make_enum(effect.getParameter(Framework::BaseProcessors::BaseEffect::Algorithm::name())->getInternalValue<u32>()).value(); }

	///////////////////////////////////////////////
	///////////////////////////////////////////////
	//////////////// Constructors /////////////////
	///////////////////////////////////////////////
	///////////////////////////////////////////////

	baseEffect::baseEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId, std::string_view effectType) :
		BaseProcessor(processorTree, parentModuleId, effectType)
	{
		createProcessorParameters(Framework::BaseProcessors::BaseEffect::enum_names<nested_enum::OuterNodes>());
	}

	utilityEffect::utilityEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Utility::id().value())
	{

	}

	filterEffect::filterEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Filter::id().value())
	{
		fillAndSetParameters<Framework::BaseProcessors::BaseEffect::Filter::type>(*this);
	}

	dynamicsEffect::dynamicsEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Dynamics::id().value())
	{
		fillAndSetParameters<Framework::BaseProcessors::BaseEffect::Dynamics::type>(*this);
	}

	phaseEffect::phaseEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Phase::id().value())
	{
		fillAndSetParameters<Framework::BaseProcessors::BaseEffect::Phase::type>(*this);
	}

	pitchEffect::pitchEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Pitch::id().value())
	{

	}

	stretchEffect::stretchEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Stretch::id().value())
	{

	}

	warpEffect::warpEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Warp::id().value())
	{

	}

	destroyEffect::destroyEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId) :
		baseEffect(processorTree, parentModuleId, Framework::BaseProcessors::BaseEffect::Destroy::id().value())
	{

	}

	///////////////////////////////////////////////
	///////////////////////////////////////////////
	////////////////// Utilities //////////////////
	///////////////////////////////////////////////
	///////////////////////////////////////////////

	std::pair<simd_float, simd_float> baseEffect::getShiftedBounds(BoundRepresentation representation,
	                                                               float sampleRate, u32 FFTSize, bool isLinearShift) const noexcept
	{
		using namespace utils;
		using namespace Framework;
		simd_float lowBound = getParameter(BaseProcessors::BaseEffect::LowBound::name())->getInternalValue<simd_float>(sampleRate, true);
		simd_float highBound = getParameter(BaseProcessors::BaseEffect::HighBound::name())->getInternalValue<simd_float>(sampleRate, true);
		// TODO: do dynamic min frequency based on FFTOrder
		// TODO: get shifted bounds as normalised value/frequency/FFT bin
		float nyquistFreq = sampleRate * 0.5f;
		float maxOctave = (float)std::log2(nyquistFreq / kMinFrequency);

		if (isLinearShift)
		{
			// TODO: linear shift
			simd_float boundShift = getParameter(BaseProcessors::BaseEffect::ShiftBounds::name())
				->getInternalValue<simd_float>(sampleRate) * nyquistFreq;
			lowBound = simd_float::clamp(exp2(lowBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, nyquistFreq);
			highBound = simd_float::clamp(exp2(highBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, nyquistFreq);
			// snapping to 0 hz if it's below the minimum frequency
			lowBound &= simd_float::greaterThan(lowBound, kMinFrequency);
			highBound &= simd_float::greaterThan(highBound, kMinFrequency);
		}
		else
		{
			simd_float boundShift = getParameter(BaseProcessors::BaseEffect::ShiftBounds::name())
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
		}
		return { lowBound, highBound };
	}

	std::pair<u32, u32> vector_call baseEffect::minimiseRange(simd_int lowIndices, 
		simd_int highIndices, u32 binCount, bool isProcessedRange)
	{
		// 1. all the indices in the respective bounds are the same (mono)
		// 2. the indices in the respective bounds are different (stereo)

		// 1.
		if (utils::areAllElementsSame(lowIndices) && utils::areAllElementsSame(highIndices))
		{
			u32 start, end;
			if (isProcessedRange)
			{
				start = lowIndices[0];
				end = highIndices[0];
				if ((((start + 1) & (binCount - 1)) == end) ||
					 (((end + 1) & (binCount - 1)) == start) ||
					 (start == end))
					return { start, binCount };
				else
					return { start, ((binCount + end - start) & (binCount - 1)) + 1 };
			}
			else
			{
				start = highIndices[0];
				end = lowIndices[0];
				if ((((start + 1) & (binCount - 1)) == end) || (((end + 1) & (binCount - 1)) == start))
					return { start, 0 };
				else
					return { (start + 1) & (binCount - 1), (binCount + end - start) & (binCount - 1) };
			}
		}

		// 2.
		// trying to rationalise what parts to cover is proving too complicated
		return { 0, binCount };
	}

	void baseEffect::copyUnprocessedData(Framework::SimdBufferView<Framework::complex<float>, simd_float> source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, simd_int lowBoundIndices,
		simd_int highBoundIndices, u32 binCount) noexcept
	{
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, false);

		u32 channels = std::min(source.getChannels(), destination.getChannels());

		// this is not possible, so we'll take it to mean that we have stereo bounds
		if (index == 0 && numBins == binCount)
		{
			for (u32 i = 0; i < channels; i += source.getRelativeSize())
			{
				index = 0;
				for (; index < numBins; ++index)
				{
					destination.writeSimdValueAt(utils::maskLoad(destination.readSimdValueAt(i, index), source.readSimdValueAt(i, index),
						isOutsideBounds(index, lowBoundIndices, highBoundIndices)), i, index);
				}
			}
		}
		// mono bounds
		else
		{
			for (u32 i = 0; i < channels; i += source.getRelativeSize())
			{
				for (u32 j = 0; j < numBins; j++)
				{
					destination.writeSimdValueAt(source.readSimdValueAt(i, index), i, index);
					index = (index + 1) & (binCount - 1);
				}
			}
		}
	}

	strict_inline simd_float vector_call filterEffect::getDistancesFromCutoffs(simd_int positionIndices,
		simd_int cutoffIndices, simd_int lowBoundIndices, u32 FFTSize, float sampleRate)
	{
		// 1. both positionIndices and cutoffIndices are >= lowBound and < FFTSize_ or <= highBound and > 0
		// 2. cutoffIndices/positionIndices is >= lowBound and < FFTSize_ and
		//     positionIndices/cutoffIndices is <= highBound and > 0

		using namespace utils;

		simd_mask cutoffAbovePositions = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

		// preparing masks for 1.
		simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
		simd_mask cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundIndices);
		simd_mask bothAboveOrBelowLowMask = ~(positionsAboveLowMask ^ cutoffAboveLowMask);

		// preparing masks for 2.
		simd_mask positionsBelowLowBoundAndCutoffsMask = ~positionsAboveLowMask & cutoffAboveLowMask;
		simd_mask cutoffBelowLowBoundAndPositionsMask = positionsAboveLowMask & ~cutoffAboveLowMask;

		// masking for 1.
		simd_int precedingIndices = maskLoad(cutoffIndices, positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
		simd_int succeedingIndices = maskLoad(positionIndices, cutoffIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);

		// masking for 2.
		// first 2 are when cutoffs/positions are above/below lowBound
		// second 2 are when positions/cutoffs are above/below lowBound
		precedingIndices = maskLoad(precedingIndices, cutoffIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
		succeedingIndices = maskLoad(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
		precedingIndices = maskLoad(precedingIndices, positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);
		succeedingIndices = maskLoad(succeedingIndices, cutoffIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);

		simd_float precedingIndicesRatios = binToNormalised(toFloat(precedingIndices), FFTSize, sampleRate);
		simd_float succeedingIndicesRatios = binToNormalised(toFloat(succeedingIndices), FFTSize, sampleRate);

		return getDecimalPlaces(simd_float(1.0f) + succeedingIndicesRatios - precedingIndicesRatios);
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
	// 
	// This is with the exception of dc and nyquist bins, which are combined in a single "bin" 
	// because of their lack of imaginary component/phase. 
	// This is done so that the buffers have a size of a power-of-2 for fast index calculation with wrap-around.

	//// Guidelines
	// 
	// 1. When dealing with dc or nyquist it's best to have a small section after your main algorithm to process them separately.
	//    Other attempts at dealing with them are either impossible, very time consuming and/or of questionable efficiency.
	// 2. Whenever in doubt, look at other algorithm implementations for ideas

	perf_inline void filterEffect::runNormal(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		using BaseParameters = BaseProcessors::BaseEffect::type;
		using Parameters = BaseProcessors::BaseEffect::Filter::Normal::type;

		simd_float lowBoundNorm = getParameter(BaseParameters::LowBound::name())->getInternalValue<simd_float>(sampleRate, true);
		simd_float highBoundNorm = getParameter(BaseParameters::HighBound::name())->getInternalValue<simd_float>(sampleRate, true);
		simd_float boundShift = getParameter(BaseParameters::ShiftBounds::name())->getInternalValue<simd_float>(sampleRate);
		simd_float boundsDistance = modOnceUnsigned(simd_float(1.0f) + highBoundNorm - lowBoundNorm, 1.0f);

		// getting the boundaries in terms of bin position
		auto [lowBoundIndices, highBoundIndices] = [&]()
		{
			auto [low, high] = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount * 2);
			return std::pair{ toInt(low), toInt(high) };
		}();

		// minimising the bins to iterate on
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

		// cutoff is described as exponential normalised value of the sample rate
		// it is dependent on the values of the low/high bounds
		simd_float cutoffNorm = modOnceUnsigned(lowBoundNorm + boundShift + boundsDistance * 
			getParameter(Parameters::Cutoff::name())->getInternalValue<simd_float>(sampleRate, true), 1.0f);
		simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, binCount * 2, sampleRate));

		// if mask scalars are negative/positive -> brickwall/linear slope
		// slopes are logarithmic
		simd_float slopes = getParameter(Parameters::Slope::name())->getInternalValue<simd_float>(sampleRate) / 2.0f;
		simd_mask slopeMask = unsignSimd(slopes, true);
		simd_mask slopeZeroMask = simd_float::equal(slopes, 0.0f);

		// if scalars are negative/positive, attenuate at/around cutoff
		// (gains is gain reduction in db and NOT a gain multiplier)
		simd_float gains = getParameter(Parameters::Gain::name())->getInternalValue<simd_float>(sampleRate);
		simd_mask gainMask = unsignSimd(gains, true);

		for (u32 i = 0; i < numBins; i++)
		{
			u32 currentIndex = (index + i) & (binCount - 1);
			// the distances are logarithmic
			simd_float distancesFromCutoff = getDistancesFromCutoffs(simd_int(currentIndex),
				cutoffIndices, lowBoundIndices, binCount * 2, sampleRate);

			// calculating linear slope and brickwall, both are ratio of the gain attenuation
			// the higher tha value the more it will be affected by it
			simd_float gainRatio = maskLoad(simd_float::clamp(maskLoad(distancesFromCutoff, simd_float(1.0f), slopeZeroMask) 
				/ maskLoad(slopes, simd_float(1.0f), slopeZeroMask), 0.0f, 1.0f),
				simd_float(1.0f) & simd_float::greaterThanOrEqual(distancesFromCutoff, slopes),
				~slopeMask);
			simd_float currentGains = maskLoad(gains * gainRatio, gains * (simd_float(1.0f) - gainRatio), gainMask);

			// convert db reduction to amplitude multiplier
			currentGains = dbToAmplitude(-currentGains);

			destination.writeSimdValueAt(source.readSimdValueAt(0, currentIndex) * currentGains, 0, currentIndex);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, binCount);
	}

	void filterEffect::runPhase(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		// getting the boundaries in terms of bin position
		auto [lowBoundIndices, highBoundIndices] = [&]()
		{
			auto [low, high] = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount * 2);
			return std::pair{ toInt(low), toInt(high) };
		}();

		// minimising the bins to iterate on
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

		// if scalars are negative/positive, attenuate phases in/outside the range
		// (gains is gain reduction in db and NOT a gain multiplier)
		simd_float gains = getParameter(BaseProcessors::BaseEffect::Filter::Phase::Gain::name())->getInternalValue<simd_float>(sampleRate);
		simd_mask gainMask = unsignSimd(gains, true);

		simd_float lowPhaseBound = getParameter(BaseProcessors::BaseEffect::Filter::Phase::LowPhaseBound::name())
			->getInternalValue<simd_float>(sampleRate);
		simd_float highPhaseBound = getParameter(BaseProcessors::BaseEffect::Filter::Phase::HighPhaseBound::name())
			->getInternalValue<simd_float>(sampleRate);

		for (u32 i = 0; i < numBins; i += 2)
		{
			u32 oneIndex = (index + i) & (binCount - 1);
			u32 twoIndex = (index + i + 1) & (binCount - 1);
			simd_float one = source.readSimdValueAt(0, oneIndex);
			simd_float two = source.readSimdValueAt(0, twoIndex);

			simd_float phases = complexPhase({ one, two });

		}
	}

	void filterEffect::run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
	{
		using namespace Framework;
		switch (getEffectAlgorithm<BaseProcessors::BaseEffect::Filter::type>(*this))
		{
		case BaseProcessors::BaseEffect::Filter::Normal:
			runNormal(source, destination, binCount, sampleRate);
			break;
		case BaseProcessors::BaseEffect::Filter::Regular:
		default:
			break;
		}
	}

	simd_float vector_call dynamicsEffect::matchPower(simd_float target, simd_float current) noexcept
	{
		simd_float result = 1.0f;
		result = result & simd_float::lessThan(0.0f, target);
		result = utils::maskLoad(result, simd_float::sqrt(target / current), simd_float::greaterThan(current, 0.0f));

		result = utils::maskLoad(result, simd_float(1.0f), simd_float::greaterThan(result, 1e30f));
		result = result & simd_float::lessThanOrEqual(1e-37f, result);
		return result;
	}

	perf_inline void dynamicsEffect::runContrast(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		// getting the boundaries in terms of bin position
		auto [lowBoundIndices, highBoundIndices] = [&]()
		{
			auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount * 2);
			return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
		}();

		// minimising the bins to iterate on
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

		// calculating contrast
		simd_float depthParameter = getParameter(BaseProcessors::BaseEffect::Dynamics::Contrast::Depth::name())
			->getInternalValue<simd_float>(sampleRate);
		simd_float contrast = depthParameter * depthParameter;
		contrast = maskLoad(
			lerp(simd_float(0.0f), simd_float(kContrastMaxNegativeValue), contrast),
			lerp(simd_float(0.0f), simd_float(kContrastMaxPositiveValue), contrast),
			simd_float::greaterThanOrEqual(depthParameter, 0.0f));

		simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0f + 1.0f));
		simd_float max = exp(simd_float(80.0f) / (contrast * 2.0f + 1.0f));
		min = maskLoad(simd_float(1e-30f), min, simd_float::greaterThan(contrast, 0.0f));
		max = maskLoad(simd_float(1e30f), max, simd_float::greaterThan(contrast, 0.0f));

		simd_float inPower = 0.0f;
		for (u32 i = 0; i < numBins; i++)
		{
			u32 currentIndex = (index + i) & (binCount - 1);
			inPower += complexMagnitude(source.readSimdValueAt(0, currentIndex), false);
		}
		
		simd_int boundDistanceCount = maskLoad(((simd_int(binCount) + highBoundIndices - lowBoundIndices) & simd_int(binCount - 1)) + 1,
		                                       simd_int(0), simd_int::equal(lowBoundIndices, highBoundIndices));
		simd_float inScale = matchPower(toFloat(boundDistanceCount), inPower);
		simd_float outPower = 0.0f;

		// applying gain
		for (u32 i = 0; i < numBins; i++)
		{
			u32 currentIndex = (index + i) & (binCount - 1);
			simd_float bin = inScale * source.readSimdValueAt(0, currentIndex);
			simd_float magnitude = complexMagnitude(bin, true);

			bin = maskLoad(bin, simd_float(0.0f), simd_float::greaterThan(min, magnitude));
			bin = maskLoad(bin, bin * pow(magnitude, contrast), simd_float::greaterThan(max, magnitude));

			outPower += complexMagnitude(bin, false);
			destination.writeSimdValueAt(bin, 0, currentIndex);
		}

		// normalising 
		simd_float outScale = matchPower(inPower, outPower);
		for (u32 i = 0; i < numBins; i++)
		{
			u32 currentIndex = (index + i) & (binCount - 1);
			destination.multiply(outScale, 0, currentIndex);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, binCount);
	}

	perf_inline void dynamicsEffect::runClip(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		// getting the boundaries in terms of bin position
		auto [lowBoundIndices, highBoundIndices] = [&]()
		{
			auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount * 2);
			return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
		}();

		COMPLEX_ASSERT(source.getSize() == destination.getSize());

		auto simdChannels = std::min(source.getSimdChannels(), destination.getSimdChannels());
		auto sourceSize = source.getSize();
		auto sourceData = source.getData().getData().get();
		auto destinationData = destination.getData().getData().get();

		// getting the min/max power in the range selected
		std::pair<simd_float, simd_float> powerMinMax{ 1e30f, 1e-30f };
		for (u32 i = 0; i < simdChannels; ++i)
		{
			for (u32 j = 0; j < binCount; j++)
			{
				// while calculating the power min-max we can copy over data
				simd_float value = sourceData[i * sourceSize + j];
				destinationData[i * sourceSize + j] = value;

				simd_float magnitudeForMin = complexMagnitude(value, false);
				simd_float magnitudeForMax = magnitudeForMin;

				magnitudeForMin = simd_float::min(powerMinMax.first, magnitudeForMin);
				magnitudeForMax = simd_float::max(powerMinMax.second, magnitudeForMax);

				simd_mask indexMask = isInsideBounds(j, lowBoundIndices, highBoundIndices);
				powerMinMax.first = maskLoad(powerMinMax.first, magnitudeForMin, indexMask);
				powerMinMax.second = maskLoad(powerMinMax.second, magnitudeForMax, indexMask);
			}
		}

		// calculating clipping
		simd_float thresholdParameter = getParameter(BaseProcessors::BaseEffect::Dynamics::Clip::Threshold::name())
			->getInternalValue<simd_float>(sampleRate);
		simd_float threshold = exp(lerp(log(simd_float::max(powerMinMax.first, 1e-36f)),
		                                log(simd_float::max(powerMinMax.second, 1e-36f)),
		                                simd_float(1.0f) - thresholdParameter));
		simd_float sqrtThreshold = simd_float::sqrt(threshold);

		// minimising the bins to iterate on
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

		// doing clipping
		simd_float inPower = 0.0f;
		simd_float outPower = 0.0f;
		for (u32 i = 0; i < simdChannels; ++i)
		{
			for (u32 j = 0; j < numBins; ++j)
			{
				u32 currentIndex = (index + j) & (binCount - 1);

				simd_float bin = destinationData[i * sourceSize + currentIndex];
				simd_float magnitude = complexMagnitude(bin, false);
				simd_float multiplier = maskLoad(simd_float(1.0f), sqrtThreshold / simd_float::sqrt(magnitude),
					simd_float::greaterThanOrEqual(magnitude, threshold));

				simd_mask indexMask = isInsideBounds(currentIndex, lowBoundIndices, highBoundIndices);
				destinationData[i * sourceSize + currentIndex] = maskLoad(bin, bin * multiplier, indexMask);

				inPower += maskLoad(simd_float(0.0f), magnitude, indexMask);
				outPower += maskLoad(simd_float(0.0f), simd_float::min(magnitude, threshold), indexMask);
			}

			// normalising 
			simd_float outScale = matchPower(inPower, outPower);
			for (u32 j = 0; j < numBins; j++)
			{
				u32 currentIndex = (index + j) & (binCount - 1);
				destinationData[i * sourceSize + currentIndex] *= maskLoad(simd_float(1.0f), outScale, 
					isInsideBounds(currentIndex, lowBoundIndices, highBoundIndices));
			}
		}
	}

	void dynamicsEffect::run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
	{
		using namespace Framework;
		switch (getEffectAlgorithm<BaseProcessors::BaseEffect::Dynamics::type>(*this))
		{
		case BaseProcessors::BaseEffect::Dynamics::Contrast:
			// based on dtblkfx contrast
			runContrast(source, destination, binCount, sampleRate);
			break;
		case BaseProcessors::BaseEffect::Dynamics::Clip:
			// based on dtblkfx clip
			runClip(source, destination, binCount, sampleRate);
			break;
		default:
			
			utils::convertBuffer<utils::complexCartToPolar>(source, destination, binCount);
			utils::convertBuffer<utils::complexPolarToCart>(destination, destination, binCount);

			/*for (u32 i = 0; i < 10; i++)
				destination.writeSimdValueAt(source.readSimdValueAt(0, i), 0, i);

			for (u32 i = 10; i <= binCount; i++)
				destination.writeSimdValueAt(0.0f, 0, i);*/

			break;
		}
	}

	void phaseEffect::runShift(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		static constexpr simd_mask phaseMask = std::array{ 0U, kFullMask, 0U, kFullMask };
		static constexpr simd_mask phaseLeftMask = std::array{ 0U, kFullMask, 0U, 0U };
		static constexpr simd_mask phaseRightMask = std::array{ 0U, 0U, 0U, kFullMask };

		auto [lowBoundIndices, highBoundIndices] = [&]()
		{
			auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, binCount * 2);
			return std::pair{ toInt(shiftedBoundsIndices.first), toInt(shiftedBoundsIndices.second) };
		}();

		// minimising the bins to iterate on
		auto [index, numBins] = minimiseRange(lowBoundIndices, highBoundIndices, binCount, true);

		simd_float shift = (getParameter(BaseProcessors::BaseEffect::Phase::Shift::PhaseShift::name())
			->getInternalValue<simd_float>(sampleRate, true) * 2.0f - 1.0f) * kPi;
		simd_float interval = getParameter(BaseProcessors::BaseEffect::Phase::Shift::Interval::name())->getInternalValue<simd_float>(sampleRate);
		
		u32 shiftSlope = getParameter(BaseProcessors::BaseEffect::Phase::Shift::Slope::name())->getInternalValue<u32>(sampleRate);
		simd_float (*slopeFunction)(simd_float);
		if (shiftSlope == 0)
			slopeFunction = [](simd_float x) { return x; };
		else
			slopeFunction = [](simd_float x) { return modOnceSigned(x + x, kPi); };

		// overwrite old data
		SimdBuffer<Framework::complex<float>, simd_float>::applyToThisNoMask<MathOperations::Assign>
			(destination, source, destination.getChannels(), binCount, 0, 0, 0, 0);

		// if interval between bins is 0 this means every bin is affected,
		// otherwise the interval specifies how many octaves up the next affected bin is
		if (completelyEqual(interval, 0.0f))
		{
			simd_int offsetBin = toInt(normalisedToBin(getParameter(BaseProcessors::BaseEffect::Phase::Shift::Offset::name())
				->getInternalValue<simd_float>(sampleRate, true), 2 * binCount, sampleRate));

			// find the smallest offset forward and start from there
			u32 minOffset = horizontalMin(offsetBin)[0];
			u32 indexChange = std::max(0U, minOffset - index);
			numBins -= indexChange;
			index += indexChange;

			for (u32 i = 0; i < numBins; ++i)
			{
				u32 currentIndex = (index + i) & (binCount - 1);
				simd_mask offsetMask = simd_int::greaterThanOrEqualSigned(currentIndex, offsetBin);
				simd_mask insideRangeMask = isInsideBounds(currentIndex, lowBoundIndices, highBoundIndices);
				simd_float currentValue = destination.readSimdValueAt(0, currentIndex);
				destination.writeSimdValueAt(maskLoad(currentValue, modOnceSigned(currentValue + shift, kPi),
					phaseMask & offsetMask & insideRangeMask), 0, currentIndex);

				shift = slopeFunction(shift);
			}
		}
		else
		{
			// offset is skewed towards an exp-like curve so we need to linearise it
			simd_float offsetNorm = getParameter(BaseProcessors::BaseEffect::Phase::Shift::Offset::name())
				->getInternalValue<simd_float>(sampleRate) * 2.0f / sampleRate;
			simd_float binStep = 1.0f / (float)binCount;
			simd_float logBase = log2(interval + 1.0f);
			COMPLEX_ASSERT(simd_float::lessThanOrEqual(logBase, 0.0f).anyMask() == 0);

			// if offset is 0 we need to give it a starting value based on interval
			// and shift dc component's amplitude
			{
				simd_mask zeroMask = simd_float::lessThan(offsetNorm, binStep);
				simd_float dcNyquistBins = destination.readSimdValueAt(0, 0);
				simd_float modifiedShift = (-simd_float::abs(shift / kPi) + 0.5f) * 2.0f;
				destination.writeSimdValueAt(maskLoad(dcNyquistBins, dcNyquistBins * modifiedShift, ~phaseMask), 0, 0);

				shift = maskLoad(shift, slopeFunction(shift), zeroMask);

				simd_float startOffset = interval * binStep;
				COMPLEX_ASSERT(simd_float::lessThanOrEqual(startOffset, 0.0f).anyMask() == 0);
				
				// this is derived below, the next 2 lines get the next bin after dc in case any channels started there
				simd_float multiple = simd_float::ceil(log2(binStep / startOffset) / logBase);
				startOffset *= exp2(logBase * multiple);
				offsetNorm = maskLoad(offsetNorm, startOffset, zeroMask);
			}
			
			simd_float lastOffsetNorm = offsetNorm;
			// if inteval < 1, then it's possible for the next while loop to make any progress, 
			// so we make sure that interval * offsetNorm will yield a number at least as big as a single bin step
			{
				simd_float increment = interval * offsetNorm;
				simd_float nextBin = (simd_float::round(offsetNorm * (float)binCount) + 1.0f) / (float)binCount;
				while (simd_float::greaterThan(binStep, increment).anyMask())
				{
					// normal algorithm
					simd_int indices = toInt(simd_float::round(offsetNorm * (float)binCount));
					simd_mask indexMask = isInsideBounds(indices, lowBoundIndices, highBoundIndices);

					if (simd_int::lessThanSigned(indices, binCount).anyMask() == 0)
						break;

					lastOffsetNorm = maskLoad(lastOffsetNorm, offsetNorm, indexMask);
					u32 indexOne = indices[0];
					u32 indexTwo = indices[2];

					destination.writeMaskedSimdValueAt(modOnceSigned(destination.readSimdValueAt(0, indexOne) + shift, kPi),
						indexMask & phaseLeftMask, 0, indexOne);

					destination.writeMaskedSimdValueAt(modOnceSigned(destination.readSimdValueAt(0, indexTwo) + shift, kPi),
						indexMask & phaseRightMask, 0, indexTwo);

					shift = maskLoad(shift, slopeFunction(shift), indexMask);

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
				simd_int indices = toInt(simd_float::round(offsetNorm * (float)binCount));
				simd_mask indexMask = isInsideBounds(indices, lowBoundIndices, highBoundIndices);

				if (simd_int::lessThanSigned(indices, binCount).anyMask() == 0)
					break;

				lastOffsetNorm = maskLoad(lastOffsetNorm, offsetNorm, indexMask);
				u32 indexOne = indices[0];
				u32 indexTwo = indices[2];

				destination.writeMaskedSimdValueAt(modOnceSigned(destination.readSimdValueAt(0, indexOne) + shift, kPi),
					indexMask & phaseLeftMask, 0, indexOne);

				destination.writeMaskedSimdValueAt(modOnceSigned(destination.readSimdValueAt(0, indexTwo) + shift, kPi),
					indexMask & phaseRightMask, 0, indexTwo);

				shift = maskLoad(shift, slopeFunction(shift), indexMask);

				offsetNorm = simd_float::mulAdd(offsetNorm, interval, offsetNorm);
			}

			// shifting nyquist component's amplitude in case it was encountered 
			/*{
				offsetNorm = lastOffsetNorm;
				offsetNorm = simd_float::mulAdd(offsetNorm, interval, offsetNorm);
				simd_int indices = toInt(normalisedToBin(offsetNorm, 2 * binCount, sampleRate));
				simd_mask nyquistMask = simd_int::equal(indices, binCount);
				simd_float dcNyquistBins = destination.readSimdValueAt(0, 0);
				destination.writeSimdValueAt(maskLoad(dcNyquistBins, dcNyquistBins * shift / kPi, phaseMask & nyquistMask), 0, 0);
			}*/
		}
	}

	void phaseEffect::run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept
	{
		using namespace Framework;
		switch (getEffectAlgorithm<BaseProcessors::BaseEffect::Phase::type>(*this))
		{
		case BaseProcessors::BaseEffect::Phase::Shift:
			runShift(source, destination, binCount, sampleRate);
			break;
		default:
			baseEffect::run(source, destination, binCount, sampleRate);
			break;
		}
	}

	EffectModule::EffectModule(Plugin::ProcessorTree *moduleTree, u64 parentModuleId, std::string_view effectType) noexcept :
		BaseProcessor(moduleTree, parentModuleId, Framework::BaseProcessors::EffectModule::id().value())
	{
		using namespace Framework;
		static constexpr auto effectTypes = BaseProcessors::BaseEffect::enum_ids<nested_enum::InnerNodes>();

		subProcessors_.emplace_back(createSubProcessor(effectType));
		dataBuffer_.reserve(kNumChannels, kMaxFFTBufferLength);

		createProcessorParameters(BaseProcessors::EffectModule::enum_names<nested_enum::OuterNodes>());

		auto scaledValue = (double)(std::ranges::find(effectTypes, effectType) - effectTypes.begin());
		auto *parameter = getParameter(BaseProcessors::EffectModule::ModuleType::name());
		parameter->updateValues(kDefaultSampleRate, (float)unscaleValue(scaledValue, parameter->getParameterDetails()));
	}

	BaseProcessor *EffectModule::updateSubProcessor([[maybe_unused]] size_t index, BaseProcessor *newSubModule) noexcept
	{
		static constexpr auto effectTypes = Framework::BaseProcessors::BaseEffect::enum_ids<nested_enum::InnerNodes>();
		COMPLEX_ASSERT(std::ranges::count(effectTypes, newSubModule->getProcessorType())
			&& "You're inserting a non-Effect into an EffectModule");

		auto *replacedEffect = subProcessors_[0];
		subProcessors_[0] = newSubModule;
		return replacedEffect;
	}

	[[nodiscard]] BaseProcessor *EffectModule::createSubProcessor(std::string_view type) const noexcept
	{
		// traverses all effect enums until it finds a match for the id and uses the linked type to call the appropriate constructor
		return nested_enum::recurse_over_types<[]<auto Self, typename U, typename ... Us>(const EffectModule *effectModule, const std::string_view &type)
			{
				if (U::id().value() == type)
					return static_cast<baseEffect *>(effectModule->makeSubProcessor<typename U::linked_type>());

				if constexpr (sizeof...(Us) > 0)
					return Self.template operator()<Self, Us...>(effectModule, type);
				else
				{
					COMPLEX_ASSERT_FALSE("Uncaught EffectType was provided, please add it to the list");
					return static_cast<baseEffect *>(nullptr);
				}
			}>(Framework::BaseProcessors::BaseEffect::enum_subtypes<nested_enum::InnerNodes>(), this, type);
	}

	void EffectModule::processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate)
	{
		using namespace Framework;

		if (!getParameter(BaseProcessors::EffectModule::ModuleEnabled::name())->getInternalValue<u32>(sampleRate))
			return;

		auto *effect = utils::as<baseEffect>(subProcessors_[0]);

		if (auto neededType = effect->neededDataType(); neededType != ComplexDataSource::Both && source.dataType != neededType)
		{
			if (neededType == ComplexDataSource::Polar)
			{
				utils::convertBuffer<utils::complexCartToPolar>(source.sourceBuffer, source.conversionBuffer, binCount);
				source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);
				source.dataType = ComplexDataSource::Polar;
			}
			else
			{
				utils::convertBuffer<utils::complexPolarToCart>(source.sourceBuffer, source.conversionBuffer, binCount);
				source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);
				source.dataType = ComplexDataSource::Cartesian;
			}
			
			source.sourceBuffer = source.conversionBuffer;
		}

		// getting exclusive access to data
		utils::lockAtomic(dataBuffer_.getLock(), true, utils::WaitMechanism::Spin);

		effect->run(source.sourceBuffer, dataBuffer_, binCount, sampleRate);

		// if the mix is 100% for all channels, we can skip mixing entirely
		simd_float wetMix = getParameter(BaseProcessors::EffectModule::ModuleMix::name())->getInternalValue<simd_float>(sampleRate);
		if (!utils::completelyEqual(wetMix, 1.0f))
		{
			simd_float dryMix = simd_float(1.0f) - wetMix;
			for (u32 i = 0; i < binCount; i++)
			{
				dataBuffer_.writeSimdValueAt(simd_float::mulAdd(
					dryMix * source.sourceBuffer.readSimdValueAt(0, i),
					wetMix, dataBuffer_.readSimdValueAt(0, i)), 0, i);
			}
		}

		// switching to being a reader and allowing other readers to participate
		// seq_cst because the following atomic could be reordered prior to this one
		dataBuffer_.getLock().lock.store(1, std::memory_order_seq_cst);
		if (source.sourceBuffer != source.conversionBuffer)
			source.sourceBuffer.getLock().lock.fetch_sub(1, std::memory_order_relaxed);

		source.sourceBuffer = dataBuffer_;
	}
}