/*
  ==============================================================================

    constants.hpp
    Created: 11 Jun 2023 3:25:10am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "platform_definitions.hpp"

namespace common
{
  // general constants; don't change
  inline constexpr float kPi = 3.141592653589793f;
  inline constexpr float k2Pi = kPi * 2.0f;
  inline constexpr float kEpsilon = 1e-20f;
  inline constexpr u32 kFloatMantissaMask = 0x007fffffU;
  inline constexpr u32 kFloatExponentMask = 0x7f800000U;
  inline constexpr u32 kFloatExponentUnit = 1 << 23;
  inline constexpr u32 kNotFloatExponentMask = ~kFloatExponentMask;
  inline constexpr float kInvPi = 1.0f / kPi;
  inline constexpr float kInv2Pi = 1.0f / k2Pi;
  inline constexpr double kDefaultSampleRate = 44100.0;
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
  inline constexpr float kInfDb = 764.616188299f;
  inline constexpr float kMinusInfDb = -758.595589072f;

  // FFT constants; some internal processing relies that sizes be powers of 2
  inline constexpr u32 kMinFFTOrder = 7;                                              // (can be changed)    128 samples min
  inline constexpr u32 kMaxFFTOrder = 14;                                             // (can be changed)    16384 samples max
  inline constexpr u32 kDefaultFFTOrder = 12;                                         // (can be changed)    4096 samples default
  inline constexpr float kMinWindowOverlap = 0.0f;                                    // (can be changed)    minimum window overlap
  inline constexpr float kMaxWindowOverlap = 0.96875f;                                // (can be changed)    maximum window overlap
  inline constexpr float kDefaultWindowOverlap = 0.5f;                                // (can be changed)    default window overlap
  inline constexpr float kAlphaLowerBound = 1.0f;                                     // (can be changed)    lower bound for alpha exponent
  inline constexpr float kAlphaUpperBound = 10.0f;                                    // (can be changed)    upper bound for alpha exponent
  inline constexpr u32 kWindowResolution = (1 << 10) + 1;                             // (can be changed)    1025 samples window lookup resolution 
                                                                                      //                     (+ 1 in order to have a distinct sample in the center)
  
  // processing constants
  inline constexpr double kMinFrequency = kMidi0Frequency;                            // (can be changed)    lowest frequency that will be displayed

  // GUI constants
  inline constexpr float kWindowScaleIncrements = 0.25f;
  inline constexpr float kMinWindowScaleFactor = 0.5f;
  inline constexpr float kMaxWindowScaleFactor = 3.0f;
  inline constexpr int kParameterUpdateIntervalHz = 60;

  // used for updating parameters
  enum class UpdateFlag : u8 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };
}

using namespace common;
