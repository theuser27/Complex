/*
	==============================================================================

		spectral_support_functions.h
		Created: 14 Sep 2021 12:55:12am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "simd_buffer.h"
#include "simd_utils.h"

namespace utils
{
	// TODO: fix this mess

	strict_inline void vector_call complexValueMerge(simd_float &one, simd_float &two)
	{
		// TODO: implement complexMerge for NEON
	#if COMPLEX_SSE4_1
		auto one_ = _mm_unpacklo_ps(one.value, two.value);
		two.value = _mm_unpackhi_ps(one.value, two.value);
		one.value = one_;
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexValueMerge not implemented yet");
	#endif
	}

	strict_inline simd_float vector_call complexCartAdd(simd_float one, simd_float two)
	{	return simd_float::add(one.value, two.value); }

	strict_inline simd_float vector_call complexCartSub(simd_float one, simd_float two)
	{	return simd_float::sub(one.value, two.value); }

	strict_inline simd_float vector_call complexCartMul(simd_float one, simd_float two)
	{
	#if COMPLEX_SSE4_1
		auto realSums = simd_float::mul(one.value, two.value);
		auto imaginarySums = simd_float::mul(one.value, _mm_shuffle_ps(two.value, two.value, _MM_SHUFFLE(2, 3, 0, 1)));

		realSums = _mm_hsub_ps(realSums, realSums);
		imaginarySums = _mm_hadd_ps(imaginarySums, imaginarySums);
		return _mm_unpacklo_ps(realSums, imaginarySums);
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexCartMul not implemented yet");
	#endif
	}

	strict_inline simd_float vector_call complexPolarMul(simd_float one, simd_float two)
	{
		auto magnitudes = simd_float::mul(one.value, two.value);
		auto phases = simd_float::add(one.value, two.value);
	#if COMPLEX_SSE4_1
		magnitudes = _mm_shuffle_ps(magnitudes, magnitudes, _MM_SHUFFLE(2, 0, 2, 0));
		phases = _mm_shuffle_ps(phases, phases, _MM_SHUFFLE(3, 1, 3, 1));
		return _mm_unpacklo_ps(magnitudes, phases);
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexPolarMul not implemented yet");
	#endif
	}

	strict_inline simd_float vector_call complexMagnitude(simd_float value, bool toSqrt)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		auto imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexMagnitude not implemented yet");
	#endif
		value = simd_float::mulAdd(simd_float::mul(real, real), imaginary, imaginary);
		return (toSqrt) ? simd_float::sqrt(value) : value;
	}

	strict_inline simd_float vector_call complexMagnitude(simd_float one, simd_float two, bool toSqrt)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		auto imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexMagnitude not implemented yet");
	#endif
		one = simd_float::mulAdd(simd_float::mul(real, real), imaginary, imaginary);
		return (toSqrt) ? simd_float::sqrt(one) : one;
	}

	strict_inline simd_float vector_call complexPhase(simd_float value)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		auto imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexPhase not implemented yet");
	#endif

		return atan2(imaginary, real);
	}

	strict_inline simd_float vector_call complexPhase(simd_float one, simd_float two)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		auto imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexPhase not implemented yet");
	#endif

		return atan2(imaginary, real);
	}

	strict_inline simd_float vector_call complexReal(simd_float one, simd_float two)
	{
	#if COMPLEX_SSE4_1
		auto magnitude = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		auto phase = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexReal not implemented yet");
	#endif

		return simd_float::mul(magnitude, utils::cos(phase).value);
	}

	strict_inline simd_float vector_call complexImaginary(simd_float one, simd_float two)
	{
	#if COMPLEX_SSE4_1
		auto magnitude = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		auto phase = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexImaginary not implemented yet");
	#endif

		return simd_float::mul(magnitude, utils::sin(phase).value);
	}

	strict_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
	{
		auto magnitudes = complexMagnitude(one, two, true);
		auto phases = complexPhase(one, two);
		complexValueMerge(magnitudes, phases);
		one = magnitudes;
		two = phases;
	}

	strict_inline void vector_call complexPolarToCart(simd_float &one, simd_float &two)
	{
		// TODO: generalise
		auto realValues = complexReal(one, two);
		auto imaginaryValues = complexImaginary(one, two);
		complexValueMerge(realValues, imaginaryValues);
		one = realValues;
		two = imaginaryValues;
	}

	/// <summary>
	/// converts an array/vector of real and imaginary numbers into magnitudes and phases 
	/// </summary>
	/// <param name="values">- a pointer to the first element of the complex array</param>
	/// <param name="numTotalElements">- number of all elements in the array/vector</param>
	/// <returns>returns a polar representation of the complex number</returns>
	/*static strict_inline void cartesianToPolar(simd_float *values, int numTotalElements)
	{
		COMPLEX_ASSERT(numTotalElements % (2 * simd_float::kSize) == 0);

		float scalars[simd_float::kSize], returnValue[simd_float::kSize];
		size_t iterations = numTotalElements / (2 * simd_float::kSize);

		for (int i = 0; i < iterations; i++)
		{
			std::memcpy(scalars, &values[i], sizeof(values[i]));
			for (uint16_t i = 0; i < simd_float::kSize; i += 2)
			{
				
				returnValue[i] = hypotf(scalars[i], scalars[i + 1]);
				returnValue[i + 1] = atan2f(scalars[i], scalars[i + 1]);
			}
			std::memcpy(&values[i], returnValue, sizeof(returnValue));
		}
	}

	/// <summary>
	/// converts an array/vector of magnitudes and phases back into real and imaginary numbers
	/// </summary>
	/// <param name="values">- a pointer to the first element of the complex array</param>
	/// <param name="numTotalElements">- number of all elements in the array/vector</param>
	/// <returns>returns a cartesian representation of the complex number</returns>
	static strict_inline void polarToCartesian(simd_float* values, size_t numTotalElements)
	{
		COMPLEX_ASSERT(numTotalElements % (2 * simd_float::kSize) == 0);

		float scalars[simd_float::kSize], returnValue[simd_float::kSize];
		size_t iterations = numTotalElements / (2 * simd_float::kSize);

		for (size_t i = 0; i < iterations; i++)
		{
			std::memcpy(scalars, &values[i], sizeof(values[i]));
			for (uint16_t i = 0; i < simd_float::kSize; i+=2)
			{
				returnValue[i] = scalars[i] * cosf(scalars[i + 1]);
				returnValue[i + 1] = scalars[i] * sinf(scalars[i + 1]);
			}
			std::memcpy(&values[i], returnValue, sizeof(returnValue));
		}
	}

	/// <summary>
	/// Adds an array of inputs, puts the output in the *first input* 
	/// and scales it down by the number of inputs.
	/// </summary>
	/// <param name="inputs">- the input arrays MUST be in Cartesian form</param>
	static strict_inline void AddAllCartesian(std::vector<EffectsChainData>* inputs, int numElements)
	{
		int numInputs = inputs->size();
		simd_float simdNumInputs = (float)numInputs;

		// iterating through inputs
		for (int i = 1; i < numInputs; i++)
		{
			// iterating through channels
			for (int j = 0; j < kNumChannels; j++)
			{
				// iterating through values
				for (int k = 0; k < numElements; k++)
					inputs->at(0).workBuffer->at(j, k) += inputs->at(i).workBuffer->at(j, k);
			}
		}

		// scaling down the values by the number of inputs
		for (int i = 0; i < kNumChannels; i++)
		{
			for (int j = 0; j < numElements; j++)
				inputs->at(0).workBuffer->at(i, j) /= simdNumInputs;
		}
	}

	static strict_inline void Add2Cartesian(simd_float* one, simd_float* two, u32 numElements)
	{
		simd_float normFactor = 0.5f;
		for (u32 j = 0; j < numElements; j++)
		{
			one[j] += two[j];
			one[j] *= normFactor; // scaling down
		}
	}*/

	/*static strict_inline void Multiply2Polar(simd_float *one, simd_float *two, int numElements)
	{
		for (int j = 0; j < numElements; j++)
			simd_float::mulPolar(one[j], two[j]);
	}*/

	/*/// <summary>
	/// Adds an array of inputs, puts the output in the *first input*
	/// </summary>
	/// <param name="inputs">- the input arrays MUST be in Polar form</param>
	static strict_inline void MultiplyPolar(simd_float* inputs[], int numInputs, int numElements)
	{
			// iterating through the number of inputs
			for (int i = 1; i < numInputs; i++)
			{
					// iterating through the values
					for (int j = 0; j < numElements; j++)
							simd_float::mulPolar(inputs[0][j], inputs[i][j]);
			}
	}*/

	strict_inline simd_float circularLerp(Framework::SimdBuffer<std::complex<float>, simd_float> &bufferOne, 
		Framework::SimdBuffer<std::complex<float>, simd_float> &bufferTwo, simd_float symmetricRange, u32 frames) noexcept
	{
		COMPLEX_ASSERT(bufferOne.getSize() == bufferTwo.getSize());

		/*for (u32 i = 0; i < bufferOne.getSize(); i += 2)
		{*/
			// extracting only phase info
			simd_float pointOne = complexPhase(bufferOne.readSimdValueAt(0, i), bufferOne.readSimdValueAt(0, i + 1));
			simd_float pointTwo = complexPhase(bufferTwo.readSimdValueAt(0, i), bufferTwo.readSimdValueAt(0, i + 1));

			float asdf = std::bit_cast<int>(2);

			simd_float difference = pointTwo - pointOne;
			simd_mask differenceSignMask = unsignFloat(difference);

			simd_float altDifference = (symmetricRange * 2.0f) - difference;
			simd_mask lessThanMask = simd_float::lessThan(difference, altDifference);

			simd_mask distanceMask = maskLoad(~differenceSignMask & kSignMask, differenceSignMask, lessThanMask);
			simd_float distance = maskLoad(altDifference, difference, lessThanMask);
			distance |= distanceMask;


			return distance;

		//}
	}

}
