/*
	==============================================================================

		EffectModules.cpp
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#include "EffectModules.h"

namespace Generation
{
	std::pair<simd_float, simd_float> baseEffect::getShiftedBounds(BoundRepresentation representation,
		float maxFrequency, u32 FFTSize, bool isLinearShift) const noexcept
	{
		using namespace utils;
		simd_float lowBound = moduleParameters_[1]->getInternalValue<simd_float>(true);
		simd_float highBound = moduleParameters_[2]->getInternalValue<simd_float>(true);
		// TODO: do dynamic min frequency based on FFTOrder
		// TODO: get shifted bounds as normalised value/frequency/FFT bin
		float maxOctave = log2(maxFrequency / kMinFrequency);

		if (isLinearShift)
		{
			// TODO: linear shift
			simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>() * maxFrequency;
			lowBound = simd_float::clamp(exp2(lowBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, maxFrequency);
			highBound = simd_float::clamp(exp2(highBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, maxFrequency);
			// snapping to 0 hz if it's below the minimum frequency
			lowBound &= simd_float::greaterThan(lowBound, kMinFrequency);
			highBound &= simd_float::greaterThan(highBound, kMinFrequency);
		}
		else
		{
			simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>();
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
				lowBound = normalisedToBin(lowBound, FFTSize, maxFrequency * 2.0f);
				highBound = normalisedToBin(highBound, FFTSize, maxFrequency * 2.0f);

				break;
			default:
				break;
			}
		}
		return { lowBound, highBound };
	}

	std::pair<u32, u32> vector_call baseEffect::getRange(const simd_int &lowIndices, const simd_int &highIndices,
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
		// trying to rationalise what what parts to cover is proving too complicated
		return { 0, effectiveFFTSize };
	}

	void baseEffect::copyUnprocessedData(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
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
				destination.writeSIMDValueAt(utils::maskLoad(destination.getSIMDValueAt(0, index),
					source.getSIMDValueAt(0, index), isOutsideBounds(index, lowBoundIndices, highBoundIndices)), 0, index);
			}
		}
		// mono bounds
		else
		{
			for (u32 i = 0; i < numBins; i++)
			{
				destination.writeSIMDValueAt(source.getSIMDValueAt(0, index), 0, index);
				index = (index + 1) & (effectiveFFTSize - 1);
			}
		}
	}

	strict_inline simd_float vector_call filterEffect::getDistancesFromCutoffs(const simd_int &positionIndices,
		const simd_int &cutoffIndices, const simd_int &lowBoundIndices, u32 FFTSize, float sampleRate) const noexcept
	{
		// 1. both positionIndices and cutoffIndices are >= lowBound and < FFTSize_ or <= highBound and > 0
		// 2. cutoffIndices/positionIndices is >= lowBound and < FFTSize_ and
		//     positionIndices/cutoffIndices is <= highBound and > 0

		using namespace utils;

		simd_mask cutoffAbovePositions = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

		// masks for 1.
		simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
		simd_mask cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundIndices);
		simd_mask bothAboveOrBelowLowMask = ~(positionsAboveLowMask ^ cutoffAboveLowMask);

		// masks for 2.
		simd_mask positionsBelowLowBoundAndCutoffsMask = ~positionsAboveLowMask & cutoffAboveLowMask;
		simd_mask cutoffBelowLowBoundAndPositionsMask = positionsAboveLowMask & ~cutoffAboveLowMask;

		// masking for 1.
		simd_int precedingIndices = utils::maskLoad(cutoffIndices, positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
		simd_int succeedingIndices = utils::maskLoad(positionIndices, cutoffIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);

		// masking for 2.
		// first 2 are when cutoffs/positions are above/below lowBound
		// second 2 are when positions/cutoffs are above/below lowBound
		precedingIndices = utils::maskLoad(precedingIndices, cutoffIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
		succeedingIndices = utils::maskLoad(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
		precedingIndices = utils::maskLoad(precedingIndices, positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);
		succeedingIndices = utils::maskLoad(succeedingIndices, cutoffIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);

		simd_float precedingIndicesRatios = utils::binToNormalised(utils::toFloat(precedingIndices), FFTSize, sampleRate);
		simd_float succeedingIndicesRatios = utils::binToNormalised(utils::toFloat(succeedingIndices), FFTSize, sampleRate);

		return utils::getDecimalPlaces(simd_float(1.0f) + succeedingIndicesRatios - precedingIndicesRatios);
	}

	perf_inline void filterEffect::runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const u32 effectiveFFTSize, const float sampleRate) noexcept
	{
		using namespace utils;

		simd_float lowBoundNorm = moduleParameters_[1]->getInternalValue<simd_float>(true);
		simd_float highBoundNorm = moduleParameters_[2]->getInternalValue<simd_float>(true);
		simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>();
		simd_float boundsDistance = modOnce(simd_float(1.0f) + highBoundNorm - lowBoundNorm);

		// getting the boundaries in terms of bin position
		auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate / 2.0f, effectiveFFTSize * 2);
		simd_int lowBoundIndices = toInt(shiftedBoundsIndices.first);
		simd_int highBoundIndices = toInt(shiftedBoundsIndices.second);

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, effectiveFFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// cutoff is described as exponential normalised value of the sample rate
		// it is dependent on the values of the low/high bounds
		simd_float cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance * 
			moduleParameters_[5]->getInternalValue<simd_float>());
		simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, effectiveFFTSize * 2, sampleRate));

		// if mask scalars are negative -> brickwall, if positive -> linear slope
		// slopes are logarithmic
		simd_float slopes = moduleParameters_[6]->getInternalValue<simd_float>() / 2.0f;
		simd_mask slopeMask = unsignFloat(slopes, true);
		simd_mask slopeZeroMask = simd_float::equal(slopes, 0.0f);

		// if scalars are negative -> attenuate at cutoff, if positive -> around cutoff
		// (gains is gain reduction in db and NOT a gain multiplier)
		simd_float gains = moduleParameters_[4]->getInternalValue<simd_float>();
		simd_mask gainMask = unsignFloat(gains, true);

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

			destination.writeSIMDValueAt(source.getSIMDValueAt(0, index) * currentGains, 0, index);

			index = (index + 1) & (effectiveFFTSize - 1);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, effectiveFFTSize);
	}

	void filterEffect::run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept
	{
		switch (moduleParameters_[0]->getInternalValue<u32>())
		{
		case static_cast<u32>(Framework::FilterTypes::Normal):
			runNormal(source, destination, effectiveFFTSize, sampleRate);
			break;
		default:
			break;
		}
	}

	perf_inline void contrastEffect::runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept
	{
		using namespace utils;

		// getting the boundaries in terms of bin position
		auto shiftedBoundsIndices = getShiftedBounds(BoundRepresentation::BinIndex, sampleRate / 2.0f, effectiveFFTSize * 2);
		simd_int lowBoundIndices = toInt(shiftedBoundsIndices.first);
		simd_int highBoundIndices = toInt(shiftedBoundsIndices.second);
		simd_int boundDistanceCount = maskLoad(((simd_int(effectiveFFTSize) + highBoundIndices - lowBoundIndices) & simd_int(effectiveFFTSize - 1)) + 1,
																					 simd_int(0), simd_int::equal(lowBoundIndices, highBoundIndices));

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, effectiveFFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// calculating contrast
		simd_float contrastParameter = moduleParameters_[4]->getInternalValue<simd_float>();
		simd_float contrast = contrastParameter * contrastParameter;
		contrast = maskLoad(
			interpolate(simd_float(0.0f), simd_float(kMaxNegativeValue), contrast),
			interpolate(simd_float(0.0f), simd_float(kMaxPositiveValue), contrast),
			simd_float::greaterThanOrEqual(contrastParameter, 0.0f));

		simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0f + 1.0f));
		simd_float max = exp(simd_float(80.0f) / (contrast * 2.0f + 1.0f));
		min = maskLoad(simd_float(1e-30f), min, simd_float::greaterThan(contrast, 0.0f));
		max = maskLoad(simd_float(1e30f), max, simd_float::greaterThan(contrast, 0.0f));

		simd_float inPower = 0.0f;
		for (u32 i = 0; i < numBins; i++)
		{
			inPower += complexMagnitude(source.getSIMDValueAt(0, index), false);
			index = (index + 1) & (effectiveFFTSize - 1);
		}
		index = processedIndexAndRange.first;
		
		simd_float inScale = matchPower(toFloat(boundDistanceCount), inPower);
		simd_float outPower = 0.0f;

		// applying gain
		for (u32 i = 0; i < numBins; i++)
		{
			simd_float bin = inScale * source.getSIMDValueAt(0, index);
			simd_float magnitude = complexMagnitude(bin, false);

			bin = maskLoad(bin, simd_float(0.0f), simd_float::greaterThan(min, magnitude));
			bin = maskLoad(bin, bin * pow(magnitude, contrast), simd_float::greaterThan(max, magnitude));

			outPower += complexMagnitude(bin, false);
			destination.writeSIMDValueAt(bin, 0, index);

			index = (index + 1) & (effectiveFFTSize - 1);
		}
		index = processedIndexAndRange.first;

		simd_float outScale = matchPower(inPower, outPower);
		
		for (u32 i = 0; i < numBins; i++)
		{
			destination.multiply(outScale, 0, index);
			index = (index + 1) & (effectiveFFTSize - 1);
		}

		copyUnprocessedData(source, destination, lowBoundIndices, highBoundIndices, effectiveFFTSize);
	}

	void contrastEffect::run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept
	{
		switch (moduleParameters_[0]->getInternalValue<u32>())
		{
		case static_cast<u32>(Framework::ContrastTypes::Contrast):
			// based on dtblkfx' contrast
			runContrast(source, destination, effectiveFFTSize, sampleRate);
			break;
		case 1:
			baseEffect::run(source, destination, effectiveFFTSize, sampleRate);
			break;
		default:
			
			for (size_t i = 0; i < 10; i++)
				destination.writeSIMDValueAt(source.getSIMDValueAt(0, i), 0, i);

			for (size_t i = 10; i <= effectiveFFTSize; i++)
				destination.writeSIMDValueAt(0.0f, 0, i);

			break;
		}
	}

	EffectModule::EffectModule(AllModules *globalModulesState, u64 parentModuleId, std::string_view effectType) noexcept : 
		PluginModule(globalModulesState, parentModuleId, Framework::kPluginModules[3])
	{
		subModules_.emplace_back();
		insertSubModule(0, effectType);

		moduleParameters_.data.reserve(Framework::effectModuleParameterList.size());
		createModuleParameters(Framework::effectModuleParameterList.data(), Framework::effectModuleParameterList.size());
	}

	bool EffectModule::insertSubModule([[maybe_unused]] u32 index, std::string_view moduleType) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		std::shared_ptr<PluginModule> newEffect{};

		if (moduleType == Framework::kEffectModuleNames[0])
			newEffect = createSubModule<utilityEffect>();
		else if (moduleType == Framework::kEffectModuleNames[1])
			newEffect = createSubModule<filterEffect>();
		else if (moduleType == Framework::kEffectModuleNames[2])
			newEffect = createSubModule<contrastEffect>();
		else if (moduleType == Framework::kEffectModuleNames[3])
			newEffect = createSubModule<dynamicsEffect>();
		else if (moduleType == Framework::kEffectModuleNames[4])
			newEffect = createSubModule<phaseEffect>();
		else if (moduleType == Framework::kEffectModuleNames[5])
			newEffect = createSubModule<pitchEffect>();
		else if (moduleType == Framework::kEffectModuleNames[6])
			newEffect = createSubModule<stretchEffect>();
		else if (moduleType == Framework::kEffectModuleNames[7])
			newEffect = createSubModule<warpEffect>();
		else if (moduleType == Framework::kEffectModuleNames[8])
			newEffect = createSubModule<destroyEffect>();
		else COMPLEX_ASSERT(false && "You're inserting a non-Effect into an EffectModule");

		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		currentlyUsing_.store(0, std::memory_order_release);

		return true;
	}

	bool EffectModule::copySubModule(const std::shared_ptr<PluginModule> &newSubModule, [[maybe_unused]] u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;
		
		COMPLEX_ASSERT((std::find(Framework::kEffectModuleNames.begin(), Framework::kEffectModuleNames.end(),
			static_cast<baseEffect *>(newSubModule.get())->getEffectType()) != Framework::kEffectModuleNames.end()) 
			&& "You're inserting a non-Effect into an EffectModule");

		std::shared_ptr<PluginModule> newEffect = newSubModule->createCopy(moduleId_);
		
		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		currentlyUsing_.store(0, std::memory_order_release);
		
		return true;
	}

	bool EffectModule::moveSubModule(std::shared_ptr<PluginModule> newSubModule, [[maybe_unused]] u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT((std::find(Framework::kEffectModuleNames.begin(), Framework::kEffectModuleNames.end(),
			static_cast<baseEffect *>(newSubModule.get())->getEffectType()) != Framework::kEffectModuleNames.end())
			&& "You're inserting a non-Effect into an EffectModule");

		std::shared_ptr<PluginModule> newEffect{};
		newEffect.swap(newSubModule);
		newEffect->setParentModuleId(moduleId_);

		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		currentlyUsing_.store(0, std::memory_order_release);
		
		return true;
	}

	void EffectModule::processEffect(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate)
	{
		if (!moduleParameters_[0]->getInternalValue<u32>())
		{
			source.swap(destination);
			return;
		}

		// by design this shouldn't even wait because change of effect happens after all processing is done
		// this here is simply a sanity check
		utils::spinAndLock(currentlyUsing_, 0, 1);

		static_cast<baseEffect *>(subModules_[0].get())->run(source, destination, effectiveFFTSize, sampleRate);

		// if the mix is 100% for all channels, we can skip mixing entirely
		simd_float wetMix = moduleParameters_[2]->getInternalValue<simd_float>();
		if (!utils::completelyEqual(wetMix, 1.0f))
		{
			simd_float dryMix = simd_float(1.0f) - wetMix;
			for (u32 i = 0; i < effectiveFFTSize; i++)
			{
				destination.writeSIMDValueAt(simd_float::mulAdd(
					dryMix * source.getSIMDValueAt(0, i),
					wetMix, destination.getSIMDValueAt(0, i)), 0, i);
			}
		}

		// if there's no gain change for any channels, we can skip it entirely
		simd_float gain = moduleParameters_[3]->getInternalValue<simd_float>();
		if (!utils::completelyEqual(gain, 0.0f))
		{
			simd_float magnitude = utils::dbToAmplitude(gain);
			for (u32 i = 0; i < effectiveFFTSize; i++)
				destination.multiply(magnitude, 0, i);
		}

		currentlyUsing_.store(0, std::memory_order_release);
	}
}