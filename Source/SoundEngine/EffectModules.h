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
		std::pair<u32, u32> getRange(const simd_int &lowIndices,
			const simd_int &highIndices, u32 FFTSize, bool isProcessedRange)
		{
			using namespace utils;

			simd_int boundDistances = maskLoad(highIndices - lowIndices,
				(simd_int(FFTSize) + lowIndices - highIndices) & simd_int(FFTSize - 1),
				simd_int::greaterThanOrEqualSigned(highIndices, lowIndices));

			u32 start, end;

			if (isProcessedRange)
			{
				auto startArray = lowIndices.getArrayOfValues();
				auto endArray = (lowIndices + boundDistances).getArrayOfValues();
				start = *std::min_element(startArray.begin(), startArray.end());
				end = *std::max_element(endArray.begin(), endArray.end());
			}
			else
			{
				auto startArray = (lowIndices + boundDistances).getArrayOfValues();
				auto endArray = (lowIndices + simd_int(FFTSize)).getArrayOfValues();
				start = *std::min_element(startArray.begin(), startArray.end());
				end = *std::max_element(endArray.begin(), endArray.end());
			}

			return { start, std::min(end - start, FFTSize) };
		}

		// first - low shifted boundary, second - high shifted boundary
		std::pair<simd_float, simd_float> getShiftedBounds(simd_float lowBound,
			simd_float highBound, float maxFrequency, bool isLinearShift)
		{
			using namespace utils;

			float maxOctave = log2f(maxFrequency / kMinFrequency);

			if (isLinearShift)
			{
				simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>() * maxFrequency;
				lowBound = clamp(exp2(lowBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, maxFrequency);
				highBound = clamp(exp2(highBound * maxOctave) * kMinFrequency + boundShift, kMinFrequency, maxFrequency);
				// snapping to 0 hz if it's below the minimum frequency
				lowBound &= simd_float::greaterThan(lowBound, kMinFrequency);
				highBound &= simd_float::greaterThan(highBound, kMinFrequency);
			}
			else
			{
				simd_float boundShift = moduleParameters_[3]->getInternalValue<simd_float>();
				lowBound = exp2(utils::clamp(lowBound + boundShift, 0.0f, 1.0f) * maxOctave);
				highBound = exp2(utils::clamp(highBound + boundShift, 0.0f, 1.0f) * maxOctave);
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

		static constexpr float kLowestDb = -100.0f;
		static constexpr float kLowestAmplitude = utils::dbToMagnitudeConstexpr(kLowestDb);
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
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept;

		simd_int getDistancesFromCutoff(const simd_int positionIndices,
			const simd_int &cutoffIndices, const simd_mask &boundMask, const simd_int &lowBoundIndices, u32 FFTSize) const noexcept
		{
			using namespace utils;

			/*
			* 1. when lowBound < highBound
			* 2. when lowBound > highBound
			*		2.1 when positionIndices and cutoffIndices are (>= lowBound and < FFTSize_) or (<= highBound and > 0)
			*		2.2 when either cutoffIndices/positionIndices is >= lowBound and < FFTSize_
			*  			and positionIndices/cutoffIndices is <= highBound and > 0
			* 
			* we redistribute the indices so that all preceding/succeeding indices go into a single variable
			* but doing that requires a lot of masking
			* 
			* for 1. and 2.1 we just look for larger and smaller indices and subtract them from each other
			* 
			* for 2.2 we need one of the masks to be above/below and one below/above the low/highBound (I chose low)
			*/

			simd_int distance;
			simd_mask greaterThanOrEqualMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

			simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundIndices);
			simd_mask cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundIndices);

			// writing indices computed for the general case 1. and 2.1
			simd_int precedingIndices  = maskLoad(cutoffIndices, positionIndices, greaterThanOrEqualMask);
			simd_int succeedingIndices = maskLoad(cutoffIndices, positionIndices, ~greaterThanOrEqualMask);

			// masking for 1. and 2.1; (PA & CA) | (~PA & ~CA) == ~(PA ^ CA)
			distance = maskLoad((simd_int(FFTSize) + precedingIndices - succeedingIndices) & simd_int(FFTSize - 1),
				simd_int(0), boundMask | ~(positionsAboveLowMask ^ cutoffAboveLowMask));

			// if all values are already set, return
			if (simd_int::equal(distance, simd_int(0)).sum() == 0)
				return distance;

			simd_mask positionsPrecedingMask = ~positionsAboveLowMask & cutoffAboveLowMask;
			simd_mask cutoffPrecedingMask = positionsAboveLowMask & ~cutoffAboveLowMask;

			// overwriting indices that fall in case 2.2 (if such exist)
			precedingIndices = (positionsPrecedingMask & positionIndices) | (cutoffPrecedingMask & cutoffIndices	);
			succeedingIndices = (positionsPrecedingMask & cutoffIndices	) | (cutoffPrecedingMask & positionIndices);

			// inverse mask of previous assignment
			distance |= maskLoad((simd_int(FFTSize) + precedingIndices - succeedingIndices) & simd_int(FFTSize - 1),
				simd_int(0), ~boundMask & (positionsAboveLowMask ^ cutoffAboveLowMask));
			return distance;
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
