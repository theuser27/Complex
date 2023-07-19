/*
  ==============================================================================

    platform_definitions.h
    Created: 28 Jun 2023 11:32:37pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#if defined (__GNUC__)
	#if defined (__clang__)
		#define COMLPEX_CLANG 1
	#else
		#define COMPLEX_GCC 1
	#endif

	#ifdef NDEBUG
		#define COMPLEX_DEBUG 0
	#else
		#define COMPLEX_DEBUG 1
	#endif

	#if defined (__x86_64__)
		#define COMPLEX_X64 1
	#elif defined (__aarch64__) 
		#define COMPLEX_ARM 1
	#endif
	
#elif defined (_MSC_VER)
	#define COMPLEX_MSVC 1
	#if defined (_M_X64)
		#define COMPLEX_X64 1

		#ifdef _DEBUG
			#define COMPLEX_DEBUG 1
		#else
			#define COMPLEX_DEBUG 0
		#endif

		// MSVC doesn't define these macros for some reason
		// if your computer doesn't support FMA you can change the value to 0
		#define __SSE4_1__ 1
		#define __FMA__ 1

	#elif defined (_M_ARM64)
		#define COMPLEX_ARM 1
	#else
		#error Your CPU architecture is unsupported
	#endif
#endif

#if __SSE4_1__
	#define COMPLEX_SSE4_1 1
	#define COMPLEX_SIMD_ALIGNMENT 16
	#if __FMA__
		#define COMPLEX_FMA 1
	#endif
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
	#define COMPLEX_NEON 1
	#define COMPLEX_SIMD_ALIGNMENT 16
#endif

#if defined (COMPLEX_MSVC)
	// strict_inline is used when something absolutely needs to be inlined
	#define strict_inline __forceinline
	// perf_inline is used for inlining to increase performance (can be changed for tests)
	#define perf_inline __forceinline
	#define vector_call __vectorcall
#else
	// strict_inline is used when something absolutely needs to be inlined
	#define strict_inline inline __attribute__((always_inline))
	// perf_inline is used for inlining to increase performance (can be changed for tests)
	#define perf_inline inline __attribute__((always_inline))
	#define vector_call
#endif
