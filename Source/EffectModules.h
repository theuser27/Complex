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
		baseEffect(bool initialise = false)
		{ if (initialise) this->initialise(); };

		virtual void initialise() noexcept
		{
			typeParameter_ = 0;
			lowBoundaryParameter_ = 0.0f;
			highBoundaryParameter_ = 1.0f;
			boundaryShiftParameter_ = 0.0f;
		}

		virtual void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination)
		{ 
			// swapping pointers since we're not doing anything with the data
			source.swap(destination);
		};

		virtual void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept
		{
			if (parameter == baseParameterIds[0])
				typeParameter_ = std::get<u32>(newValue);
			else if (parameter == baseParameterIds[1])
				lowBoundaryParameter_ = std::get<float>(newValue);
			else if (parameter == baseParameterIds[2])
				highBoundaryParameter_ = std::get<float>(newValue);
			else if (parameter == baseParameterIds[3])
				boundaryShiftParameter_ = std::get<float>(newValue);
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
		}

		void setSampleRate(float newSampleRate) noexcept { sampleRate_ = newSampleRate; }
		void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }

	protected:
		// returns the range of bins that need to be processed
		force_inline std::pair<u32, u32> getProcessingRange(simd_int &lowBins, simd_int &highBins)
		{
			auto lowArray = simd_float::min(lowBins, highBins).getArrayOfValues();
			auto highArray = simd_float::max(lowBins, highBins).getArrayOfValues();

			return { *std::min_element(lowArray.begin(), lowArray.end()), 
				*std::max_element(highArray.begin(), highArray.end()) };
		}

		// first - low shifted boundary, second - high shifted boundary
		force_inline std::pair<simd_float, simd_float> getShiftedBoundaries()
		{
			// TODO
			return { lowBoundaryParameter_, highBoundaryParameter_ };
		}

		float sampleRate_ = kDefaultSampleRate;
		u32 FFTSize_ = 1 << kDefaultFFTOrder;

		u32 typeParameter_ = 0;																		// internal fx type
		simd_float lowBoundaryParameter_ = 0.0f;									// normalised frequency boundaries of processed region
		simd_float highBoundaryParameter_ = 1.0f;
		simd_float boundaryShiftParameter_ = 0.0f;								// shifted version of the freq boundaries
	};

	class utilityEffect : public baseEffect
	{
	public:
		utilityEffect() = default;

	private:
		bool toReverseSpectrum = false;						// reverses the spectrum bins
		bool flipLeftPhase = false;								// flipping phases of channels
		bool flipRightPhase = false;
		simd_float pan = 0.0f;										// channel pan control

		// TODO:
		// idea: mix 2 input signals (left and right/right and left channels)
		// idea: flip the phases, panning
	};

	class filterEffect : public baseEffect
	{
	public:
		filterEffect() = default;

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination) override
		{
			using namespace utils;

			auto shiftedBoundaries = getShiftedBoundaries();

			// getting the boundaries in terms of bin position
			simd_int lowBoundaryIndices = utils::floorToInt(shiftedBoundaries.first * FFTSize_);
			simd_int highBoundaryIndices = utils::floorToInt(shiftedBoundaries.second * FFTSize_);

			// getting the distances between the boundaries and which masks precede the others 
			simd_mask boundaryMask = simd_int::greaterThanOrEqualSigned(highBoundaryIndices, lowBoundaryIndices);
			simd_int boundaryDistances = 
				(boundaryMask & ((simd_int(FFTSize_) + highBoundaryIndices - lowBoundaryIndices) & simd_int(FFTSize_ - 1))) |
				(~boundaryMask & ((simd_int(FFTSize_) + lowBoundaryIndices - highBoundaryIndices) & simd_int(FFTSize_ - 1)));

			// minimising the bins to iterate on and process
			auto lowestHighestIndices = getProcessingRange(lowBoundaryIndices, highBoundaryIndices);
			u32 numBinsToProcess = (FFTSize_ + lowestHighestIndices.first - lowestHighestIndices.second) % FFTSize_;

			// position to start processing
			u32 positionIndex = lowestHighestIndices.first;

			// calculating the bins where the cutoff lies
			simd_int cutoffIndices = floorToInt(interpolate(simd_float(lowBoundaryIndices), 
				simd_float(highBoundaryIndices), cutoffParameter));

			// if mask scalars are negative -> brickwall, if positive -> linear slope
			// clearing sign and calculating end of slope bins
			simd_float slopes = slopeParameter;
			simd_mask slopeMask = unsignFloat(slopes);
			slopes = floorToInt(interpolate((float)FFTSize_, 1.0f, slopes));

			//
			// if mask scalars are negative -> attenuate at cutoff, if positive -> around cutoff
			// clearing sign (gains is the gain reduction, not the gain multiplier)
			simd_float gains = gainParameter;
			simd_mask gainMask = unsignFloat(gains);

			simd_int distancesFromCutoff;

			for (u32 i = 0; i < numBinsToProcess; i++)
			{
				distancesFromCutoff = getDistancesFromCutoff(simd_int(positionIndex + i),
					cutoffIndices, boundaryMask, lowBoundaryIndices);

				// calculating linear slope and brickwall, both are ratio of the gain attenuation
				// the higher tha value the more it will be affected by it
				simd_float gainRatio = simd_float::clamp(0.0f, 1.0f, simd_float(distancesFromCutoff) / slopes) & slopeMask;
				gainRatio = simd_float::min(floor(simd_float(distancesFromCutoff) / (slopes + 1.0f)), 1.0f) & ~slopeMask;

				gains = ((gains * gainRatio) & ~gainMask) + ((gains * (simd_float(1.0f) - gainRatio)) & gainMask);
				// convert gain attenuation to gain multiplier
				gains = simd_float(1.0f) - gains;

				// TODO: refactor simdBuffer since there won't more than 2 channels
				destination.writeSIMDValueAt(source.getSIMDValueAt(0, (positionIndex + i) % FFTSize_) * gains, 0, (positionIndex + i) % FFTSize_);
			}

			// copying the unprocessed data
			for (u32 i = 0; i < FFTSize_; i++)
			{
				// utilising boundary mask since it doesn't serve a purpose anymore
				boundaryMask = simd_int::greaterThanOrEqualSigned(lowBoundaryIndices, positionIndex)
					| simd_int::greaterThanOrEqualSigned(positionIndex, highBoundaryIndices);
				destination.writeSIMDValueAt((source.getSIMDValueAt(0, i) & boundaryMask)
					+ (destination.getSIMDValueAt(0, i) & ~boundaryMask), 0, i);
			}
		}

	private:
		force_inline simd_int getDistancesFromCutoff(const simd_int positionIndices, 
			const simd_int &cutoffIndices, const simd_mask &boundaryMask, const simd_int &lowBoundaryIndices)
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
			
			// in case the simd version doesn't work, we just unpack and compare
			/*std::array<float, kSimdRatio> precedingIndices;
			std::array<float, kSimdRatio> succeedingIndices;
			auto cutoffIndicesArray = cutoffIndices.getArrayOfValues();
			auto positionIndicesArray = positionIndices.getArrayOfValues();
			auto lowBoundaryIndicesArray = lowBoundaryIndices.getArrayOfValues();
			for (size_t i = 0; i < kSimdRatio; i++)
			{
				if (cutoffIndicesArray[i] >= positionIndicesArray[i])
				{
					precedingIndices[i] = cutoffIndicesArray[i];
					succeedingIndices[i] = positionIndicesArray[i];
				}
				else
				{
					precedingIndices[i] = positionIndicesArray[i];
					succeedingIndices[i] = cutoffIndicesArray[i];
				}

				if ((positionIndicesArray[i] < lowBoundaryIndicesArray[i]) && 
					(cutoffIndicesArray[i] >= lowBoundaryIndicesArray[i]))
				{
					precedingIndices[i] = positionIndicesArray[i];
					succeedingIndices[i] = cutoffIndicesArray[i];
				}
				else if ((cutoffIndicesArray[i] < lowBoundaryIndicesArray[i]) &&
					(positionIndicesArray[i] >= lowBoundaryIndicesArray[i]))
				{
					precedingIndices[i] = cutoffIndicesArray[i];
					succeedingIndices[i] = positionIndicesArray[i];
				}
			}
			distance = ~boundaryMask & ((simd_int(FFTSize_) + simd_int(precedingIndices)
				- simd_int(succeedingIndices)) & simd_int(FFTSize_ - 1));
			return distance;*/

			simd_int distance;
			simd_mask greaterThanOrEqualMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, positionIndices);

			simd_mask positionsAboveLowMask = simd_mask::greaterThanOrEqualSigned(positionIndices, lowBoundaryIndices);
			simd_mask cutoffAboveLowMask = simd_mask::greaterThanOrEqualSigned(cutoffIndices, lowBoundaryIndices);

			// writing indices computed for the general case 1. and 2.1
			simd_int precedingIndices  = ( greaterThanOrEqualMask & cutoffIndices) | (~greaterThanOrEqualMask & positionIndices);
			simd_int succeedingIndices = (~greaterThanOrEqualMask & cutoffIndices) | ( greaterThanOrEqualMask & positionIndices);

			// masking for 1. and 2.1; (PA & CA) | (~PA & ~CA) == ~(PA ^ CA)
			distance = (boundaryMask | ~(positionsAboveLowMask ^ cutoffAboveLowMask))
				& ((simd_int(FFTSize_) + precedingIndices - succeedingIndices) & simd_int(FFTSize_ - 1));

			// if all values are already set, return
			if (simd_int::equal(distance, simd_int(0)).anyMask() == 0)
				return distance;

			simd_mask positionsPrecedingMask = ~positionsAboveLowMask & cutoffAboveLowMask;
			simd_mask cutoffPrecedingMask = positionsAboveLowMask & ~cutoffAboveLowMask;

			// overwriting indices that fall in case 2.2 (if such exist)
			precedingIndices = (positionsPrecedingMask & positionIndices) | (cutoffPrecedingMask & cutoffIndices	);
			succeedingIndices = (positionsPrecedingMask & cutoffIndices	) | (cutoffPrecedingMask & positionIndices);

			// inverse mask of previous assignment
			distance |= (~boundaryMask & (positionsAboveLowMask ^ cutoffAboveLowMask))
				& ((simd_int(FFTSize_) + precedingIndices - succeedingIndices) & simd_int(FFTSize_ - 1));
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
		u32 slopeTypeParameter = 0;

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
		contrastEffect()
		{
		}

		void run(Framework::SimdBuffer<std::complex<float>, simd_float> &source,
			Framework::SimdBuffer<std::complex<float>, simd_float> &destination) override
		{
			
		};

		void setParameter(std::variant<simd_float, u32, bool> &newValue, std::string_view parameter) noexcept override
		{

		}

	private:
		simd_float contrast = 0.0f;
		// contrast, specops noise filter/focus, thinner
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
				mix_ = std::get<float>(newValue);
			else if (parameter == moduleParameterIds[2])
				gain_ = std::get<float>(newValue);
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
