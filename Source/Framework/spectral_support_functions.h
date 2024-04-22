/*
	==============================================================================

		spectral_support_functions.h
		Created: 14 Sep 2021 12:55:12am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <Third Party/gcem/gcem.hpp>
#include "sync_primitives.h"
#include "simd_buffer.h"
#include "simd_utils.h"

namespace utils
{
	// layout of complex cartesian and polar vectors is assumed to be
	// { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

	// this number of iterations produces results with max error of <= 0.01 degrees
	inline constexpr size_t defaultCordicIterations = 12;
	inline constexpr simd_mask kMagnitudeMask = { kFullMask, 0U };
	inline constexpr simd_mask kPhaseMask = { 0U, kFullMask };

	/**
	 * \brief 
	 * \tparam Iterations number of iterations the algorithm should do
	 * \param radians [-inf; inf] phases the algorithm should use to create the cis pairs
	 * \return { unscaled cos part, unscaled sin part, scaling factor }
	 */
	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::array<simd_float, 3> vector_call cordicRotation(simd_float radians)
	{
		static constexpr simd_mask kFloatExponentMask = 0x7f800000;
		static constexpr simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

		static const simd_float kFactor = []()
		{
			float result = 1.0f;
			for (size_t i = 0; i < Iterations; i++)
				result *= 1.0f / gcem::sqrt(1.0f + std::exp2(-2.0f * (float)i));
			return result;
		}();

		// thetaDeltas[i] = atan(2^(-i))
		static const std::array<simd_float, Iterations + 1> thetaDeltas = []()
		{
			std::array<simd_float, Iterations + 1> angles{};
			for (i32 i = 0; i <= (i32)Iterations; i++)
				angles[i] = gcem::atan(std::exp2((float)(-i)));
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
		static constexpr simd_mask kFloatExponentMask = 0x7f800000;
		static constexpr simd_mask kNotFloatExponentMask = ~kFloatExponentMask;

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

		return { maskLoad(x, simd_float(0.0f), zeroOverZeroMask), 
			maskLoad(angle, simd_float(0.0f), zeroOverZeroMask), kFactor };
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

	strict_inline simd_float vector_call atan2Fast(simd_float y, simd_float x)
	{
		// https://www.desmos.com/calculator/oxzturzmjn
		// max error ~= 0.01 degrees
		static constexpr simd_float a = 0.35496f;
		static constexpr simd_float b = -0.0815f;

		simd_float yxDiv = y / x;
		simd_float yxDivSqr = yxDiv * yxDiv;
		simd_float xyDiv = reciprocal(yxDiv);
		simd_float xyDivSqr = xyDiv * xyDiv;

		simd_float firstHalf = yxDiv / (yxDivSqr * simd_float::abs(yxDiv) * b + yxDivSqr * a + 1.0f);
		simd_float secondHalf = (simd_float(kPi * 0.5f) ^ getSign(xyDiv)) - xyDiv / (xyDivSqr * simd_float::abs(xyDiv) * b + xyDivSqr * a + 1.0f);
		simd_float angle = maskLoad(firstHalf, secondHalf, simd_float::greaterThan(simd_float::abs(yxDiv), 1.0f));

		simd_mask realEqualZeroMask = simd_float::equal(x, 0.0f);
		simd_mask imaginaryEqualZeroMask = simd_float::equal(y, 0.0f);
		simd_float extraShift = (simd_float(kPi) & ~realEqualZeroMask) & simd_float::lessThanOrEqual(x, 0.0f);
		extraShift ^= getSign(y);

		angle += extraShift;
		angle &= ~(realEqualZeroMask & imaginaryEqualZeroMask);

		return angle;
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
		// pade approximants of sine
		static constexpr simd_float kNum1 = 166320.0f * kPi;
		static constexpr simd_float kNum2 = -22260.0f * kPi * kPi * kPi;
		static constexpr simd_float kNum3 = 551.0f * kPi * kPi * kPi * kPi * kPi;
		static constexpr simd_float kDen1 = 166320.0f;
		static constexpr simd_float kDen2 = 5460.0f * kPi * kPi;
		static constexpr simd_float kDen3 = 75.0f * kPi * kPi * kPi * kPi;

		// correction for angles after +/-pi, normalises to +/-1
		radians /= kPi;
		radians -= simd_float::round(radians * 0.5f) * 2.0f;

		simd_mask cosSign = simd_float::greaterThanOrEqual(radians, 0.0f);
		simd_float cosPosition = radians + 0.5f - (simd_float(1.0f) & cosSign);
		simd_float cosPosition2 = cosPosition * cosPosition;
		
		simd_mask sinSign = simd_float::greaterThan(simd_float::abs(radians), 0.5f);
		simd_float sinPosition = radians - ((simd_float(1.0f) & sinSign) ^ getSign(radians));
		simd_float sinPosition2 = sinPosition * sinPosition;

		simd_float cos = (cosPosition * simd_float::mulAdd(kNum1, cosPosition2, simd_float::mulAdd(kNum2, cosPosition2, kNum3))) /
		                                simd_float::mulAdd(kDen1, cosPosition2, simd_float::mulAdd(kDen2, cosPosition2, kDen3));
		simd_float sin = (sinPosition * simd_float::mulAdd(kNum1, sinPosition2, simd_float::mulAdd(kNum2, sinPosition2, kNum3))) /
		                                simd_float::mulAdd(kDen1, sinPosition2, simd_float::mulAdd(kDen2, sinPosition2, kDen3));

		return { cos ^ (cosSign & kSignMask), sin ^ (sinSign & kSignMask) };
	}

	template<size_t Iterations = defaultCordicIterations>
	strict_inline std::pair<simd_float, simd_float> vector_call phasor(simd_float real, simd_float imaginary)
	{
	#ifdef COMPLEX_INTEL_SVML
		return { simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), 
			       simd_float(_mm_atan2_ps(imaginary.value, real.value)) };
	#else
		auto result = cordicVectoring<Iterations>(real, imaginary);
		return { result[0] * result[2], result[1] };
	#endif
	}

	strict_inline std::pair<simd_float, simd_float> vector_call phasorFast(simd_float real, simd_float imaginary)
	{ return { simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), atan2Fast(imaginary, real) }; }

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
		static_assert(false, "not implemented yet");
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
		static_assert(false, "not implemented yet");
	#endif
	}

	strict_inline simd_float vector_call complexMagnitude(simd_float value, bool toSqrt)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		auto imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif
		value = simd_float::mulAdd(simd_float::mul(real, real), imaginary, imaginary);
		return (toSqrt) ? simd_float::sqrt(value) : value;
	}

	strict_inline simd_float vector_call complexMagnitude(const std::array<simd_float, kComplexSimdRatio> &values, bool toSqrt)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(2, 0, 2, 0));
		auto imaginary = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif
		simd_float value = simd_float::mulAdd(simd_float::mul(real, real), imaginary, imaginary);
		return (toSqrt) ? simd_float::sqrt(value) : value;
	}

	strict_inline simd_float vector_call complexPhase(simd_float value)
	{
	#if COMPLEX_SSE4_1
		simd_float real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif

		return atan2(imaginary, real);
	}

	strict_inline simd_float vector_call complexPhase(const std::array<simd_float, kComplexSimdRatio> &values)
	{
	#if COMPLEX_SSE4_1
		auto real = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(2, 0, 2, 0));
		auto imaginary = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif

		return atan2Fast(imaginary, real);
	}

	strict_inline simd_float vector_call complexReal(simd_float value)
	{
	#if COMPLEX_SSE4_1
		simd_float magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		simd_float phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif

		return magnitude * cos(phase);
	}

	strict_inline simd_float vector_call complexImaginary(simd_float value)
	{
	#if COMPLEX_SSE4_1
		auto magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
		auto phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
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
		static_assert(false, "not implemented yet");
	#endif
	}

	strict_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
	{
	#if COMPLEX_SSE4_1
		simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
		simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
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
		static_assert(false, "not implemented yet");
	#endif
		auto [real, imaginary] = cisFast(phases);
		complexValueMerge(real, imaginary);
		one = real * magnitudesOne;
		two = imaginary * magnitudesTwo;
	}

	strict_inline void vector_call complexTranspose(std::array<simd_float, kComplexSimdRatio> &rows)
	{
	#if COMPLEX_SSE4_1
		auto low = _mm_movelh_ps(rows[0].value, rows[1].value);
		auto high = _mm_movehl_ps(rows[1].value, rows[0].value);
		rows[0].value = low;
		rows[1].value = high;
	#elif COMPLEX_NEON
		static_assert(false, "not implemented yet");
	#endif
	}

	template<auto ConversionFunction>
	strict_inline void convertBuffer(const Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
		Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, size_t size)
	{
		const simd_float *sourceData = source.getData().getData().get();
		simd_float *destinationData = destination.getData().getData().get();
		u32 sourceSize = source.getSize();
		u32 destinationSize = destination.getSize();
		
		for (u32 i = 0; i < source.getSimdChannels(); i++)
		{
			// we skip dc and nyquist since they don't have "phase" 
			// so we can do the next pair and then do 2 pairs at once
			destinationData[destinationSize * i] = sourceData[sourceSize * i];
			simd_float secondPair = sourceData[sourceSize * i + 1];
			simd_float dummy = 0.0f;
			ConversionFunction(secondPair, dummy);
			destinationData[destinationSize * i + 1] = secondPair;

			for (u32 j = 2; j < size; j += 2)
			{
				simd_float one = sourceData[sourceSize * i + j];
				simd_float two = sourceData[sourceSize * i + j + 1];
				ConversionFunction(one, two);
				destinationData[destinationSize * i + j] = one;
				destinationData[destinationSize * i + j + 1] = two;
			}
		}
	}

	template<auto ConversionFunction>
	strict_inline void convertBufferInPlace(Framework::SimdBuffer<Framework::complex<float>, simd_float> &buffer, size_t size)
	{
		simd_float *data = buffer.getData().getData().get();
		u32 bufferSize = buffer.getSize();

		for (u32 i = 0; i < buffer.getSimdChannels(); i++)
		{
			// we skip dc and nyquist since they don't have "phase" 
			// so we can do the next pair and then do 2 pairs at once
			simd_float secondPair = data[bufferSize * i + 1];
			simd_float dummy = 0.0f;
			ConversionFunction(secondPair, dummy);
			data[bufferSize * i + 1] = secondPair;

			for (u32 j = 2; j < size; j += 2)
			{
				simd_float one = data[bufferSize * i + j];
				simd_float two = data[bufferSize * i + j + 1];
				ConversionFunction(one, two);
				data[bufferSize * i + j] = one;
				data[bufferSize * i + j + 1] = two;
			}
		}
	}
}
