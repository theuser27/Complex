/*
  ==============================================================================

    constants.h
    Created: 11 Jun 2023 3:25:10am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "platform_definitions.h"

namespace common
{
	// general constants; don't change
	inline constexpr float kPi = 3.141592653589793;
	inline constexpr float k2Pi = kPi * 2.0f;
	inline constexpr float kEpsilon = 1e-10f;
	inline constexpr double kDefaultSampleRate = 44100;
	inline constexpr u32 kSimdRatio = 4;
	inline constexpr u32 kComplexSimdRatio = kSimdRatio / 2;
	inline constexpr u32 kMidiSize = 128;
	inline constexpr u32 kMidiKeyCenter = 60;
	inline constexpr double kMidi0Frequency = 8.1757989156f;
	inline constexpr u32 kNotesPerOctave = 12;
	inline constexpr u32 kCentsPerNote = 100;
	inline constexpr u32 kCentsPerOctave = kNotesPerOctave * kCentsPerNote;
	inline constexpr float kAmplitudeToDbConversionMult = 6.02059991329f;
	inline constexpr float kDbToAmplitudeConversionMult = 1.0f / kAmplitudeToDbConversionMult;
	inline constexpr float kExpConversionMult = 1.44269504089f;
	inline constexpr float kLogConversionMult = 0.69314718056f;

	// channel constants
	inline constexpr u32 kNumInputsOutputs = 1;																				// (can be changed)   in/out sources
	inline constexpr u32 kNumSidechains = kNumInputsOutputs - 1;											// (can be changed)   in/out sources
	inline constexpr u32 kNumChannels = 2;																						// (can't be changed) currently the plugin only works with stereo signals
	inline constexpr u32 kNumTotalChannels = kNumInputsOutputs * kNumChannels;
	inline constexpr u32 kSimdsPerInput = kSimdRatio / kNumChannels;
	inline constexpr u32 kSimdsPerComplexInput = kComplexSimdRatio / kNumChannels;

	// FFT constants; some internal processing relies that sizes be powers of 2
	inline constexpr u32 kMinFFTOrder = 7;																						// (can be changed)   128 samples min
	inline constexpr u32 kMaxFFTOrder = 14;																						// (can be changed)   16384 samples max
	inline constexpr u32 kDefaultFFTOrder = 12;																				// (can be changed)   4096 samples default
	inline constexpr u32 kMaxPreBufferLength = 1 << (kMaxFFTOrder + 5);								// (can be changed)   pre FFT buffer size
	inline constexpr u32 kMaxFFTBufferLength = 1 << kMaxFFTOrder;											// (can't be changed) mid and post FFT buffers size
	inline constexpr float kMinWindowOverlap = 0.0f;																	// (can be changed)   minimum window overlap
	inline constexpr float kMaxWindowOverlap = 0.96875f;															// (can be changed)   maximum window overlap
	inline constexpr float kDefaultWindowOverlap = 0.5f;															// (can be changed)   default window overlap
	inline constexpr u32 kWindowResolution = (1 << 10) + 1;														// (can be changed)   1025 samples window lookup resolution 
																																										//										(one more sample in order to have 
																																										//                    a distinct sample in the middle)

	// misc constants
	inline constexpr u32 kMaxNumLanes = 16;																						// (can be changed)   an artificial limit is needed
	inline constexpr u32 kMaxEffectModes = 16;																				// (can be changed)   types of effects per module; an artificial limit is needed
	inline constexpr u32 kMaxParameterMappings = 64;																	// (can be changed)   max number of parameters that can be mapped out
	inline constexpr u32 kInitialEffectCount = 50;																		// (can be changed)   initial number of effect slots in a lane

	// processing constants
	inline constexpr double kMinFrequency = kMidi0Frequency;													// (can be changed)   lowest frequency that will be displayed

	// GUI constants
	inline constexpr int kMinWindowWidth = 426;
	inline constexpr int kMinWindowHeight = 500;
	inline constexpr int kDefaultWindowWidth = 430;
	inline constexpr int kDefaultWindowHeight = 700;
	inline constexpr float kWindowScaleIncrements = 0.25f;
	inline constexpr float kMinWindowScaleFactor = 0.5f;
	inline constexpr float kMaxWindowScaleFactor = 3.0f;
	inline constexpr int kParameterUpdateIntervalHz = 60;

	// used for updating parameters
	enum class UpdateFlag : u32 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };
}

using namespace common;
