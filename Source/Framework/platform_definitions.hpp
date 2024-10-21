/*
  ==============================================================================

    platform_definitions.hpp
    Created: 28 Jun 2023 11:32:37pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined (_WIN32) || defined (_WIN64)
  #define COMPLEX_WINDOWS 1
#elif defined (LINUX) || defined (__linux__)
  #define COMPLEX_LINUX 1
#elif defined (__APPLE_CPP__) || defined (__APPLE_CC__)
  #define COMPLEX_MAC 1
#else
  #error Unsupported Platform
#endif

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
  #endif
#endif



namespace common
{
  // using rust/zig naming because yes
  using u8 = uint8_t;
  using u16 = uint16_t;
  using u32 = uint32_t;
  using u64 = uint64_t;

  using i8 = int8_t;
  using i16 = int16_t;
  using i32 = int32_t;
  using i64 = int64_t;

  using usize = size_t;
  using isize = ptrdiff_t;

  // if this fails then we have a big problemo
  // should hold true on platforms where rust and zig can run
  static_assert(sizeof(size_t) == sizeof(uintptr_t));
  static_assert(sizeof(ptrdiff_t) == sizeof(intptr_t));

  #if __SSE4_1__
    #define COMPLEX_SSE4_1 1
    #define COMPLEX_SIMD_ALIGNMENT 16

    inline constexpr u32 kSimdRatio = 4;
    #if __FMA__
      #define COMPLEX_FMA 1
    #endif
  #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    #define COMPLEX_NEON 1
    #define COMPLEX_SIMD_ALIGNMENT 16

    inline constexpr u32 kSimdRatio = 4;
  #else
    #error Your CPU architecture is unsupported
  #endif

  inline constexpr u32 kComplexSimdRatio = kSimdRatio / 2;
}

#if COMPLEX_DEBUG
  namespace common
  {
    void complexPrintAssertMessage(const char *conditionString, const char *fileName, 
      const char *functionName, int line, bool hasMoreArgs = false, ...);

    #if !COMPLEX_MSVC
      #if COMPLEX_X64
        __attribute__((always_inline)) inline void trap_instruction() { __asm__ volatile("int $0x03"); }
      #elif COMPLEX_ARM
        #if COMPLEX_LINUX
          __attribute__((always_inline)) inline void trap_instruction() { __asm__ volatile(".inst 0xd4200000"); }
        #elif COMPLEX_MAC
          __attribute__((always_inline)) inline void trap_instruction() { __builtin_debugtrap(); }
        #endif
      #endif
    #endif
  }

  #if COMPLEX_MSVC
    extern "C" void __cdecl __debugbreak(void);
    #define COMPLEX_ASSERT(condition, ...) (void)((!!(condition)) || (complexPrintAssertMessage(#condition, \
      __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), __debugbreak(), false))
    #define COMPLEX_ASSERT_FALSE(...) (void)(complexPrintAssertMessage(nullptr, \
      __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), __debugbreak(), false)
  #else
    #define COMPLEX_ASSERT(condition, ...) (void)((!!(condition)) || (complexPrintAssertMessage(#condition, \
      __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), ::common::trap_instruction(), false))
    #define COMPLEX_ASSERT_FALSE(...) (void)(complexPrintAssertMessage(nullptr, \
      __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), ::common::trap_instruction(), false)
  #endif
#else
  #define COMPLEX_ASSERT(...) ((void)0)
  #define COMPLEX_ASSERT_FALSE(...) ((void)0)
#endif

#if defined (COMPLEX_MSVC)
  // strict_inline is used when something absolutely needs to be inlined
  #define strict_inline __forceinline
  // perf_inline is used for inlining to increase performance (can be changed for A/B testing)
  #define perf_inline __forceinline
  #define vector_call __vectorcall

  #if defined (COMPLEX_X64)
    #define COMPLEX_INTEL_SVML
  #endif
#else
  // strict_inline is used when something absolutely needs to be inlined
  #define strict_inline inline __attribute__((always_inline))
  // perf_inline is used for inlining to increase performance (can be changed for A/B testing)
  #define perf_inline inline __attribute__((always_inline))
  #define vector_call
#endif

using namespace common;
