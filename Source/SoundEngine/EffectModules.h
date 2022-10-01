/*
	==============================================================================

		EffectModules.h
		Created: 27 Jul 2021 12:30:19am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <algorithm>
#include "./Framework/common.h"
#include "./Framework/spectral_support_functions.h"
#include "./Framework/simd_buffer.h"
#include "./Framework/parameters.h"
#include "./Framework/parameter_value.h"
#include "PluginModule.h"

namespace Generation
{
	// base class for the actual effects
	class baseEffect : public PluginModule
	{
	public:
		baseEffect() = delete;
		baseEffect(const baseEffect &) = delete;
		baseEffect(baseEffect &&) = delete;

		baseEffect(AllModules *globalModulesState, u64 parentModuleId, std::string_view effectType) :
			PluginModule(globalModulesState, parentModuleId, Framework::kPluginModules[4]), effectType(effectType)
		{
			createModuleParameters(Framework::baseEffectParameterList.data(), Framework::baseEffectParameterList.size());
		}
		virtual ~baseEffect() override = default;

		baseEffect(const baseEffect &other, u64 parentModuleId) noexcept : 
			PluginModule(other, parentModuleId), effectType(other.effectType) {}
		baseEffect &operator=(const baseEffect &other) noexcept
		{
			PluginModule::operator=(other);
			effectType = other.effectType;
			return *this;
		}

		baseEffect(baseEffect &&other, u64 parentModuleId) noexcept : 
			PluginModule(std::move(other), parentModuleId), effectType(other.effectType) {}
		baseEffect &operator=(baseEffect &&other) noexcept
		{
			PluginModule::operator=(std::move(other));
			effectType = other.effectType;
			return *this;
		}

		auto getEffectType() const noexcept { return effectType; }

		virtual void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source, Framework::SimdBuffer<std::complex<float>, 
			simd_float> &destination, [[maybe_unused]] u32 effectiveFFTSize, [[maybe_unused]] float sampleRate) noexcept
		{ 
			// swapping pointers since we're not doing anything with the data
			source.swap(destination);
		};

	protected:
		enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

		// first - low shifted boundary, second - high shifted boundary
		std::pair<simd_float, simd_float> getShiftedBounds(BoundRepresentation representation,
			float maxFrequency, u32 FFTSize, bool isLinearShift = false) const noexcept;

		simd_mask vector_call isOutsideBounds(const simd_int &positionIndices, const simd_int &lowBoundIndices,
			const simd_int &highBoundIndices) const noexcept
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsGreaterThanHigh = simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsLessThanLow = simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsGreaterThanHigh | positionsLessThanLow)) |
				(~highAboveLow & (positionsGreaterThanHigh & positionsLessThanLow));
		}

		simd_mask vector_call isInsideBounds(const simd_int &positionIndices, const simd_int &lowBoundIndices,
			const simd_int &highBoundIndices) const noexcept
		{
			simd_mask highAboveLow = simd_int::greaterThanSigned(highBoundIndices, lowBoundIndices);
			simd_mask positionsLessOrThanEqualHigh = ~simd_int::greaterThanSigned(positionIndices, highBoundIndices);
			simd_mask positionsGreaterOrThanEqualLow = ~simd_int::greaterThanSigned(lowBoundIndices, positionIndices);
			return (highAboveLow & (positionsLessOrThanEqualHigh & positionsGreaterOrThanEqualLow)) |
				(~highAboveLow & (positionsLessOrThanEqualHigh | positionsGreaterOrThanEqualLow));
		}

		// returns starting point and distance to end of processed/unprocessed range
		std::pair<u32, u32> vector_call getRange(const simd_int &lowIndices, const simd_int &highIndices,
			u32 effectiveFFTSize, bool isProcessedRange) const noexcept;

		void copyUnprocessedData(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const simd_int &lowBoundIndices,
			const simd_int &highBoundIndices, u32 effectiveFFTSize) const noexcept;

		//// Parameters
		//
		// 1. internal fx type (mono) - [0, n]
		// 2. and 3. normalised frequency boundaries of processed region (stereo) - [0.0f, 1.0f]
		// 4. shifting of the freq boundaries (stereo) - [-1.0f; 1.0f]

		std::string_view effectType{};
};

	class utilityEffect : public baseEffect
	{
	public:
		utilityEffect() = delete;
		utilityEffect(const utilityEffect &) = delete;
		utilityEffect(utilityEffect &&) = delete;

		utilityEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[0])
		{
			
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
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
		filterEffect() = delete;
		filterEffect(const filterEffect &) = delete;
		filterEffect(filterEffect &&) = delete;

		filterEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[1])
		{
			auto parameterSize = Framework::baseEffectParameterList.size() + 
				Framework::filterEffectParameterList.size();
			moduleParameters_.data.reserve(parameterSize);
			createModuleParameters(Framework::filterEffectParameterList.data(), Framework::filterEffectParameterList.size());
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept override;

	private:
		void runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const u32 effectiveFFTSize, const float sampleRate) noexcept;

		simd_float vector_call getDistancesFromCutoffs(const simd_int &positionIndices,
			const simd_int &cutoffIndices, const simd_int &lowBoundIndices, u32 FFTSize, float sampleRate) const noexcept;

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
		// Regular - triangles, squares, saws, pointy, sweep and custom
		// TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
	};

	class contrastEffect final : public baseEffect
	{
	public:
		contrastEffect() = delete;
		contrastEffect(const contrastEffect &) = delete;
		contrastEffect(contrastEffect &&) = delete;

		contrastEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[2])
		{
			auto parameterSize = Framework::baseEffectParameterList.size() +
				Framework::contrastEffectParameterList.size();
			moduleParameters_.data.reserve(parameterSize);
			createModuleParameters(Framework::contrastEffectParameterList.data(), Framework::contrastEffectParameterList.size());
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept override;

	private:
		void runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate) noexcept;

		simd_float vector_call matchPower(simd_float target, simd_float current)
		{
			simd_float result = 1.0f;
			result = utils::maskLoad(result, simd_float(0.0f), simd_float::greaterThanOrEqual(0.0f, target));
			result = utils::maskLoad(result, simd_float::sqrt(target / current), simd_float::greaterThan(current, 0.0f));

			result = utils::maskLoad(result, simd_float(1.0f), simd_float::greaterThan(result, 1e30f));
			result = utils::maskLoad(result, simd_float(0.0f), simd_float::greaterThan(1e-37f, result));
			return result;
		}

		//// Parameters
		//
		// 5. contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
		 
		
		// dtblkfx contrast, specops noise filter/focus, thinner

		static constexpr float kMaxPositiveValue = 4.0f;
		static constexpr float kMaxNegativeValue = -0.5f;
	};

	class dynamicsEffect : public baseEffect
	{
	public:
		dynamicsEffect() = delete;
		dynamicsEffect(const dynamicsEffect &) = delete;
		dynamicsEffect(dynamicsEffect &&) = delete;

		dynamicsEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[3])
		{

		}

		// spectral compander, gate (threshold), clipping
	};

	class phaseEffect : public baseEffect
	{
	public:
		phaseEffect() = delete;
		phaseEffect(const phaseEffect &) = delete;
		phaseEffect(phaseEffect &&) = delete;

		phaseEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[4])
		{

		}

		// phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
	};

	class pitchEffect : public baseEffect
	{
	public:
		pitchEffect() = delete;
		pitchEffect(const pitchEffect &) = delete;
		pitchEffect(pitchEffect &&) = delete;

		pitchEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[5])
		{

		}

		// resample, shift, const shift, harmonic shift, harmonic repitch
	};

	class stretchEffect : public baseEffect
	{
	public:
		stretchEffect() = delete;
		stretchEffect(const stretchEffect &) = delete;
		stretchEffect(stretchEffect &&) = delete;

		stretchEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[6])
		{

		}

		// specops geometry
	};

	class warpEffect : public baseEffect
	{
	public:
		warpEffect() = delete;
		warpEffect(const warpEffect &) = delete;
		warpEffect(warpEffect &&) = delete;

		warpEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[7])
		{

		}

		// vocode, harmonic match, cross/warp mix
	};

	class destroyEffect : public baseEffect
	{
	public:
		destroyEffect() = delete;
		destroyEffect(const destroyEffect &) = delete;
		destroyEffect(destroyEffect &&) = delete;

		destroyEffect(AllModules *globalModulesState, u64 parentModuleId) : 
			baseEffect(globalModulesState, parentModuleId, Framework::kEffectModuleNames[8])
		{

		}

		// resize, specops effects category
	};

	//TODO: freezer and glitcher classes


	// class for the whole FX unit
	class EffectModule : public PluginModule
	{
	public:
		EffectModule() = delete;
		EffectModule(const EffectModule &) = delete;
		EffectModule(EffectModule &&) = delete;

		EffectModule(AllModules *globalModulesState, u64 parentModuleId, std::string_view effectType) noexcept;

		EffectModule(const EffectModule &other, u64 parentModuleId) noexcept : PluginModule(other, parentModuleId)
		{ COMPLEX_ASSERT(other.moduleType_ == Framework::kPluginModules[3] && "You're trying to copy a non-EffectModule into EffectModule"); }
		EffectModule &operator=(const EffectModule &other)
		{ PluginModule::operator=(other); return *this; }

		EffectModule(EffectModule &&other, u64 parentModuleId) noexcept : PluginModule(std::move(other), parentModuleId)
		{ COMPLEX_ASSERT(moduleType_ == Framework::kPluginModules[3] && "You're trying to move a non-EffectModule into EffectModule"); }
		EffectModule &operator=(EffectModule &&other) noexcept
		{ PluginModule::operator=(std::move(other)); return *this; }

		~EffectModule() noexcept { PluginModule::~PluginModule(); }

		bool insertSubModule(u32 index, std::string_view moduleType) noexcept override;
		bool copySubModule(const std::shared_ptr<PluginModule> &newSubModule, u32 index) noexcept override;
		bool moveSubModule(std::shared_ptr<PluginModule> newSubModule, u32 index) noexcept override;

		void processEffect(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 effectiveFFTSize, float sampleRate);

	private:
		//// Parameters
		//  
		// 1. module enabled
		// 2. effect type
		// 3. module mix
		// 4. module gain

		static constexpr float kMaxPositiveGain = utils::dbToMagnitudeConstexpr(30.0f);
		static constexpr float kMaxNegativeGain = utils::dbToMagnitudeConstexpr(-30.0f);
	};
}
