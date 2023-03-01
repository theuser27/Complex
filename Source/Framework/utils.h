/*
	==============================================================================

		utils.h
		Created: 27 Jul 2021 12:53:28am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <concepts>
#include <span>
#include <Third Party/gcem/gcem.hpp>
#include "common.h"
#include "parameters.h"

namespace utils
{
	static constexpr double kLogOf2 = std::numbers::ln2_v<double>;
	static constexpr double kInvLogOf2 = 1.0f / kLogOf2;

	enum class GeneralOperations { Add, AddCopy, AddMove, Update, UpdateCopy, UpdateMove, Remove };
	enum class MathOperations { Assign, Add, Multiply, FadeInAdd, FadeOutAdd, Interpolate };

	// get you the N-th element of a parameter pack
	template <auto N, auto I = 0>
	auto getNthElement(auto arg, auto... args)
	{
		if constexpr (I == N)
			return arg;
		else if constexpr (sizeof...(args) > 0)
			return getNthElement<N, I + 1>(args...);
	}

	// returns the string name of a registered class
	template<typename T>
	constexpr std::string_view getClassName()
	{ return std::string_view(refl::reflect<T>().name.c_str()); }

	// broken somehow, don't use
	template<typename T>
	constexpr auto extractBaseTypes()
	{
		using namespace refl;

		constexpr auto baseTypes = reflect<T>().declared_bases;

		if constexpr (baseTypes.size == 0)
			return std::array<std::string_view, 0>();
		else
		{
			std::array<std::string_view, baseTypes.size> baseTypesStrings;
			size_t i = 0;

			for_each(baseTypes, [&baseTypesStrings, &i](auto t) 
				{ baseTypesStrings[i++] = t.name.str(); });

			return baseTypesStrings;
		}
	}

	// broken somehow, don't use
	template<typename T>
	constexpr bool containsType(std::string_view name)
	{
		using namespace refl;
		
		if (name == getClassName<T>())
			return true;

		constexpr std::array baseTypesStrings = extractBaseTypes<T>();
		for (auto str : baseTypesStrings)
			if (str == name) 
				return true;
		
		return false;
	}

	template<typename T>
	bool isType(std::string_view name) { return name == getClassName<T>(); }

	/*template<typename... Ts>
	constexpr decltype(auto) getArrayOfClassNames()
	{
		std::array<std::string_view, sizeof...(Ts)> buffer{};
		std::tuple tupl = std::make_tuple(refl::reflect<Ts>()...);

		for (size_t i = 0; i < sizeof...(Ts); i++)
			buffer[i] = std::string_view(std::get<i>(tupl).name.c_str());

		return buffer;
	}*/

	class Downcastable
	{
	public:
		template<typename T>
		T *findParentComponent() const
		{
			for (auto *p = parent; p != nullptr; p = p->parent)
				if (p->getType() == utils::getClassName<T>())
					return static_cast<T *>(p);

			return nullptr;
		}

		template<typename T>
		T *as()
		{
			COMPLEX_ASSERT(utils::isType<T>(type)
				&& "You're trying to cast to a type that's not compatible");
			return static_cast<T *>(this);
		}

		std::string_view getType() const noexcept { return type; }

	protected:
		const std::string_view type{};
		const Downcastable *parent = nullptr;
	};

	strict_inline float mod(double value, double *divisor) noexcept
	{ return static_cast<float>(modf(value, divisor)); }

	strict_inline float mod(float value, float *divisor) noexcept
	{ return modff(value, divisor); }

	strict_inline u32 log2(u32 value) noexcept
	{
	#if defined(__GNUC__) || defined(__clang__)
		constexpr u32 kMaxBitIndex = sizeof(u32) * 8 - 1;
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

	constexpr strict_inline bool closeToZero(float value) noexcept
	{ return value <= kEpsilon && value >= -kEpsilon; }

	strict_inline float amplitudeToDb(float amplitude) noexcept
	{ return 20.0f * log10f(amplitude); }

	strict_inline float dbToAmplitude(float decibels) noexcept
	{ return powf(10.0f, decibels / 20.0f); }

	constexpr strict_inline float amplitudeToDbConstexpr(float amplitude) noexcept
	{ return 20.0f * gcem::log10(amplitude); }

	constexpr strict_inline float dbToAmplitudeConstexpr(float decibels) noexcept
	{ return gcem::pow(10.0f, decibels / 20.0f); }

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
	{ return powf(2.0f, cents / kCentsPerOctave); }

	strict_inline double midiCentsToFrequency(double cents) noexcept
	{ return kMidi0Frequency * centsToRatio(cents); }

	strict_inline double midiNoteToFrequency(double note) noexcept
	{ return midiCentsToFrequency(note * kCentsPerNote); }

	strict_inline double frequencyToMidiNote(double frequency) noexcept
	{ return (float)kNotesPerOctave * logf(frequency / kMidi0Frequency) * kInvLogOf2; }

	strict_inline double frequencyToMidiCents(double frequency) noexcept
	{ return kCentsPerNote * frequencyToMidiNote(frequency); }

	inline constexpr float kLowestDb = -kNormalisedToDbMultiplier;
	inline constexpr float kHighestDb = kNormalisedToDbMultiplier;
	inline constexpr float kLowestAmplitude = dbToAmplitudeConstexpr(kLowestDb);
	inline constexpr float kHighestAmplitude = dbToAmplitudeConstexpr(kHighestDb);

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

	strict_inline float rms(const float *buffer, i32 num) noexcept
	{
		float squared_total(0.0f);
		for (i32 i = 0; i < num; i++)
			squared_total += buffer[i] * buffer[i];

		return sqrtf(squared_total / num);
	}

	constexpr strict_inline i32 prbs32(i32 x) noexcept
	{
		// maximal length, taps 32 31 29 1, from wikipedia
		return ((u32)x >> 1) ^ (-(x & 1) & 0xd0000001UL);
	}

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

	strict_inline void spinAndLock(std::atomic<bool> &atomic, bool expectedState) noexcept
	{
		bool expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, !expectedState, std::memory_order_acq_rel))
		{ expected = expectedState; }
	}

	strict_inline void spinAndLock(std::atomic<i8> &atomic, i8 expectedState, i8 desiredState) noexcept
	{
		i8 expected = expectedState;
		while (!atomic.compare_exchange_weak(expected, desiredState, std::memory_order_acq_rel))
		{ expected = expectedState; }
	}

	class ScopedSpinLock
	{
	public:
		// spins if the atomic is held by a different thread
		ScopedSpinLock(std::atomic<bool> &atomic) noexcept : atomic_(atomic)
		{ spinAndLock(atomic_, false); }

		~ScopedSpinLock() noexcept 
		{ atomic_.store(false, std::memory_order_release); }

		ScopedSpinLock(const ScopedSpinLock &) = delete;
		ScopedSpinLock(ScopedSpinLock &&) = delete;
		ScopedSpinLock operator=(const ScopedSpinLock &) = delete;
		ScopedSpinLock operator=(ScopedSpinLock &&) = delete;

	private:
		std::atomic<bool> &atomic_;
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

	inline double scaleValue(double value, Framework::ParameterDetails details, bool scalePercent = false, bool skewOnly = false) noexcept
	{
		using namespace Framework;

		double result{};
		double sign{};
		double sampleRate{};
		switch (details.scale)
		{
		case ParameterScale::Toggle:
			result = std::round(value);
			break;
		case ParameterScale::Indexed:
			result = (!skewOnly) ? std::round(value * (details.maxValue - details.minValue) + details.minValue) :
				std::round(value * (details.maxValue - details.minValue) + details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::Linear:
			result = (!skewOnly) ? value * (details.maxValue - details.minValue) + details.minValue : value;
			break;
		case ParameterScale::Clamp:
			result = std::clamp(value, (double)details.minValue, (double)details.maxValue);
			result = (!skewOnly) ? result : result / ((double)details.maxValue - (double)details.minValue);
			break;
		case ParameterScale::SymmetricQuadratic:
			value = value * 2.0f - 1.0f;
			sign = unsignFloat(value);
			value *= value;
			result = (!skewOnly) ? (value * (details.maxValue - details.minValue) * 0.5) * sign : value * sign;
			break;
		case ParameterScale::Quadratic:
			result = (!skewOnly) ? value * value * (details.maxValue - details.minValue) + details.minValue : value * value;
			break;
		case ParameterScale::Cubic:
			result = (!skewOnly) ? value * value * value * (details.maxValue - details.minValue) + details.minValue : value * value * value;
			break;
		case ParameterScale::SymmetricLoudness:
			value = (value * 2.0f - 1.0f);
			sign = unsignFloat(value);
			result = (!skewOnly) ? normalisedToDb(value, kNormalisedToDbMultiplier) * sign : normalisedToDb(value, 1.0) * sign;
			break;
		case ParameterScale::Loudness:
			result = (!skewOnly) ? normalisedToDb(value, kNormalisedToDbMultiplier) : normalisedToDb(value, 1.0);
			break;
		case ParameterScale::SymmetricFrequency:
			value = value * 2.0f - 1.0f;
			sign = unsignFloat(value);
			sampleRate = RuntimeInfo::sampleRate.load(std::memory_order_acquire);
			result = (!skewOnly) ? normalisedToFrequency(value, sampleRate) * sign : (normalisedToFrequency(value, sampleRate) * sign) / sampleRate;
			break;
		case ParameterScale::Frequency:
			sampleRate = RuntimeInfo::sampleRate.load(std::memory_order_acquire);
			result = (!skewOnly) ? normalisedToFrequency(value, sampleRate) : normalisedToFrequency(value, sampleRate) / sampleRate;
			break;
		}

		if (details.displayUnits == "%" && scalePercent)
			result *= 100.0f;

		return result;
	}

	inline double unscaleValue(double value, Framework::ParameterDetails details, bool unscalePercent = false) noexcept
	{
		using namespace Framework;

		if (details.displayUnits == "%" && unscalePercent)
			value /= 100.0f;

		double result{};
		double sign{};
		switch (details.scale)
		{
		case ParameterScale::Toggle:
			result = std::round(value);
			break;
		case ParameterScale::Indexed:
			result = (value - details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::Linear:
			result = (value - details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::SymmetricQuadratic:
			sign = unsignFloat(value);
			value = (value - details.minValue) / (details.maxValue - details.minValue);
			value = std::sqrt(value);
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Quadratic:
			result = std::sqrt((value - details.minValue) / (details.maxValue - details.minValue));
			break;
		case ParameterScale::Cubic:
			value = (value - details.minValue) / (details.maxValue - details.minValue);
			result = std::pow(value, 1.0f / 3.0f);
			break;
		case ParameterScale::SymmetricLoudness:
			sign = unsignFloat(value);
			value = dbToAmplitude(value);
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Loudness:
			result = dbToAmplitude(value);
			break;
		case ParameterScale::SymmetricFrequency:
			sign = unsignFloat(value);
			value = frequencyToNormalised(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire));
			result = (value * sign + 1.0f) / 2.0f;
			break;
		case ParameterScale::Frequency:
			result = frequencyToNormalised(value, RuntimeInfo::sampleRate.load(std::memory_order_acquire));
			break;
		}

		return result;
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