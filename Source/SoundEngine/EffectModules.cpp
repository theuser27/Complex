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
	perf_inline void filterEffect::runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const u32 FFTSize, const float sampleRate) noexcept
	{
		using namespace utils;

		u32 FFTOrder = ilog2(FFTSize);

		simd_float lowBoundNorm = moduleParameters_[1]->getInternalValue<simd_float>(true);
		simd_float highBoundNorm = moduleParameters_[2]->getInternalValue<simd_float>(true);
		simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>();
		simd_float boundsDistance = modOnce(simd_float(1.0f) + highBoundNorm - lowBoundNorm);

		// getting the boundaries in terms of bin position
		auto shiftedBoundsHz = getShiftedBounds(lowBoundNorm, highBoundNorm, sampleRate / 2.0f, false);
		simd_int lowBoundIndices = toInt(simd_float::ceil((shiftedBoundsHz.first / (sampleRate / 2.0f)) * (float)FFTSize));
		simd_int highBoundIndices = toInt(simd_float::floor((shiftedBoundsHz.second / (sampleRate / 2.0f)) * (float)FFTSize));

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, FFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// cutoff is described as exponential normalised value of the sample rate
		// it is dependent on the values of the low/high bounds
		simd_float cutoffNorm = modOnce(lowBoundNorm + boundShift + boundsDistance * 
			moduleParameters_[5]->getInternalValue<simd_float>());
		simd_int cutoffIndices = toInt(normalisedToBin(cutoffNorm, FFTOrder));

		// if mask scalars are negative -> brickwall, if positive -> linear slope
		// slopes are logarithmic
		simd_float slopes = moduleParameters_[6]->getInternalValue<simd_float>();
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
				cutoffIndices, lowBoundIndices, FFTOrder, FFTSize);

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

			index = (index + 1) & (FFTSize - 1);
		}

		auto unprocessedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, FFTSize, false);
		index = unprocessedIndexAndRange.first;
		numBins = unprocessedIndexAndRange.second;

		// copying the unprocessed data
		for (u32 i = 0; i < numBins; i++)
		{
			simd_mask boundsMask = simd_int::greaterThanOrEqualSigned(lowBoundIndices, index)
				| simd_int::greaterThanOrEqualSigned(index, highBoundIndices);
			destination.writeSIMDValueAt((source.getSIMDValueAt(0, index) & boundsMask) +
				(destination.getSIMDValueAt(0, index) & ~boundsMask), 0, index);

			index = (index + 1) & (FFTSize - 1);
		}
	}

	void filterEffect::run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept
	{
		switch (moduleParameters_[0]->getInternalValue<u32>())
		{
		case static_cast<u32>(Framework::FilterTypes::Normal):
			runNormal(source, destination, FFTSize, sampleRate);
			break;
		default:
			break;
		}
	}

	perf_inline void contrastEffect::runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept
	{
		using namespace utils;

		auto shiftedBounds = getShiftedBounds(moduleParameters_[1]->getInternalValue<simd_float>(),
			moduleParameters_[2]->getInternalValue<simd_float>(), sampleRate / 2.0f, false);

		// getting the boundaries in terms of bin position
		simd_int lowBoundIndices = toInt(simd_float::ceil((shiftedBounds.first / (sampleRate / 2.0f)) * (float)FFTSize));
		simd_int highBoundIndices = toInt(simd_float::floor((shiftedBounds.second / (sampleRate / 2.0f)) * (float)FFTSize));

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, FFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// calculating contrast, verbatim from dtblkfx
		simd_float contrastParameter = moduleParameters_[4]->getInternalValue<simd_float>();
		simd_float contrast = contrastParameter * contrastParameter;
		contrast = maskLoad(
			interpolate(simd_float(kMinPositiveValue), simd_float(kMaxPositiveValue), contrast),
			interpolate(simd_float(kMinPositiveValue), simd_float(kMaxNegativeValue), contrast),
			simd_float::greaterThanOrEqual(contrastParameter, simd_float(kMinPositiveValue)));

		simd_float min = exp(simd_float(-80.0f) / (contrast * 2.0 + 1.0f));
		simd_float max = exp(simd_float(80.0f) / (contrast * 2.0 + 1.0f));
		min = maskLoad(min, simd_float(1e-30f), simd_float::greaterThan(contrast, 0.0f));
		max = maskLoad(max, simd_float(1e30f), simd_float::greaterThan(contrast, 0.0f));

		simd_float inPower;

		for (u32 i = 0; i < FFTSize; i++)
			inPower += complexMagnitude(source.getSIMDValueAt(0, i));
		simd_float inScale = sqrt(simd_float((float)(FFTSize + 1)) / inPower);

		simd_float outPower;

		// applying gain
		for (u32 i = 0; i < numBins; i++)
		{
			simd_float bin = inScale * source.getSIMDValueAt(0, index);
			simd_float magnitude = complexMagnitude(bin);

			bin = maskLoad(simd_float(0.0f), bin, simd_float::greaterThan(min, magnitude));
			bin = maskLoad(bin * pow(magnitude, contrast), bin, simd_float::greaterThan(max, magnitude));

			outPower += complexMagnitude(bin);
			destination.writeSIMDValueAt(bin, 0, index);

			index = (index + 1) % FFTSize;
		}

		// power matching
		simd_float outScale = sqrt(inPower / outPower);
		index = processedIndexAndRange.first;
		for (u32 i = 0; i < numBins; i++)
		{
			destination.multiply(outScale, 0, index);
			index = (index + 1) % FFTSize;
		}

		// copying the unprocessed data
		auto unprocessedIndexAndRange = getRange(lowBoundIndices, highBoundIndices, FFTSize, false);
		index = unprocessedIndexAndRange.first;
		numBins = unprocessedIndexAndRange.second;
		simd_mask boundsMask;

		for (u32 i = 0; i < numBins; i++)
		{
			// utilising boundary mask since it doesn't serve a purpose anymore
			boundsMask = simd_int::greaterThanOrEqualSigned(lowBoundIndices, index)
				| simd_int::greaterThanOrEqualSigned(index, highBoundIndices);
			destination.writeSIMDValueAt((source.getSIMDValueAt(0, index) & boundsMask) +
				(destination.getSIMDValueAt(0, index) & ~boundsMask), 0, index);

			index = (index + 1) % FFTSize;
		}
	}

	void contrastEffect::run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept
	{
		switch (moduleParameters_[0]->getInternalValue<u32>())
		{
		case static_cast<u32>(Framework::ContrastTypes::Contrast):
			runContrast(source, destination, FFTSize, sampleRate);
			break;
		default:
			break;
		}
	}

	EffectModule::EffectModule(u64 parentModuleId, std::string_view effectType) noexcept : PluginModule(parentModuleId, Framework::kPluginModules[3])
	{
		subModules_.reserve(1);
		if (effectType == Framework::kEffectModuleNames[0])
			subModules_.emplace_back(std::move(std::make_shared<utilityEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[1])
			subModules_.emplace_back(std::move(std::make_shared<filterEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[2])
			subModules_.emplace_back(std::move(std::make_shared<contrastEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[3])
			subModules_.emplace_back(std::move(std::make_shared<dynamicsEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[4])
			subModules_.emplace_back(std::move(std::make_shared<phaseEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[5])
			subModules_.emplace_back(std::move(std::make_shared<pitchEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[6])
			subModules_.emplace_back(std::move(std::make_shared<stretchEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[7])
			subModules_.emplace_back(std::move(std::make_shared<warpEffect>(moduleId_)));
		else if (effectType == Framework::kEffectModuleNames[8])
			subModules_.emplace_back(std::move(std::make_shared<destroyEffect>(moduleId_)));
		else COMPLEX_ASSERT(false && "You're inserting a non-Effect into an EffectModule");
		
		AllModules::addModule(subModules_.back());

		moduleParameters_.data.reserve(Framework::effectModuleParameterList.size());
		createModuleParameters(Framework::effectModuleParameterList.data(), Framework::effectModuleParameterList.size());
	}

	bool EffectModule::insertSubModule([[maybe_unused]] u32 index, std::string_view moduleType) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		std::shared_ptr<PluginModule> newEffect;

		if (moduleType == Framework::kEffectModuleNames[0])
			newEffect = std::make_shared<utilityEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[1])
			newEffect = std::make_shared<filterEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[2])
			newEffect = std::make_shared<contrastEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[3])
			newEffect = std::make_shared<dynamicsEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[4])
			newEffect = std::make_shared<phaseEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[5])
			newEffect = std::make_shared<pitchEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[6])
			newEffect = std::make_shared<stretchEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[7])
			newEffect = std::make_shared<warpEffect>(moduleId_);
		else if (moduleType == Framework::kEffectModuleNames[8])
			newEffect = std::make_shared<destroyEffect>(moduleId_);
		else COMPLEX_ASSERT(false && "You're inserting a non-Effect into an EffectModule");

		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		// triggering destructor here so that we don't have to potentially expand
		newEffect->~PluginModule();
		AllModules::addModule(subModules_.back());
		currentlyUsing_.store(0, std::memory_order_release);

		return true;
	}

	bool EffectModule::copySubModule(const std::shared_ptr<PluginModule> &newSubModule, [[maybe_unused]] u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;
		
		std::shared_ptr<PluginModule> newEffect = std::make_shared<PluginModule>(*newSubModule, moduleId_);
		
		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		// triggering destructor here so that we don't have to potentially expand
		newEffect->~PluginModule();
		AllModules::addModule(subModules_.back());
		currentlyUsing_.store(0, std::memory_order_release);
		
		return true;
	}

	bool EffectModule::moveSubModule(std::shared_ptr<PluginModule> newSubModule, [[maybe_unused]] u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		std::shared_ptr<PluginModule> newEffect = std::make_shared<PluginModule>(std::move(*newSubModule), moduleId_);

		utils::spinAndLock(currentlyUsing_, 0, 1);
		subModules_[0].swap(newEffect);
		// triggering destructor here so that we don't have to potentially expand
		newEffect->~PluginModule();
		AllModules::addModule(subModules_.back());
		currentlyUsing_.store(0, std::memory_order_release);
		
		return true;
	}

	void EffectModule::processEffect(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate)
	{
		if (!moduleParameters_[0]->getInternalValue<u32>())
		{
			source.swap(destination);
			return;
		}

		// by design this shouldn't even wait because change of effect happens after all processing is done
		// this here is simply a sanity check
		utils::spinAndLock(currentlyUsing_, 0, 1);

		static_cast<baseEffect *>(subModules_[0].get())->run(source, destination, FFTSize, sampleRate);

		// if the mix is 100% for all channels, we can skip mixing entirely
		simd_float wetMix = moduleParameters_[2]->getInternalValue<simd_float>();
		simd_mask wetMask = simd_float::notEqual(1.0f, wetMix);
		if (wetMask.sum() != 0) 
		{
			simd_float dryMix = simd_float(1.0f) - wetMix;
			for (u32 i = 0; i < FFTSize; i++)
			{
				destination.writeSIMDValueAt(simd_float::mulAdd(
					dryMix * source.getSIMDValueAt(0, i),
					wetMix, destination.getSIMDValueAt(0, i)), 0, i);
			}
		}

		// if there's no gain change for any channels, we can skip it entirely
		simd_float gain = moduleParameters_[3]->getInternalValue<simd_float>();
		simd_mask gainMask = simd_float::notEqual(0.0f, gain);
		if (gainMask.sum() != 0)
		{
			simd_float magnitude = utils::dbToAmplitude(gain);
			for (u32 i = 0; i < FFTSize; i++)
				destination.multiply(magnitude, 0, i);
		}

		currentlyUsing_.store(0, std::memory_order_release);
	}
}