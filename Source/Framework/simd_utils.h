/*
	==============================================================================

		simd_utils.h
		Created: 26 Aug 2021 3:53:04am
		Author:  theuser27

	==============================================================================
*/

#pragma once

//#include "utils.h"
#include "matrix.h"

namespace utils
{
	strict_inline simd_float vector_call lerp(simd_float from, simd_float to, simd_float t) noexcept
	{ return simd_float::mulAdd(from, to - from, t); }

	strict_inline Framework::Matrix vector_call getLinearInterpolationMatrix(simd_float t) noexcept
	{ return Framework::Matrix({ 0.0f, simd_float(1.0f) - t, t, 0.0f }); }

	strict_inline Framework::Matrix vector_call getCatmullInterpolationMatrix(simd_float t) noexcept
	{
		simd_float half_t = t * 0.5f;
		simd_float half_t2 = t * half_t;
		simd_float half_t3 = t * half_t2;
		simd_float half_three_t3 = half_t3 * 3.0f;

		return Framework::Matrix({ 
			simd_float::mulAdd(-half_t3, half_t2, 2.0f) - half_t,
			simd_float::mulSub(half_three_t3, half_t2, 5.0f) + 1.0f,
			simd_float::mulAdd(half_t, half_t2, 4.0f) - half_three_t3,
			half_t3 - half_t2 });
	}

	strict_inline simd_float vector_call toSimdFloatFromUnaligned(const float *unaligned) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_loadu_ps(unaligned);
	#elif COMPLEX_NEON
		return vld1q_f32(unaligned);
	#endif
	}

	template<u32 size>
	strict_inline Framework::Matrix vector_call getValueMatrix(const float *buffer, simd_int indices) noexcept
	{
		static_assert(size <= kSimdRatio, "Size of matrix cannot be larger than size of simd package");
		std::array<simd_float, size> values;
		for (u32 i = 0; i < values.size(); i++)
			values[i] = toSimdFloatFromUnaligned(buffer + indices[i]);
		return Framework::Matrix(values);
	}

	template<u32 size>
	strict_inline Framework::Matrix vector_call getValueMatrix(const float *const *buffers, simd_int indices) noexcept
	{
		static_assert(size <= kSimdRatio, "Size of matrix cannot be larger than size of simd package");
		std::array<simd_float, size> values;
		for (u32 i = 0; i < values.size(); i++)
			values[i] = toSimdFloatFromUnaligned(buffers[i] + indices[i]);
		return Framework::Matrix(values);
	}

	strict_inline bool vector_call completelyEqual(simd_float left, simd_float right) noexcept
	{ return simd_float::notEqual(left, right).sum() == 0; }

	strict_inline bool vector_call completelyEqual(simd_int left, simd_int right) noexcept
	{ return simd_int::notEqual(left, right).sum() == 0; }

	// 0s for loading values from one, 1s for loading values from two
	template<CommonConcepts::SimdValue SIMD>
	strict_inline SIMD vector_call maskLoad(SIMD zeroValue, SIMD oneValue, simd_mask mask) noexcept
	{
		SIMD oldValues = zeroValue & ~mask;
		SIMD newValues = oneValue & mask;
		return oldValues + newValues;
	}

	strict_inline void vector_call copyBuffer(simd_float *dest, 
		const simd_float *source, int size) noexcept
	{
		for (int i = 0; i < size; ++i)
			dest[i] = source[i];
	}

	strict_inline void vector_call addBuffers(simd_float *dest, 
		const simd_float *b1, const simd_float *b2, int size) noexcept
	{
		for (int i = 0; i < size; ++i)
			dest[i] = b1[i] + b2[i];
	}

	strict_inline simd_float vector_call toFloat(simd_int integers) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_cvtepi32_ps(integers.value);
	#elif COMPLEX_NEON
		return vcvtq_f32_s32(vreinterpretq_s32_u32(integers.value));
	#endif
	}

	strict_inline simd_int vector_call toInt(simd_float floats) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_cvtps_epi32(floats.value);
	#elif COMPLEX_NEON
		return vreinterpretq_u32_s32(vcvtq_s32_f32(floats.value));
	#endif
	}

	strict_inline simd_float vector_call reinterpretToFloat(simd_int value) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_castsi128_ps(value.value);
	#elif COMPLEX_NEON
		return vreinterpretq_f32_u32(value.value);
	#endif
	}

	strict_inline simd_int vector_call reinterpretToInt(simd_float value) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_castps_si128(value.value);
	#elif COMPLEX_NEON
		return vreinterpretq_u32_f32(value.value);
	#endif
	}

	strict_inline simd_float vector_call getDecimalPlaces(simd_float value) noexcept
	{ return value - simd_float::floor(value); }

	strict_inline simd_float modOnce(simd_float value, simd_float mod) noexcept
	{
		simd_mask lessMask = simd_float::lessThanOrEqual(value, mod);
		simd_float lower = value - mod;
		return maskLoad(lower, value, lessMask);
	}

	template<size_t shift>
	strict_inline simd_int vector_call shiftRight(simd_int values) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_srli_epi32(values.value, shift);
	#elif COMPLEX_NEON
		return vshrq_n_u32(values.value, shift);
	#endif
	}

	template<size_t shift>
	strict_inline simd_int vector_call shiftLeft(simd_int values) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_slli_epi32(values.value, shift);
	#elif COMPLEX_NEON
		return vshlq_n_u32(values.value, shift);
	#endif
	}

	template<size_t shift>
	strict_inline simd_float vector_call shiftRight(simd_float value) noexcept
	{ return reinterpretToFloat(shiftRight<shift>(reinterpretToInt(value))); }

	template<size_t shift>
	strict_inline simd_float vector_call shiftLeft(simd_float value) noexcept
	{ return reinterpretToFloat(shiftLeft<shift>(reinterpretToInt(value))); }

	strict_inline simd_float vector_call pow2ToFloat(simd_int value) noexcept
	{ return reinterpretToFloat(shiftLeft<23>(value + 127)); }



	strict_inline simd_float vector_call exp2(simd_float exponent) noexcept
	{
		// taylor expansion of 2^x at 0
		// coefficients are (ln(2)^n) / n!
		static constexpr float kCoefficient0 = 1.0f;
		static constexpr float kCoefficient1 = 16970.0f / 24483.0f;
		static constexpr float kCoefficient2 = 1960.0f / 8161.0f;
		static constexpr float kCoefficient3 = 1360.0f / 24483.0f;
		static constexpr float kCoefficient4 = 80.0f / 8161.0f;
		static constexpr float kCoefficient5 = 32.0f / 24483.0f;

		// the closer the exponent is to a whole number, the more accurate it's going to be
		// since it only requires to add it the overall floating point exponent
		simd_float rounded = simd_float::round(exponent);
		simd_float t = exponent - rounded;
		simd_float int_pow = pow2ToFloat(toInt(rounded));

		// we exp2 whatever decimal number is left with the taylor series
		// the domain we're in is [0.0f, 0.5f], we don't expect negative numbers
		simd_float interpolate = simd_float::mulAdd(kCoefficient2, t, simd_float::mulAdd(kCoefficient3,
			t, simd_float::mulAdd(kCoefficient4, t, kCoefficient5)));
		interpolate = simd_float::mulAdd(kCoefficient0, t, simd_float::mulAdd(kCoefficient1, t, interpolate));
		return int_pow * interpolate;
	}

	strict_inline simd_float vector_call log2(simd_float value) noexcept
	{
		// i have no idea how these coefficients have been derived
		static constexpr float kCoefficient0 = -1819.0f / 651.0f;
		static constexpr float kCoefficient1 = 5.0f;
		static constexpr float kCoefficient2 = -10.0f / 3.0f;
		static constexpr float kCoefficient3 = 10.0f / 7.0f;
		static constexpr float kCoefficient4 = -1.0f / 3.0f;
		static constexpr float kCoefficient5 = 1.0f / 31.0f;

		// effectively log2s only the exponent; gets it in terms an int
		simd_int floored_log2 = shiftRight<23>(reinterpretToInt(value)) - 0x7f;
		// 0x7fffff masks the entire mantissa
		// then we bring the exponent to 2^0 to get the entire number between [1, 2]
		simd_float t = (value & 0x7fffff) | (0x7f << 23);

		// we log2 the mantissa with the taylor series coefficients
		simd_float interpolate = simd_float::mulAdd(kCoefficient2, t, (simd_float::mulAdd(kCoefficient3, t,
			simd_float::mulAdd(kCoefficient4, t, kCoefficient5))));
		interpolate = simd_float::mulAdd(kCoefficient0, t, simd_float::mulAdd(kCoefficient1, t, interpolate));

		// we add the int with the mantissa to get our final result
		return toFloat(floored_log2) + interpolate;
	}

	strict_inline simd_float vector_call exp(simd_float exponent) noexcept
	{ return exp2(exponent * kExpConversionMult); }

	strict_inline simd_float vector_call log(simd_float value) noexcept
	{ return log2(value) * kLogConversionMult; }

	strict_inline simd_float vector_call pow(simd_float base, simd_float exponent) noexcept
	{ return exp2(log2(base) * exponent); }

	strict_inline simd_float vector_call midiOffsetToRatio(simd_float note_offset) noexcept
	{ return exp2(note_offset * (1.0f / kNotesPerOctave)); }

	strict_inline simd_float vector_call midiNoteToFrequency(simd_float note) noexcept
	{ return midiOffsetToRatio(note) * kMidi0Frequency; }

	// fast approximation of the original equation
	strict_inline simd_float vector_call amplitudeToDb(simd_float magnitude) noexcept
	{ return log2(magnitude) * kAmplitudeToDbConversionMult; }

	// fast approximation of the original equation
	strict_inline simd_float vector_call dbToAmplitude(simd_float decibels) noexcept
	{ return exp2(decibels * kDbToAmplitudeConversionMult); }

	strict_inline simd_float vector_call normalisedToDb(simd_float normalised, float maxDb) noexcept
	{ return pow(maxDb + 1.0f, normalised) - 1.0f; }

	strict_inline simd_float vector_call dbToNormalised(simd_float db, float maxDb) noexcept
	{ return log2(db + 1.0f) / std::log2(maxDb); }

	strict_inline simd_float vector_call normalisedToFrequency(simd_float normalised, float sampleRate) noexcept
	{
		static float lastSampleRate = kDefaultSampleRate;
		static float lastLog2 = (float)std::log2(lastSampleRate / (kMinFrequency * 2.0));

		if (lastSampleRate != sampleRate)
		{
			lastSampleRate = sampleRate;
			lastLog2 = (float)std::log2(lastSampleRate / (2.0 * kMinFrequency));
		}

		return exp2(simd_float(lastLog2) * normalised) * kMinFrequency;
	}

	strict_inline simd_float vector_call frequencyToNormalised(simd_float frequency, float sampleRate) noexcept
	{
		static float lastSampleRate = kDefaultSampleRate;
		static float lastLog2 = (float)std::log2(lastSampleRate / (kMinFrequency * 2.0));

		if (lastSampleRate != sampleRate)
		{
			lastSampleRate = sampleRate;
			lastLog2 = (float)std::log2((double)lastSampleRate / (2.0 * kMinFrequency));
		}
		return log2(frequency / kMinFrequency) / simd_float(lastLog2);
	}

	strict_inline simd_float vector_call binToNormalised(simd_float bin, u32 FFTSize, float sampleRate) noexcept
	{
		static float lastSampleRate = kDefaultSampleRate;
		static float lastLog2 = (float)std::log2(lastSampleRate / (kMinFrequency * 2.0));

		if (lastSampleRate != sampleRate)
		{
			lastSampleRate = sampleRate;
			lastLog2 = (float)std::log2((double)lastSampleRate / (2.0 * kMinFrequency));
		}

		// for 0 logarithm doesn't produce valid values
		// so we mask that with dummy values to not get errors
		simd_mask zeroMask = simd_float::equal(bin, 0.0f);
		bin = maskLoad(bin, simd_float(1.0f), zeroMask);
		simd_float result = log2((bin / (float)FFTSize) * (sampleRate / (float)kMinFrequency)) / lastLog2;

		return maskLoad(result, simd_float(0.0f), zeroMask);
	}

	strict_inline simd_float vector_call normalisedToBin(simd_float normalised, u32 FFTSize, float sampleRate) noexcept
	{
		static float lastSampleRate = kDefaultSampleRate;
		static float lastLog2 = (float)std::log2(lastSampleRate / (kMinFrequency * 2.0));

		if (lastSampleRate != sampleRate)
		{
			lastSampleRate = sampleRate;
			lastLog2 = (float)std::log2((double)lastSampleRate / (2.0 * kMinFrequency));
		}

		return simd_float::ceil(exp2(normalised * lastLog2) * ((float)FFTSize * (float)kMinFrequency / sampleRate)) - 1.0f;
	}

	strict_inline float exp2(float value) noexcept
	{
		simd_float input = value;
		simd_float result = exp2(input);
		return result[0];
	}

	strict_inline float log2(float value) noexcept
	{
		simd_float input = value;
		simd_float result = log2(input);
		return result[0];
	}

	strict_inline float exp(float exponent) noexcept
	{ return exp2(exponent * kExpConversionMult); }

	strict_inline float log(float value) noexcept
	{ return log2(value) * kLogConversionMult; }

	strict_inline float pow(float base, float exponent) noexcept
	{ return exp2(log2(base) * exponent); }

	strict_inline simd_float powerScale(simd_float value, simd_float power)
	{
		static constexpr float kMinPowerMag = 0.005f;
		simd_mask zero_mask = simd_float::lessThan(power, kMinPowerMag) & simd_float::lessThan(-power, kMinPowerMag);
		simd_float numerator = exp(power * value) - 1.0f;
		simd_float denominator = exp(power) - 1.0f;
		simd_float result = numerator / denominator;
		return maskLoad(result, value, zero_mask);
	}

	strict_inline float powerScale(float value, float power)
	{
		static constexpr float kMinPower = 0.01f;

		if (fabsf(power) < kMinPower)
			return value;

		float numerator = exp(power * value) - 1.0f;
		float denominator = exp(power) - 1.0f;
		return numerator / denominator;
	}

	strict_inline simd_mask vector_call getSign(simd_int value) noexcept
	{
		static const simd_mask signMask = kSignMask;
		return value & signMask;
	}

	strict_inline simd_mask vector_call getSign(simd_float value) noexcept
	{
		static const simd_mask signMask = kSignMask;
		return reinterpretToInt(value) & signMask;
	}

	// conditionally unsigns ints if they are negative and returns full mask where values are negative
	strict_inline simd_mask vector_call unsignSimd(simd_int &value, bool returnFullMask = false) noexcept
	{
		static const simd_mask signMask = kSignMask;
		simd_mask mask = simd_mask::equal(value & signMask, signMask);
		value = maskLoad(value, ~value - 1, mask);
		return (returnFullMask) ? simd_mask::equal(mask, signMask) : mask;
	}

	// conditionally unsigns floats if they are negative and returns full mask where values are negative
	strict_inline simd_mask vector_call unsignSimd(simd_float &value, bool returnFullMask = false) noexcept
	{
		static const simd_mask signMask = kSignMask;
		simd_mask mask = reinterpretToInt(value) & signMask;
		value ^= mask;
		return (returnFullMask) ? simd_mask::equal(mask, signMask) : mask;
	}

	strict_inline simd_float vector_call getStereoDifference(simd_float value) noexcept
	{
	#if COMPLEX_SSE4_1
		return (value - simd_float(_mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 3, 0, 1)))) * 0.5f;
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON getStereoDifference not implemented yet");
	#endif
	}

	strict_inline simd_int vector_call getStereoDifference(simd_int value) noexcept
	{
	#if COMPLEX_SSE4_1
		return shiftRight<1>(value - simd_int(_mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 3, 0, 1))));
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON getStereoDifference not implemented yet");
	#endif
	}

	strict_inline bool vector_call areAllElementsSame(simd_int value) noexcept
	{
	#if COMPLEX_SSE4_1
		simd_mask mask = value ^ simd_int(_mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 3, 0, 1)));
		mask |= value ^ simd_int(_mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3)));
		return mask.sum() == 0;
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON areAllElementsSame not implemented yet");
	#endif
	}

	strict_inline bool vector_call areAllElementsSame(simd_float value) noexcept
	{
		simd_int intValue = reinterpretToInt(value);
	#if COMPLEX_SSE4_1
		simd_mask mask = intValue ^ simd_int(_mm_shuffle_epi32(intValue.value, _MM_SHUFFLE(2, 3, 0, 1)));
		mask |= intValue ^ simd_int(_mm_shuffle_epi32(intValue.value, _MM_SHUFFLE(0, 1, 2, 3)));
		return mask.sum() == 0;
	#elif COMPLEX_NEON
		static_assert(false, "ARM NEON areAllElementsSame not implemented yet");
	#endif
	}

}