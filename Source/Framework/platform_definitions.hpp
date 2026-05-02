/*
  ==============================================================================

    platform_definitions.hpp
    Created: 28 Jun 2023 11:32:37pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

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
  #define COMPLEX_NO_UNIQUE_ADDRESS [[no_unique_address]]
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
  #define COMPLEX_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
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
#else
  #error Unsupported Compiler
#endif

// don't forget to define -msse4.1 (and -mfma) on gcc/clang
#if __SSE4_1__
  #define COMPLEX_SSE4_1 1
  #define COMPLEX_SIMD_ALIGNMENT 16
  #if __FMA__
    #define COMPLEX_FMA 1
  #endif
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
  #define COMPLEX_NEON 1
  #define COMPLEX_SIMD_ALIGNMENT 16
#else
  #error Unsupported CPU Architecture
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

#if COMPLEX_DEBUG
  namespace common
  {
    // using int instead of bool to avoid standard argument promotion bs
    void complexPrintAssertMessage(const char *conditionString, const char *fileName,
      const char *functionName, int line, int hasMoreArgs = false, ...);
  }

  #if COMPLEX_MSVC
    extern "C" void __cdecl __debugbreak(void);
    #define COMPLEX_TRAP __debugbreak()
  #else
    #if COMPLEX_X64
      #define COMPLEX_TRAP do { __asm__ volatile("int $0x03"); } while(0)
    #elif COMPLEX_ARM
      #if COMPLEX_LINUX
        #define COMPLEX_TRAP do { __asm__ volatile(".inst 0xd4200000"); } while(0)
      #elif COMPLEX_MAC
        #define COMPLEX_TRAP __builtin_debugtrap()
      #endif
    #endif
  #endif

  #define COMPLEX_ASSERT(condition, ...) (void)((!!(condition)) || (complexPrintAssertMessage(#condition, \
    __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_TRAP, false))
  #define COMPLEX_ASSERT_FALSE(...) (void)(complexPrintAssertMessage(nullptr, \
    __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_TRAP, false)
#else
  #define COMPLEX_TRAP ((void)0)
  #define COMPLEX_ASSERT(...) ((void)0)
  #define COMPLEX_ASSERT_FALSE(...) ((void)0)
#endif

namespace common
{
  // use an actually sane naming scheme

#if COMPLEX_MSVC
  using u8 = unsigned __int8;
  using u16 = unsigned __int16;
  using u32 = unsigned __int32;
  using u64 = unsigned __int64;

  using i8 = __int8;
  using i16 = __int16;
  using i32 = __int32;
  using i64 = __int64;
#else
  using u8 = __UINT8_TYPE__;
  using u16 = __UINT16_TYPE__;
  using u32 = __UINT32_TYPE__;
  using u64 = __UINT64_TYPE__;

  using i8 = __INT8_TYPE__;
  using i16 = __INT16_TYPE__;
  using i32 = __INT32_TYPE__;
  using i64 = __INT64_TYPE__;
#endif

  using usize = decltype(sizeof(int));
  using isize = decltype(static_cast<int *>(nullptr) - static_cast<int *>(nullptr));

  // these asserts ensure u/intptr_t are the same size as size_t and ptrdiff_t
  // should hold true on platforms where rust and zig can run
  static_assert(sizeof(usize) == sizeof(void *));
  static_assert(sizeof(isize) == sizeof(usize));
}

using namespace common;
