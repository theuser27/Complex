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
#include <complex>

#if DEBUG
	#include <cassert>
	#define COMPLEX_ASSERT(x) assert(x)
	#define COMPLEX_ASSERT_FALSE(x) assert(false && (x))
#else
	#define COMPLEX_ASSERT(x) ((void)0)
	#define COMPLEX_ASSERT_FALSE(x) ((void)0)
#endif

#include <Third Party/gcem/gcem.hpp>
#include <Third Party/magic_enum/magic_enum.hpp>
#include "JuceHeader.h"
#include "constants.h"

namespace common
{
	// used for updating parameters
	enum class UpdateFlag : u32 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };

	namespace commonConcepts
	{
		template<typename T>
		concept Pointer = std::is_pointer_v<T>;

		template<typename Derived, typename Base>
		concept DerivedOrIs = std::derived_from<Derived, Base> || std::same_as<Derived, Base>;

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
	{ Lerp = 0, Hann, Hamming, Triangle, Sine, Rectangle, Exp, HannExp, Lanczos };

	enum class EffectTypes : u32 { Utility, Filter, Dynamics, Phase, Pitch, Stretch, Warp, Destroy };

	// Normal - Lowpass/Highpass/Bandpass/Notch
	// Regular - Harmonic/Bin based filters (like dtblkfx peaks)
	enum class FilterModes : u32 { Normal = 0, Regular };
	// Contrast - dtblkfx contrast
	// Compressor - specops spectral compander/compressor
	enum class DynamicsModes : u32 { Contrast = 0, Clip, Compressor };
}