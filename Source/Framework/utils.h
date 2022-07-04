/*
	==============================================================================

		utils.h
		Created: 27 Jul 2021 12:53:28am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "common.h"
#include <concepts>
#include "../Complex/Source/Third Party/gcem/include/gcem.hpp"

namespace utils
{
	static constexpr float kLogOf2 = std::numbers::ln2_v<float>;		
	static constexpr float kInvLogOf2 = 1.0f / kLogOf2;

	enum class Operations { Assign, Add, Multiply, MultiplyAdd };

	strict_inline float min(float one, float two)
	{ return fmin(one, two); }

	strict_inline float max(float one, float two)
	{ return fmax(one, two); }

	strict_inline float clamp(float value, float min, float max)
	{ return fmin(max, fmax(value, min)); }

	constexpr strict_inline size_t imax(size_t one, size_t two)
	{ return (one > two) ? one : two; }

	constexpr strict_inline size_t imin(size_t one, size_t two)
	{ return (one > two) ? two : one; }

	template<typename T>
	constexpr strict_inline T interpolate(T from, T to, T t)
	{ return t * (to - from) + from; }

	strict_inline float mod(double value, double *divisor)
	{ return static_cast<float>(modf(value, divisor)); }

	strict_inline float mod(float value, float *divisor)
	{ return modff(value, divisor); }

	constexpr strict_inline size_t iclamp(size_t value, size_t min, size_t max)
	{ return value > max ? max : (value < min ? min : value); }

	strict_inline i32 ilog2(i32 value)
	{
	#if defined(__GNUC__) || defined(__clang__)
		constexpr i32 kMaxBitIndex = sizeof(i32) * 8 - 1;
		return kMaxBitIndex - __builtin_clz(std::max(value, 1));
	#elif defined(_MSC_VER)
		unsigned long result = 0;
		_BitScanReverse(&result, value);
		return result;
	#else
		i32 num = 0;
		while (value >>= 1)
			num++;
		return num;
	#endif
	}

	constexpr strict_inline bool closeToZero(float value)
	{ return value <= kEpsilon && value >= -kEpsilon; }

	strict_inline float magnitudeToDb(float magnitude)
	{ return 20.0f * log10f(magnitude); }

	strict_inline float dbToMagnitude(float decibels)
	{ return powf(10.0f, decibels / 20.0f); }

	constexpr strict_inline float dbToMagnitudeConstexpr(float decibels)
	{ return gcem::pow(10.0f, decibels / 20.0f); }

	strict_inline float centsToRatio(float cents)
	{ return powf(2.0f, cents / kCentsPerOctave); }

	strict_inline float midiCentsToFrequency(float cents)
	{ return kMidi0Frequency * centsToRatio(cents); }

	strict_inline float midiNoteToFrequency(float note)
	{ return midiCentsToFrequency(note * kCentsPerNote); }

	strict_inline float frequencyToMidiNote(float frequency)
	{ return (float)kNotesPerOctave * logf(frequency / kMidi0Frequency) * kInvLogOf2; }

	strict_inline float frequencyToMidiCents(float frequency)
	{ return kCentsPerNote * frequencyToMidiNote(frequency); }

	template<std::unsigned_integral T>
	strict_inline bool isPowerOfTwo(T value)
	{ return (value & (value - 1)) == 0; }

	strict_inline i32 nextPowerOfTwo(float value)
	{ return static_cast<i32>(roundf(powf(2.0f, ceilf(logf(value) * kInvLogOf2)))); }

	strict_inline void zeroBuffer(float *buffer, int size)
	{
		for (int i = 0; i < size; ++i)
			buffer[i] = 0.0f;
	}

	strict_inline void copyBuffer(float *dest, const float *source, int size)
	{
		for (int i = 0; i < size; ++i)
			dest[i] = source[i];
	}

	constexpr strict_inline bool isSilent(const float *buffer, i32 length)
	{
		for (i32 i = 0; i < length; ++i)
			if (!closeToZero(buffer[i]))
				return false;
		return true;
	}

	strict_inline float rms(const float *buffer, i32 num)
	{
		float squared_total(0.0f);
		for (i32 i = 0; i < num; i++)
			squared_total += buffer[i] * buffer[i];

		return sqrtf(squared_total / num);
	}

	constexpr strict_inline i32 prbs32(i32 x)
	{
		// maximal length, taps 32 31 29 1, from wikipedia
		return ((u32)x >> 1) ^ (-(x & 1) & 0xd0000001UL);
	}

	// copies samples from "otherBuffer" to "thisBuffer" to respective channels
	// while anticipating wrapping around in both buffers 
	strict_inline void copyBuffer(AudioBuffer<float> &thisBuffer, const AudioBuffer<float> &otherBuffer,
		u32 numChannels, u32 numSamples, u32 thisStartIndex, u32 otherStartIndex,
		Operations operation = Operations::Assign) noexcept
	{
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumSamples()) >= numSamples);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumSamples()) >= numSamples);
		COMPLEX_ASSERT(isPowerOfTwo(static_cast<u32>(thisBuffer.getNumSamples())));
		COMPLEX_ASSERT(isPowerOfTwo(static_cast<u32>(otherBuffer.getNumSamples())));

		float(*opFunction)(float, float);
		switch (operation)
		{
		case utils::Operations::Add:
			opFunction = [](float one, float two) { return one + two; };
			break;
		case utils::Operations::Multiply:
			opFunction = [](float one, float two) { return one * two; };
			break;
		default:
		case utils::Operations::Assign:
			opFunction = [](float one, float two) { return two; };
			break;
		}

		auto otherDataReadPointers = otherBuffer.getArrayOfReadPointers();
		auto thisDataReadPointers = thisBuffer.getArrayOfReadPointers();
		auto thisDataWritePointers = thisBuffer.getArrayOfWritePointers();

		auto thisBufferSize = thisBuffer.getNumSamples() - 1;
		auto otherBufferSize = otherBuffer.getNumSamples() - 1;

		for (u32 i = 0; i < numChannels; i++)
		{
			for (u32 k = 0; k < numSamples; k++)
			{
				thisDataWritePointers[i][(thisStartIndex + k) & thisBufferSize]
					= opFunction(thisDataReadPointers[i][(thisStartIndex + k) & thisBufferSize],
						otherDataReadPointers[i][(otherStartIndex + k) & otherBufferSize]);
			}
		}
	}

	strict_inline void copyBufferChannels(AudioBuffer<float> &thisBuffer, const AudioBuffer<float> &otherBuffer,
		u32 numChannels, const bool *channelsToCopy, u32 numSamples, u32 thisStartIndex, u32 otherStartIndex,
		Operations operation = Operations::Assign) noexcept
	{
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumSamples()) >= numSamples);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumSamples()) >= numSamples);
		COMPLEX_ASSERT(isPowerOfTwo(static_cast<u32>(thisBuffer.getNumSamples())));
		COMPLEX_ASSERT(isPowerOfTwo(static_cast<u32>(otherBuffer.getNumSamples())));

		float(*opFunction)(float, float);
		switch (operation)
		{
		case utils::Operations::Add:
			opFunction = [](float one, float two) { return one + two; };
			break;
		case utils::Operations::Multiply:
			opFunction = [](float one, float two) { return one * two; };
			break;
		default:
		case utils::Operations::Assign:
			opFunction = [](float one, float two) { return two; };
			break;
		}

		auto otherDataReadPointers = otherBuffer.getArrayOfReadPointers();
		auto thisDataReadPointers = thisBuffer.getArrayOfReadPointers();
		auto thisDataWritePointers = thisBuffer.getArrayOfWritePointers();

		auto thisBufferSize = thisBuffer.getNumSamples() - 1;
		auto otherBufferSize = otherBuffer.getNumSamples() - 1;

		for (u32 i = 0; i < numChannels; i++)
		{
			if (!channelsToCopy[i])
				continue;

			for (u32 k = 0; k < numSamples; k++)
			{
				thisDataWritePointers[i][(thisStartIndex + k) & thisBufferSize]
					= opFunction(thisDataReadPointers[i][(thisStartIndex + k) & thisBufferSize],
						otherDataReadPointers[i][(otherStartIndex + k) & otherBufferSize]);
			}
		}
	}

	// for debugging purposes
	strict_inline void printBuffer(const float *begin, u32 numSamples)
	{
		for (u32 i = 0; i < numSamples; i++)
		{
			DBG(begin[i]);
		}
		DBG("\n");
	}
}