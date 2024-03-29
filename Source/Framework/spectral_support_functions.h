/*
	==============================================================================

		spectral_support_functions.h
		Created: 14 Sep 2021 12:55:12am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <complex>
#include "simd_utils.h"
#include "simd_buffer.h"

namespace utils
{
	// layout of complex cartesian and polar vectors is assumed to be
	// { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

	// this number of iterations produces results with max error of <= 0.01 degrees
	static constexpr size_t defaultCordicIterations = 12;
	
	/**
	 * \brief 
	 * \tparam Iterations number of iterations the algorithm should do
	 * \param radians [-inf; inf] phases the algorithm should use to create the cis pairs
	 * \return { unscaled cos part, unscaled sin part, scaling factor }
	 */
	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::array<simd_float, 3> vector_call cordicRotation(simd_float radians)
	{
		static const simd_mask kFloatExponentMask = 0x7f800000;
		static const simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

		static const simd_float kFactor = []()
		{
			float result = 1.0f;
			for (size_t i = 0; i < Iterations; i++)
				result *= 1.0f / std::sqrt(1.0f + std::exp2(-2.0f * (float)i));
			return result;
		}();

		// thetaDeltas[i] = atan(2^(-i))
		static const std::array<simd_float, Iterations + 1> thetaDeltas = []()
		{
			std::array<simd_float, Iterations + 1> angles{};
			for (i32 i = 0; i <= (i32)Iterations; i++)
				angles[i] = std::atan(std::exp2((float)(-i)));
			return angles;
		}();

		// correction for angles after +/-pi
		simd_float correctionRotations = simd_float::round(radians / (2.0f * kPi));
		radians -= (correctionRotations * 2.0f * kPi);

		// correction so that the algorithm works
		simd_mask sinMask = unsignSimd(radians);
		radians -= kPi * 0.5f;

		std::pair<simd_float, simd_float> result = { 0.0f, 1.0f };
		for (u32 i = 0; i <= (u32)Iterations; i++)
		{
			simd_mask signMask = getSign(radians);
			radians -= thetaDeltas[i] ^ signMask;

			simd_float prevX = result.first;
			simd_float prevY = result.second;

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
	 // TODO: fix cordic vectoring, produces inf db jumps (division by 0?)
	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::array<simd_float, 3> vector_call cordicVectoring(simd_float x, simd_float y)
	{
		static const simd_mask kFloatExponentMask = 0x7f800000;
		static const simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

		static const simd_float kFactor = []()
		{
			float result = 1.0f;
			for (size_t i = 0; i < Iterations; i++)
				result *= 1.0f / std::sqrt(1.0f + std::exp2(-2.0f * (float)i));
			return result;
		}();

		// thetaDeltas[i] = atan(2^(-i))
		static const std::array<simd_float, Iterations + 1> thetaDeltas = []()
		{
			std::array<simd_float, Iterations + 1> angles{};
			for (i32 i = 0; i <= (i32)Iterations; i++)
				angles[i] = std::atan(std::exp2((float)(-i)));
			return angles;
		}();

		simd_mask zeroOverZeroMask = simd_mask::equal(0, reinterpretToInt(x & kFloatExponentMask)) & 
			simd_mask::equal(0, reinterpretToInt(y & kFloatExponentMask));

		simd_mask xNegativeMask = unsignSimd(x);
		simd_mask signMask = getSign(y);
		simd_float angle = (simd_float(kPi) ^ signMask) & simd_mask::equal(xNegativeMask, kSignMask);
		for (u32 i = 0; i <= (u32)Iterations; i++)
		{
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

			signMask = getSign(y);
		}

		return { 
			maskLoad(x, simd_float(0.0f), zeroOverZeroMask), 
			maskLoad(angle, simd_float(0.0f), zeroOverZeroMask), 
			kFactor };
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline simd_float vector_call sin(simd_float radians)
	{
	#ifdef COMPLEX_INTEL_SVML
		return _mm_sin_ps(radians.value);
	#else
		auto result = cordicRotation<Iterations>(radians);
		return result[1] * result[2];
	#endif
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline simd_float vector_call cos(simd_float radians)
	{
	#ifdef COMPLEX_INTEL_SVML
		return _mm_cos_ps(radians.value);
	#else
		auto result = cordicRotation<Iterations>(radians);
		return result[0] * result[2];
	#endif
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline simd_float vector_call tan(simd_float radians)
	{
	#ifdef COMPLEX_INTEL_SVML
		return _mm_tan_ps(radians.value);
	#else
		auto result = cordicRotation<Iterations>(radians);
		return result[1] / result[0];
	#endif
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline simd_float vector_call atan2(simd_float y, simd_float x)
	{
	#ifdef COMPLEX_INTEL_SVML
		return _mm_atan2_ps(y.value, x.value);
	#else
		return cordicVectoring<Iterations>(x, y)[1];
	#endif
	}

	// Cos(x) + I * Sin(x)
	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::pair<simd_float, simd_float> vector_call cis(simd_float radians)
	{
	#ifdef COMPLEX_INTEL_SVML
		simd_float cos;
		simd_float sin = _mm_sincos_ps(&cos.value, radians.value);
		return { cos, sin };
	#else
		auto result = cordicRotation<Iterations>(radians);
		return { result[0] * result[2], result[1] * result[2] };
	#endif
	}

	strict_inline std::pair<simd_float, simd_float> vector_call cisFast(simd_float radians)
	{
		// routine is based on Bhaskara I's sine approximation
		radians /= kPi;

		// correction for angles after +/-pi
		simd_float correctionRotations = simd_float::round(radians * 0.5f);
		radians -= (correctionRotations * 2.0f);

		simd_mask sinSign = simd_float::lessThan(radians, 0.0f);
		simd_float sinPosition = radians - 0.5f + (simd_float(1.0f) & sinSign);
		sinPosition *= sinPosition;

		simd_mask cosSign = simd_float::greaterThan(simd_float::abs(radians), 0.5f);
		simd_float cosPosition = radians - (simd_float(1.0f) & cosSign ^ getSign(radians));
		cosPosition *= cosPosition;

		simd_float cos = ((simd_float(1.0f) - cosPosition * 4.0f) * reciprocal(cosPosition + 1.0f)) ^ (cosSign & kSignMask);
		simd_float sin = ((simd_float(1.0f) - sinPosition * 4.0f) * reciprocal(sinPosition + 1.0f)) ^ (sinSign & kSignMask);

		return { cos, sin };
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::pair<simd_float, simd_float> vector_call phasor(simd_float real, simd_float imaginary)
	{
	#ifdef COMPLEX_INTEL_SVML
		return { 
			simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), 
			simd_float(_mm_atan2_ps(imaginary.value, real.value))
		};
	#else
		auto result = cordicVectoring<Iterations>(real, imaginary);
		return { result[0] * result[2], result[1] };
	#endif
	}

	strict_inline std::pair<simd_float, simd_float> vector_call phasorFast(simd_float real, simd_float imaginary)
	{
		// https://www.desmos.com/calculator/oxzturzmjn
		// max error ~= 0.01 degrees
		static const simd_float a = 0.35496f;
		static const simd_float b = -0.0815f;

		simd_float yxDiv = imaginary / real;
		simd_float yxDivSqr = yxDiv * yxDiv;
		simd_float xyDiv = reciprocal(yxDiv);
		simd_float xyDivSqr = xyDiv * xyDiv;

		simd_float firstHalf = yxDiv / (yxDivSqr * simd_float::abs(yxDiv) * b + yxDivSqr * a + 1.0f);
		simd_float secondHalf = (simd_float(kPi * 0.5f) ^ getSign(xyDiv)) - xyDiv / (xyDivSqr * simd_float::abs(xyDiv) * b + xyDivSqr * a + 1.0f);
		simd_float angle = maskLoad(firstHalf, secondHalf, simd_float::greaterThan(simd_float::abs(yxDiv), 1.0f));

		simd_mask realEqualZeroMask = simd_float::equal(real, 0.0f);
		simd_mask imaginaryEqualZeroMask = simd_float::equal(imaginary, 0.0f);
		simd_float extraShift = (simd_float(kPi) & ~realEqualZeroMask) & simd_float::lessThanOrEqual(real, 0.0f);
		extraShift ^= getSign(imaginary);

		angle += extraShift;
		angle &= ~(realEqualZeroMask & imaginaryEqualZeroMask);

		return { simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), angle };
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

	strict_inline simd_float vector_call complexPhase(simd_float one, simd_float two)
	{
	#if COMPLEX_SSE4_1
		simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
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
		auto [magnitude, phase] = phasorFast(real, imaginary);
		complexValueMerge(magnitude, phase);
		one = magnitude;
		two = phase;
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
		auto [real, imaginary] = cisFast(phases);
		complexValueMerge(real, imaginary);
		one = real * magnitudesOne;
		two = imaginary * magnitudesTwo;
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
			// dc and nyquist don't have "phase" so the previously stored result is wrong
			destination.writeSimdValueAt(source.readSimdValueAt(i * kComplexSimdRatio, 0), i * kComplexSimdRatio, 0);
		}
	}
}
