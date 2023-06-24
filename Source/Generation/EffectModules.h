/*
	==============================================================================

		EffectModules.h
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <algorithm>
#include "Framework/common.h"
#include "Framework/spectral_support_functions.h"
#include "Framework/simd_buffer.h"
#include "Framework/parameters.h"
#include "Framework/parameter_value.h"
#include "BaseProcessor.h"

namespace Interface
{
	class EffectModuleSection;
}

namespace Generation
{
	struct ComplexDataSource
	{
		// scratch buffer for conversion between cartesian and polar data
		Framework::SimdBuffer<std::complex<float>, simd_float> conversionBuffer{ kNumChannels, kMaxFFTBufferLength };
		// is the data in sourceBuffer polar or cartesian
		bool isPolar = false;
		Framework::SimdBufferView<std::complex<float>, simd_float> sourceBuffer;
	};

	// base class for the actual effects
	class baseEffect : public BaseProcessor
	{
	public:
		baseEffect() = delete;
		baseEffect(const baseEffect &) = delete;
		baseEffect(baseEffect &&) = delete;

		baseEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId, std::string_view effectType) :
			BaseProcessor(globalModulesState, parentModuleId, effectType)
		{ createProcessorParameters(Framework::baseEffectParameterList.data(), Framework::baseEffectParameterList.size()); }

		baseEffect(const baseEffect &other, u64 parentModuleId) noexcept : BaseProcessor(other, parentModuleId) { }
		baseEffect &operator=(const baseEffect &other) noexcept
		{ BaseProcessor::operator=(other); return *this; }

		baseEffect(baseEffect &&other, u64 parentModuleId) noexcept : BaseProcessor(std::move(other), parentModuleId) { }
		baseEffect &operator=(baseEffect &&other) noexcept
		{ BaseProcessor::operator=(std::move(other)); return *this; }

		// Inherited via BaseProcessor
		[[nodiscard]] BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept override
		{ return makeSubProcessor<baseEffect>(*this, parentModuleId.value_or(parentProcessorId_)); }
		[[nodiscard]] BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override
		{ COMPLEX_ASSERT(false && "This type cannot have subProcessors"); return nullptr; }

		virtual void run([[maybe_unused]] Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			[[maybe_unused]] Framework::SimdBuffer<std::complex<float>, simd_float> &destination,
			[[maybe_unused]] u32 effectiveFFTSize, [[maybe_unused]] float sampleRate) noexcept { }

		virtual bool needsPolarData() const noexcept { return false; }
		auto getEffectMode() const { return processorParameters_[(u32)Framework::BaseEffectParameters::Mode]->getInternalValue<u32>(); }

	protected:
		enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

		void setEffectModeStrings(Framework::EffectTypes type) const
		{
			auto strings = Framework::Parameters::getEffectModesStrings(type);
			auto modeIndex = (u32)Framework::BaseEffectParameters::Mode;
			auto details = Framework::baseEffectParameterList[modeIndex];
			details.stringLookup = strings;
			processorParameters_[modeIndex]->setParameterDetails(details);
		}

		// first - low shifted boundary, second - high shifted boundary
		[[nodiscard]] std::pair<simd_float, simd_float> getShiftedBounds(BoundRepresentation representation,
			float maxFrequency, u32 FFTSize, bool isLinearShift = false) const noexcept;

		[[nodiscard]] simd_mask vector_call isOutsideBounds(simd_int positionIndices, 
			simd_int lowBoundIndices, simd_int highBoundIndices) const noexcept
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsGreaterThanHigh = simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsLessThanLow = simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsGreaterThanHigh | positionsLessThanLow)) |
				(~highAboveLow & (positionsGreaterThanHigh & positionsLessThanLow));
		}

		[[nodiscard]] simd_mask vector_call isInsideBounds(simd_int positionIndices, 
			simd_int lowBoundIndices, simd_int highBoundIndices) const noexcept
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsLessOrThanEqualHigh = ~simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsGreaterOrThanEqualLow = ~simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsLessOrThanEqualHigh & positionsGreaterOrThanEqualLow)) |
				(~highAboveLow & (positionsLessOrThanEqualHigh | positionsGreaterOrThanEqualLow));
		}

		// returns starting point and distance to end of processed/unprocessed range
		std::pair<u32, u32> vector_call getRange(simd_int lowIndices, simd_int highIndices,
			u32 effectiveFFTSize, bool isProcessedRange) const noexcept;

		void copyUnprocessedData(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const simd_int &lowBoundIndices,
			const simd_int &highBoundIndices, u32 effectiveFFTSize) const noexcept;

		//// Parameters
		//
		// 1. internal fx type (mono) - [0, n]
		// 2. and 3. normalised frequency boundaries of processed region (stereo) - [0.0f, 1.0f]
		// 4. shifting of the freq boundaries (stereo) - [-1.0f; 1.0f]
	};

	class utilityEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{A8129602-D351-46D5-B85B-D5764C071575}")

		utilityEffect() = delete;
		utilityEffect(const utilityEffect &) = delete;
		utilityEffect(utilityEffect &&) = delete;

		utilityEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		void run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept override
		{
			baseEffect::run(source, destination, effectiveFFTSize, sampleRate);
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
		DEFINE_CLASS_TYPE("{809BD1B8-AA18-467E-8DD2-E396F70D6253}")

		filterEffect() = delete;
		filterEffect(const filterEffect &) = delete;
		filterEffect(filterEffect &&) = delete;

		filterEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		void run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept override;

	private:
		void runNormal(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) const noexcept;

		simd_float vector_call getDistancesFromCutoffs(simd_int positionIndices,
			simd_int cutoffIndices, simd_int lowBoundIndices, u32 FFTSize, float sampleRate) const noexcept;

		//// Parameters
		// 
		// 5. gain (stereo) - range[-100.0 db, 100.0 db]; lowers the loudness of the cutoff/around the cutoff point for negative/positive values 
		//	  at min/max values the bins around/outside the cutoff are zeroed 
		//	  *parameter values are interpreted linearly, so the control needs to have an exponential slope
		// 
		// 6. cutoff (stereo) - range[0.0f, 1.0f]; controls where the filtering starts
		//	  at 0.0f/1.0f it's at the low/high boundary
		//    *parameter values are interpreted linearly, so the control needs to have an exponential slope
		// 
		// 7. slope (stereo) - range[-1.0f, 1.0f]; controls the slope transition
		//	  at 0.0f it stretches from the cutoff to the frequency boundaries
		//	  at 1.0f only the center bin is left unaffected


		//// Modes
		// Normal - normal filtering
		// Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
		// TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
	};



	class dynamicsEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{D5DADD9A-5B0F-45C6-ADF5-B6A6415AF2D7}")

		dynamicsEffect() = delete;
		dynamicsEffect(const dynamicsEffect &) = delete;
		dynamicsEffect(dynamicsEffect &&) = delete;

		dynamicsEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		void run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept override;

	private:
		void runContrast(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) const noexcept;

		simd_float vector_call matchPower(simd_float target, simd_float current) const
		{
			simd_float result = 1.0f;
			result = utils::maskLoad(result, simd_float(0.0f), simd_float::greaterThanOrEqual(0.0f, target));
			result = utils::maskLoad(result, simd_float::sqrt(target / current), simd_float::greaterThan(current, 0.0f));

			result = utils::maskLoad(result, simd_float(1.0f), simd_float::greaterThan(result, 1e30f));
			result = utils::maskLoad(result, simd_float(0.0f), simd_float::greaterThan(1e-37f, result));
			return result;
		}


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

		// specops noise filter/focus, thinner
		// spectral compander, gate (threshold), clipping
	};



	class phaseEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{5670932B-8B6F-4475-9926-000F6C36C5AD}")

		phaseEffect() = delete;
		phaseEffect(const phaseEffect &) = delete;
		phaseEffect(phaseEffect &&) = delete;

		phaseEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		// phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
		// phase filter - filtering based on phase
		// mfreeformphase impl
	};



	class pitchEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{71133386-9421-4B23-91F9-C826DFC506B8}")

		pitchEffect() = delete;
		pitchEffect(const pitchEffect &) = delete;
		pitchEffect(pitchEffect &&) = delete;

		pitchEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		// resample, shift, const shift, harmonic shift, harmonic repitch
	};



	class stretchEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{D700C4AA-EC95-4703-9837-7AD5BDF5C810}")

		stretchEffect() = delete;
		stretchEffect(const stretchEffect &) = delete;
		stretchEffect(stretchEffect &&) = delete;

		stretchEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		// specops geometry
	};



	class warpEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{5FC3802A-B916-4D36-A853-78A29A5F5687}")

		warpEffect() = delete;
		warpEffect(const warpEffect &) = delete;
		warpEffect(warpEffect &&) = delete;

		warpEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		// vocode, harmonic match, cross/warp mix
	};



	class destroyEffect final : public baseEffect
	{
	public:
		DEFINE_CLASS_TYPE("{EA1DD088-A73A-4FD4-BB27-38EC0BF91850}")

		destroyEffect() = delete;
		destroyEffect(const destroyEffect &) = delete;
		destroyEffect(destroyEffect &&) = delete;

		destroyEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		// resize, specops effects category
	};

	// TODO: freezer and glitcher classes


	static constexpr std::array effectsTypes = { utilityEffect::getClassType(), filterEffect::getClassType(),
		dynamicsEffect::getClassType(), phaseEffect::getClassType(), pitchEffect::getClassType(),
		stretchEffect::getClassType(), warpEffect::getClassType(), destroyEffect::getClassType(), };

	// class for the whole FX unit
	class EffectModule : public BaseProcessor
	{
	public:
		DEFINE_CLASS_TYPE("{763F9D86-D535-4D63-B486-F863F88CC259}")

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

		void processEffect(ComplexDataSource &source, u32 effectiveFFTSize, float sampleRate);

		// Inherited via BaseProcessor
		BaseProcessor *updateSubProcessor(size_t index, BaseProcessor *newSubModule) noexcept override;
		[[nodiscard]] BaseProcessor *createSubProcessor(std::string_view type) const noexcept override;

	private:
		[[nodiscard]] BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept override
		{ return makeSubProcessor<EffectModule>(*this, parentModuleId.value_or(parentProcessorId_)); }
		baseEffect *createEffect(std::string_view type) const noexcept;

		Framework::SimdBuffer<std::complex<float>, simd_float> buffer_{ kNumChannels, kMaxFFTBufferLength };

		//// Parameters
		//  
		// 1. module enabled
		// 2. effect type
		// 3. module mix
	};
}
