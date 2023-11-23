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
#include "nested_enum.h"
#include "JuceHeader.h"
#include "constants.h"

namespace CommonConcepts
{
	template<typename T>
	concept SimdInt = std::same_as<T, simd_int>;

	template<typename T>
	concept SimdFloat = std::same_as<T, simd_float>;

	template<typename T>
	concept SimdValue = SimdInt<T> || SimdFloat<T>;

	template<typename T>
	concept Pointer = std::is_pointer_v<T>;

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

namespace Framework
{
	// used for updating parameters
	enum class UpdateFlag : u32 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };

	NESTED_ENUM((WindowTypes, u32), (, Lerp, Hann, Hamming, Triangle, Sine, Rectangle, Exp, HannExp, Lanczos))
}