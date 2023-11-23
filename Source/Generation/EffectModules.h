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
#include "Framework/simd_buffer.h"
#include "Framework/parameters.h"
#include "Framework/parameter_value.h"
#include "BaseProcessor.h"
#include "Plugin/ProcessorTree.h"

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
		{ createProcessorParameters<Framework::BaseProcessors::BaseEffect::type>(); }

		baseEffect(const baseEffect &other, u64 parentModuleId) noexcept :
			BaseProcessor(other, parentModuleId) { }
		baseEffect &operator=(const baseEffect &other) noexcept
		{
			if (this != &other)
			{
				
				BaseProcessor::operator=(other);
			}
			return *this;
		}

		baseEffect(baseEffect &&other, u64 parentModuleId) noexcept :
			BaseProcessor(std::move(other), parentModuleId) { }
		baseEffect &operator=(baseEffect &&other) noexcept
		{
			if (this != &other)
			{

				BaseProcessor::operator=(std::move(other));
			}
			return *this;
		}

		virtual void run([[maybe_unused]] Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			[[maybe_unused]] Framework::SimdBuffer<std::complex<float>, simd_float> &destination,
			[[maybe_unused]] u32 binCount, [[maybe_unused]] float sampleRate) noexcept { }

		virtual bool needsPolarData() const noexcept { return false; }
		template<nested_enum::NestedEnum E>
		E getEffectAlgorithm() const
		{ return E::make_enum(getParameter(Framework::BaseProcessors::BaseEffect::Algorithm::self())->getInternalValue<u32>()).value(); }

		/*template<commonConcepts::Enum E>
		void updateParameterDetails()
		{
			using namespace Framework;
			constexpr auto enumValues = magic_enum::enum_values<E>();
			auto updateParameters = [&]()
			{
				for (size_t i = 0; i < enumValues.size(); i++)
				{
					ParameterDetails details = Parameters::getDetails<E>(enumValues[i]);
					processorParameters_[i + magic_enum::enum_count<BaseEffectParameters>()]->setParameterDetails(details);
					processorParameters_.updateKey(i + magic_enum::enum_count<BaseEffectParameters>(), 
						Parameters::getEnumString(enumValues[i]));
				}
			};
			getProcessorTree()->executeOutsideProcessing(updateParameters);
		}*/

	protected:
		[[nodiscard]] BaseProcessor *createCopy([[maybe_unused]] std::optional<u64> parentModuleId) const noexcept override
		{ COMPLEX_ASSERT_FALSE("This type cannot be copied directly, you need a derived type"); return nullptr; }
		[[nodiscard]] BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override
		{ COMPLEX_ASSERT_FALSE("This type cannot have subProcessors"); return nullptr; }

		enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

		// helper to set the create
		template<nested_enum::NestedEnum Type>
		void fillAndSetParameters()
		{
			using namespace Framework;

			auto *parameter = getParameter(BaseProcessors::BaseEffect::Algorithm::self());
			auto details = parameter->getParameterDetails();
			details.stringLookup = Parameters::getEffectModesStrings(Type::self());
			parameter->setParameterDetails(details);
			
			utils::applyOne([&]<typename T>(T &&) { createProcessorParameters<typename std::remove_cvref_t<T>::type>(); }, Type::template enum_subtypes<nested_enum::InnerNodes>());
		}

		// first - low shifted boundary, second - high shifted boundary
		[[nodiscard]] std::pair<simd_float, simd_float> getShiftedBounds(BoundRepresentation representation,
			float sampleRate, u32 FFTSize, bool isLinearShift = false) const noexcept;

		[[nodiscard]] static strict_inline simd_mask vector_call isOutsideBounds(
			simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsGreaterThanHigh = simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsLessThanLow = simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsGreaterThanHigh | positionsLessThanLow)) |
				(~highAboveLow & (positionsGreaterThanHigh & positionsLessThanLow));
		}

		[[nodiscard]] static strict_inline simd_mask vector_call isInsideBounds(
			simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsLessOrThanEqualHigh = ~simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsGreaterOrThanEqualLow = ~simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsLessOrThanEqualHigh & positionsGreaterOrThanEqualLow)) |
				(~highAboveLow & (positionsLessOrThanEqualHigh | positionsGreaterOrThanEqualLow));
		}

		// returns starting point and distance to end of processed/unprocessed range
		static std::pair<u32, u32> vector_call minimiseRange(simd_int lowIndices, simd_int highIndices,
			u32 binCount, bool isProcessedRange);

		static void copyUnprocessedData(const Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const simd_int &lowBoundIndices,
			const simd_int &highBoundIndices, u32 effectiveFFTSize) noexcept;


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
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override
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
		DEFINE_CLASS_TYPE("{809BD1B8-AA18-467E-8DD2-E396F70D6253}")

		filterEffect() = delete;
		filterEffect(const filterEffect &) = delete;
		filterEffect(filterEffect &&) = delete;

		filterEffect(Plugin::ProcessorTree *globalModulesState, u64 parentModuleId);

		void run(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override;

	private:
		void runNormal(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

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
		// 
		// Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
		// 
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
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) noexcept override;

	private:
		void runContrast(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		void runClip(Framework::SimdBufferView<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

		static simd_float vector_call matchPower(simd_float target, simd_float current) noexcept
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
		// randomising bin positions
		// bin sorting - by amplitude, phase
		// TODO: freezer and glitcher classes
	};


	static constexpr std::array effectsTypes = { utilityEffect::getClassType(), filterEffect::getClassType(),
		dynamicsEffect::getClassType(), phaseEffect::getClassType(), pitchEffect::getClassType(),
		stretchEffect::getClassType(), warpEffect::getClassType(), destroyEffect::getClassType(), };

	// class for the whole FX unit
	class EffectModule final : public BaseProcessor
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
		[[nodiscard]] BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept override
		{ return makeSubProcessor<EffectModule>(*this, parentModuleId.value_or(parentProcessorId_)); }

		baseEffect *getEffect() const noexcept { return utils::as<baseEffect *>(subProcessors_[0]); }
	private:
		baseEffect *createEffect(std::string_view type) const noexcept;

		Framework::SimdBuffer<std::complex<float>, simd_float> buffer_{ kNumChannels, kMaxFFTBufferLength };

		//// Parameters
		//  
		// 1. module enabled
		// 2. effect type
		// 3. module mix
	};
}
