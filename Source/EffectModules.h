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
#include "common.h"
#include "spectral_support_functions.h"
#include "simd_buffer.h"

namespace Generation
{
	using namespace effectTypes;
	using namespace moduleTypes;

	// base class for the actual effects
	class baseEffect
	{
	public:
		baseEffect() = default;

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
		force_inline std::pair<u32, u32> getRange(const simd_int &lowIndices, const simd_int &highIndices, u32 FFTSize, bool isProcessedRange)
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
		force_inline std::pair<simd_float, simd_float> getShiftedBoundaries(simd_float lowBoundary, 
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
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept override
		{
			if (parameter == parameterNames[0])
				return gainParameter;
			else if (parameter == parameterNames[1])
				return cutoffParameter;
			else if (parameter == parameterNames[2])
				return slopeParameter;
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override
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

	private:
		force_inline simd_int getDistancesFromCutoff(const simd_int positionIndices, 
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
		* slope - range[0.0f, 1.0f]; controls the slope transition
		*	 at 0.0f it stretches from the cutoff to the frequency boundaries
		*	 at 1.0f only the center bin is left unaffected
		*/
		simd_float gainParameter = 0.0f;
		simd_float cutoffParameter = 0.0f;
		simd_float slopeParameter = 0.0f;

		static constexpr float kLowestDb = -100.0f;
		static constexpr float kLowestAmplitude = 0.00001f;
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
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept override
		{
			if (parameter == parameterNames[0])
				return contrastParameter;
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination, u32 FFTSize, float sampleRate) noexcept override
		{
			
		};

	private:
		simd_float contrastParameter = 0.0f;
		// contrast, specops noise filter/focus, thinner

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
		EffectModule() : isEnabled_(true),
			fxPtr_(std::make_unique<baseEffect>())
		{
			// TODO: change this when you define the default fx
		}

		EffectModule(const EffectModule &other) : isEnabled_(other.isEnabled_),
			fxPtr_(std::make_unique<baseEffect>(*other.fxPtr_)) { }

		EffectModule &operator=(const EffectModule &other)
		{
			if (this != &other)
			{
				isEnabled_ = other.isEnabled_;
				fxPtr_ = std::make_unique<baseEffect>(*other.fxPtr_);
			}

			return *this;
		}

		EffectModule(EffectModule &&other) noexcept
		{
			if (this != &other)
			{
				isEnabled_ = other.isEnabled_;
				fxPtr_ = std::exchange(other.fxPtr_, nullptr);

				other.isEnabled_ = false;
			}
		}

		EffectModule &operator=(EffectModule &&other) noexcept
		{
			if (this != &other)
			{
				isEnabled_ = other.isEnabled_;
				fxPtr_ = std::exchange(other.fxPtr_, nullptr);

				other.isEnabled_ = false;
			}

			return *this;
		}

		~EffectModule() = default;

		//baseEffect *getEffect() { return fxPtr_.get(); }
		//void copyEffect(baseEffect *fx) { fxPtr_ = std::make_unique<baseEffect>(*fx); }

		void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept
		{
			if (parameter == moduleParameterIds[0])
				isEnabled_ = std::get<bool>(newValue);
			else if (parameter == moduleParameterIds[1])
			{
				moduleType = static_cast<ModuleTypes>(std::get<u32>(newValue));
				changeEffect(moduleType);
			}
			else if (parameter == moduleParameterIds[1])
				mix_ = std::get<simd_float>(newValue);
			else if (parameter == moduleParameterIds[2])
				gain_ = std::get<simd_float>(newValue);
			else
				fxPtr_->setParameter(newValue, parameter);
		}

		std::variant<simd_float, u32, bool> getParameter(std::string_view parameter) const noexcept
		{
			if (parameter == moduleParameterIds[0])
				return isEnabled_;
			else if (parameter == moduleParameterIds[1])
				return static_cast<u32>(moduleType);
			else if (parameter == moduleParameterIds[2])
				return mix_;
			else if (parameter == moduleParameterIds[3])
				return gain_;
			else
				return fxPtr_->getParameter(parameter);
		}

		void changeEffect(ModuleTypes newType, bool reinitialise = false)
		{
			// TODO: call the allocation thread to fetch you the particular module type
			std::unique_ptr<baseEffect> newFx;

			switch (newType)
			{
			case ModuleTypes::Utility:
				break;
			case ModuleTypes::Filter:
				break;
			case ModuleTypes::Contrast:
				break;
			case ModuleTypes::Dynamics:
				break;
			case ModuleTypes::Phase:
				break;
			case ModuleTypes::Pitch:
				break;
			case ModuleTypes::Stretch:
				break;
			case ModuleTypes::Warp:
				break;
			case ModuleTypes::Destroy:
				break;
			default:
				break;
			}
		}
		void processEffect();

	private:
		bool isEnabled_ = true;
		simd_float mix_ = 100.0f;
		simd_float gain_ = 0.0f;
		ModuleTypes moduleType = ModuleTypes::Utility;
		std::unique_ptr<baseEffect> fxPtr_;
	};
}
