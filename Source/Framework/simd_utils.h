/*
	==============================================================================

		simd_utils.h
		Created: 26 Aug 2021 3:53:04am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "utils.h"
#include "matrix.h"
#include <complex>
#include <Framework/parameters.h>

namespace utils
{
	template<float(*func)(float)>
	strict_inline simd_float vector_call map(simd_float value) noexcept
	{
		simd_float result;
		for (int i = 0; i < simd_float::kSize; ++i)
			result.set(i, func(value[i]));
		return result;
	}

	template<float(*func)(float, float)>
	static strict_inline simd_float vector_call map(simd_float one, simd_float two) noexcept
	{
		simd_float result;
		for (int i = 0; i < simd_float::kSize; ++i)
			result.set(i, func(one[i], two[i]));
		return result;
	}

	strict_inline simd_float vector_call sin(simd_float value) noexcept { return map<sinf>(value); }
	strict_inline simd_float vector_call cos(simd_float value) noexcept { return map<cosf>(value); }
	strict_inline simd_float vector_call tan(simd_float value) noexcept { return map<tanf>(value); }
	strict_inline simd_float vector_call atan2(simd_float one, simd_float two) noexcept { return map<atan2f>(one, two); }

	strict_inline simd_float vector_call sqrt(simd_float value) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_sqrt_ps(value.value);
	#elif COMPLEX_NEON
		return map<sqrtf>(value);
	#endif
	}

	strict_inline simd_float vector_call interpolate(simd_float &from, simd_float &to, float t) noexcept
	{ return simd_float::mulAdd(from, to - from, t); }

	strict_inline simd_float vector_call interpolate(simd_float &from, simd_float &to, simd_float &t) noexcept
	{ return simd_float::mulAdd(from, to - from, t); }

	strict_inline simd_float vector_call interpolate(float from, float to, simd_float &t) noexcept
	{ return simd_float::mulAdd(from, to - from, t); }

	// t are clamped to the range of low and high
	strict_inline simd_float vector_call interpolateClamp(simd_float &from, simd_float &to,
		simd_float t, simd_float &low, simd_float &high) noexcept
	{
		t = simd_float::clamp(low, high, t);
		return simd_float::mulAdd(from, to - from, t);
	}


	strict_inline Framework::Matrix vector_call getLinearInterpolationMatrix(simd_float t) noexcept
	{ return Framework::Matrix({ 0.0f, simd_float(1.0f) - t, t, 0.0f }); }

	strict_inline Framework::Matrix vector_call getCatmullInterpolationMatrix(simd_float t) noexcept
	{
		simd_float half_t = t * 0.5f;
		simd_float half_t2 = t * half_t;
		simd_float half_t3 = t * half_t2;
		simd_float half_three_t3 = half_t3 * 3.0f;

		return Framework::Matrix({ half_t2 * 2.0f - half_t3 - half_t,
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

	// 0s for loading values from one, 1s for loading values from two
	template<commonConcepts::SimdValue SIMD>
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

	strict_inline simd_float modOnce(simd_float value) noexcept
	{
		simd_mask less_mask = simd_float::lessThanOrEqual(value, 1.0f);
		simd_float lower = value - 1.0f;
		return maskLoad(lower, value, less_mask);
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
	{ return reinterpretToFloat(shiftRight<shift>(reinterpretToInt(value))); }

	strict_inline simd_float vector_call pow2ToFloat(simd_int value) noexcept
	{ return reinterpretToFloat(shiftLeft<23>(value + 127)); }



	static constexpr float kAmplitudeToDbConversionMult = 6.02059991329f;
	static constexpr float kDbToAmplitudeConversionMult = 1.0f / kAmplitudeToDbConversionMult;
	static constexpr float kExpConversionMult = 1.44269504089f;
	static constexpr float kLogConversionMult = 0.69314718056f;

	strict_inline simd_float vector_call exp2(simd_float exponent) noexcept
	{
		static constexpr float kCoefficient0 = 1.0f;
		static constexpr float kCoefficient1 = 16970.0 / 24483.0;
		static constexpr float kCoefficient2 = 1960.0 / 8161.0;
		static constexpr float kCoefficient3 = 1360.0 / 24483.0;
		static constexpr float kCoefficient4 = 80.0 / 8161.0;
		static constexpr float kCoefficient5 = 32.0 / 24483.0;

		simd_int integer = toInt(simd_float::round(exponent));
		simd_float t = exponent - toFloat(integer);
		simd_float int_pow = pow2ToFloat(integer);

		simd_float cubic = t * (t * (t * kCoefficient5 + kCoefficient4) + kCoefficient3) + kCoefficient2;
		simd_float interpolate = t * (t * cubic + kCoefficient1) + kCoefficient0;
		return int_pow * interpolate;
	}

	strict_inline simd_float vector_call log2(simd_float value) noexcept
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
	{ return log2(db + 1.0f) / log2(maxDb); }

	strict_inline simd_float vector_call normalisedToFrequency(simd_float normalised, float sampleRate) noexcept
	{ return pow(sampleRate / (2.0f * kMinFrequency), normalised) * kMinFrequency; }

	strict_inline simd_float vector_call frequencyToNormalised(simd_float frequency, float sampleRate) noexcept
	{ return log2(frequency / kMinFrequency) / log2(simd_float(sampleRate / 2.0f * kMinFrequency)); }

	strict_inline simd_float vector_call binToNormalised(simd_float bin, float FFTOrder) noexcept
	{ return log2(bin) / (FFTOrder - 1.0f); }

	strict_inline simd_float vector_call normalisedToBin(simd_float normalised, float FFTOrder) noexcept
	{ return simd_float::ceil(exp2(normalised * (FFTOrder - 1))); }

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

	// conditionally unsigns ints if they are negative and returns full mask where values are negative
	strict_inline simd_mask vector_call unsignInt(simd_int &value, bool returnFullMask = false) noexcept
	{
		static const simd_mask signMask = kSignMask;
		simd_mask mask = value & signMask;
		value ^= mask;
		return (returnFullMask) ? simd_mask::equal(mask, signMask) : mask;
	}

	// conditionally unsigns floats if they are negative and returns full mask where values are negative
	strict_inline simd_mask vector_call unsignFloat(simd_float &value, bool returnFullMask = false) noexcept
	{
		static const simd_mask signMask = kSignMask;
		simd_mask mask = reinterpretToInt(value) & signMask;
		value ^= mask;
		return (returnFullMask) ? simd_mask::equal(mask, signMask) : mask;
	}

	strict_inline simd_float scaleValue(simd_float value, const Framework::ParameterDetails *details,
		bool scalePercent = false) noexcept
	{
		using namespace Framework;

		simd_float result{};
		simd_mask sign{};
		switch (details->scale)
		{
		case ParameterScale::Toggle:
			result = utils::reinterpretToFloat(simd_float::notEqual(simd_float::round(value), 0.0f));
			break;
		case ParameterScale::Indexed:
			result = simd_float::round(value * (details->maxValue - details->minValue) + details->minValue);
			break;
		case ParameterScale::Linear:
			result = value * (details->maxValue - details->minValue) + details->minValue;
			break;
		case ParameterScale::SymmetricQuadratic:
			value = value * 2.0f - 1.0f;
			sign = utils::unsignFloat(value);
			value *= value;
			result = (value * (details->maxValue - details->minValue) / 2.0f) | sign;
			break;
		case ParameterScale::Quadratic:
			result = value * value * (details->maxValue - details->minValue) + details->minValue;
			break;
		case ParameterScale::Cubic:
			result = value * value * value * (details->maxValue - details->minValue) + details->minValue;
			break;
		case ParameterScale::SymmetricLoudness:
			value = (value * 2.0f - 1.0f);
			sign = utils::unsignFloat(value);
			result = utils::normalisedToDb(value, kNormalisedToDbMultiplier) | sign;
			break;
		case ParameterScale::Loudness:
			result = utils::normalisedToDb(value, kNormalisedToDbMultiplier);
			break;
		case ParameterScale::SymmetricFrequency:
			value = value * 2.0f - 1.0f;
			sign = utils::unsignFloat(value);
			result = utils::normalisedToFrequency(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire)) | sign;
			break;
		case ParameterScale::Frequency:
			result = utils::normalisedToFrequency(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire));
			break;
		case ParameterScale::Overlap:
			result = simd_float::clamp(value, details->minValue, details->maxValue);
			break;
		}

		if (details->displayUnits == "%" && scalePercent)
			result *= 100.0f;

		return result;
	}

	strict_inline float unscaleValue(float value, const Framework::ParameterDetails *details,
		bool unscalePercent = false) noexcept
	{
		using namespace Framework;

		if (details->displayUnits == "%" && unscalePercent)
			value /= 100.0f;

		float result{};
		float sign{};
		switch (details->scale)
		{
		case ParameterScale::Toggle:
			result = std::round(value);
			break;
		case ParameterScale::Indexed:
			result = (value - details->minValue) / (details->maxValue - details->minValue);
			break;
		case ParameterScale::Linear:
			result = (value - details->minValue) / (details->maxValue - details->minValue);
			break;
		case ParameterScale::SymmetricQuadratic:
			sign = utils::unsignFloat(value);
			value = (value - details->minValue) / (details->maxValue - details->minValue);
			value = std::sqrtf(value);
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Quadratic:
			result = std::sqrtf((value - details->minValue) / (details->maxValue - details->minValue));
			break;
		case ParameterScale::Cubic:
			value = (value - details->minValue) / (details->maxValue - details->minValue);
			result = std::powf(value, 1.0f / 3.0f);
			break;
		case ParameterScale::SymmetricLoudness:
			sign = utils::unsignFloat(value);
			value = utils::dbToAmplitude(value);
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Loudness:
			result = utils::dbToAmplitude(value);
			break;
		case ParameterScale::SymmetricFrequency:
			sign = utils::unsignFloat(value);
			value = utils::frequencyToNormalised(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire))[0];
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Frequency:
			result = utils::frequencyToNormalised(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire))[0];
			break;
		}

		return result;
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

	strict_inline void wait() noexcept
	{
	#if COMPLEX_SSE4_1
		_mm_pause();
	#elif COMPLEX_NEON
		__yield();
	#endif
	}
}