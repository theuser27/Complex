/*
	==============================================================================

		common.h
		Created: 22 May 2021 6:24:24pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <memory>
#include <numbers>

#if DEBUG
	#include <cassert>
	#define COMPLEX_ASSERT(x) assert(x)
#else
	#define COMPLEX_ASSERT(x) ((void)0)
#endif

#include "JuceHeader.h"
#include "simd_values.h"

namespace common
{
	// general constants; don't change
	static constexpr float kPi = std::numbers::pi_v<float>;
	static constexpr float k2Pi = kPi * 2.0f;
	static constexpr float kEpsilon = 1e-16f;
	static constexpr double kDefaultSampleRate = 44100;
	static constexpr u32 kSimdRatio = simd_float::kSize;
	static constexpr u32 kComplexSimdRatio = simd_float::kComplexSize;
	static constexpr u32 kMidiSize = 128;
	static constexpr u32 kMidiKeyCenter = 60;
	static constexpr double kMidi0Frequency = 8.1757989156f;
	static constexpr u32 kNotesPerOctave = 12;
	static constexpr u32 kCentsPerNote = 100;
	static constexpr u32 kCentsPerOctave = kNotesPerOctave * kCentsPerNote;

	// channel constants
	static constexpr u32 kNumInputsOutputs = 1;																				// (can be changed)   in/out sources
	static constexpr u32 kNumChannels = 2;																						// (can't be changed) currently the plugin only works with stereo signals
	static constexpr u32 kNumTotalChannels = kNumInputsOutputs * kNumChannels;
	static constexpr u32 kSimdsPerInput = kSimdRatio / kNumChannels;
	static constexpr u32 kSimdsPerComplexInput = kComplexSimdRatio / kNumChannels;

	// FFT constants; some internal processing relies that sizes be powers of 2
	static constexpr u32 kMinFFTOrder = 7;																						// (can be changed)   128 samples min
	static constexpr u32 kMaxFFTOrder = 14;																						// (can be changed)   16384 samples max
	static constexpr u32 kDefaultFFTOrder = 12;																				// (can be changed)   4096 samples default
	static constexpr u32 kMaxPreBufferLength = 1 << (kMaxFFTOrder + 5);								// (can be changed)   pre FFT buffer size
	static constexpr u32 kMaxFFTBufferLength = 1 << kMaxFFTOrder;											// (can't be changed) mid and post FFT buffers size
	static constexpr float kMinWindowOverlap = 0.0f;																	// (can be changed)   minimum window overlap
	static constexpr float kMaxWindowOverlap = 0.96875f;															// (can be changed)   maximum window overlap
	static constexpr float kDefaultWindowOverlap = 0.5f;															// (can be changed)   default window overlap
	static constexpr u32 kWindowResolution = (1 << 10) + 1;														// (can be changed)   1025 samples window lookup resolution 
																																										//										(one more sample in order to have 
																																										//                    a distinct sample in the middle)
	
	// misc constants
	static constexpr u32 kMaxNumChains = 16;																					// (can be changed)   an artificial limit is needed
	static constexpr u32 kMaxEffectTypes = 16;																				// (can be changed)   types of effects per module; an artificial limit is needed
	static constexpr u32 kMaxParameterMappings = 64;																	// (can be changed)   max number of parameters that can be mapped out
	static constexpr u32 kInitialNumEffects = 16;																			// (can be changed)   initial number of effect slots in a chain

	// processing constants
	static constexpr double kMinFrequency = kMidi0Frequency / 4.0;										// (can be changed)   lowest frequency that will be displayed
	static constexpr float kNormalisedToDbMultiplier = 120.0f;												// (can be changed)   symmetrical loudness range in which we're processing	

	// used for updating parameters
	enum class UpdateFlag : u32 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };

	// various constants provided at runtime
	struct RuntimeInfo
	{
		inline static std::atomic<u32> samplesPerBlock{};
		inline static std::atomic<float> sampleRate = kDefaultSampleRate;
		inline static std::atomic<u32> FFTSize = 1 << kDefaultFFTOrder;
		inline static std::atomic<UpdateFlag> updateFlag{ UpdateFlag::AfterProcess };
	};

	namespace commonConcepts
	{
		template<typename T>
		concept SimdInt = std::same_as<T, simd_int>;

		template<typename T>
		concept SimdFloat = std::same_as<T, simd_float>;

		template<typename T>
		concept SimdValue = SimdInt<T> || SimdFloat<T>;

		template<typename T>
		concept Addable = requires (T x) { x + x; x += x; };

		template<typename T>
		concept Multipliable = requires (T x) { x * x; x *= x; };

		template<typename T>
		concept OperatorParen = requires (T x) { x.operator(); };

		template<typename T>
		concept OperatorBracket = requires (T x) { x.operator[]; };

		template<typename T>
		concept ParameterRepresentation = std::same_as<T, float> || std::same_as<T, u32> || SimdValue<T>;
	}

}

using namespace common;

namespace Framework
{
	enum class WindowTypes : u32 
	{ Rectangle, Hann, Hamming, Triangle, Sine, Exponential, HannExponential, Lanczos, Size, Blackman, BlackmanHarris, Custom };

	enum class EffectModuleTypes : u32 { Utility, Filter, Contrast, Dynamics, Phase, Pitch, Stretch, Warp, Destroy, Size };

	// Normal - Lowpass/Highpass/Bandpass/Notch
	// Regular - Harmonic/Bin based filters (like dtblkfx peaks)
	enum class FilterTypes : u32 { Normal, Regular, Size };
	enum class ContrastTypes : u32 { Contrast, Size };
}