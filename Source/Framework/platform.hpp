
// Created: 2023-06-28 23:32:37

#pragma once

#if defined (_WIN32) || defined (_WIN64)
  #define COMPLEX_WINDOWS 1
#elif defined (LINUX) || defined (__linux__)
  #define COMPLEX_LINUX 1
#elif defined (__APPLE__ ) || defined (__MACH__)
  #define COMPLEX_MAC 1
#else
  #error Unsupported Platform
#endif

#if defined (__GNUC__)
  #define COMPLEX_NO_UNIQUE_ADDRESS [[no_unique_address]]
  #if defined (__clang__)
    #define COMPLEX_CLANG 1
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


#if COMPLEX_MSVC
  // strict_inline is used when something absolutely needs to be inlined
  #define strict_inline __forceinline
  #define vector_call __vectorcall

  #define COMPLEX_NO_FAST_MATH_BEGIN _Pragma("float_control(precise, on)")
  #define COMPLEX_NO_FAST_MATH_END _Pragma("float_control(precise, off)")

  // msvc is special because it will not placement new objects with FAMs, so we have to lie to it...
  // this is also more or less guaranteed to work because of windows backwards compatibility
  // https://devblogs.microsoft.com/oldnewthing/20040826-00/?p=38043
  #define COMPLEX_FAM 1

  extern "C" __declspec(noreturn) void __fastfail(unsigned int code);
  #pragma intrinsic(__fastfail)

  namespace common { [[noreturn]] strict_inline void fail_fast() { ::__fastfail(7 /*FAST_FAIL_FATAL_APP_EXIT*/); } }
  #define COMPLEX_TRAP() ::common::fail_fast()

  #if COMPLEX_X64
    extern "C" void _mm_pause(void);
    #define COMPLEX_PAUSE() _mm_pause()
  #elif COMPLEX_ARM
    extern "C" void __yield(void);
    #define COMPLEX_PAUSE() __yield()
  #endif

  #if COMPLEX_X64
    #define COMPLEX_INTEL_SVML
  #endif
#else
  // strict_inline is used when something absolutely needs to be inlined
  #define strict_inline inline __attribute__((always_inline))
  #define vector_call

  #define COMPLEX_NO_FAST_MATH_BEGIN _Pragma("GCC push_options") _Pragma("GCC optimize (\"no-fast-math\")")
  #define COMPLEX_NO_FAST_MATH_END _Pragma("GCC pop_options")

  #define COMPLEX_FAM

  #define COMPLEX_TRAP() __builtin_trap()	

  #if COMPLEX_X64
    #define COMPLEX_PAUSE() __asm__ __volatile__ ("pause")
  #elif COMPLEX_ARM
    #define COMPLEX_PAUSE() __asm__ __volatile__ ("yield")
  #endif

#endif


#if COMPLEX_DEBUG
  #if COMPLEX_MSVC
    extern "C" void __cdecl __debugbreak(void);
    #define COMPLEX_DEBUG_TRAP() __debugbreak()
  #elif COMPLEX_CLANG
    #define COMPLEX_DEBUG_TRAP() __builtin_debugtrap()
  #else
    #if COMPLEX_X64
      #define COMPLEX_DEBUG_TRAP() [](){ __asm__ volatile("int $0x03"); }()
    #elif COMPLEX_ARM
      #define COMPLEX_DEBUG_TRAP() [](){ __asm__ volatile(".inst 0xd4200000"); }()
    #endif
  #endif

  #define COMPLEX_LOG(format, ...) ::common::complexLogMessage(__FILE__, __func__, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
  #define COMPLEX_DEBUG_LOG(format, ...) ::common::complexLogMessage(nullptr, nullptr, __LINE__, format __VA_OPT__(,) __VA_ARGS__)

  #define COMPLEX_ASSERT(condition, ...) (void)((!!(condition)) || (::common::complexPrintAssertMessage(#condition, \
    __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_DEBUG_TRAP(), false))
  #define COMPLEX_ASSERT_FALSE(...) (void)(::common::complexPrintAssertMessage(nullptr, \
    __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_DEBUG_TRAP(), false)
#else
  #define COMPLEX_DEBUG_TRAP ((void)0)
  #define COMPLEX_LOG(...) ((void)0)
  #define COMPLEX_DEBUG_LOG(...) ((void)0)
  #define COMPLEX_ASSERT(...) ((void)0)
  #define COMPLEX_ASSERT_FALSE(...) ((void)0)
#endif

#define COMPLEX_HARD_ASSERT(condition, ...) (void)((!!(condition)) || (::common::complexPrintAssertMessage(#condition, \
  __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_TRAP(), false))
#define COMPLEX_HARD_ASSERT_FALSE(...) (void)(::common::complexPrintAssertMessage(nullptr, \
    __FILE__, __func__, __LINE__ __VA_OPT__(, true,) __VA_ARGS__), COMPLEX_TRAP(), false)

#define sizealignof(T, /*count*/ ...) __VA_OPT__((__VA_ARGS__) *) sizeof(T), alignof(T)
#define countof(array) (sizeof(array) / sizeof(array[0]))

#define COMPLEX_KB(bytes) (bytes << 10)
#define COMPLEX_MB(bytes) (bytes << 20)
#define COMPLEX_GB(bytes) (bytes << 30)

namespace common
{
  // use an actually sane naming scheme

#ifdef _MSC_VER
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

  enum class byte : unsigned char { };

  using usize = decltype(sizeof(0));
  using isize = decltype(static_cast<int *>(nullptr) - static_cast<int *>(nullptr));
  static_assert(sizeof(isize) == sizeof(usize));

  // this assert ensures u/intptr_t are the same size as size_t and ptrdiff_t
  // should hold true on platforms where rust and zig can run
  static_assert(sizeof(usize) == sizeof(void *));

  // not an actual uuid container, use unix nanoseconds for ids
  using uuid = u64;

  // using int instead of bool to avoid standard argument promotion bs
  void complexPrintAssertMessage(const char *conditionString, const char *fileName,
    const char *functionName, int line, int hasMoreArgs = false, ...);

  void complexLogMessage(const char *fileName, const char *functionName, 
    int line, const char *format, ...);
}

using namespace common;

extern "C"
{
  void *memcpy(void *destination, const void *source, usize count);
  void *memmove(void *destination, const void *source, usize count);
  void *memset(void *destination, int value, usize count);

  float fabsf(float arg);
  float truncf(float arg);
  float floorf(float arg);
  float ceilf(float arg);
  float roundf(float arg);
  float logf(float arg);
  float log2f(float arg);
  float log10f(float arg);
  float powf(float base, float exponent);
  float sqrtf(float arg);

  double fabs(double arg);
  double trunc(double arg);
  double floor(double arg);
  double ceil(double arg);
  double round(double arg);
  double log(double arg);
  double log2(double arg);
  double log10(double arg);
  double pow(double base, double exponent);
  double sqrt(double arg);

  unsigned long strtoul(const char *string, char **string_end, int base);
  float         strtof(const char *string, char **string_end);
  double        strtod(const char *string, char **string_end);

  int stbsp_snprintf(char *buffer, int count, const char *format, ...);
}

#define zeroset(destination, /*count*/ ...) memset((destination), 0, sizeof(*(destination)) __VA_OPT__(* (__VA_ARGS__)))
#define valcpy(destination, source, /*count*/ ...) memcpy((destination), (source), sizeof(*(destination)) __VA_OPT__(* (__VA_ARGS__)))


#ifdef _MSC_VER
  #define MSVC_CDECL __cdecl
  #define PLACEMENT_NEW_CONSTEXPR [[msvc::constexpr]]
#else
  #define MSVC_CDECL
  #define PLACEMENT_NEW_CONSTEXPR constexpr
#endif


// if these cause ODR errors, then we're including headers from C++ which shouldn't be happening
// track down the included headers causing the issues and remove them

[[nodiscard]] PLACEMENT_NEW_CONSTEXPR void *MSVC_CDECL operator new(decltype(sizeof(0)), void *pointer) noexcept { return pointer; }
PLACEMENT_NEW_CONSTEXPR void MSVC_CDECL operator delete(void *, void *) noexcept { }

[[nodiscard]] PLACEMENT_NEW_CONSTEXPR void *MSVC_CDECL operator new[](decltype(sizeof(0)), void *pointer) noexcept { return pointer; }
PLACEMENT_NEW_CONSTEXPR void MSVC_CDECL operator delete[](void *, void *) noexcept { }


#undef MSVC_CDECL
#undef PLACEMENT_NEW_CONSTEXPR
