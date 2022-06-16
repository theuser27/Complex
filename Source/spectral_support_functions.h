/*
	==============================================================================

		spectral_support_functions.h
		Created: 14 Sep 2021 12:55:12am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "common.h"
#include "simd_buffer.h"

namespace Framework
{
	// TODO: fix this mess

	static force_inline simd_float vector_call mulPolar(simd_float one, simd_float two)
	{
		auto one_ = one.getArrayOfValues();
		auto two_ = two.getArrayOfValues();
		for (u16 i = 0; i < simd_float::kSize; i += 2)
		{
			one_[i] *= two_[i];
			one_[i + 1] += two_[i + 1];
		}
		return simd_float(one_);
	}

	/// <summary>
	/// converts an array/vector of real and imaginary numbers into magnitudes and phases 
	/// </summary>
	/// <param name="values">- a pointer to the first element of the complex array</param>
	/// <param name="numTotalElements">- number of all elements in the array/vector</param>
	/// <returns>returns a polar representation of the complex number</returns>
	/*static force_inline void cartesianToPolar(simd_float *values, int numTotalElements)
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
	static force_inline void polarToCartesian(simd_float* values, size_t numTotalElements)
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
	static force_inline void AddAllCartesian(std::vector<EffectsChainData>* inputs, int numElements)
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

	static force_inline void Add2Cartesian(simd_float* one, simd_float* two, u32 numElements)
	{
		simd_float normFactor = 0.5f;
		for (u32 j = 0; j < numElements; j++)
		{
			one[j] += two[j];
			one[j] *= normFactor; // scaling down
		}
	}*/

	/*static force_inline void Multiply2Polar(simd_float *one, simd_float *two, int numElements)
	{
		for (int j = 0; j < numElements; j++)
			simd_float::mulPolar(one[j], two[j]);
	}*/

	/*/// <summary>
	/// Adds an array of inputs, puts the output in the *first input*
	/// </summary>
	/// <param name="inputs">- the input arrays MUST be in Polar form</param>
	static force_inline void MultiplyPolar(simd_float* inputs[], int numInputs, int numElements)
	{
			// iterating through the number of inputs
			for (int i = 1; i < numInputs; i++)
			{
					// iterating through the values
					for (int j = 0; j < numElements; j++)
							simd_float::mulPolar(inputs[0][j], inputs[i][j]);
			}
	}*/



}
