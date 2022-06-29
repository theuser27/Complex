/*
	==============================================================================

		common.h
		Created: 22 May 2021 6:24:24pm
		Author:  Lenovo

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
	static constexpr float kPi = std::numbers::pi_v<float>;
	static constexpr float k2Pi = kPi * 2.0f;
	static constexpr float kEpsilon = 1e-16f;
	static constexpr u32 kDefaultSampleRate = 44100;
	static constexpr u32 kNumInputsOutputs = 1;																				// input sources
	static constexpr u32 kNumChannels = 2;																						// how many channels each input is going to be
																																										// currently the plugin will work only with stereo signals
	static constexpr u32 kNumTotalChannels = kNumInputsOutputs * kNumChannels;				// total channels to process
	static constexpr u32 kSimdRatio = simd_float::kSize;
	static constexpr u32 kComplexSimdRatio = simd_float::kComplexSize;
	static constexpr u32 kSimdsPerInput = kSimdRatio / kNumChannels;
	static constexpr u32 kSimdsPerComplexInput = kComplexSimdRatio / kNumChannels;

	static constexpr i32 kMidiSize = 128;
	static constexpr i32 kMidiKeyCenter = 60;
	static constexpr double kMidi0Frequency = 8.1757989156f;
	static constexpr double kMinFrequency = kMidi0Frequency / 4.0f;										// lowest frequency that will be displayed
	static constexpr i32 kNotesPerOctave = 12;
	static constexpr i32 kCentsPerNote = 100;
	static constexpr i32 kCentsPerOctave = kNotesPerOctave * kCentsPerNote;
																																										// FFT sizes must be powers of 2 (some internal processing relies on that)
	static constexpr u32 kMinFFTOrder = 7;																						// 128 samples min
	static constexpr u32 kMaxFFTOrder = 14;																						// 16384 samples max
	static constexpr u32 kDefaultFFTOrder = 12;																				// 4096 samples default
	static constexpr u32 kMaxPreBufferLength = 1 << (kMaxFFTOrder + 5);								// pre FFT buffer size
	static constexpr u32 kMaxFFTBufferLength = 1 << (kMaxFFTOrder + 1);								// mid and post FFT buffers size
	static constexpr float kMinWindowOverlap = 0.0f;																	// minimum window overlap
	static constexpr float kMaxWindowOverlap = 0.96875f;															// maximum window overlap
	static constexpr float kDefaultWindowOverlap = 0.5f;															// default window overlap
	static constexpr u32 kWindowResolution = (1 << 10) + 1;														// 1025 samples window lookup resolution (one more sample  
																																										// in order to have a distinct sample in the middle)
	static constexpr u32 kNumFx = 4;																									// temporary number of fx in a chain
	static constexpr u32 kMaxNumChains = 16;

	namespace moduleTypes
	{
		enum class ModuleTypes : u32 { Utility, Filter, Contrast, Dynamics, Phase, Pitch, Stretch, Warp, Destroy };
		static constexpr std::string_view moduleParameterIds[] = { "MODULE_IS_ENABLED", "MODULE_TYPE", "MODULE_MIX", "MODULE_GAIN" };
	}
	namespace effectTypes
	{
		// Normal - Lowpass/Highpass/Bandpass/Notch
		// Regular - Harmonic/Bin based filters
		enum class FilterTypes : u32 { Normal, Regular };
		enum class ContrastTypes : u32 { Contrast };
		enum class PeakTypes : u32 { Even, Odd, Both, Between };

		static constexpr std::string_view baseParameterIds[] = { "FX_TYPE", "FX_LOW_BOUNDARY", "FX_HIGH_BOUNDARY", "FX_SHIFT_BOUNDARY", 
			"FX_IS_LINEAR_SHIFT", "FX_PARAM_1", "FX_PARAM_2", "FX_PARAM_3", "FX_PARAM_4", "FX_PARAM_5", "FX_PARAM_6", "FX_PARAM_7", "FX_PARAM_8", 
			"FX_PARAM_9", "FX_PARAM_10", "FX_PARAM_11", "FX_PARAM_12", "FX_PARAM_13", "FX_PARAM_14", "FX_PARAM_15", "FX_PARAM_16" };
	}

}

using namespace common;