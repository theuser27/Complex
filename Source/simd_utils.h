/*
	==============================================================================

		simd_utils.h
		Created: 26 Aug 2021 3:53:04am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "utils.h"
#include "matrix.h"
#include "simd_buffer.h"
#include <complex>

namespace utils
{
		template<float(*func)(float)>
		force_inline simd_float map(simd_float value)
		{
			simd_float result;
			for (int i = 0; i < simd_float::kSize; ++i)
				result.set(i, func(value[i]));
			return result;
		}

		template<float(*func)(float, float)>
		static force_inline simd_float map(simd_float one, simd_float two) {
			simd_float result;
			for (int i = 0; i < kSize; ++i)
				result.set(i, func(one[i], two[i]));
			return result;
		}

		force_inline simd_float sin(simd_float value) { return map<sinf>(value); }
		force_inline simd_float cos(simd_float value) { return map<cosf>(value); }
		force_inline simd_float tan(simd_float value) { return map<tanf>(value); }
		force_inline simd_float atan2(simd_float one, simd_float two) { return map<atan2f>(one, two); }

		force_inline simd_float sqrt(simd_float value)
		{
		#if COMPLEX_AVX2
			return _mm256_sqrt_ps(value.value);
		#elif COMPLEX_SSE3
			return _mm_sqrt_ps(value.value);
		#elif COMPLEX_NEON
			return map<sqrtf>(value);
		#endif
		}

		force_inline simd_float interpolate(simd_float &from, simd_float &to, float t)
		{ return simd_float::mulAdd(from, to - from, t); }

		force_inline simd_float interpolate(simd_float &from, simd_float &to, simd_float &t)
		{ return simd_float::mulAdd(from, to - from, t); }

		force_inline simd_float interpolate(float from, float to, simd_float &t)
		{ return simd_float::mulAdd(from, to - from, t); }

		// t are clamped to the range of low and high
		force_inline simd_float interpolateClamp(simd_float &from, simd_float &to, 
			simd_float t, simd_float &low, simd_float &high)
		{
			t = simd_float::clamp(low, high, t);
			return simd_float::mulAdd(from, to - from, t);
		}


		force_inline Framework::matrix getLinearInterpolationMatrix(simd_float t)
		{ return Framework::matrix({ 0.0f, simd_float(1.0f) - t, t, 0.0f }); }

		force_inline Framework::matrix getCatmullInterpolationMatrix(simd_float t)
		{
			simd_float half_t = t * 0.5f;
			simd_float half_t2 = t * half_t;
			simd_float half_t3 = t * half_t2;
			simd_float half_three_t3 = half_t3 * 3.0f;

			return Framework::matrix({ half_t2 * 2.0f - half_t3 - half_t,
				simd_float::mulSub(half_three_t3, half_t2, 5.0f) + 1.0f,
				simd_float::mulAdd(half_t, half_t2, 4.0f) - half_three_t3,
				half_t3 - half_t2 });
		}

		force_inline simd_float toSimdFloatFromUnaligned(const float* unaligned)
		{
		#if COMPLEX_AVX2
			return _mm256_loadu_ps(unaligned);
		#elif COMPLEX_SSE3
			return _mm_loadu_ps(unaligned);
		#elif COMPLEX_NEON
			return vld1q_f32(unaligned);
		#endif
		}

		template<u32 size> requires requires () { size <= kSimdRatio; }
		force_inline Framework::matrix getValueMatrix(const float* buffer, simd_int indices)
		{
			std::array<simd_float, size> values;
			for (u32 i = 0; i < values.size(); i++)
				values[i] = toSimdFloatFromUnaligned(buffer + indices[i]);
			return Framework::matrix(values);
		}

		//// THISSSSSS
		template<u32 size> requires requires () { size <= kSimdRatio; }
		force_inline Framework::matrix getValueMatrix(const float* const* buffers, simd_int indices)
		{
			std::array<simd_float, size> values;
			for (u32 i = 0; i < values.size(); i++)
				values[i] = toSimdFloatFromUnaligned(buffers[i] + indices[i]);
			return Framework::matrix(values);
		}

		force_inline simd_float clamp(simd_float value, float min, float max)
		{ return simd_float::max(simd_float::min(value, max), min); }

		force_inline simd_float clamp(simd_float value, simd_float min, simd_float max)
		{ return simd_float::max(simd_float::min(value, max), min); }

		force_inline simd_int clamp(simd_int value, simd_int min, simd_int max)
		{ return simd_int::max(simd_int::min(value, max), min); }

		force_inline bool completelyEqual(simd_float left, simd_float right)
		{ return simd_float::notEqual(left, right).sum() == 0; }

		force_inline simd_float maskLoad(simd_float oneValue, simd_float zeroValue, simd_mask mask)
		{ return (oneValue & mask) + (zeroValue & ~mask); }

		force_inline simd_int maskLoad(simd_int oneValue, simd_int zeroValue, simd_mask mask)
		{ return (oneValue & mask) | (zeroValue & ~mask); }

		// 0s for loading values from one, 1s for loading values from two
		template<SimdValue SIMD>
		force_inline SIMD maskLoad(SIMD currentValue, SIMD newValue, simd_mask mask) 
		{
			SIMD oldValues = currentValue & ~mask;
			SIMD newValues = newValue & mask;
			return oldValues + newValues;
		}

		force_inline void copyBuffer(simd_float* dest, const simd_float* source, int size)
		{
			for (int i = 0; i < size; ++i)
				dest[i] = source[i];
		}

		force_inline void addBuffers(simd_float* dest, const simd_float* b1, const simd_float* b2, int size)
		{
			for (int i = 0; i < size; ++i)
				dest[i] = b1[i] + b2[i];
		}

		force_inline simd_float toFloat(simd_int integers)
		{
		#if COMPLEX_AVX2
			return _mm256_cvtepi32_ps(integers.value);
		#elif COMPLEX_SSE3
			return _mm_cvtepi32_ps(integers.value);
		#elif COMPLEX_NEON
			return vcvtq_f32_s32(vreinterpretq_s32_u32(integers.value));
		#endif
		}

		force_inline simd_int toInt(simd_float floats)
		{
		#if COMPLEX_AVX2
			return _mm256_cvtps_epi32(floats.value);
		#elif COMPLEX_SSE3
			return _mm_cvtps_epi32(floats.value);
		#elif COMPLEX_NEON
			return vreinterpretq_u32_s32(vcvtq_s32_f32(floats.value));
		#endif
		}

		force_inline simd_float reinterpretToFloat(simd_int value)
		{
		#if COMPLEX_AVX2
			return _mm256_castsi256_ps(value.value);
		#elif COMPLEX_SSE3
			return _mm_castsi128_ps(value.value);
		#elif COMPLEX_NEON
			return vreinterpretq_f32_u32(value.value);
		#endif
		}

		force_inline simd_int reinterpretToInt(simd_float value)
		{
		#if COMPLEX_AVX2
			return _mm256_castps_si256(value.value);
		#elif COMPLEX_SSE3
			return _mm_castps_si128(value.value);
		#elif COMPLEX_NEON
			return vreinterpretq_u32_f32(value.value);
		#endif
		}

		force_inline simd_float truncate(simd_float value)
		{ return toFloat(toInt(value)); }

		force_inline simd_float floor(simd_float value)
		{
			simd_float truncated = truncate(value);
			return truncated + (simd_float(-1.0f) & simd_float::greaterThan(truncated, value));
		}

		force_inline simd_float ceil(simd_float value)
		{
			simd_float truncated = truncate(value);
			return truncated + (simd_float(1.0f) & simd_float::lessThan(truncated, value));
		}

		force_inline simd_int floorToInt(simd_float value)
		{ return toInt(floor(value)); }

		force_inline simd_int ceilToInt(simd_float value)
		{ return toInt(ceil(value)); }

		force_inline simd_int roundToInt(simd_float value)
		{ return floorToInt(value + 0.5f); }

		force_inline simd_float round(simd_float value)
		{ return floor(value + 0.5f); }

		force_inline simd_float mod(simd_float value)
		{ return value - floor(value); }

		template<size_t shift>
		force_inline simd_int shiftRight(simd_int values)
		{
		#if COMPLEX_AVX2
			return _mm256_srli_epi32(values.value, shift);
		#elif COMPLEX_SSE3
			return _mm_srli_epi32(values.value, shift);
		#elif COMPLEX_NEON
			return vshrq_n_u32(values.value, shift);
		#endif
		}

		template<size_t shift>
		force_inline simd_int shiftLeft(simd_int values)
		{
		#if COMPLEX_AVX2
			return _mm256_slli_epi32(values.value, shift);
		#elif COMPLEX_SSE3
			return _mm_slli_epi32(values.value, shift);
		#elif COMPLEX_NEON
			return vshlq_n_u32(values.value, shift);
		#endif
		}

		template<size_t shift>
		force_inline simd_float shiftRight(simd_float value)
		{ return reinterpretToFloat(shiftRight<shift>(reinterpretToInt(value))); }
		
		template<size_t shift>
		force_inline simd_float shiftLeft(simd_float value)
		{ return reinterpretToFloat(shiftRight<shift>(reinterpretToInt(value))); }

		force_inline simd_float pow2ToFloat(simd_int value)
		{ return reinterpretToFloat(shiftLeft<23>(value + 127)); }

		
		
		namespace
		{
			constexpr float kDbGainConversionMult = 6.02059991329f;
			constexpr float kDbMagnitudeConversionMult = 1.0f / kDbGainConversionMult;
			constexpr float kExpConversionMult = 1.44269504089f;
			constexpr float kLogConversionMult = 0.69314718056f;
		}

		force_inline simd_float exp2(simd_float exponent)
		{
			static constexpr float kCoefficient0 = 1.0f;
			static constexpr float kCoefficient1 = 16970.0 / 24483.0;
			static constexpr float kCoefficient2 = 1960.0 / 8161.0;
			static constexpr float kCoefficient3 = 1360.0 / 24483.0;
			static constexpr float kCoefficient4 = 80.0 / 8161.0;
			static constexpr float kCoefficient5 = 32.0 / 24483.0;

			simd_int integer = roundToInt(exponent);
			simd_float t = exponent - utils::toFloat(integer);
			simd_float int_pow = utils::pow2ToFloat(integer);

			simd_float cubic = t * (t * (t * kCoefficient5 + kCoefficient4) + kCoefficient3) + kCoefficient2;
			simd_float interpolate = t * (t * cubic + kCoefficient1) + kCoefficient0;
			return int_pow * interpolate;
		}

		force_inline simd_float log2(simd_float value)
		{
			static constexpr float kCoefficient0 = -1819.0 / 651.0;
			static constexpr float kCoefficient1 = 5.0;
			static constexpr float kCoefficient2 = -10.0 / 3.0;
			static constexpr float kCoefficient3 = 10.0 / 7.0;
			static constexpr float kCoefficient4 = -1.0 / 3.0;
			static constexpr float kCoefficient5 = 1.0 / 31.0;

			simd_int floored_log2 = utils::shiftRight<23>(reinterpretToInt(value)) - 0x7f;
			simd_float t = (value & 0x7fffff) | (0x7f << 23);

			simd_float cubic = t * (t * (t * kCoefficient5 + kCoefficient4) + kCoefficient3) + kCoefficient2;
			simd_float interpolate = t * (t * cubic + kCoefficient1) + kCoefficient0;
			return utils::toFloat(floored_log2) + interpolate;
		}

		force_inline simd_float exp(simd_float exponent)
		{ return exp2(exponent * kExpConversionMult); }

		force_inline simd_float log(simd_float value)
		{ return log2(value) * kLogConversionMult; }

		force_inline simd_float pow(simd_float base, simd_float exponent)
		{ return exp2(log2(base) * exponent); }

		force_inline simd_float midiOffsetToRatio(simd_float note_offset)
		{ return exp2(note_offset * (1.0f / kNotesPerOctave)); }

		force_inline simd_float midiNoteToFrequency(simd_float note) 
		{ return midiOffsetToRatio(note) * kMidi0Frequency; }

		// fast approximation of the original equation
		force_inline simd_float magnitudeToDb(simd_float magnitude) 
		{	return log2(magnitude) * kDbGainConversionMult; }

		// fast approximation of the original equation
		force_inline simd_float dbToMagnitude(simd_float decibels) 
		{	return exp2(decibels * kDbMagnitudeConversionMult); }

		force_inline float exp2(float value)
		{
			simd_float input = value;
			simd_float result = exp2(input);
			return result[0];
		}

		force_inline float log2(float value)
		{
			simd_float input = value;
			simd_float result = log2(input);
			return result[0];
		}

		force_inline float exp(float exponent)
		{ return exp2(exponent * kExpConversionMult); }

		force_inline float log(float value)
		{ return log2(value) * kLogConversionMult; }

		force_inline float pow(float base, float exponent)
		{ return exp2(log2(base) * exponent); }

		// conditionally unsigns ints if they are < 0 and returns mask
		force_inline simd_mask unsignInt(simd_int &value)
		{
			simd_mask mask = simd_mask::equal(value & simd_mask(kSignMask), simd_mask(kSignMask));
			value = (value ^ (mask & (simd_int(-1)))) + (mask & simd_int(1));
			return mask;
		}

		// conditionally unsigns floats if they are < 0 and returns mask
		force_inline simd_mask unsignFloat(simd_float &value)
		{
			simd_mask mask = simd_mask::equal(toInt(value) & simd_mask(kSignMask), simd_mask(kSignMask));
			value = (value ^ (mask & simd_mask(kSignMask)));
			return mask;
		}
}