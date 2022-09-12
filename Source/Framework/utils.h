/*
	==============================================================================

		utils.h
		Created: 27 Jul 2021 12:53:28am
		Author:  theuser27

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

	enum class GeneralOperations { Add, Remove, Update };
	enum class MathOperations { Assign, Add, Multiply, MultiplyAdd };

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

	strict_inline float magnitudeToDbConstexpr(float magnitude)
	{ return 20.0f * gcem::log10(magnitude); }

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

	strict_inline void zeroBuffer(float *buffer, u32 size)
	{
		for (u32 i = 0; i < size; ++i)
			buffer[i] = 0.0f;
	}

	strict_inline void copyBuffer(float *dest, const float *source, u32 size)
	{
		for (u32 i = 0; i < size; ++i)
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

	template <typename T> 
	constexpr int signum(T val)
	{ return (T(0) < val) - (val < T(0)); }

	template<std::floating_point T>
	constexpr strict_inline T unsignFloat(T &value)
	{
		int sign = signum(value);
		value *= (float)sign;
		return (T)sign;
	}

	template<std::signed_integral T>
	constexpr strict_inline int unsignInt(T &value)
	{
		int sign = signum(value);
		value *= sign;
		return (T)sign;
	}

	strict_inline void spinAndLock(std::atomic<bool> &atomic, bool expectedState)
	{
		bool expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, !expectedState, std::memory_order_acq_rel))
		{ expected = expectedState; }
	}

	strict_inline void spinAndLock(std::atomic<i8> &atomic, i8 expectedState, i8 desiredState)
	{
		i8 expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, desiredState, std::memory_order_acq_rel))
		{ expected = expectedState; }
	}

	struct atomic_simd_float
	{
		atomic_simd_float(simd_float value) : value(value) {}

		simd_float load() const noexcept
		{
			spinAndLock(guard, false);
			simd_float currentValue = value;
			guard.store(false, std::memory_order_release);
			return currentValue;
		}

		void store(simd_float newValue) noexcept
		{
			spinAndLock(guard, false);
			value = newValue;
			guard.store(false, std::memory_order_release);
		}

		simd_float add(const simd_float &other) noexcept
		{
			spinAndLock(guard, false);
			value += other;
			simd_float result = value;
			guard.store(false, std::memory_order_release);
			return result;
		}

	private:
		simd_float value = 0.0f;
		mutable std::atomic<bool> guard = false;
	};

	/*template<typename T>
	struct span
	{
		const T *pointer = nullptr;
		size_t extent = 0;

		size_t size() const noexcept { return extent; }
		const T &operator[](size_t index) const noexcept { return pointer[index]; }
	};*/

	// copies samples from "otherBuffer" to "thisBuffer" to respective channels
	// while anticipating wrapping around in both buffers 
	strict_inline void copyBuffer(AudioBuffer<float> &thisBuffer, const AudioBuffer<float> &otherBuffer,
		u32 numChannels, u32 numSamples, u32 thisStartIndex, u32 otherStartIndex, 
		const bool *channelsToCopy = nullptr, MathOperations operation = MathOperations::Assign) noexcept
	{
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumChannels()) >= numChannels);
		COMPLEX_ASSERT(static_cast<u32>(thisBuffer.getNumSamples()) >= numSamples);
		COMPLEX_ASSERT(static_cast<u32>(otherBuffer.getNumSamples()) >= numSamples);

		float(*opFunction)(float, float);
		switch (operation)
		{
		case utils::MathOperations::Add:
			opFunction = [](float one, float two) { return one + two; };
			break;
		case utils::MathOperations::Multiply:
			opFunction = [](float one, float two) { return one * two; };
			break;
		default:
		case utils::MathOperations::Assign:
			opFunction = []([[maybe_unused]] float one, float two) { return two; };
			break;
		}

		u32 thisBufferSize{};
		u32 otherBufferSize{};

		u32(*indexFunction)(u32, u32);
		switch (isPowerOfTwo(static_cast<u32>(thisBuffer.getNumSamples())) & 
			isPowerOfTwo(static_cast<u32>(otherBuffer.getNumSamples())))
		{
		case true:
			indexFunction = [](u32 value, u32 size) { return value & size; };
			thisBufferSize = thisBuffer.getNumSamples() - 1;
			otherBufferSize = otherBuffer.getNumSamples() - 1;
			break;
		case false:
			indexFunction = [](u32 value, u32 size) { return value % size; };
			thisBufferSize = thisBuffer.getNumSamples();
			otherBufferSize = otherBuffer.getNumSamples();
			break;
		}

		auto otherDataReadPointers = otherBuffer.getArrayOfReadPointers();
		auto thisDataReadPointers = thisBuffer.getArrayOfReadPointers();
		auto thisDataWritePointers = thisBuffer.getArrayOfWritePointers();

		if (channelsToCopy)
		{
			for (u32 i = 0; i < numChannels; i++)
			{
				if (!channelsToCopy[i])
					continue;

				for (u32 k = 0; k < numSamples; k++)
				{
					u32 thisIndex = indexFunction(thisStartIndex + k, thisBufferSize);
					thisDataWritePointers[i][thisIndex] = opFunction(thisDataReadPointers[i][thisIndex],
							otherDataReadPointers[i][indexFunction(otherStartIndex + k, otherBufferSize)]);
				}
			}
		}
		else
		{
			for (u32 i = 0; i < numChannels; i++)
			{
				for (u32 k = 0; k < numSamples; k++)
				{
					u32 thisIndex = indexFunction(thisStartIndex + k, thisBufferSize);
					thisDataWritePointers[i][thisIndex] = opFunction(thisDataReadPointers[i][thisIndex],
							otherDataReadPointers[i][indexFunction(otherStartIndex + k, otherBufferSize)]);
				}
			}
		}
	}

	// for debugging purposes
	strict_inline void printBuffer([[maybe_unused]] const float *begin, u32 numSamples)
	{
		for (u32 i = 0; i < numSamples; i++)
		{
			DBG(begin[i]);
		}
		DBG("\n");
	}
}