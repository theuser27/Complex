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

		baseEffect(u64 parentModuleId, std::string_view effectType) : 
			PluginModule(parentModuleId, Framework::kPluginModules[4]), effectType(effectType)
		{
			createModuleParameters(Framework::baseEffectParameterList.data(), Framework::baseEffectParameterList.size());
		}
		virtual ~baseEffect() override = default;

		baseEffect(const baseEffect &other, u64 parentModuleId) noexcept : PluginModule(other, parentModuleId) {}
		baseEffect &operator=(const baseEffect &other) noexcept
		{ PluginModule::operator=(other); return *this; }

		baseEffect(baseEffect &&other, u64 parentModuleId) noexcept : PluginModule(std::move(other), parentModuleId) {}
		baseEffect &operator=(baseEffect &&other) noexcept
		{ PluginModule::operator=(std::move(other)); return *this; }

		virtual void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, [[maybe_unused]] u32 FFTSize, [[maybe_unused]] float sampleRate) noexcept
		{ 
			// swapping pointers since we're not doing anything with the data
			source.swap(destination);
		};

	protected:
		// returns starting point and distance to end of processed/unprocessed range
		std::pair<u32, u32> getRange(const simd_int &lowIndices, const simd_int &highIndices, 
			u32 FFTSize, bool isProcessedRange) const noexcept
		{
			u32 start, end;
			std::array<u32, kSimdRatio> startArray{};
			std::array<u32, kSimdRatio> endArray{};

			if (isProcessedRange)
			{
				startArray = lowIndices.getArrayOfValues();
				endArray = highIndices.getArrayOfValues();
			}
			else
			{
				startArray = highIndices.getArrayOfValues();
				endArray = lowIndices.getArrayOfValues();
			}

			start = *std::min_element(startArray.begin(), startArray.end());
			end = *std::max_element(endArray.begin(), endArray.end());

			return { start & (FFTSize - 1), (FFTSize + end - start) & (FFTSize - 1) };
		}

		// first - low shifted boundary, second - high shifted boundary
		std::pair<simd_float, simd_float> getShiftedBounds(simd_float lowBound,
			simd_float highBound, float maxFrequency, bool isLinearShift) const noexcept
		{
			using namespace utils;
			// TODO: do dynamic min frequency based on FFTOrder
			float maxOctave = log2f(maxFrequency / kMinFrequency);

			if (isLinearShift)
			{
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
				lowBound = exp2(simd_float::clamp(lowBound + boundShift, 0.0f, 1.0f) * maxOctave);
				highBound = exp2(simd_float::clamp(highBound + boundShift, 0.0f, 1.0f) * maxOctave);
				// snapping to 0 hz if it's below the minimum frequency
				lowBound = (lowBound & simd_float::greaterThan(lowBound, 1.0f)) * kMinFrequency;
				highBound = (highBound & simd_float::greaterThan(highBound, 1.0f)) * kMinFrequency;
			}
			return { lowBound, highBound };
		}

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

		utilityEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[0])
		{
			
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override
		{
			baseEffect::run(source, destination, FFTSize, sampleRate);
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

		filterEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[1])
		{
			auto parameterSize = Framework::baseEffectParameterList.size() + 
				Framework::filterEffectParameterList.size();
			moduleParameters_.data.reserve(parameterSize);
			createModuleParameters(Framework::filterEffectParameterList.data(), Framework::filterEffectParameterList.size());
		}
		~filterEffect() override = default;

		filterEffect(const filterEffect &other, u64 parentModuleId) noexcept : baseEffect(other, parentModuleId) {}
		filterEffect &operator=(const filterEffect &other) noexcept
		{ baseEffect::operator=(other); return *this; }

		filterEffect(filterEffect &&other, u64 parentModuleId) noexcept : baseEffect(std::move(other), parentModuleId) {}
		filterEffect &operator=(filterEffect &&other) noexcept
		{ baseEffect::operator=(std::move(other)); return *this; }

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override;

	private:
		void runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, const u32 FFTSize, const float sampleRate) noexcept;

		strict_inline simd_float vector_call getDistancesFromCutoffs(const simd_int &positionIndices, 
			const simd_int &cutoffIndices, const simd_int &lowBoundIndices, const u32 FFTOrder, const u32 FFTSize) const noexcept
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
			simd_int precedingIndices  = utils::maskLoad(cutoffIndices  , positionIndices, bothAboveOrBelowLowMask & cutoffAbovePositions);
			simd_int succeedingIndices = utils::maskLoad(positionIndices, cutoffIndices  , bothAboveOrBelowLowMask & cutoffAbovePositions);

			// masking for 2.
			// first 2 are when cutoffs/positions are above/below lowBound
			// second 2 are when positions/cutoffs are above/below lowBound
			precedingIndices  = utils::maskLoad(precedingIndices , cutoffIndices  , ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
			succeedingIndices = utils::maskLoad(succeedingIndices, positionIndices, ~bothAboveOrBelowLowMask & positionsBelowLowBoundAndCutoffsMask);
			precedingIndices  = utils::maskLoad(precedingIndices , positionIndices, ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);
			succeedingIndices = utils::maskLoad(succeedingIndices, cutoffIndices  , ~bothAboveOrBelowLowMask & cutoffBelowLowBoundAndPositionsMask);

			simd_float precedingIndicesRatios = utils::binToNormalised(utils::toFloat(precedingIndices), FFTOrder);
			simd_float succeedingIndicesRatios = utils::binToNormalised(utils::toFloat(succeedingIndices), FFTOrder);

			return utils::getDecimalPlaces(simd_float(1.0f) + succeedingIndicesRatios - precedingIndicesRatios);
		}

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
		// TODO: a class for the per bin eq
		// TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
	};

	class contrastEffect final : public baseEffect
	{
	public:
		contrastEffect() = delete;
		contrastEffect(const contrastEffect &) = delete;
		contrastEffect(contrastEffect &&) = delete;

		contrastEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[2])
		{
			auto parameterSize = Framework::baseEffectParameterList.size() +
				Framework::contrastEffectParameterList.size();
			moduleParameters_.data.reserve(parameterSize);
			createModuleParameters(Framework::contrastEffectParameterList.data(), Framework::contrastEffectParameterList.size());
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override;

	private:
		void runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept;

		//// Parameters
		//
		// 5. contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
		 
		
		// dtblkfx contrast, specops noise filter/focus, thinner

		static constexpr float kMinPositiveValue = 0.0f;
		static constexpr float kMaxPositiveValue = 4.0f;
		static constexpr float kMaxNegativeValue = -0.5f;
	};

	class dynamicsEffect : public baseEffect
	{
	public:
		dynamicsEffect() = delete;
		dynamicsEffect(const dynamicsEffect &) = delete;
		dynamicsEffect(dynamicsEffect &&) = delete;

		dynamicsEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[3])
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

		phaseEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[4])
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

		pitchEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[5])
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

		stretchEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[6])
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

		warpEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[7])
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

		destroyEffect(u64 parentModuleId) : baseEffect(parentModuleId, Framework::kEffectModuleNames[8])
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

		EffectModule(u64 parentModuleId, std::string_view effectType) noexcept;

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
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate);

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
