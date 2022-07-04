/*
	==============================================================================

		EffectModules.cpp
		Created: 27 Jul 2021 12:30:19am
		Author:  Lenovo

	==============================================================================
*/

#include "EffectModules.h"

namespace Generation
{
	perf_inline void filterEffect::runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept
	{
		using namespace utils;

		auto shiftedBoundaries = getShiftedBoundaries(lowBoundaryParameter_,
			highBoundaryParameter_, sampleRate / 2.0f, isLinearShiftParameter_);

		// getting the boundaries in terms of bin position
		simd_int lowBoundaryIndices = utils::ceilToInt((shiftedBoundaries.first / (sampleRate / 2.0f)) * FFTSize);
		simd_int highBoundaryIndices = utils::floorToInt((shiftedBoundaries.second / (sampleRate / 2.0f)) * FFTSize);

		// getting the distances between the boundaries and which masks precede the others 
		simd_mask boundaryMask = simd_int::greaterThanOrEqualSigned(highBoundaryIndices, lowBoundaryIndices);

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundaryIndices, highBoundaryIndices, FFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// calculating the bins where the cutoff lies
		simd_int cutoffIndices = floorToInt(interpolate(simd_float(lowBoundaryIndices),
			simd_float(highBoundaryIndices), cutoffParameter));

		// if mask scalars are negative -> brickwall, if positive -> linear slope
		// clearing sign and calculating end of slope bins
		simd_float slopes = slopeParameter;
		simd_mask slopeMask = unsignFloat(slopes);
		slopes = ceilToInt(interpolate((float)FFTSize, 1.0f, slopes));

		// if mask scalars are negative -> attenuate at cutoff, if positive -> around cutoff
		// clearing sign (gains is the gain reduction, not the gain multiplier)
		simd_float gains = gainParameter;
		simd_mask gainMask = unsignFloat(gains);

		for (u32 i = 0; i < numBins; i++)
		{
			simd_int distancesFromCutoff = getDistancesFromCutoff(simd_int(index),
				cutoffIndices, boundaryMask, lowBoundaryIndices, FFTSize);

			// calculating linear slope and brickwall, both are ratio of the gain attenuation
			// the higher tha value the more it will be affected by it
			simd_float gainRatio = maskLoad(simd_float::clamp(0.0f, 1.0f, simd_float(distancesFromCutoff) / slopes),
				simd_float::min(floor(simd_float(distancesFromCutoff) / (slopes + 1.0f)), 1.0f),
				~slopeMask);
			gains = maskLoad(gains * gainRatio, gains * (simd_float(1.0f) - gainRatio), ~gainMask);

			// convert gain attenuation to gain multiplier and zeroing gains lower than lowest amplitude
			gains *= kLowestDb;
			gains = dbToMagnitude(gains);
			gains &= simd_float::greaterThan(gains, kLowestAmplitude);

			destination.writeSIMDValueAt(source.getSIMDValueAt(0, index) * gains, 0, index);

			index = (index + 1) % FFTSize;
		}

		auto unprocessedIndexAndRange = getRange(lowBoundaryIndices, highBoundaryIndices, FFTSize, false);
		index = unprocessedIndexAndRange.first;
		numBins = unprocessedIndexAndRange.second;

		// copying the unprocessed data
		for (u32 i = 0; i < numBins; i++)
		{
			// utilising boundary mask since it doesn't serve a purpose anymore
			boundaryMask = simd_int::greaterThanOrEqualSigned(lowBoundaryIndices, index)
				| simd_int::greaterThanOrEqualSigned(index, highBoundaryIndices);
			destination.writeSIMDValueAt((source.getSIMDValueAt(0, index) & boundaryMask) +
				(destination.getSIMDValueAt(0, index) & ~boundaryMask), 0, index);

			index = (index + 1) % FFTSize;
		}
	}

	perf_inline void contrastEffect::runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate)
	{
		using namespace utils;

		auto shiftedBoundaries = getShiftedBoundaries(lowBoundaryParameter_,
			highBoundaryParameter_, sampleRate / 2.0f, isLinearShiftParameter_);

		// getting the boundaries in terms of bin position
		simd_int lowBoundaryIndices = utils::ceilToInt((shiftedBoundaries.first / (sampleRate / 2.0f)) * FFTSize);
		simd_int highBoundaryIndices = utils::floorToInt((shiftedBoundaries.second / (sampleRate / 2.0f)) * FFTSize);

		// minimising the bins to iterate on
		auto processedIndexAndRange = getRange(lowBoundaryIndices, highBoundaryIndices, FFTSize, true);
		u32 index = processedIndexAndRange.first;
		u32 numBins = processedIndexAndRange.second;

		// calculating contrast, verbatim from dtblkfx
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

			bin = maskLoad(0.0f, bin, simd_float::greaterThan(min, magnitude));
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
		auto unprocessedIndexAndRange = getRange(lowBoundaryIndices, highBoundaryIndices, FFTSize, false);
		index = unprocessedIndexAndRange.first;
		numBins = unprocessedIndexAndRange.second;
		simd_mask boundaryMask;

		for (u32 i = 0; i < numBins; i++)
		{
			// utilising boundary mask since it doesn't serve a purpose anymore
			boundaryMask = simd_int::greaterThanOrEqualSigned(lowBoundaryIndices, index)
				| simd_int::greaterThanOrEqualSigned(index, highBoundaryIndices);
			destination.writeSIMDValueAt((source.getSIMDValueAt(0, index) & boundaryMask) +
				(destination.getSIMDValueAt(0, index) & ~boundaryMask), 0, index);

			index = (index + 1) % FFTSize;
		}
	}

	void EffectModule::processEffect(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate)
	{
		if (!isEnabledParameter_)
		{
			source.swap(destination);
			return;
		}

		// lock-free but may not be wait-free
		// the only reason we'd have to wait is if the allocation thread is swapping pointers,
		// however, i seriously doubt that will cause me to miss a block
		bool expected = false;
		while (isInUse.compare_exchange_weak(expected, true, std::memory_order_acq_rel));

		effect_->run(source, destination, FFTSize, sampleRate);

		isInUse.store(false, std::memory_order_release);
		isInUse.notify_one();

		simd_mask wetMask = simd_float::notEqual(1.0f, mixParameter_);
		simd_mask gainMask = simd_float::notEqual(0.0f, gainParameter_);

		if (wetMask.sum() != 0) 
		{
			simd_float dryMix = simd_float(1.0f) - mixParameter_;
			for (u32 i = 0; i < FFTSize; i++)
			{
				destination.writeSIMDValueAt(simd_float::mulAdd(
					dryMix * source.getSIMDValueAt(0, i),
					mixParameter_, destination.getSIMDValueAt(0, i)), 0, i);
			}
		}

		if (gainMask.sum() != 0)
		{
			simd_float gain = utils::dbToMagnitude(gainParameter_);
			for (u32 i = 0; i < FFTSize; i++)
				destination.multiply(gain, 0, i);
		}
	}
}



