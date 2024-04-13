/*
	==============================================================================

		EffectModules.h
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/constants.h"
#include "Framework/simd_buffer.h"
#include "BaseProcessor.h"

namespace Generation
{
	// base class for the actual effects
	class baseEffect : public BaseProcessor
	{
	public:
		baseEffect() = delete;
		baseEffect(const baseEffect &) = delete;
		baseEffect(baseEffect &&) = delete;

		baseEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId, std::string_view effectType);

		baseEffect(const baseEffect &other, u64 parentModuleId) noexcept : BaseProcessor(other, parentModuleId) { }
		baseEffect &operator=(const baseEffect &other) noexcept = default;

		baseEffect(baseEffect &&other, u64 parentModuleId) noexcept : BaseProcessor(std::move(other), parentModuleId) { }
		baseEffect &operator=(baseEffect &&other) noexcept = default;

		virtual void run([[maybe_unused]] Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			[[maybe_unused]] Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination,
			[[maybe_unused]] u32 binCount, [[maybe_unused]] float sampleRate) noexcept { }

		virtual Framework::ComplexDataSource::DataSourceType neededDataType() const noexcept { return Framework::ComplexDataSource::Cartesian; }

	protected:
		[[nodiscard]] BaseProcessor *createCopy([[maybe_unused]] std::optional<u64> parentModuleId) const noexcept override
		{ COMPLEX_ASSERT_FALSE("This type cannot be copied directly, you need a derived type"); return nullptr; }
		[[nodiscard]] BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override
		{ COMPLEX_ASSERT_FALSE("This type cannot have subProcessors"); return nullptr; }

		enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

		// first - low shifted boundary, second - high shifted boundary
		[[nodiscard]] std::pair<simd_float, simd_float> getShiftedBounds(BoundRepresentation representation,
			float sampleRate, u32 FFTSize, bool isLinearShift = false) const noexcept;

		[[nodiscard]] static strict_inline simd_mask vector_call isOutsideBounds(
			simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
		{
			simd_mask highAboveLow = simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices);
			simd_mask belowLowBounds = simd_int::lessThanSigned(positionIndices, lowBoundIndices);
			simd_mask aboveHighBounds = simd_int::greaterThanSigned(positionIndices, highBoundIndices);

			// 1st case example:  |   *  [    ]     | or
			//                    |      [    ]  *  |
			// 2nd case examples: |     ]   *  [    |
			return (belowLowBounds | aboveHighBounds) & ((belowLowBounds & aboveHighBounds) ^ highAboveLow);
		}

		[[nodiscard]] static strict_inline simd_mask vector_call isInsideBounds(
			simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
		{ return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices); }

		// returns starting point and distance to end of processed/unprocessed range
		static std::pair<u32, u32> vector_call minimiseRange(simd_int lowIndices, simd_int highIndices,
			u32 binCount, bool isProcessedRange);

		static void copyUnprocessedData(Framework::SimdBufferView<Framework::complex<float>, simd_float> source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, simd_int lowBoundIndices,
			simd_int highBoundIndices, u32 binCount) noexcept;

		template<typename Type>
		friend void fillAndSetParameters(baseEffect &effect);

		//// Parameters
		//
		// 1. internal fx type (mono) - [0, n]
		// 2. and 3. normalised frequency boundaries of processed region (stereo) - [0.0f, 1.0f]
		// 4. shifting of the freq boundaries (stereo) - [-1.0f; 1.0f]
	};

	class utilityEffect final : public baseEffect
	{
	public:
		utilityEffect() = delete;
		utilityEffect(const utilityEffect &) = delete;
		utilityEffect(utilityEffect &&) = delete;

		utilityEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		void run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override
		{
			baseEffect::run(source, destination, binCount, sampleRate);
		}

	private:
		//// Parameters
		// 
		// 5. channel pan (stereo) - [-1.0f, 1.0f]
		// 6. flip phases (stereo) - [0, 1]
		// 7. reverse spectrum bins (stereo) - [0, 1]

		// TODO:
		// idea: mix 2 input signals (left and right/right and left channels)
		// idea: flip the phases, panning
	};

	class filterEffect final : public baseEffect
	{
	public:
		filterEffect() = delete;
		filterEffect(const filterEffect &) = delete;
		filterEffect(filterEffect &&) = delete;

		filterEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		void run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override;

	private:
		void runNormal(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		void runPhase(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		static simd_float vector_call getDistancesFromCutoffs(simd_int positionIndices,
			simd_int cutoffIndices, simd_int lowBoundIndices, u32 FFTSize, float sampleRate);

		//// Modes
		// 
		// Normal - normal filtering
		// 5. gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
		//    lowers the loudness at/around the cutoff point for negative/positive values
		//    *parameter values are interpreted linearly, so the control needs to have an exponential slope
		// 
		// 6. cutoff (stereo) - range[0.0f, 1.0f]; controls where the filtering starts
		//    at 0.0f/1.0f it's at the low/high boundary
		//    *parameter values are interpreted linearly, so the control needs to have an exponential slope
		// 
		// 7. slope (stereo) - range[-1.0f, 1.0f]; controls the slope transition
		//    at 0.0f it stretches from the cutoff to the frequency boundaries
		//    at 1.0f only the center bin is left unaffected
		// 
		// 
		// Threshold - frequency gating
		// 5. threshold (stereo) - range[0%, 100%]; threshold is relative to the loudest bin in the spectrum
		// 
		// 6. gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
		//    positive/negative values lower the loudness of the bins below/above the threshold
		// 
		// 7. tilt (stereo) - range[-100%, +100%]; tilt relative to the loudest bin in the spectrum
		//    at min/max values the left/right-most part of the masked spectrum will have no threshold
		// 
		// 8. delta - instead of the absolute loudness it uses the averaged loudness change from the 2 neighbouring bins
		// 
		// Tilt filter
		// Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
		// Regular key tracked - regular filtering based on fundamental frequency (like dtblkfx autoharm)
		// 
		// TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
	};



	class dynamicsEffect final : public baseEffect
	{
	public:
		dynamicsEffect() = delete;
		dynamicsEffect(const dynamicsEffect &) = delete;
		dynamicsEffect(dynamicsEffect &&) = delete;

		dynamicsEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		void run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override;

	private:
		void runContrast(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		void runClip(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		static simd_float vector_call matchPower(simd_float target, simd_float current) noexcept;


		//// Dtblkfx Contrast
		/// Parameters
		//
		// 5. contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
		//
		/// Constants
		static constexpr float kContrastMaxPositiveValue = 4.0f;
		static constexpr float kContrastMaxNegativeValue = -0.5f;
		//
		// 
		//// Compressor
		//
		// Depth, Threshold, Ratio
		//

		// specops noise filter/focus, thinner
		// spectral compander, gate (threshold), clipping
	};



	class phaseEffect final : public baseEffect
	{
	public:
		phaseEffect() = delete;
		phaseEffect(const phaseEffect &) = delete;
		phaseEffect(phaseEffect &&) = delete;

		phaseEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		void run(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override;

		Framework::ComplexDataSource::DataSourceType neededDataType() const noexcept override { return Framework::ComplexDataSource::Polar; }

	private:
		void runShift(Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
			Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		// phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
		// phase filter - filtering based on phase
		// mfreeformphase impl
	};



	class pitchEffect final : public baseEffect
	{
	public:
		pitchEffect() = delete;
		pitchEffect(const pitchEffect &) = delete;
		pitchEffect(pitchEffect &&) = delete;

		pitchEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		// resample, shift, const shift, harmonic shift, harmonic repitch
	};



	class stretchEffect final : public baseEffect
	{
	public:
		stretchEffect() = delete;
		stretchEffect(const stretchEffect &) = delete;
		stretchEffect(stretchEffect &&) = delete;

		stretchEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		// specops geometry
	};



	class warpEffect final : public baseEffect
	{
	public:
		warpEffect() = delete;
		warpEffect(const warpEffect &) = delete;
		warpEffect(warpEffect &&) = delete;

		warpEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		// vocode, harmonic match, cross/warp mix
	};



	class destroyEffect final : public baseEffect
	{
	public:
		destroyEffect() = delete;
		destroyEffect(const destroyEffect &) = delete;
		destroyEffect(destroyEffect &&) = delete;

		destroyEffect(Plugin::ProcessorTree *processorTree, u64 parentModuleId);

		// resize, specops effects category
		// randomising bin positions
		// bin sorting - by amplitude, phase
		// TODO: freezer and glitcher classes
	};


	// class for the whole FX unit
	class EffectModule final : public BaseProcessor
	{
	public:
		EffectModule() = delete;
		EffectModule(const EffectModule &) = delete;
		EffectModule(EffectModule &&) = delete;

		EffectModule(Plugin::ProcessorTree *moduleTree, u64 parentModuleId, std::string_view effectType) noexcept;

		EffectModule(const EffectModule &other, u64 parentModuleId) noexcept : BaseProcessor(other, parentModuleId) { }
		EffectModule &operator=(const EffectModule &other)
		{ BaseProcessor::operator=(other); return *this; }

		EffectModule(EffectModule &&other, u64 parentModuleId) noexcept : BaseProcessor(std::move(other), parentModuleId) { }
		EffectModule &operator=(EffectModule &&other) noexcept
		{ BaseProcessor::operator=(std::move(other)); return *this; }

		void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate);

		// Inherited via BaseProcessor
		BaseProcessor *updateSubProcessor(size_t index, BaseProcessor *newSubModule) noexcept override;
		[[nodiscard]] BaseProcessor *createSubProcessor(std::string_view type) const noexcept override;
		[[nodiscard]] BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept override
		{ return makeSubProcessor<EffectModule>(*this, parentModuleId.value_or(parentProcessorId_)); }

		baseEffect *getEffect() const noexcept { return utils::as<baseEffect>(subProcessors_[0]); }
	private:
		Framework::SimdBuffer<Framework::complex<float>, simd_float> buffer_{ kNumChannels, kMaxFFTBufferLength };
	};
}
