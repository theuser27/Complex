/*
	==============================================================================

		utils.h
		Created: 27 Jul 2021 12:53:28am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <cmath>
#include <atomic>
#include <concepts>
#include <numbers>
#include <span>
#include <type_traits>
#include <Third Party/gcem/gcem.hpp>
#include "constants.h"

namespace utils
{
	static constexpr double kLogOf2 = std::numbers::ln2_v<double>;
	static constexpr double kInvLogOf2 = 1.0f / kLogOf2;

	enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

	// get you the N-th element of a parameter pack
	template <auto N, auto I = 0, typename T, typename ... Args>
	decltype(auto) getNthElement(T &&arg, [[maybe_unused]] Args&& ... args)
	{
		if constexpr (I == N)
			return std::forward<T>(arg);
		else if constexpr (sizeof...(args) > 0)
			return getNthElement<N, I + 1>(std::forward<Args>(args)...);
	}

	// helper type for the std::visit visitor
	template<class... Ts>
	struct overloaded : Ts... { using Ts::operator()...; };

	strict_inline float mod(double value, double *divisor) noexcept
	{ return static_cast<float>(modf(value, divisor)); }

	strict_inline float mod(float value, float *divisor) noexcept
	{ return modff(value, divisor); }

	strict_inline u32 log2(u32 value) noexcept
	{
	#if defined(COMPLEX_GCC) || defined(COMPLEX_CLANG)
		constexpr u32 kMaxBitIndex = sizeof(u32) * 8 - 1;
		return kMaxBitIndex - __builtin_clz(std::max(value, 1));
	#elif defined(COMPLEX_MSVC)
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

	constexpr strict_inline bool closeToZero(float value) noexcept
	{ return value <= kEpsilon && value >= -kEpsilon; }

	strict_inline double amplitudeToDb(double amplitude) noexcept
	{ return 20.0 * log10(amplitude); }

	strict_inline double dbToAmplitude(double decibels) noexcept
	{ return pow(10.0, decibels / 20.0); }

	constexpr strict_inline double amplitudeToDbConstexpr(double amplitude) noexcept
	{ return 20.0 * gcem::log10(amplitude); }

	constexpr strict_inline double dbToAmplitudeConstexpr(double decibels) noexcept
	{ return gcem::pow(10.0, decibels / 20.0); }

	strict_inline double normalisedToDb(double normalised, double maxDb) noexcept
	{ return std::pow(maxDb + 1.0, normalised) - 1.0; }

	strict_inline double dbToNormalised(double db, double maxDb) noexcept
	{ return std::log2(db + 1.0f) / std::log2(maxDb); }

	strict_inline double normalisedToFrequency(double normalised, double sampleRate) noexcept
	{
		double sampleRateLog2 = std::log2(sampleRate / (kMinFrequency * 2.0));
		return std::exp2(sampleRateLog2 * normalised) * kMinFrequency;
	}

	strict_inline double frequencyToNormalised(double frequency, double sampleRate) noexcept
	{
		double sampleRateLog2 = std::log2(sampleRate / (kMinFrequency * 2.0));
		return std::log2(frequency / kMinFrequency) / sampleRateLog2;
	}

	strict_inline double binToNormalised(double bin, u32 FFTSize, double sampleRate) noexcept
	{
		// at 0 logarithm doesn't produce valid values
		if (bin == 0.0)
			return 0.0;

		double sampleRateLog2 = std::log2(sampleRate / (kMinFrequency * 2.0));
		return std::log2((bin / (double)FFTSize) * (sampleRate / kMinFrequency)) / sampleRateLog2;
	}

	strict_inline double normalisedToBin(double normalised, u32 FFTSize, double sampleRate) noexcept
	{
		double sampleRateLog2 = std::log2(sampleRate / (kMinFrequency * 2.0));
		return std::ceil(std::exp2(normalised * sampleRateLog2) * ((FFTSize * kMinFrequency) / sampleRate)) - 1.0;
	}

	strict_inline double centsToRatio(double cents) noexcept
	{ return pow(2.0, cents / (double)kCentsPerOctave); }

	strict_inline double midiCentsToFrequency(double cents) noexcept
	{ return kMidi0Frequency * centsToRatio(cents); }

	strict_inline double midiNoteToFrequency(double note) noexcept
	{ return midiCentsToFrequency(note * kCentsPerNote); }

	strict_inline double frequencyToMidiNote(double frequency) noexcept
	{ return (double)kNotesPerOctave * log(frequency / kMidi0Frequency) * kInvLogOf2; }

	strict_inline double frequencyToMidiCents(double frequency) noexcept
	{ return kCentsPerNote * frequencyToMidiNote(frequency); }



	template<std::integral T>
	strict_inline bool isPowerOfTwo(T value) noexcept
	{ return (value & (value - 1)) == 0; }

	strict_inline float nextPowerOfTwo(float value) noexcept
	{ return roundf(powf(2.0f, ceilf(logf(value) * (float)kInvLogOf2))); }

	constexpr strict_inline bool isSilent(const float *buffer, i32 length) noexcept
	{
		for (i32 i = 0; i < length; ++i)
			if (!closeToZero(buffer[i]))
				return false;
		return true;
	}

	strict_inline float rms(const float *buffer, u32 num) noexcept
	{
		float squared_total(0.0f);
		for (u32 i = 0; i < num; i++)
			squared_total += buffer[i] * buffer[i];

		return sqrtf(squared_total / (float)num);
	}

	/*constexpr strict_inline i32 prbs32(i32 x) noexcept
	{
		// maximal length, taps 32 31 29 1, from wikipedia
		return (i32)(((u32)x >> 1U) ^ (-(x & 1U) & 0xd0000001U));
	}*/

	template <typename T> 
	constexpr strict_inline int signum(T val) noexcept
	{ return (T(0) < val) - (val < T(0)); }

	template<std::floating_point T>
	constexpr strict_inline T unsignFloat(T &value) noexcept
	{
		int sign = signum(value);
		value *= (float)sign;
		return (T)sign;
	}

	template<std::signed_integral T>
	constexpr strict_inline int unsignInt(T &value) noexcept
	{
		int sign = signum(value);
		value *= sign;
		return (T)sign;
	}

	template<std::floating_point T>
	constexpr strict_inline T smoothStep(T value) noexcept
	{
		T sqr = value * value;
		return (T(3) * sqr) - (T(2) * sqr * value);
	}

	strict_inline void wait() noexcept
	{
	#if COMPLEX_SSE4_1
		_mm_pause();
	#elif COMPLEX_NEON
		__yield();
	#endif
	}

	strict_inline void longWait(size_t iterations) noexcept
	{
		for (size_t i = 0; i < iterations; i++)
		{
		#if COMPLEX_SSE4_1
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
		#elif COMPLEX_NEON
			__yield();
			__yield();
			__yield();
			__yield();
			__yield();
		#endif
		}
	}

	class ScopedBlock
	{
	public:
		ScopedBlock(std::atomic<bool> &atomic, bool expected = false) noexcept : atomic_(atomic), expected_(expected)
		{
			while (!atomic.compare_exchange_strong(expected, !expected, std::memory_order_acq_rel, std::memory_order_relaxed))
			{
				atomic.wait(expected, std::memory_order_relaxed);
				expected = expected_;
			}
		}

		ScopedBlock(ScopedBlock &&other) noexcept : atomic_(other.atomic_), expected_(other.expected_) { }
		ScopedBlock &operator=(ScopedBlock &&other) noexcept
		{
			std::swap(atomic_, other.atomic_);
			std::swap(expected_, other.expected_);
			return *this;
		}

		~ScopedBlock() noexcept
		{
			atomic_.get().store(expected_, std::memory_order_release);
			atomic_.get().notify_all();
		}

		ScopedBlock(const ScopedBlock &) = delete;
		ScopedBlock operator=(const ScopedBlock &) = delete;

	private:
		std::reference_wrapper<std::atomic<bool>> atomic_;
		bool expected_;
	};

	strict_inline void spinAndLock(std::atomic<bool> &atomic, bool expectedState) noexcept
	{
		bool expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, !expectedState, std::memory_order_acq_rel))
		{
			expected = expectedState;
			// taking advantage of the MESI protocol where we only want shared access to read until the value is changed,
			// instead of read/write with exclusive access thereby slowing down other threads trying to read as well
			while (atomic.load(std::memory_order_relaxed) != expected) { wait(); }
		}
	}

	template<typename T>
	strict_inline void spinAndLock(std::atomic<T> &atomic, T expectedState, T desiredState) noexcept
	{
		T expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, desiredState, std::memory_order_acq_rel))
		{
			expected = expectedState;
			// see the comment for spinAndLock with atomic<bool>
			while (atomic.load(std::memory_order_relaxed) != expected) { wait(); }
		}
	}

	// a very quick and dirty lock_guard implementation
	class ScopedSpinLock
	{
	public:
		// spins if the atomic is held by a different thread
		ScopedSpinLock(std::atomic<bool> &atomic, bool expected = false, bool notify = false) noexcept : 
			atomic_(atomic), expected_(expected), notify_(notify)
		{ spinAndLock(atomic_, expected); }

		ScopedSpinLock(ScopedSpinLock &&other) noexcept : atomic_(other.atomic_), expected_(other.expected_), notify_(other.notify_) { }
		ScopedSpinLock &operator=(ScopedSpinLock &&other) noexcept
		{
			std::swap(atomic_, other.atomic_);
			std::swap(expected_, other.expected_);
			std::swap(notify_, other.notify_);
			return *this;
		}

		~ScopedSpinLock() noexcept
		{
			atomic_.get().store(expected_, std::memory_order_release);
			if (notify_)
				atomic_.get().notify_all();
		}

		ScopedSpinLock(const ScopedSpinLock &) = delete;
		ScopedSpinLock operator=(const ScopedSpinLock &) = delete;

	private:
		std::reference_wrapper<std::atomic<bool>> atomic_;
		bool expected_;
		bool notify_;
	};

	struct atomic_simd_float
	{
		atomic_simd_float(simd_float value) : value(value) {}

		simd_float load() const noexcept
		{
			ScopedSpinLock lock(guard);
			simd_float currentValue = value;
			return currentValue;
		}

		void store(simd_float newValue) noexcept
		{
			ScopedSpinLock lock(guard);
			value = newValue;
		}

		simd_float add(const simd_float &other) noexcept
		{
			ScopedSpinLock lock(guard);
			value += other;
			simd_float result = value;
			return result;
		}

	private:
		simd_float value = 0.0f;
		mutable std::atomic<bool> guard = false;
	};

	template<typename T>
	strict_inline T as(auto pointer)
	{
		static_assert((std::is_pointer_v<T> && std::is_pointer_v<decltype(pointer)>) || 
			(std::is_lvalue_reference_v<T> && std::is_lvalue_reference_v<decltype(pointer)>), 
			"Types are neither pointers, nor lvalue refs");

	#if COMPLEX_DEBUG
		auto *castPointer = dynamic_cast<T>(pointer);
		COMPLEX_ASSERT(castPointer && "Unsuccessful cast");
		return castPointer;
	#else
		return static_cast<T>(pointer);
	#endif
	}
}