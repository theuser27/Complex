/*
	==============================================================================

		EffectModules.h
		Created: 27 Jul 2021 12:30:19am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include <variant>
#include <algorithm>
#include "./Framework/common.h"
#include "./Framework/spectral_support_functions.h"
#include "./Framework/simd_buffer.h"

namespace Generation
{
	using namespace effectTypes;
	using namespace moduleTypes;

	// base class for the actual effects
	class baseEffect
	{
	public:
		baseEffect() = default;

		baseEffect(const baseEffect &other) noexcept : typeParameter_(other.typeParameter_),
			lowBoundaryParameter_(other.lowBoundaryParameter_),
			highBoundaryParameter_(other.highBoundaryParameter_),
			isLinearShiftParameter_(other.isLinearShiftParameter_) { }
		baseEffect &operator=(const baseEffect &other) noexcept
		{
			if (this != &other)
			{
				typeParameter_ = other.typeParameter_;
				lowBoundaryParameter_ = other.lowBoundaryParameter_;
				highBoundaryParameter_ = other.highBoundaryParameter_;
				isLinearShiftParameter_ = other.isLinearShiftParameter_;
			}
			return *this;
		}

		baseEffect(baseEffect &&other) noexcept : typeParameter_(other.typeParameter_),
			lowBoundaryParameter_(other.lowBoundaryParameter_),
			highBoundaryParameter_(other.highBoundaryParameter_),
			isLinearShiftParameter_(other.isLinearShiftParameter_) { }
		baseEffect &operator=(baseEffect &&other) noexcept
		{
			if (this != &other)
			{
				typeParameter_ = other.typeParameter_;
				lowBoundaryParameter_ = other.lowBoundaryParameter_;
				highBoundaryParameter_ = other.highBoundaryParameter_;
				isLinearShiftParameter_ = other.isLinearShiftParameter_;
			}
			return *this;
		}

		virtual void initialise() noexcept
		{
			typeParameter_ = 0;
			lowBoundaryParameter_ = 0.0f;
			highBoundaryParameter_ = 1.0f;
			boundaryShiftParameter_ = 0.0f;
		}

		virtual void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept
		{
			if (parameter == baseParameterIds[0])
				typeParameter_ = std::get<u32>(newValue);
			else if (parameter == baseParameterIds[1])
				lowBoundaryParameter_ = std::get<simd_float>(newValue);
			else if (parameter == baseParameterIds[2])
				highBoundaryParameter_ = std::get<simd_float>(newValue);
			else if (parameter == baseParameterIds[3])
				boundaryShiftParameter_ = std::get<simd_float>(newValue);
			else if (parameter == baseParameterIds[4])
				isLinearShiftParameter_ = std::get<bool>(newValue);
		}

		virtual std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept
		{
			if (parameter == baseParameterIds[0])
				return typeParameter_;
			else if (parameter == baseParameterIds[1])
				return lowBoundaryParameter_;
			else if (parameter == baseParameterIds[2])
				return highBoundaryParameter_;
			else if (parameter == baseParameterIds[3])
				return boundaryShiftParameter_;
			else if (parameter == baseParameterIds[4])
				return isLinearShiftParameter_;
		}

		virtual void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept
		{ 
			// swapping pointers since we're not doing anything with the data
			source.swap(destination);
		};



	protected:
		// returns starting point and distance to end of processed/unprocessed range
		perf_inline std::pair<u32, u32> getRange(const simd_int &lowIndices,
			const simd_int &highIndices, u32 FFTSize, bool isProcessedRange)
		{
			using namespace utils;

			simd_int boundaryDistances = maskLoad(highIndices - lowIndices,
				(simd_int(FFTSize) + lowIndices - highIndices) & simd_int(FFTSize - 1),
				simd_int::greaterThanOrEqualSigned(highIndices, lowIndices));

			u32 start, end;

			if (isProcessedRange)
			{
				auto startArray = lowIndices.getArrayOfValues();
				auto endArray = (lowIndices + boundaryDistances).getArrayOfValues();
				start = *std::min_element(startArray.begin(), startArray.end());
				end = *std::max_element(endArray.begin(), endArray.end());
			}
			else
			{
				auto startArray = (lowIndices + boundaryDistances).getArrayOfValues();
				auto endArray = (lowIndices + simd_int(FFTSize)).getArrayOfValues();
				start = *std::min_element(startArray.begin(), startArray.end());
				end = *std::max_element(endArray.begin(), endArray.end());
			}

			return { start, std::min(end - start, FFTSize) };
		}

		// first - low shifted boundary, second - high shifted boundary
		perf_inline std::pair<simd_float, simd_float> getShiftedBoundaries(simd_float lowBoundary,
			simd_float highBoundary, float maxFrequency, bool isLinearShift)
		{
			using namespace utils;

			float maxOctave = log2f(maxFrequency / kMinFrequency);

			if (isLinearShift)
			{
				simd_float boundaryShift = boundaryShiftParameter_ * maxFrequency;
				lowBoundary = clamp(exp2(lowBoundary * maxOctave) * kMinFrequency + boundaryShift, kMinFrequency, maxFrequency);
				highBoundary = clamp(exp2(highBoundary * maxOctave) * kMinFrequency + boundaryShift, kMinFrequency, maxFrequency);
				// snapping to 0 hz if it's below the minimum frequency
				lowBoundary &= simd_float::greaterThan(lowBoundary, kMinFrequency);
				highBoundary &= simd_float::greaterThan(highBoundary, kMinFrequency);
			}
			else
			{
				lowBoundary = exp2(utils::clamp(lowBoundary + boundaryShiftParameter_, 0.0f, 1.0f) * maxOctave);
				highBoundary = exp2(utils::clamp(highBoundary + boundaryShiftParameter_, 0.0f, 1.0f) * maxOctave);
				// snapping to 0 hz if it's below the minimum frequency
				lowBoundary = (lowBoundary & simd_float::greaterThan(lowBoundary, 1.0f)) * kMinFrequency;
				highBoundary = (highBoundary & simd_float::greaterThan(highBoundary, 1.0f)) * kMinFrequency;
			}
			return { lowBoundary, highBoundary };
		}

		u32 typeParameter_ = 0;																		// internal fx type
		simd_float lowBoundaryParameter_ = 0.0f;									// normalised frequency boundaries of processed region [0.0f, 1.0f]
		simd_float highBoundaryParameter_ = 1.0f;
		simd_float boundaryShiftParameter_ = 0.0f;								// shifting of the freq boundaries [-1.0f; 1.0f]
		bool isLinearShiftParameter_ = false;											// are the boundaries being shifted logarithmically or linearly

		static constexpr float kLowestDb = -100.0f;
		static constexpr float kLowestAmplitude = utils::dbToMagnitudeConstexpr(kLowestDb);
	};

	class utilityEffect : public baseEffect
	{
	public:
		utilityEffect() = default;

	private:
		simd_int toReverseSpectrum = 0;						// reverses the spectrum bins
		simd_int flipPhase = 0;										// flipping phases of channels
		simd_float pan = 0.0f;										// channel pan control

		// TODO:
		// idea: mix 2 input signals (left and right/right and left channels)
		// idea: flip the phases, panning
	};

	class filterEffect : public baseEffect
	{
	public:
		filterEffect() = default;

		void initialise() noexcept override
		{
			baseEffect::initialise();
			gainParameter = 0.0f;
			cutoffParameter = 0.0f;
			slopeParameter = 0.0f;
		}

		void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept override
		{
			if (parameter == parameterNames[0])
				gainParameter = std::get<simd_float>(newValue);
			else if (parameter == parameterNames[1])
				cutoffParameter = std::get<simd_float>(newValue);
			else if (parameter == parameterNames[2])
				slopeParameter = std::get<simd_float>(newValue);
			else
				baseEffect::setParameter(newValue, parameter);
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept override
		{
			if (parameter == parameterNames[0])
				return gainParameter;
			else if (parameter == parameterNames[1])
				return cutoffParameter;
			else if (parameter == parameterNames[2])
				return slopeParameter;
			else
				return baseEffect::getParameter(parameter);
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override
		{
			switch (typeParameter_)
			{
			case static_cast<u32>(FilterTypes::Normal):
				runNormal(source, destination, FFTSize, sampleRate);
				break;
			default:
				break;
			}
		}

	private:
		perf_inline void runNormal(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept;

		perf_inline simd_int getDistancesFromCutoff(const simd_int positionIndices,
			const simd_int &cutoffIndices, const simd_mask &boundaryMask, const simd_int &lowBoundaryIndices, u32 FFTSize) const noexcept
		{
			using namespace utils;

			/*
			* 1. when lowBoundary < highBoundary
			* 2. when lowBoundary > highBoundary
			*		2.1 when positionIndices and cutoffIndices are (>= lowBoundary and < FFTSize_) or (<= highBoundary and > 0)
			*		2.2 when either cutoffIndices/positionIndices is >= lowBoundary and < FFTSize_
			*  			and positionIndices/cutoffIndices is <= highBoundary and > 0
			* 
			* we redistribute the indices so that all preceding/succeeding indices go into a single variable
			* but doing that requires a lot of masking
			* 
			* for 1. and 2.1 we just look for larger and smaller indices and subtract them from each other
			* 
			* for 2.2 we need one of the masks to be above/below and one below/above the low/highBoundary (I chose low)
			*/

			simd_int distance;
			simd_mask greaterThanOrEqualMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

			simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundaryIndices);
			simd_mask cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundaryIndices);

			// writing indices computed for the general case 1. and 2.1
			simd_int precedingIndices  = maskLoad(cutoffIndices, positionIndices, greaterThanOrEqualMask);
			simd_int succeedingIndices = maskLoad(cutoffIndices, positionIndices, ~greaterThanOrEqualMask);

			// masking for 1. and 2.1; (PA & CA) | (~PA & ~CA) == ~(PA ^ CA)
			distance = maskLoad((simd_int(FFTSize) + precedingIndices - succeedingIndices) & simd_int(FFTSize - 1),
				simd_int(0), boundaryMask | ~(positionsAboveLowMask ^ cutoffAboveLowMask));

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
				simd_int(0), ~boundaryMask & (positionsAboveLowMask ^ cutoffAboveLowMask));
			return distance;
		}

		/*
		* gain - range[-1.0f, 1.0f]; lowers the loudness of the cutoff/around the cutoff point for negative/positive values 
		*	 at min/max values the bins around/outside the cutoff are zeroed 
		*	 *parameter values are interpreted linearly, so the control needs to have an exponential slope
		* 
		* cutoff - range[0.0f, 1.0f]; controls where the filtering starts
		*	 at 0.0f/1.0f it's at the low/high boundary
		*  *parameter values are interpreted linearly, so the control needs to have an exponential slope
		* 
		* slope - range[-1.0f, 1.0f]; controls the slope transition
		*	 at 0.0f it stretches from the cutoff to the frequency boundaries
		*	 at 1.0f only the center bin is left unaffected
		*/
		simd_float gainParameter = 0.0f;
		simd_float cutoffParameter = 0.0f;
		simd_float slopeParameter = 0.0f;

		static constexpr std::string_view parameterNames[] = {"Gain", "Cutoff", "Slope"};
	};

	class peakEffect : public baseEffect
	{
		// triangles, squares, saws, pointy, sweep and custom
		// TODO: a class for the per bin eq
		// TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
	};

	class contrastEffect : public baseEffect
	{
	public:
		contrastEffect() = default;

		void initialise() noexcept override
		{
			baseEffect::initialise();
			contrastParameter = 0.0f;
		}

		void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept override
		{
			if (parameter == parameterNames[0])
				contrastParameter = std::get<simd_float>(newValue);
			else
				baseEffect::setParameter(newValue, parameter);
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept override
		{
			if (parameter == parameterNames[0])
				return contrastParameter;
			else
				return baseEffect::getParameter(parameter);
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override
		{
			switch (typeParameter_)
			{
			case static_cast<u32>(ContrastTypes::Contrast):
				runContrast(source, destination, FFTSize, sampleRate);
				break;
			default:
				break;
			}
		};

	private:
		strict_inline void runContrast(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate);

		/*
		* contrast - range[-1.0f, 1.0f]; controls the relative loudness of the bins
		*/
		simd_float contrastParameter = 0.0f;
		// dtblkfx contrast, specops noise filter/focus, thinner

		static constexpr float kMinPositiveValue = 0.0f;
		static constexpr float kMaxPositiveValue = 4.0f;
		static constexpr float kMaxNegativeValue = -0.5f;
		static constexpr std::string_view parameterNames[] = { "Contrast" };
	};

	class phaseEffect : public baseEffect
	{
		// phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
	};

	class pitchEffect : public baseEffect
	{
		// resample, shift, const shift, harmonic shift, harmonic repitch
	};

	class dynamicsEffect : public baseEffect
	{
		// spectral compander, gate (threshold), clipping
	};

	class stretchEffect : public baseEffect
	{
		// specops geometry
	};

	class warpEffect : public baseEffect
	{
		// vocode, harmonic match, cross/warp mix
	};

	class destroyEffect : public baseEffect
	{
		// resize, specops effects category
	};

	//TODO: freezer and glitcher classes


	// class for the whole FX unit
	class EffectModule
	{
	public:
		EffectModule() = default;

		EffectModule(ModuleTypes type)
		{
			switch (type)
			{
			case ModuleTypes::Utility:
				effect_ = std::make_unique<utilityEffect>();
				break;
			case ModuleTypes::Filter:
				effect_ = std::make_unique<filterEffect>();
				break;
			case ModuleTypes::Contrast:
				effect_ = std::make_unique<contrastEffect>();
				break;
			case ModuleTypes::Dynamics:
				effect_ = std::make_unique<dynamicsEffect>();
				break;
			case ModuleTypes::Phase:
				effect_ = std::make_unique<phaseEffect>();
				break;
			case ModuleTypes::Pitch:
				effect_ = std::make_unique<pitchEffect>();
				break;
			case ModuleTypes::Stretch:
				effect_ = std::make_unique<stretchEffect>();
				break;
			case ModuleTypes::Warp:
				effect_ = std::make_unique<warpEffect>();
				break;
			case ModuleTypes::Destroy:
				effect_ = std::make_unique<destroyEffect>();
				break;
			default:
				effect_ = std::make_unique<utilityEffect>();
				break;
			}
		}

		EffectModule(const EffectModule &other) : isEnabledParameter_(other.isEnabledParameter_),
			mixParameter_(other.mixParameter_), gainParameter_(other.gainParameter_),
			moduleTypeParameter_(other.moduleTypeParameter_),
			effect_(std::make_unique<baseEffect>(*other.effect_)) { }
		EffectModule &operator=(const EffectModule &other)
		{
			if (this != &other)
			{
				isEnabledParameter_ = other.isEnabledParameter_;
				mixParameter_ = other.mixParameter_;
				gainParameter_ = other.gainParameter_;
				moduleTypeParameter_ = other.moduleTypeParameter_;
				effect_ = std::make_unique<baseEffect>(*other.effect_);
			}
			return *this;
		}

		EffectModule(EffectModule &&other) noexcept : isEnabledParameter_(other.isEnabledParameter_),
			mixParameter_(other.mixParameter_), gainParameter_(other.gainParameter_), 
			moduleTypeParameter_(other.moduleTypeParameter_), effect_(std::exchange(other.effect_, nullptr)) { }
		EffectModule &operator=(EffectModule &&other) noexcept
		{
			if (this != &other)
			{
				isEnabledParameter_ = other.isEnabledParameter_;
				isEnabledParameter_ = other.isEnabledParameter_;
				mixParameter_ = other.mixParameter_;
				gainParameter_ = other.gainParameter_;
				moduleTypeParameter_ = other.moduleTypeParameter_;
				effect_ = std::exchange(other.effect_, nullptr);
			}
			return *this;
		}

		~EffectModule() = default;

		//baseEffect *getEffect() { return fxPtr_.get(); }
		//void copyEffect(baseEffect *fx) { fxPtr_ = std::make_unique<baseEffect>(*fx); }

		void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept
		{
			if (parameter == moduleParameterIds[0])
				isEnabledParameter_ = std::get<bool>(newValue);
			else if (parameter == moduleParameterIds[1])
			{
				moduleTypeParameter_ = static_cast<ModuleTypes>(std::get<u32>(newValue));
				changeEffect(moduleTypeParameter_);
			}
			else if (parameter == moduleParameterIds[1])
				mixParameter_ = std::get<simd_float>(newValue);
			else if (parameter == moduleParameterIds[2])
				gainParameter_ = std::get<simd_float>(newValue);
			else
				effect_->setParameter(newValue, parameter);
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept
		{
			if (parameter == moduleParameterIds[0])
				return isEnabledParameter_;
			else if (parameter == moduleParameterIds[1])
				return static_cast<u32>(moduleTypeParameter_);
			else if (parameter == moduleParameterIds[2])
				return mixParameter_;
			else if (parameter == moduleParameterIds[3])
				return gainParameter_;
			else
				return effect_->getParameter(parameter);
		}

		// call on an allocation thread to fetch you the particular module type
		void changeEffect(ModuleTypes newType)
		{
			std::unique_ptr<baseEffect> newEffect;
			switch (newType)
			{
			case ModuleTypes::Utility:
				newEffect = std::make_unique<utilityEffect>();
				break;
			case ModuleTypes::Filter:
				newEffect = std::make_unique<filterEffect>();
				break;
			case ModuleTypes::Contrast:
				newEffect = std::make_unique<contrastEffect>();
				break;
			case ModuleTypes::Dynamics:
				newEffect = std::make_unique<dynamicsEffect>();
				break;
			case ModuleTypes::Phase:
				newEffect = std::make_unique<phaseEffect>();
				break;
			case ModuleTypes::Pitch:
				newEffect = std::make_unique<pitchEffect>();
				break;
			case ModuleTypes::Stretch:
				newEffect = std::make_unique<stretchEffect>();
				break;
			case ModuleTypes::Warp:
				newEffect = std::make_unique<warpEffect>();
				break;
			case ModuleTypes::Destroy:
				newEffect = std::make_unique<destroyEffect>();
				break;
			default:
				newEffect = std::make_unique<utilityEffect>();
				break;
			}

			// blocks alloc thread until processing finishes
			bool expected = false;
			while (isInUse.compare_exchange_weak(expected, true));
			std::swap(effect_, newEffect);
			isInUse.store(false, std::memory_order_release);
		}

		void processEffect(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate);

	private:
		bool isEnabledParameter_ = true;
		simd_float mixParameter_ = 1.0f;
		simd_float gainParameter_ = 0.0f;
		ModuleTypes moduleTypeParameter_ = ModuleTypes::Utility;
		std::unique_ptr<baseEffect> effect_;

		std::atomic<bool> isInUse = false;

		static constexpr float kMaxPositiveGain = utils::dbToMagnitudeConstexpr(30.0f);
		static constexpr float kMaxNegativeGain = utils::dbToMagnitudeConstexpr(-30.0f);
	};
}
