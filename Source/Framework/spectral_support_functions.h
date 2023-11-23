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
	// layout of complex cartesian and polar vectors is assumed to be
	// { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

	// returns { unscaled cos, unscaled sin, scale factor }
	/**
	 * \brief 
	 * \tparam Iterations number of iterations the algorithm should do
	 * \param radians phases the algorithm should use to create the cis pairs
	 * \return { unscaled cos part, unscaled sin part, scaling factor }
	 */
	template<size_t Iterations = 15>
	strict_inline std::array<simd_float, 3> vector_call cordicRotation(simd_float radians)
	{
		static simd_mask kFloatExponentMask = 0x7f800000;
		static simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

		static simd_float kFactor = []()
		{
			float result = 1.0f;
			for (size_t i = 0; i < Iterations; i++)
				result *= 1.0f / std::sqrt(1.0f + std::exp2(-2.0f * (float)i));
			return result;
		}();

		// thetaDeltas[i] = atan(2^(-i))
		static std::array<simd_float, Iterations + 1> thetaDeltas = []()
		{
			std::array<simd_float, Iterations + 1> angles{};
			for (i32 i = 0; i <= (i32)Iterations; i++)
				angles[i] = std::atan(std::exp2((float)(-i)));
			return angles;
		}();

		// correction for angles after +/-pi
		auto correctionRotations = simd_float::round(radians / (2.0f * kPi));
		radians -= (correctionRotations * 2.0f * kPi);

		// correction so that the algorithm works
		auto sinMask = unsignSimd(radians);
		radians -= kPi * 0.5f;

		std::pair<simd_float, simd_float> result = { 0.0f, 1.0f };
		for (u32 i = 0; i <= (u32)Iterations; i++)
		{
			auto signMask = getSign(radians);
			radians -= thetaDeltas[i] ^ signMask;

			auto prevX = result.first;
			auto prevY = result.second;

			// x[i] = x[i - 1] - y[i - 1] * 2^(-i) * "sign"
			result.first = prevX - (((prevY & kNotFloatExponentMask) |
				(reinterpretToInt(prevY & kFloatExponentMask) - (i << 23))) ^ signMask);
			// y[i] = y[i - 1] - x[i - 1] * 2^(-i) * "sign"
			result.second = prevY + (((prevX & kNotFloatExponentMask) |
				(reinterpretToInt(prevX & kFloatExponentMask) - (i << 23))) ^ signMask);
		}

		return { result.first, result.second | sinMask, kFactor };
	}

	/**
	 * \brief 
	 * \tparam Iterations number of iterations the algorithm should do
	 * \param x only the real parts
	 * \param y only the imaginary parts
	 * \return { unscaled magnitude, phase, scaling factor }
	 */
	template<size_t Iterations = 15>
	strict_inline std::array<simd_float, 3> vector_call cordicVectoring(simd_float x, simd_float y)
	{
		static simd_mask kFloatExponentMask = 0x7f800000;
		static simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

		static simd_float kFactor = []()
		{
			float result = 1.0f;
			for (size_t i = 0; i < Iterations; i++)
				result *= 1.0f / std::sqrt(1.0f + std::exp2(-2.0f * (float)i));
			return result;
		}();

		// thetaDeltas[i] = atan(2^(-i))
		static std::array<simd_float, Iterations + 1> thetaDeltas = []()
		{
			std::array<simd_float, Iterations + 1> angles{};
			for (i32 i = 0; i <= (i32)Iterations; i++)
				angles[i] = std::atan(std::exp2((float)(-i)));
			return angles;
		}();


		simd_mask xNegativeMask = unsignSimd(x);
		simd_float angle = (simd_float(kPi) ^ getSign(y)) & simd_mask::equal(xNegativeMask, kSignMask);
		for (u32 i = 0; i <= (u32)Iterations; i++)
		{
			simd_mask signMask = getSign(y);
			angle += thetaDeltas[i] ^ (signMask ^ xNegativeMask);

			simd_float prevX = x;
			simd_float prevY = y;

			// depending on the values the exponents might be 0 and if that is the case the int will underflow, so we need to mask against that
			simd_mask prevXExponent = maskLoad(reinterpretToInt(prevX & kFloatExponentMask) - (i << 23), 
				simd_mask(0), simd_mask::equal(0, reinterpretToInt(prevX & kFloatExponentMask)));
			simd_mask prevYExponent = maskLoad(reinterpretToInt(prevY & kFloatExponentMask) - (i << 23), 
				simd_mask(0), simd_mask::equal(0, reinterpretToInt(prevY & kFloatExponentMask)));

			// x[i] = x[i - 1] + y[i - 1] * 2^(-i) * "sign"
			x = prevX + (((prevY & kNotFloatExponentMask) | prevYExponent) ^ signMask);
			// y[i] = y[i - 1] - x[i - 1] * 2^(-i) * "sign"
			y = prevY - (((prevX & kNotFloatExponentMask) | prevXExponent) ^ signMask);
		}

		return { x, angle, kFactor };
	}

	template<size_t Iterations = 15>
	strict_inline simd_float vector_call sin(simd_float radians)
	{
		auto result = cordicRotation<Iterations>(radians);
		return result[1] * result[2];
	}

	template<size_t Iterations = 15>
	strict_inline simd_float vector_call cos(simd_float radians)
	{
		auto result = cordicRotation<Iterations>(radians);
		return result[0] * result[2];
	}

	template<size_t Iterations = 15>
	strict_inline simd_float vector_call tan(simd_float radians)
	{
		auto result = cordicRotation<Iterations>(radians);
		return result[1] / result[0];
	}

	template<size_t Iterations = 15>
	strict_inline simd_float vector_call atan2(simd_float y, simd_float x)
	{
		return cordicVectoring<Iterations>(x, y)[1];
	}

	// Cos(x) + I * Sin(x)
	template<size_t Iterations = 15>
	strict_inline std::pair<simd_float, simd_float> vector_call cis(simd_float radians)
	{
		auto result = cordicRotation<Iterations>(radians);
		return { result[0] * result[2], result[1] * result[2] };
	}

	template<size_t Iterations = 15>
	strict_inline std::pair<simd_float, simd_float> vector_call phasor(simd_float real, simd_float imaginary)
	{
		auto result = cordicVectoring<Iterations>(real, imaginary);
		return { result[0] * result[2], result[1] };
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

	strict_inline simd_float vector_call complexPhase(simd_float value)
	{
	#if COMPLEX_SSE4_1
		simd_float real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexPhase not implemented yet");
	#endif

		return atan2(imaginary, real);
	}

	strict_inline simd_float vector_call complexReal(simd_float value)
	{
	#if COMPLEX_SSE4_1
		simd_float magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexReal not implemented yet");
	#endif

		return magnitude * cos(phase);
	}

	strict_inline simd_float vector_call complexImaginary(simd_float value)
	{
	#if COMPLEX_SSE4_1
		auto magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		auto phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexImaginary not implemented yet");
	#endif

		return simd_float::mul(magnitude, utils::sin(phase).value);
	}

	strict_inline void vector_call complexValueMerge(simd_float &one, simd_float &two)
	{
	#if COMPLEX_SSE4_1
		auto one_ = _mm_unpacklo_ps(one.value, two.value);
		two.value = _mm_unpackhi_ps(one.value, two.value);
		one.value = one_;
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexValueMerge not implemented yet");
	#endif
	}

	strict_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
	{
	#if COMPLEX_SSE4_1
		simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexImaginary not implemented yet");
	#endif
		auto magnitudePhasePairs = phasor(real, imaginary);
		complexValueMerge(magnitudePhasePairs.first, magnitudePhasePairs.second);
		one = magnitudePhasePairs.first;
		two = magnitudePhasePairs.second;
	}

	strict_inline void vector_call complexPolarToCart(simd_float &one, simd_float &two)
	{
	#if COMPLEX_SSE4_1
		simd_float magnitudesOne = _mm_shuffle_ps(one.value, one.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float magnitudesTwo = _mm_shuffle_ps(two.value, two.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float phases = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON complexImaginary not implemented yet");
	#endif
		auto realImaginaryPairs = cis(phases);
		complexValueMerge(realImaginaryPairs.first, realImaginaryPairs.second);
		one = realImaginaryPairs.first * magnitudesOne;
		two = realImaginaryPairs.second * magnitudesTwo;
	}

	template<auto ConversionFunction>
	strict_inline void convertBuffer(Framework::SimdBufferView<std::complex<float>, simd_float> source,
		Framework::SimdBuffer<std::complex<float>, simd_float> &destination)
	{
		for (u32 i = 0; i < source.getSimdChannels(); i++)
		{
			for (u32 j = 0; j < source.getSize(); j += 2)
			{
				auto one = source.readSimdValueAt(i * kComplexSimdRatio, j);
				auto two = source.readSimdValueAt(i * kComplexSimdRatio, j + 1);
				ConversionFunction(one, two);
				destination.writeSimdValueAt(one, i * kComplexSimdRatio, j);
				destination.writeSimdValueAt(two, i * kComplexSimdRatio, j + 1);
			}
		}
	}
}
