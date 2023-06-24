/*
	==============================================================================

		EffectModules.cpp
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#include "EffectModules.h"
#include "../Plugin/ProcessorTree.h"

namespace Generation
{
	///////////////////////////////////////////////
	///////////////////////////////////////////////
	//////////////// Constructors /////////////////
	///////////////////////////////////////////////
	///////////////////////////////////////////////

	utilityEffect::utilityEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{

	}

	filterEffect::filterEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{
		auto parameterSize = Framework::baseEffectParameterList.size() + (u32)Framework::FilterEffectParameters::size;
		processorParameters_.data.reserve(parameterSize);
		createProcessorParameters(Framework::filterEffectParameterList[0].data(), (u32)Framework::FilterEffectParameters::size);
		setEffectModeStrings(Framework::EffectTypes::Filter);
	}

	dynamicsEffect::dynamicsEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{
		auto parameterSize = Framework::baseEffectParameterList.size() + (u32)Framework::DynamicsEffectParameters::size;
		processorParameters_.data.reserve(parameterSize);
		createProcessorParameters(Framework::dynamicsEffectParameterList[0].data(), (u32)Framework::DynamicsEffectParameters::size);
		setEffectModeStrings(Framework::EffectTypes::Dynamics);
	}

	phaseEffect::phaseEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{

	}

	pitchEffect::pitchEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{

	}

	stretchEffect::stretchEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{

	}

	warpEffect::warpEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
	{

	}

	destroyEffect::destroyEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId) :
		baseEffect(globalModulesState, parentModuleId, getClassType())
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
		simd_float lowBound = processorParameters_[(u32)BaseEffectParameters::LowBound]->getInternalValue<simd_float>(sampleRate, true);
		simd_float highBound = processorParameters_[(u32)BaseEffectParameters::HighBound]->getInternalValue<simd_float>(sampleRate, true);
		// TODO: do dynamic min frequency based on FFTOrder
		// TODO: get shifted bounds as normalised value/frequency/FFT bin
		float nyquistFreq = sampleRate * 0.5f;
		float maxOctave = static_cast<float>(std::log2(nyquistFreq / kMinFrequency));

		if (isLinearShift)
		{
			// TODO: linear shift
			simd_float boundShift = processorParameters_[(u32)BaseEffectParameters::ShiftBounds]
				->getInternalValue<simd_float>(sampleRate) * nyquistFreq;
			lowBound = simd_float::clamp(exp2(lowBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, nyquistFreq);
			highBound = simd_float::clamp(exp2(highBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, nyquistFreq);
			// snapping to 0 hz if it's below the minimum frequency
			lowBound &= simd_float::greaterThan(lowBound, kMinFrequency);
			highBound &= simd_float::greaterThan(highBound, kMinFrequency);
		}
		else
		{
			simd_float boundShift = processorParameters_[(u32)BaseEffectParameters::ShiftBounds]
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

	std::pair<u32, u32> vector_call baseEffect::getRange(simd_int lowIndices, simd_int highIndices,
		u32 effectiveFFTSize, bool isProcessedRange) const noexcept
	{
		// 1. all the indices in the respective bounds are the same (mono)
		// 2. the indices in the respective bounds are different (stereo)

		// NB: the range for processing is [start, end] and NOT [start, end)

		bool areAllSame = utils::areAllElementsSame(lowIndices) && utils::areAllElementsSame(highIndices);

		// 1.
		if (areAllSame)
		{
			u32 start, end;
			if (isProcessedRange)
			{
				start = lowIndices[0];
				end = highIndices[0];
				if ((((start + 1) & (effectiveFFTSize - 1)) == end) || 
					 (((end + 1) & (effectiveFFTSize - 1)) == start) ||
					 (start == end))
					return { start, effectiveFFTSize };
				else
					return { start, ((effectiveFFTSize + end - start) & (effectiveFFTSize - 1)) + 1 };
			}
			else
			{
				start = highIndices[0];
				end = lowIndices[0];
				if ((((start + 1) & (effectiveFFTSize - 1)) == end) || (((end + 1) & (effectiveFFTSize - 1)) == start))
					return { start, 0 };
				else
					return { (start + 1) & (effectiveFFTSize - 1), (effectiveFFTSize + end - start) & (effectiveFFTSize - 1) };
			}
		}

		// 2.
		// trying to rationalise what parts to cover is proving too complicated
		// TODO: if the range is [start, end], then why are we specifying a range that's [start, end)????
		return { 0, effectiveFFTSize };
	}

	void baseEffect::copyUnprocessedData(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const simd_int &lowBoundIndices,
		const simd_int &highBoundIndices, u32 effectiveFFTSize) const noexcept
	{
		auto unprocessedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, effectiveFFTSize, false);
		u32 index = unprocessedIndexAndRange.first;
		u32 numBins = unprocessedIndexAndRange.second;

		// this is not possible, so we'll take it to mean that we have stereo bounds
		if (index == 0 && numBins == effectiveFFTSize)
		{
			for (; index < numBins; index++)
			{
				destination.writeSimdValueAt(utils::maskLoad(destination.readSimdValueAt(0, index),
					source.readSimdValueAt(0, index), isOutsideBounds(index, lowBoundIndices, highBoundIndices)), 0, index);
			}
		}
		// mono bounds
		else
		{
			for (u32 i = 0; i < numBins; i++)
			{
				destination.writeSimdValueAt(source.readSimdValueAt(0, index), 0, index);
				index = (index + 1) & (effectiveFFTSize - 1);
			}
		}
	}

	strict_inline simd_float vector_call filterEffect::getDistancesFromCutoffs(simd_int positionIndices,
		simd_int cutoffIndices, simd_int lowBoundIndices, u32 FFTSize, float sampleRate) const noexcept
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

	perf_inline void filterEffect::runNormal(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		simd_float lowBoundNorm = processorParameters_[(u32)BaseEffectParameters::LowBound]->getInternalValue<simd_float>(sampleRate, true);
		simd_float highBoundNorm = processorParameters_[(u32)BaseEffectParameters::HighBound]->getInternalValue<simd_float>(sampleRate, true);
		simd_float boundShift = processorParameters_[(u32)BaseEffectParameters::ShiftBounds]->getInternalValue<simd_float>(sampleRate);
		simd_float boundsDistance = modOnce(simd_float(1.0f) + highBoundNorm - lowBoundNorm, 1.0f);

		// getting the boundaries in terms of bin position
		auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, effectiveFFTSize * 2);
		simd_int lowBoundIndices = toInt(shiftedBoundsIndices.first);
		simd_int highBoundIndices = toInt(shiftedBoundsIndices.second);

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, effectiveFFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// cutoff is described as exponential normalised value of the sample rate
		// it is dependent on the values of the low/high bounds
		simd_float cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance * 
			processorParameters_[(u32)FilterEffectParameters::Normal::Cutoff]->getInternalValue<simd_float>(sampleRate), 1.0f);
		simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, effectiveFFTSize * 2, sampleRate));

		// if mask scalars are negative -> brickwall, if positive -> linear slope
		// slopes are logarithmic
		simd_float slopes = processorParameters_[(u32)FilterEffectParameters::Normal::Slope]->getInternalValue<simd_float>(sampleRate) / 2.0f;
		simd_mask slopeMask = unsignSimd(slopes, true);
		simd_mask slopeZeroMask = simd_float::equal(slopes, 0.0f);

		// if scalars are negative -> attenuate at cutoff, if positive -> around cutoff
		// (gains is gain reduction in db and NOT a gain multiplier)
		simd_float gains = processorParameters_[(u32)FilterEffectParameters::Normal::Gain]->getInternalValue<simd_float>(sampleRate);
		simd_mask gainMask = unsignSimd(gains, true);

		for (u32 i = 0; i < numBins; i++)
		{
			// the distances are logarithmic
			simd_float distancesFromCutoff = getDistancesFromCutoffs(simd_int(index),
				cutoffIndices, lowBoundIndices, effectiveFFTSize * 2, sampleRate);

			// calculating linear slope and brickwall, both are ratio of the gain attenuation
			// the higher tha value the more it will be affected by it
			simd_float gainRatio = maskLoad(simd_float::clamp(maskLoad(distancesFromCutoff, simd_float(1.0f), slopeZeroMask) 
				/ maskLoad(slopes, simd_float(1.0f), slopeZeroMask), 0.0f, 1.0f),
				simd_float(1.0f) & simd_float::greaterThanOrEqual(distancesFromCutoff, slopes),
				~slopeMask);
			simd_float currentGains = maskLoad(gains * gainRatio, gains * (simd_float(1.0f) - gainRatio), gainMask);

			// convert db reduction to amplitude multiplier
			currentGains = dbToAmplitude(-currentGains);

			destination.writeSimdValueAt(source.readSimdValueAt(0, index) * currentGains, 0, index);

			index = (index + 1) & (effectiveFFTSize - 1);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, effectiveFFTSize);
	}

	void filterEffect::run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept
	{
		using namespace Framework;
		switch (getEffectMode())
		{
		case (u32)FilterModes::Normal:
			runNormal(source, destination, effectiveFFTSize, sampleRate);
			break;
		case (u32)FilterModes::Regular:
		default:
			break;
		}
	}

	perf_inline void dynamicsEffect::runContrast(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) const noexcept
	{
		using namespace utils;
		using namespace Framework;

		// getting the boundaries in terms of bin position
		auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate, effectiveFFTSize * 2);
		simd_int lowBoundIndices = toInt(shiftedBoundsIndices.first);
		simd_int highBoundIndices = toInt(shiftedBoundsIndices.second);
		simd_int boundDistanceCount = maskLoad(((simd_int(effectiveFFTSize) + highBoundIndices - lowBoundIndices) & simd_int(effectiveFFTSize - 1)) + 1,
																					 simd_int(0), simd_int::equal(lowBoundIndices, highBoundIndices));

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, effectiveFFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// calculating contrast
		simd_float contrastParameter = processorParameters_[(u32)DynamicsEffectParameters::Contrast::Depth]
			->getInternalValue<simd_float>(sampleRate);
		simd_float contrast = contrastParameter * contrastParameter;
		contrast = maskLoad(
			lerp(simd_float(0.0f), simd_float(kContrastMaxNegativeValue), contrast),
			lerp(simd_float(0.0f), simd_float(kContrastMaxPositiveValue), contrast),
			simd_float::greaterThanOrEqual(contrastParameter, 0.0f));

		simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0f + 1.0f));
		simd_float max = exp(simd_float(80.0f) / (contrast * 2.0f + 1.0f));
		min = maskLoad(simd_float(1e-30f), min, simd_float::greaterThan(contrast, 0.0f));
		max = maskLoad(simd_float(1e30f), max, simd_float::greaterThan(contrast, 0.0f));

		simd_float inPower = 0.0f;
		for (u32 i = 0; i < numBins; i++)
		{
			inPower += complexMagnitude(source.readSimdValueAt(0, index), false);
			index = (index + 1) & (effectiveFFTSize - 1);
		}
		index = processedIndexAndRange.first;
		
		simd_float inScale = matchPower(toFloat(boundDistanceCount), inPower);
		simd_float outPower = 0.0f;

		// applying gain
		for (u32 i = 0; i < numBins; i++)
		{
			simd_float bin = inScale * source.readSimdValueAt(0, index);
			simd_float magnitude = complexMagnitude(bin, true);

			bin = maskLoad(bin, simd_float(0.0f), simd_float::greaterThan(min, magnitude));
			bin = maskLoad(bin, bin * pow(magnitude, contrast), simd_float::greaterThan(max, magnitude));

			outPower += complexMagnitude(bin, false);
			destination.writeSimdValueAt(bin, 0, index);

			index = (index + 1) & (effectiveFFTSize - 1);
		}
		index = processedIndexAndRange.first;

		// normalising 
		simd_float outScale = matchPower(inPower, outPower);
		for (u32 i = 0; i < numBins; i++)
		{
			destination.multiply(outScale, 0, index);
			index = (index + 1) & (effectiveFFTSize - 1);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, effectiveFFTSize);
	}

	void dynamicsEffect::run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept
	{
		using namespace Framework;
		switch (getEffectMode())
		{
		case (u32)DynamicsModes::Contrast:
			// based on dtblkfx' contrast
			runContrast(source, destination, effectiveFFTSize, sampleRate);
			break;
		case 1:
			baseEffect::run(source, destination, effectiveFFTSize, sampleRate);
			break;
		default:
			
			for (u32 i = 0; i < 10; i++)
				destination.writeSimdValueAt(source.readSimdValueAt(0, i), 0, i);

			for (u32 i = 10; i <= effectiveFFTSize; i++)
				destination.writeSimdValueAt(0.0f, 0, i);

			break;
		}
	}

	EffectModule::EffectModule(Plugin::ProcessorTree *moduleTree, u64 parentModuleId, std::string_view effectType) noexcept :
		BaseProcessor(moduleTree, parentModuleId, getClassType())
	{
		subProcessors_.emplace_back(createSubProcessor(effectType));
		dataBuffer_.reserve(kNumChannels, kMaxFFTBufferLength);

		processorParameters_.data.reserve(Framework::effectModuleParameterList.size());
		createProcessorParameters(Framework::effectModuleParameterList.data(), Framework::effectModuleParameterList.size());
	}

	baseEffect *EffectModule::createEffect(std::string_view type) const noexcept
	{
		if (type == utilityEffect::getClassType()) return makeSubProcessor<utilityEffect>();
		if (type == filterEffect::getClassType()) return makeSubProcessor<filterEffect>();
		if (type == dynamicsEffect::getClassType()) return makeSubProcessor<dynamicsEffect>();
		if (type == phaseEffect::getClassType()) return makeSubProcessor<phaseEffect>();
		if (type == pitchEffect::getClassType()) return makeSubProcessor<pitchEffect>();
		if (type == stretchEffect::getClassType()) return makeSubProcessor<stretchEffect>();
		if (type == warpEffect::getClassType()) return makeSubProcessor<warpEffect>();
		if (type == destroyEffect::getClassType()) return makeSubProcessor<destroyEffect>();

		COMPLEX_ASSERT_FALSE("Uncaught EffectType was provided, please add it to the list");
		return nullptr;
	}

	BaseProcessor *EffectModule::updateSubProcessor([[maybe_unused]] size_t index, BaseProcessor *newSubModule) noexcept
	{
		COMPLEX_ASSERT(magic_enum::enum_contains<Framework::EffectTypes>(newSubModule->getProcessorType()) 
			&& "You're inserting a non-Effect into an EffectModule");

		auto *replacedEffect = subProcessors_[0];
		subProcessors_[0] = newSubModule;
		return replacedEffect;
	}

	[[nodiscard]] BaseProcessor *EffectModule::createSubProcessor(std::string_view type) const noexcept
	{
		COMPLEX_ASSERT(std::ranges::count(effectsTypes, type) && 
			"You're trying to create a non-Effect subProcessor of EffectModule");

		return createEffect(type);
	}

	void EffectModule::processEffect(ComplexDataSource &source, u32 effectiveFFTSize, float sampleRate)
	{
		using namespace Framework;

		if (!processorParameters_[(u32)EffectModuleParameters::ModuleEnabled]->getInternalValue<u32>(sampleRate))
			return;

		// SAFETY: static_cast is safe because we're only supposed to have baseEffects by design
		auto *effect = static_cast<baseEffect *>(subProcessors_[0]);

		if (source.isPolar != effect->needsPolarData())
		{
			// TODO: polar/cartesian conversions
			COMPLEX_ASSERT(false);

			source.isPolar = !source.isPolar;
		}

		effect->run(source.sourceBuffer, dataBuffer_, effectiveFFTSize, sampleRate);

		// if the mix is 100% for all channels, we can skip mixing entirely
		simd_float wetMix = processorParameters_[(u32)EffectModuleParameters::ModuleMix]->getInternalValue<simd_float>(sampleRate);
		if (!utils::completelyEqual(wetMix, 1.0f))
		{
			simd_float dryMix = simd_float(1.0f) - wetMix;
			for (u32 i = 0; i < effectiveFFTSize; i++)
			{
				dataBuffer_.writeSimdValueAt(simd_float::mulAdd(
					dryMix * source.sourceBuffer.readSimdValueAt(0, i),
					wetMix, dataBuffer_.readSimdValueAt(0, i)), 0, i);
			}
		}

		source.sourceBuffer = dataBuffer_;
	}
}