// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <https://unlicense.org>

#pragma once

// The following macros can be defined by the user:
// 1. SATOMI_ARM_USE_LSE128 - to use SWPP* instructions for 128 bit atomic_exchange/store
//    Unfortunately no compiler currently provides a macro definition to check for this automatically so the user has to define something
//    and '+lse128' still needs to be enabled otherwise it will fail to compile, i.e. '-march=armv9.4-a+lse128'
//    https://developer.arm.com/documentation/ddi0602/2024-03/Base-Instructions/SWPP--SWPPA--SWPPAL--SWPPL--Swap-quadword-in-memory-
// 2. SATOMI_BREAK_ARM_MSVC_ABI_COMPATIBILITY - forces NON-conformance on clang/mingw with MSVC STL for ARM64 (without LSE capability) on Windows.
//    By default an extra memory barrier (dmb ish) will be inserted after any successful stores
//    (atomic_compare_exchange_*, atomic_exchange, atomic_store, atomic_fetch_*) if memory_order == seq_cst.
//    Use this if you want to avoid the cost of the extra fence if you're not interfacing with the MSVC STL. For more info:
//    https://reviews.llvm.org/D141748
//    https://github.com/llvm/llvm-project/commit/1ea201d73be2fdf03347e9c6be09ebed5f8e0e00

extern "C"
{
#if defined(_WIN64)

  int __stdcall WaitOnAddress(volatile void *Address, void *CompareAddress, unsigned __int64 AddressSize, unsigned long dwMilliseconds);
  void __stdcall WakeByAddressSingle(void *Address);
  void __stdcall WakeByAddressAll(void *Address);
  #pragma comment(lib, "Synchronization.lib")

#elif defined(LINUX) || defined(__linux__)

  #if !defined(__x86_64__) && !defined(__aarch64__)
    #error Unsupported processor
  #endif

#elif defined (__APPLE__)

  #if !defined(__x86_64__) && !defined(__aarch64__)
    #error Unsupported processor
  #endif

  // private macOS api (Darwin 16, macOS 10.12), needs to be weakly linked or have used dlsym to use
  // https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/sys/ulock.h#L68
  // for more info:
  // https://shift.click/blog/futex-like-apis/#darwin-macos-ios-tvos-watchos-and-more
  // https://github.com/llvm/llvm-project/blob/dc3ae608e95a62e0a4a2532d87bf34ce7c9714ef/libcxx/src/atomic.cpp#L82
  #define SATOMI_UL_COMPARE_AND_WAIT 1
  #define SATOMI_ULF_WAKE_ALL        0x00000100

  int __ulock_wait(__UINT32_TYPE__ operation, void *addr, __UINT64_TYPE__ value, __UINT32_TYPE__ timeout) __attribute__((weak_import));
  int __ulock_wake(__UINT32_TYPE__ operation, void *addr, __UINT64_TYPE__ wake_value) __attribute__((weak_import));

#else

  #error Unsupported platform

#endif

// trivially copyable && copy/move constructible/assignable and size must be a power-of-2
#define SATOMI_IS_ATOMIC_READY(T) detail::is_trivially_copyable<T> && requires(const T &v) { T(v); } && \
  requires(const T &v, T u) { u = v; } && requires(T &&v) { T(v); } && requires(T &&v, T u) { u = v; } && \
  ((sizeof(T) & (sizeof(T) - 1)) == 0)
// safely reinterpreting arbitrary types to integrals (padding bits are NOT taken into account)
#define SATOMI_BIT_CAST(To, value) __builtin_bit_cast(To, value)
#define SATOMI_HAS_PADDING_BITS(T) !__has_unique_object_representations(T) && !detail::type_list<float, double, long double>::any_of<T>()
// adding simple constexpr support for operations
#define SATOMI_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
#define SATOMI_CHECK_ALIGNMENT(alignment, x) (void)((decltype(sizeof(int))(&x) % alignment) == 0 || (SATOMI_TRAP(), true))

#define SATOMI_IS_SAME(...) detail::type_list<__VA_ARGS__>::all_same()
#define SATOMI_IS_POINTER(T) requires(T a) { [](auto *){}(a); }


#if defined(_MSC_VER) && ! (__clang__)

  void _ReadWriteBarrier(void);
  // pragma to avoid deprecation warnings
  #define SATOMI_COMPILER_BARRIER() _Pragma("warning(push)") _Pragma("warning(disable : 4996)") _ReadWriteBarrier() _Pragma("warning(pop)")
  [[noreturn]] void __fastfail(unsigned int code);
  #pragma intrinsic(__fastfail)
  #define SATOMI_TRAP() __fastfail(/*FAST_FAIL_FATAL_APP_EXIT*/ 7)
  #define SATOMI_CLEAR_PADDING_BITS(x) __builtin_zero_non_value_bits(x)
  #define SATOMI_INLINE __forceinline
  #define SATOMI_U64 unsigned __int64

  #define SATOMI_CHOOSE_SIZE(macro, size, base, ...) \
    if constexpr (sizeof(T) == 1) { __int8 out; SATOMI_CHOOSE_MEMORY_ORDER(order, out = base##8, (macro(__int8))); __VA_ARGS__ }        \
    else if constexpr (sizeof(T) == 2) { __int16 out; SATOMI_CHOOSE_MEMORY_ORDER(order, out = base##16, (macro(__int16))); __VA_ARGS__ } \
    else if constexpr (sizeof(T) == 4) { long out; SATOMI_CHOOSE_MEMORY_ORDER(order, out = base, (macro(long))); __VA_ARGS__ }        \
    else if constexpr (sizeof(T) == 8) { __int64 out; SATOMI_CHOOSE_MEMORY_ORDER(order, out = base##64, (macro(__int64))); __VA_ARGS__ }

  #if defined(_M_ARM64) || defined(_M_ARM64EC)

    __int64 __ldrexd(const volatile __int64 *);
    void __dmb(unsigned int _Type);
    #pragma intrinsic(__dmb)

    #define SATOMI_DEFINE_MEMORY_ORDERS(X, args) X args; X##_nf args; X##_acq args; X##_rel args;
    #define SATOMI_CHOOSE_MEMORY_ORDER(order, X, args) \
      if (order == memory_order_relaxed) { X##_nf args; } \
      else if (order == memory_order_consume || order == memory_order_acquire) { X##_acq args; } \
      else if (order == memory_order_release) { X##_rel args; } \
      else if (order == memory_order_acq_rel || order == memory_order_seq_cst) { X args; } \
      else { __fastfail(/*FAST_FAIL_FATAL_APP_EXIT*/ 7); }
    #define SATOMI_COMPILER_OR_MEMORY_BARRIER() __dmb(/*_ARM64_BARRIER_ISH*/ 0xB)

  #else

    #define SATOMI_DEFINE_MEMORY_ORDERS(X, args) X args;
    #define SATOMI_CHOOSE_MEMORY_ORDER(order, X, args) X args;
    // x86/x64 hardware only emits memory barriers inside _Interlocked intrinsics
    #define SATOMI_COMPILER_OR_MEMORY_BARRIER() SATOMI_COMPILER_BARRIER()

    long _InterlockedIncrement(long volatile * _Addend);
  #endif

  // necessary in order to not inject hidden memory ordering guarantees (like with /volatile:ms)
  // for more info https://learn.microsoft.com/en-us/cpp/intrinsics/arm-intrinsics?view=msvc-170#remarks
  __int8 __iso_volatile_load8(const volatile __int8 *location);
  __int16 __iso_volatile_load16(const volatile __int16 *location);
  __int32 __iso_volatile_load32(const volatile __int32 *location);
  __int64 __iso_volatile_load64(const volatile __int64 *location);
  void __iso_volatile_store8(volatile __int8 *location, __int8 value);
  void __iso_volatile_store16(volatile __int16 *location, __int16 value);
  void __iso_volatile_store32(volatile __int32 *location, __int32 value);
  void __iso_volatile_store64(volatile __int64 *location, __int64 value);

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedCompareExchange, (long volatile *target, long exchange, long comparand))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedCompareExchange8, (char volatile *target, char exchange, char comparand))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedCompareExchange16, (short volatile *target, short exchange, short comparand))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedCompareExchange64, (__int64 volatile *target, __int64 exchange, __int64 comparand))

  SATOMI_DEFINE_MEMORY_ORDERS(unsigned char _InterlockedCompareExchange128, (__int64 volatile *target, __int64 high, __int64 low, __int64 *comparand))

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedAnd, (long volatile *target, long value))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedAnd8, (char volatile *target, char value))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedAnd16, (short volatile *target, short value))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedAnd64, (__int64 volatile *target, __int64 value))

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedExchange, (long volatile *target, long value))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedExchange8, (char volatile *target, char value))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedExchange16, (short volatile *target, short value))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedExchange64, (__int64 volatile *target, __int64 value))

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedExchangeAdd, (long volatile *target, long value))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedExchangeAdd8, (char volatile *target, char value))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedExchangeAdd16, (short volatile *target, short value))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedExchangeAdd64, (__int64 volatile *target, __int64 value))

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedOr, (long volatile *target, long value))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedOr8, (char volatile *target, char value))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedOr16, (short volatile *target, short value))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedOr64, (__int64 volatile *target, __int64 value))

  SATOMI_DEFINE_MEMORY_ORDERS(long _InterlockedXor, (long volatile *target, long value))
  SATOMI_DEFINE_MEMORY_ORDERS(char _InterlockedXor8, (char volatile *target, char value))
  SATOMI_DEFINE_MEMORY_ORDERS(short _InterlockedXor16, (short volatile *target, short value))
  SATOMI_DEFINE_MEMORY_ORDERS(__int64 _InterlockedXor64, (__int64 volatile *target, __int64 value))

  #undef SATOMI_DEFINE_MEMORY_ORDERS

#else

  // currently clang doesn't implement either functions
  // but when/if it decides to it can choose
  #if __has_builtin(__builtin_clear_padding)
    #define SATOMI_CLEAR_PADDING_BITS(x) __builtin_clear_padding(x)
  #elif __has_builtin(__builtin_clear_padding)
    #define SATOMI_CLEAR_PADDING_BITS(x) __builtin_zero_non_value_bits(x)
  #else
    #define SATOMI_CLEAR_PADDING_BITS(x) (void)(x)
  #endif

  #define SATOMI_TRAP() __builtin_trap()
  #define SATOMI_INLINE inline __attribute__((always_inline))
  #define SATOMI_U64 __UINT64_TYPE__

  #if defined(_WIN32) && !defined(SATOMI_BREAK_ARM_MSVC_ABI_COMPATIBILITY)
    // stupid ABI fence by a stupid company
    #define SATOMI_MSVC_STL_SEQ_CST_FENCE "dmb ish\n\t"
  #else
    #define SATOMI_MSVC_STL_SEQ_CST_FENCE ""
  #endif

  #define SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, ...)\
    if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "", "", __VA_ARGS__) } \
    else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("a", "", "", __VA_ARGS__) } \
    else if (order == memory_order_release) { SATOMI_ATOMIC_ASM("", "l", "", __VA_ARGS__) } \
    else if (order == memory_order_acq_rel) { SATOMI_ATOMIC_ASM("a", "l", "", __VA_ARGS__) } \
    else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("a", "l", SATOMI_MSVC_STL_SEQ_CST_FENCE, __VA_ARGS__) } \
    else { __builtin_trap(); }

#endif
}

namespace satomi
{
  namespace detail
  {
    template<typename ... Ts>
    struct type_list
    {
      constexpr auto operator->*(const type_list &) const noexcept { return *this; }
      static constexpr bool all_same() noexcept
      {
        if constexpr (sizeof...(Ts) <= 1)
          return true;
        else
          return requires { (... ->* type_list<Ts>{}); };
      }

      constexpr bool operator==(type_list) const noexcept { return true; }
      template<typename ... Us>
      constexpr bool operator==(type_list<Us...>) const noexcept { return false; }
      template<typename T>
      static constexpr bool any_of() noexcept { return ((type_list<T>{} == type_list<Ts>{}) || ...); }
    };

    template<typename T> struct remove_reference
    { static constexpr bool is_ref = false; static constexpr bool is_lvalue_or_rvalue_ref = false; using type = T; };
    template<typename T> struct remove_reference<T &>
    { static constexpr bool is_ref = true; static constexpr bool is_lvalue_or_rvalue_ref = false; using type = T; };
    template<typename T> struct remove_reference<T &&>
    { static constexpr bool is_ref = true; static constexpr bool is_lvalue_or_rvalue_ref = true; using type = T; };

    template<typename T>
    inline constexpr bool is_trivially_copyable = __is_trivially_copyable(T);

    using ptrdiff_t = decltype(static_cast<int *>(nullptr) - static_cast<int *>(nullptr));

  #if defined (LINUX) || defined(__linux__) || defined(__APPLE__)

    // align(64) to avoid false sharing between slots
    struct alignas(64) waiting_slot
    {
      int wait_count = 0;
      int version = 0;
    };

    #define SATOMI_WAITING_LIST_COUNT (1 << 7)
    inline constinit waiting_slot waiter_list[SATOMI_WAITING_LIST_COUNT]{};

    SATOMI_INLINE waiting_slot &get_waiting_slot(const volatile void *address)
    {
      // shift right by 2 because of 4 byte int alignment
      auto key = ((__UINTPTR_TYPE__)address >> 2) & (SATOMI_WAITING_LIST_COUNT - 1);
      return waiter_list[key];
    }
    #undef SATOMI_WAITING_LIST_COUNT

  #endif
  }

  enum class memory_order { relaxed, consume, acquire, release, acq_rel, seq_cst };
  inline constexpr auto memory_order_relaxed = memory_order::relaxed;
  inline constexpr auto memory_order_consume = memory_order::consume;
  inline constexpr auto memory_order_acquire = memory_order::acquire;
  inline constexpr auto memory_order_release = memory_order::release;
  inline constexpr auto memory_order_acq_rel = memory_order::acq_rel;
  inline constexpr auto memory_order_seq_cst = memory_order::seq_cst;

  SATOMI_INLINE constexpr void atomic_thread_fence(memory_order order = memory_order_seq_cst)
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
      return;

  #if defined(_MSC_VER) && !defined(__clang__)

    SATOMI_COMPILER_BARRIER();
    #if defined(_M_ARM64) || defined(_M_ARM64EC)
      if (order == memory_order_acquire || order == memory_order_consume)
        __dmb(/*_ARM64_BARRIER_ISHLD*/ 0x9);
      else
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
    #else
      if (order == memory_order_seq_cst)
      {
      #pragma warning(push)
      #pragma warning(disable : 6001)  // "Using uninitialized memory 'guard'"
      #pragma warning(disable : 28113) // "Accessing a local variable guard via an Interlocked function:
                                        // This is an unusual usage which could be reconsidered."
        volatile long guard;
        (void)_InterlockedIncrement(&guard);
        SATOMI_COMPILER_BARRIER();
      #pragma warning(pop)
      }
    #endif

  #elif defined (__x86_64__)

    if (order == memory_order_seq_cst)
    {
      unsigned char dummy = 0u;
      __asm__ __volatile__ ("lock; notb %0" : "+m" (dummy) : : "memory");
    }
    else if (order != memory_order_relaxed)
      __asm__ __volatile__ ("" ::: "memory");

  #elif defined(__aarch64__)

    if (order != memory_order_relaxed)
    {
      if (order == memory_order_consume || order == memory_order_acquire)
        __asm__ __volatile__ ("dmb ishld\n\t" ::: "memory");
      else
        __asm__ __volatile__ ("dmb ish\n\t" ::: "memory");
    }

  #endif
  }

  SATOMI_INLINE constexpr void atomic_signal_fence(memory_order order = memory_order_seq_cst)
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
      return;

  #if defined(_MSC_VER) && !defined(__clang__)

    if (order != memory_order_relaxed)
      SATOMI_COMPILER_BARRIER();

  #else

    if (order != memory_order_relaxed)
      __asm__ __volatile__ ("" ::: "memory");

  #endif
  }

  SATOMI_INLINE constexpr auto kill_dependency(auto t) noexcept { return t; }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr bool atomic_compare_exchange_strong(volatile T &target,
    T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      if (const_cast<T &>(target) == expected)
      {
        const_cast<T &>(target) = desired;
        return true;
      }
      expected = const_cast<T &>(target);
      return false;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&desired);

  #if defined(_MSC_VER) && ! (__clang__)

    (void)order;

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, desired)), (SATOMI_BIT_CAST(cast, expected))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedCompareExchange,
      T ret = SATOMI_BIT_CAST(T, out);
      if (ret == expected)
        return true;
      expected = ret;
      return false;
    )
    #undef SATOMI_HELPER

    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) int128__ { __int64 v[2]; };
      auto d = SATOMI_BIT_CAST(int128__, desired);

      unsigned char result = 0;
      SATOMI_CHOOSE_MEMORY_ORDER(order, result = _InterlockedCompareExchange128, ((volatile __int64 *)&target,
        d.v[1], d.v[0], (__int64 *)&expected))

      return result != 0;
    }

  #elif defined (__x86_64__)

    (void)order;

    #define SATOMI_ATOMIC_ASM(type, affix)                \
      __asm__ __volatile__                                \
      (                                                   \
        "lock; cmpxchg" affix " %[desired], %[target]\n\t"\
        "sete %[success]"                                 \
        : [target] "+m" (target),                         \
          "+a" (expected), [success] "=q" (success)       \
        : [desired] "q" (desired)                         \
        : "cc", "memory"                                  \
      )

    bool success = 0;
    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q"); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; };
      auto e = SATOMI_BIT_CAST(uint128__, expected);
      auto d = SATOMI_BIT_CAST(uint128__, desired);

      __asm__ __volatile__
      (
        "lock; cmpxchg16b %[target]\n\t"
        "sete %[success]\n\t"
        : [target] "+m" (target),
          "+a" (e.v[0]), "+d" (e.v[1]), [success] "=q" (success)
        : "b" (d.v[0]), "c" (d.v[1])
        : "cc", "memory"
      );

      if (!success)
        expected = SATOMI_BIT_CAST(T, e);
    }

    return success;

    #undef SATOMI_ATOMIC_ASM

  #elif defined(__aarch64__)

    // builtin CAS support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS

      #define SATOMI_ATOMIC_ASM(load_order, store_order, _, type, affix, modifier)                          \
        __asm__ __volatile__                                                                                \
        (                                                                                                   \
          "cas" load_order store_order affix " " modifier "[expected], " modifier "[desired], %[target]\n\t"\
          : [target] "+Q" (target), [expected] "+r" (ret)                                                   \
          : [desired] "r" (desired)                                                                         \
          : "memory"                                                                                        \
        );

      #define SATOMI_PASTE_BLOCK(order, type, ...)                \
        auto e = SATOMI_BIT_CAST(type, expected);                 \
        auto ret = e;                                             \
        SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, type, __VA_ARGS__); \
        if (e == ret) return true;                                \
        expected = SATOMI_BIT_CAST(T, ret);                       \
        return false;

      if constexpr (sizeof(T) == 1) { SATOMI_PASTE_BLOCK(order, __UINT8_TYPE__, "b", "%w"); }
      else if constexpr (sizeof(T) == 2) { SATOMI_PASTE_BLOCK(order, __UINT16_TYPE__, "h", "%w"); }
      else if constexpr (sizeof(T) == 4) { SATOMI_PASTE_BLOCK(order, __UINT32_TYPE__, "", "%w"); }
      else if constexpr (sizeof(T) == 8) { SATOMI_PASTE_BLOCK(order, __UINT64_TYPE__, "", "%x"); }
      else if constexpr (sizeof(T) == 16)
      {
        struct alignas(16) uint128__ { SATOMI_U64 v[2]; };
        auto e = SATOMI_BIT_CAST(uint128__, expected);
        auto d = SATOMI_BIT_CAST(uint128__, desired);

        // copies values to specific registers
        // on gcc hard register constraints can be used but those are not supported on clang
        // hardcoding caller saved registers (as per ARM64 linux ABI)
        // because ARM expects arguments to start at an even register and be contiguous
        register SATOMI_U64 x8 asm ("x8") = e.v[0];
        register SATOMI_U64 x9 asm ("x9") = e.v[1];
        register SATOMI_U64 x10 asm ("x10") = d.v[0];
        register SATOMI_U64 x11 asm ("x11") = d.v[1];

        #undef SATOMI_ATOMIC_ASM
        #define SATOMI_ATOMIC_ASM(load_order, store_order, ...)                                                         \
          __asm__ __volatile__                                                                                          \
          (                                                                                                             \
            "casp" load_order store_order " %x[expected_0], %x[expected_1], %x[desired_0], %x[desired_1], %[target]\n\t"\
            : [target] "+Q" (*(SATOMI_U64 *)&target), [expected_0] "+r" (x8), [expected_1] "+r" (x9)                     \
            : [desired_0] "r" (x10), [desired_1] "r" (x11)                                                              \
            : "cc", "memory"                                                                                            \
          );

        SATOMI_CHOOSE_MEMORY_ORDER_ASM(order);

        if (e.v[0] == x8 && e.v[1] == x9)
          return true;

        e.v[0] = x8;
        e.v[1] = x9;
        expected = SATOMI_BIT_CAST(T, e);
        return false;
      }

      #undef SATOMI_PASTE_BLOCK
      #undef SATOMI_ATOMIC_ASM

    #else

      bool success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, /*zero extend instruction*/...)\
        __asm__ __volatile__                                                                                                \
        (                                                                                                                   \
          __VA_ARGS__                                                                                                       \
          "1:\n\t"                                                                                                          \
          "ld" load_order "xr" suffix " " modifier "[out], %[target]\n\t"                                                   \
          "cmp " modifier "[out], " modifier "[expected]\n\t"                                                               \
          "b.ne 2f\n\t"                                                                                                     \
          "st" store_order "xr" suffix " %w[success], " modifier "[desired], %[target]\n\t"                                 \
          "cbnz %w[success], 1b\n\t"                                                                                        \
          msvc_fence                                                                                                        \
          "2:\n\t"                                                                                                          \
          "cset %w[success], eq\n\t"                                                                                        \
          : [target] "+Q" (target), [success] "=&r" (success), [out] "=&r" (out)                                            \
          : [desired] "r" (desired), [expected] "r" (expected)                                                              \
          : "cc", "memory"                                                                                                  \
        );

      #define SATOMI_PASTE_BLOCK(order, type, ...)                \
        type out;                                                 \
        SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, type, __VA_ARGS__); \
        if (!success)                                             \
          expected = SATOMI_BIT_CAST(T, out);                     \
        return success

      if constexpr (sizeof(T) == 1) { SATOMI_PASTE_BLOCK(order, __UINT8_TYPE__, "b", "%w", "uxtb %w[expected], %w[expected]\n\t"); }
      else if constexpr (sizeof(T) == 2) { SATOMI_PASTE_BLOCK(order, __UINT16_TYPE__, "h", "%w", "uxth %w[expected], %w[expected]\n\t"); }
      else if constexpr (sizeof(T) == 4) { SATOMI_PASTE_BLOCK(order, __UINT32_TYPE__, "", "%w", ""); }
      else if constexpr (sizeof(T) == 8) { SATOMI_PASTE_BLOCK(order, __UINT64_TYPE__, "", "%x", ""); }
      else if constexpr (sizeof(T) == 16)
      {
        struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;
        auto e = SATOMI_BIT_CAST(uint128__, expected);
        auto d = SATOMI_BIT_CAST(uint128__, desired);

        #undef SATOMI_ATOMIC_ASM
        #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, ...)               \
          __asm__ __volatile__                                                            \
          (                                                                               \
            "1:\n\t"                                                                      \
            "ld" load_order "xp %x[out_0], %x[out_1], %[target]\n\t"                      \
            "cmp %x[out_0], %x[expected_0]\n\t"                                           \
            "ccmp %x[out_1], %x[expected_1], #0, eq\n\t"                                  \
            "b.ne 2f\n\t"                                                                 \
            "st" store_order "xp %w[success], %x[desired_0], %x[desired_1], %[target]\n\t"\
            "cbnz %w[success], 1b\n\t"                                                    \
            msvc_fence                                                                    \
            "2:\n\t"                                                                      \
            "cset %w[success], eq\n\t"                                                    \
            : [success] "=&r" (success), [target] "+Q" (target),                          \
              [out_0] "=&r" (out.v[0]), [out_1] "=&r" (out.v[1])                          \
            : [desired_0] "r" (d.v[0]), [desired_1] "r" (d.v[1]),                         \
              [expected_0] "r" (e.v[0]), [expected_1] "r" (e.v[1])                        \
            : "cc", "memory"                                                              \
          );

        SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)

        if (!success)
          expected = SATOMI_BIT_CAST(T, out);

        return success;
      }

      #undef SATOMI_PASTE_BLOCK
      #undef SATOMI_ATOMIC_ASM

    #endif

  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr bool atomic_compare_exchange_weak(volatile T &target,
    T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      if (const_cast<T &>(target) == expected)
      {
        const_cast<T &>(target) = desired;
        return true;
      }
      expected = const_cast<T &>(target);
      return false;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)

    // on msvc there aren't any weak versions of compare_exchange so forward to compare_exchange_strong
    return atomic_compare_exchange_strong(target, expected, desired, order);

  #elif defined(__x86_64__)

    // compare_exchange_weak and compare_exchange_strong are identical on x86-64
    return atomic_compare_exchange_strong(target, expected, desired, order);

  #elif defined(__aarch64__)

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&desired);

    #pragma push_macro("SATOMI_MSVC_STL_SEQ_CST_FENCE")
    #undef SATOMI_MSVC_STL_SEQ_CST_FENCE
    #define SATOMI_MSVC_STL_SEQ_CST_FENCE "cbnz %w[success], 1f\n\t" "dmb ish\n\t"

    #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, /*zero extend instruction*/...)\
      __asm__ __volatile__                                                                                                \
      (                                                                                                                   \
        __VA_ARGS__                                                                                                       \
        "ld" load_order "xr" suffix " " modifier "[out], %[target]\n\t"                                                   \
        "cmp " modifier "[out], " modifier "[expected]\n\t"                                                               \
        "b.ne 1f\n\t"                                                                                                     \
        "st" store_order "xr" suffix " %w[success], " modifier "[desired], %[target]\n\t"                                 \
        msvc_fence                                                                                                        \
        "1:\n\t"                                                                                                          \
        "eor %w[success], %w[success], #1\n\t"                                                                            \
        : [target] "+Q" (target), [success] "=&r" (success), [out] "=&r" (out)                                            \
        : [desired] "r" (desired), [expected] "r" (expected)                                                              \
        : "cc", "memory"                                                                                                  \
      );

    #define SATOMI_PASTE_BLOCK(order, type, ...)                \
      type out;                                                 \
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, type, __VA_ARGS__); \
      if (!success)                                             \
        expected = SATOMI_BIT_CAST(T, out);                     \
      return success

    bool success;
    if constexpr (sizeof(T) == 1) { SATOMI_PASTE_BLOCK(order, __UINT8_TYPE__, "b", "%w", "uxtb %w[expected], %w[expected]\n\t"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_PASTE_BLOCK(order, __UINT16_TYPE__, "h", "%w", "uxth %w[expected], %w[expected]\n\t"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_PASTE_BLOCK(order, __UINT32_TYPE__, "", "%w", ""); }
    else if constexpr (sizeof(T) == 8) { SATOMI_PASTE_BLOCK(order, __UINT64_TYPE__, "", "%x", ""); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;
      auto e = SATOMI_BIT_CAST(uint128__, expected);
      auto d = SATOMI_BIT_CAST(uint128__, desired);
      bool success;

      #undef SATOMI_ATOMIC_ASM
      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, ...)               \
        __asm__ __volatile__                                                            \
        (                                                                               \
          "ld" load_order "xp %x[out_0], %x[out_1], %[target]\n\t"                      \
          "cmp %x[out_0], %x[expected_0]\n\t"                                           \
          "ccmp %x[out_1], %x[expected_1], #0, eq\n\t"                                  \
          "b.ne 1f\n\t"                                                                 \
          "st" store_order "xp %w[success], %x[desired_0], %x[desired_1], %[target]\n\t"\
          msvc_fence                                                                    \
          "1:\n\t"                                                                      \
          "eor %w[success], %w[success], #1\n\t"                                        \
          : [success] "=&r" (success), [target] "+Q" (target),                          \
            [out_0] "=&r" (out.v[0]), [out_1] "=&r" (out.v[1])                          \
          : [desired_0] "r" (d.v[0]), [desired_1] "r" (d.v[1]),                         \
            [expected_0] "r" (e.v[0]), [expected_1] "r" (e.v[1])                        \
          : "cc", "memory"                                                              \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      if (!success)
        expected = SATOMI_BIT_CAST(T, out);
      return success;
    }

    #undef SATOMI_PASTE_BLOCK
    #pragma pop_macro("SATOMI_MSVC_STL_SEQ_CST_FENCE")

    return 0;

  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_exchange(volatile T &target, T desired, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T result = const_cast<T &>(target);
      const_cast<T &>(target) = desired;
      return result;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&desired);

  #if defined(_MSC_VER) && ! (__clang__)

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, desired))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedExchange,
      return SATOMI_BIT_CAST(T, out);
    )
    #undef SATOMI_HELPER

    else if constexpr (sizeof(T) == 16)
    {
      T previous = desired;
      while (!atomic_compare_exchange_strong(target, previous, desired, order)) {}
      return previous;
    }

  #elif defined(__x86_64__)

    (void)order;

    #define SATOMI_ATOMIC_ASM(type, affix)        \
      __asm__ __volatile__                        \
      (                                           \
        "xchg" affix " %[desired], %[target]\n\t" \
        : [target] "+m" (target),                 \
          [desired] "+r" (desired)                \
        :                                         \
        : "memory"                                \
      );                                          \
      return desired;

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q"); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;
      auto d = SATOMI_BIT_CAST(uint128__, desired);

      __asm__ __volatile__
      (
        // the load needs to be done in assembly because movq is guaranteed to be atomic
        "movq %[target_0], %%rax\n\t"
        "movq %[target_1], %%rdx\n\t"
        ".align 16\n\t"
        "1: lock; cmpxchg16b %[target_0]\n\t"
        "jne 1b\n\t"
        : [target_0] "+m" (((volatile uint128__ *)&target)[0]),
          [target_1] "+m" (((volatile SATOMI_U64 *)&target)[1]),
          "=&a" (out.v[0]), "=&d" (out.v[1])
        : "b" (d.v[0]), "c" (d.v[1])
        : "cc", "memory"
      );

      return SATOMI_BIT_CAST(T, out);
    }

    #undef SATOMI_ATOMIC_ASM

  #elif defined(__aarch64__)

    #define SATOMI_PASTE_BLOCK(order, type, ...)                \
      T out = desired;                                          \
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, type, __VA_ARGS__); \
      return out;

    // builtin exchange support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, ...)       \
        __asm__ __volatile__                                                                            \
        (                                                                                               \
          "swp" load_order store_order suffix " " modifier "[desired], " modifier "[out], %[target]\n\t"\
          : [target] "+Q" (target), [out] "=&r" (out)                                                   \
          : [desired] "r" (desired)                                                                     \
          : "memory"                                                                                    \
        );

    #else

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, ...) \
        bool success;                                                                             \
        __asm__ __volatile__                                                                      \
        (                                                                                         \
          "1:\n\t"                                                                                \
          "ld" load_order "xr" suffix " " modifier "[out], %[target]\n\t"                         \
          "st" store_order "xr" suffix " %w[success], " modifier "[desired], %[target]\n\t"       \
          "cbnz %w[success], 1b\n\t"                                                              \
          msvc_fence                                                                              \
          : [success] "=&r" (success), [target] "+Q" (target), [out] "=&r" (out)                  \
          : [desired] "r" (desired)                                                               \
          : "memory"                                                                              \
        );

    #endif

    if constexpr (sizeof(T) == 1) { SATOMI_PASTE_BLOCK(order, __UINT8_TYPE__, "b", "%w"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_PASTE_BLOCK(order, __UINT16_TYPE__, "h", "%w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_PASTE_BLOCK(order, __UINT32_TYPE__, "", "%w"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_PASTE_BLOCK(order, __UINT64_TYPE__, "", "%x"); }

    #undef SATOMI_ATOMIC_ASM

    else if constexpr (sizeof(T) == 16)
    {
    #ifdef SATOMI_ARM_USE_LSE128

      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;
      auto out = SATOMI_BIT_CAST(uint128__, desired);

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, ...)     \
        __asm__ __volatile__                                                  \
        (                                                                     \
          "swpp" load_order store_order " %x[out_0], %x[out_1], %[target]\n\t"\
          : [target] "+Q" (target),                                           \
            [out_0] "=&r" (out.v[0]), [out_1] "=&r" (out.v[1])                \
          :                                                                   \
          : "memory"                                                          \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)

      return SATOMI_BIT_CAST(T, out);

    #else

      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;
      bool success;
      auto d = SATOMI_BIT_CAST(uint128__, desired);

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, ...)               \
        __asm__ __volatile__                                                            \
        (                                                                               \
          "1:\n\t"                                                                      \
          "ld" load_order "xp %x[out_0], %x[out_1], %[target]\n\t"                      \
          "st" store_order "xp %w[success], %x[desired_0], %x[desired_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                    \
          msvc_fence                                                                    \
          : [success] "=&r" (success), [target] "+Q" (target),                          \
            [out_0] "=&r" (out.v[0]), [out_1] "=&r" (out.v[1])                          \
          : [desired_0] "r" (d.v[0]), [desired_1] "r" (d.v[1])                          \
          : "memory"                                                                    \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)

      return SATOMI_BIT_CAST(T, out);

    #endif
    }

    #undef SATOMI_ATOMIC_ASM
    #undef SATOMI_PASTE_BLOCK

  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_load(const volatile T &target, memory_order order = memory_order_seq_cst) noexcept
  {
    if (order == memory_order_release)
      order = memory_order_acquire;
    else if (order == memory_order_acq_rel)
      order = memory_order_seq_cst;

    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T ret = const_cast<T &>(target);
      return ret;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)


    // ldr + dmb ish, another stupid ABI
    #define SATOMI_HELPER(size)                                                   \
      auto out = __iso_volatile_load##size((const volatile __int##size *)&target);\
      if (order != memory_order_relaxed)                                          \
        SATOMI_COMPILER_OR_MEMORY_BARRIER();                                      \
      return SATOMI_BIT_CAST(T, out);

    if constexpr (sizeof(T) == 1) { SATOMI_HELPER(8); }
    else if constexpr (sizeof(T) == 2) { SATOMI_HELPER(16); }
    else if constexpr (sizeof(T) == 4) { SATOMI_HELPER(32); }
    else if constexpr (sizeof(T) == 8) { SATOMI_HELPER(64); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) int128__ { __int64 v[2]; } out;
      SATOMI_CHOOSE_MEMORY_ORDER(order, (void)_InterlockedCompareExchange128, ((volatile __int64 *)&target, 0, 0, out.v))
      return SATOMI_BIT_CAST(T, out);
    }

    #undef SATOMI_HELPER

  #elif defined(__x86_64__)

    #define SATOMI_ATOMIC_ASM(type, affix)  \
      type out;                             \
      __asm__ __volatile__                  \
      (                                     \
        "mov" affix " %[target], %[out]\n\t"\
        : [out] "=r" (out)                  \
        : [target] "m" (*(type *)&target)   \
        : "memory"                          \
      );                                    \
      return SATOMI_BIT_CAST(T, out);

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, ""); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, ""); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, ""); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q"); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;

      #if defined(__AVX__)

        // Intel Software Developer Manual Volume 3, Guaranteed Atomic Operations
        // Processors supporting AVX guarantee aligned vector moves to be atomic.
        __asm__ __volatile__
        (
          "vmovdqa %[target], %[out]\n\t"
          : [out] "=x" (out)
          : [target] "m" (target)
          : "memory"
        );

      #else

        __asm__ __volatile__
        (
          // store whatever is rbx/rcx in rax/rdx so that
          // even if we succeed to exchange we already have the value in rax/rdx
          "movq %%rbx, %%rax\n\t"
          "movq %%rcx, %%rdx\n\t"
          "lock; cmpxchg16b %[target]\n\t"
          : "=&a" (out.v[0]), "=&d" (out.v[1])
          : [target] "m" (target)
          : "cc", "memory"
        );

      #endif

      return SATOMI_BIT_CAST(T, out);
    }

    #undef SATOMI_ATOMIC_ASM

  #elif defined(__aarch64__)

    #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, ...) \
      __asm__ __volatile__                                                                      \
      (                                                                                         \
        "ld" load_order "r" suffix " " modifier "[out], %[target]\n\t"                          \
        : [out] "=r" (out)                                                                      \
        : [target] "Q" (*(type *)&target)                                                       \
        : "memory"                                                                              \
      );

    #define SATOMI_PASTE_BLOCK(order, type, ...)                \
      type out;                                                 \
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, type, __VA_ARGS__); \
      return SATOMI_BIT_CAST(T, out);

    #pragma push_macro("SATOMI_CHOOSE_MEMORY_ORDER_ASM")
    #undef SATOMI_CHOOSE_MEMORY_ORDER_ASM

    // feature macro to check for ARMv8.3 RCPC/LDAPR
    #if __ARM_FEATURE_RCPC >= 1

      #define SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, ...)\
        if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "", "", __VA_ARGS__) } \
        else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("ap", "", "", __VA_ARGS__) } \
        else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("a", "l", SATOMI_MSVC_STL_SEQ_CST_FENCE, __VA_ARGS__) } \
        else { __builtin_trap(); }

    #else

      #define SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, ...)\
        if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "", "", __VA_ARGS__) } \
        else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("a", "", "", __VA_ARGS__) } \
        else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("a", "l", SATOMI_MSVC_STL_SEQ_CST_FENCE, __VA_ARGS__) } \
        else { __builtin_trap(); }

    #endif

    if constexpr (sizeof(T) == 1) { SATOMI_PASTE_BLOCK(order, __UINT8_TYPE__, "b", "%w"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_PASTE_BLOCK(order, __UINT16_TYPE__, "h", "%w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_PASTE_BLOCK(order, __UINT32_TYPE__, "", "%w"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_PASTE_BLOCK(order, __UINT64_TYPE__, "", "%x"); }

    #undef SATOMI_CHOOSE_MEMORY_ORDER_ASM
    #pragma pop_macro("SATOMI_CHOOSE_MEMORY_ORDER_ASM")

    else if constexpr (sizeof(T) == 16)
    {
    // checking for ARMv8.4 (LDP and STP)
    #if __ARM_FEATURE_DOTPROD

      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;

      // > From v8.4a onwards, aligned 128-bit ldp and stp instructions are guaranteed to be single-copy atomic
      // https://reviews.llvm.org/D67485
      #undef SATOMI_ATOMIC_ASM

      // the load might pass an earlier store so we need either ldar or dmb ishld for seq_cst
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108891
      #define SATOMI_ATOMIC_ASM(load_order, store_order, pre_fence, post_fence) \
        __asm__ __volatile__                                                    \
        (                                                                       \
          pre_fence                                                             \
          "ld" load_order "p " "%x[out_0], %x[out_1], %[target]\n\t"            \
          post_fence                                                            \
          : [out_0] "=&r" (out.v[0]), [out_1] "=r" (out.v[1])                   \
          : [target] "Q" (*(struct uint128__ *)&target)                         \
          : "memory"                                                            \
        );

      if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "", "", ""); }

      // feature macro to check for ARMv8.9 RCPC3 (LDIAPP and STILP)
      // needs +rcpc3 extension, i.e. -march=armv8.4-a+rcpc3
      // as of gcc 16.1 it just consumes the argument and doesn't define __ARM_FEATURE_RCPC == 3
      #if __ARM_FEATURE_RCPC >= 3
        else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("iap", "", "", ""); }
        else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("iap", "", "ldar %x[out_0], %[target]\n\t", "") }
      #else
        else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("", "", "", "dmb ishld\n\t"); }
        else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("", "", "ldar %x[out_0], %[target]\n\t", "dmb ishld\n\t") }
      #endif
      else { __builtin_trap(); }

      return SATOMI_BIT_CAST(T, out);

    #else

      // WARNING!!!
      // the following implementations NEED a store (casp/stxp)
      // in order to confirm that the load was atomic
      // if the load is from read-only memory, this WILL CRASH the program
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70814#c3

    // builtin CAS support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS

      // utilise casp to load
      // desired value doesn't matter, so we can just pass the same thing
      T ret;
      (void)atomic_compare_exchange_strong(const_cast<volatile T &>(target), ret, ret, order);
      return ret;

    #else

      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } out;

      bool success;

      #undef SATOMI_ATOMIC_ASM
      #define SATOMI_ATOMIC_ASM(load_order)                           \
        __asm__ __volatile__                                          \
        (                                                             \
          "1:\n\t"                                                    \
          "ld" load_order "xp %x[value_0], %x[value_1], %[target]\n\t"\
          "stxp %w[success], %x[value_0], %x[value_1], %[target]\n\t" \
          "cbnz %w[success], 1b\n\t"                                  \
          : [success] "=&r" (success),                                \
            [value_0] "=&r" (out.v[0]), [value_1] "=&r" (out.v[1])    \
          : [target] "Q" (*(struct uint128__ *)&target)               \
          : "memory"                                                  \
        )

      if (order == memory_order_relaxed)
        SATOMI_ATOMIC_ASM("");
      else
        SATOMI_ATOMIC_ASM("a");

      return SATOMI_BIT_CAST(T, out);

    #endif
    #endif
    }

    #undef SATOMI_ATOMIC_ASM
  #endif
  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr void atomic_store(volatile T &target, T value, memory_order order = memory_order_seq_cst) noexcept
  {
    if (order == memory_order_acquire || order == memory_order_consume)
      order = memory_order_release;
    else if (order == memory_order_acq_rel)
      order = memory_order_seq_cst;

    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      const_cast<T &>(target) = value;
      return;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&value);

  #if defined(_MSC_VER) && ! (__clang__)

    #if defined(_M_ARM64) || defined(_M_ARM64EC)
      // the stupid ABI mentioned at the top
      #define SATOMI_SEQ_CST_STORE(iso_suffix, ...) SATOMI_COMPILER_OR_MEMORY_BARRIER(); __iso_volatile_store##iso_suffix(memory, v); SATOMI_COMPILER_OR_MEMORY_BARRIER();
    #else
      #define SATOMI_SEQ_CST_STORE(iso_suffix, interlocked_suffix, ...) (void)_InterlockedExchange##interlocked_suffix(__VA_ARGS__ memory, v);
    #endif

    #define SATOMI_DEFINE_STORE_MEMORY_ORDERS(iso_suffix, interlocked_suffix, ...)\
      auto memory = (volatile __int##iso_suffix *)&target;                        \
      auto v = SATOMI_BIT_CAST(__int##iso_suffix, value);                         \
      if (order == memory_order_relaxed)                                          \
        __iso_volatile_store##iso_suffix(memory, v);                              \
      else if (order == memory_order_release)                                     \
      {                                                                           \
        SATOMI_COMPILER_OR_MEMORY_BARRIER();                                      \
        __iso_volatile_store##iso_suffix(memory, v);                              \
      }                                                                           \
      else                                                                        \
      {                                                                           \
        SATOMI_SEQ_CST_STORE(iso_suffix, interlocked_suffix, __VA_ARGS__)         \
      }

    if constexpr (sizeof(T) == 1) { SATOMI_DEFINE_STORE_MEMORY_ORDERS(8, 8) }
    else if constexpr (sizeof(T) == 2) { SATOMI_DEFINE_STORE_MEMORY_ORDERS(16, 16) }
    else if constexpr (sizeof(T) == 4) { SATOMI_DEFINE_STORE_MEMORY_ORDERS(32, , (volatile long *)) } // stupid cast for a stupid company
    else if constexpr (sizeof(T) == 8) { SATOMI_DEFINE_STORE_MEMORY_ORDERS(64, 64) }
    else if constexpr (sizeof(T) == 16)
    {
      T result = value;
      while (!atomic_compare_exchange_strong(target, result, value, order)) {}
    }

    #undef SATOMI_DEFINE_STORE_MEMORY_ORDERS
    #undef SATOMI_SEQ_CST_STORE

  #elif defined(__x86_64__)

    #define SATOMI_ATOMIC_ASM(type, affix)    \
      type out = SATOMI_BIT_CAST(type, value);\
      __asm__ __volatile__                    \
      (                                       \
        "mov" affix " %[out], %[target]\n\t"  \
        : [target] "=m" (target)              \
        : [out] "r" (out)                     \
        : "memory"                            \
      );

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q"); }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; } v;

    #if defined(__AVX__)

      // Intel Software Developer Manual Volume 3, Guaranteed Atomic Operations
      // Processors supporting AVX guarantee aligned vector moves to be atomic.

      #if __clang__
        // the manual load from memory inside the asm block is because clang
        // "doesn't know how to handle indirect register inputs yet for constraint 'x'"
        __asm__ __volatile__
        (
          "vmovdqa %[value], %%xmm8\n\t"
          "vmovdqa %%xmm8, %[storage]\n\t"
          : [storage] "=m" (target)
          : [value] "m" (value)
          : "xmm8", "memory"
        );
      #else
        __asm__ __volatile__
        (
            "vmovdqa %[value], %[storage]\n\t"
            : [storage] "=m" (target)
            : [value] "x" (value)
            : "memory"
        );
      #endif

    #else

      v = SATOMI_BIT_CAST(uint128__, value);
      __asm__ __volatile__
      (
        "movq %[target_lo], %%rax\n\t"
        "movq %[target_hi], %%rdx\n\t"
        ".align 16\n\t"
        "1: lock; cmpxchg16b %[target_lo]\n\t"
        "jne 1b\n\t"
        : [target_lo] "=m" (((volatile SATOMI_U64 *)&target)[0]),
          [target_hi] "=m" (((volatile SATOMI_U64 *)&target)[1])
        : "b" (v.v[0]), "c" (v.v[1])
        : "cc", "rax", "rdx", "memory"
      );

    #endif
    }

    #undef SATOMI_ATOMIC_ASM

  #elif defined(__aarch64__)

    #pragma push_macro("SATOMI_CHOOSE_MEMORY_ORDER_ASM")
    #undef SATOMI_CHOOSE_MEMORY_ORDER_ASM
    #define SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, ...)\
      if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "", "", __VA_ARGS__) } \
      else if (order == memory_order_release) { SATOMI_ATOMIC_ASM("", "l", "", __VA_ARGS__) } \
      else if (order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("", "l", SATOMI_MSVC_STL_SEQ_CST_FENCE, __VA_ARGS__) } \
      else { __builtin_trap(); }

    #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier)\
      type v = SATOMI_BIT_CAST(type, value);                                              \
      __asm__ __volatile__                                                                \
      (                                                                                   \
        "st" store_order "r" suffix " " modifier "[value], %[target]\n\t"                 \
        msvc_fence                                                                        \
        : [target] "+Q" (target)                                                          \
        : [value] "r" (v)                                                                 \
        : "memory"                                                                        \
      );

    if constexpr (sizeof(T) == 1) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT8_TYPE__, "b", "%w"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT16_TYPE__, "h", "%w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT32_TYPE__, "", "%w"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT64_TYPE__, "", "%x"); }

    #undef SATOMI_ATOMIC_ASM
    #pragma pop_macro("SATOMI_CHOOSE_MEMORY_ORDER_ASM")

    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) uint128__ { SATOMI_U64 v[2]; };
      auto v = SATOMI_BIT_CAST(uint128__, value);
      (void)v;

    // checking for ARMv8.4 (LDP and STP)
    #if __ARM_FEATURE_DOTPROD

      #define SATOMI_ATOMIC_ASM_V8_4(store_order, pre_fence, post_fence)\
        __asm__ __volatile__                                            \
        (                                                               \
          pre_fence                                                     \
          "st" store_order "p %x[value_0], %x[value_1], %[target]\n\t"  \
          post_fence                                                    \
          : [target] "+Q" (target)                                      \
          : [value_0] "r" (v.v[0]), [value_1] "r" (v.v[1])              \
          : "memory"                                                    \
        )

      #define SATOMI_ATOMIC_ASM_LSE128(load_order, store_order)               \
        __asm__ __volatile__                                                  \
        (                                                                     \
          "swpp" load_order store_order " %x[out_0], %x[out_1], %[target]\n\t"\
          : [target] "+Q" (target),                                           \
            [out_0] "+&r" (v.v[0]), [out_1] "+&r" (v.v[1])                    \
          :                                                                   \
          : "memory"                                                          \
        )

      if (order == memory_order_relaxed)
      {
        SATOMI_ATOMIC_ASM_V8_4("", "", "");
      }
      else if (order == memory_order_release)
      {
      // feature macro to check for ARMv8.9 RCPC3 (LDIAPP and STILP)
      // needs +rcpc3 extension, i.e. -march=armv8.4-a+rcpc3
      // as of gcc 16.1 it just consumes the argument and doesn't define __ARM_FEATURE_RCPC == 3
      #if __ARM_FEATURE_RCPC >= 3

        // use stilp, doesn't require fences
        SATOMI_ATOMIC_ASM_V8_4("il", "", "");

      #elif defined(SATOMI_ARM_USE_LSE128)

        // use swpp if stp would require a fence
        // https://reviews.llvm.org/D143506
        SATOMI_ATOMIC_ASM_LSE128("", "l");

      #else

        // > From v8.4a onwards, aligned 128-bit ldp and stp instructions are guaranteed to be single-copy atomic
        // https://reviews.llvm.org/D67485
        // use dmb ish + stp
        SATOMI_ATOMIC_ASM_V8_4("", "dmb ish\n\t", "");

      #endif

      }
      else if (order == memory_order_seq_cst)
      {
      #if defined(SATOMI_ARM_USE_LSE128)

        // use swpp if stp would require a fence
        // https://reviews.llvm.org/D143506
        SATOMI_ATOMIC_ASM_LSE128("a", "l");

      #elif __ARM_FEATURE_RCPC >= 3

        // use dmb ish + stilp
        // for more info:
        // https://github.com/taiki-e/atomic-maybe-uninit/blob/4059f083af2c9413a0beb70e92dd434db05c2e19/src/arch/aarch64.rs#L527
        SATOMI_ATOMIC_ASM_V8_4("il", "dmb ish\n\t", "");

      #else

        // use dmb ish + stp + dmb ish
        // according to llvm codegen (clang 22.1.0)
        SATOMI_ATOMIC_ASM_V8_4("", "dmb ish\n\t", "dmb ish\n\t");

      #endif
      }

      #undef SATOMI_ATOMIC_ASM_V8_4
      #undef SATOMI_ATOMIC_ASM_LSE128

    // builtin CAS support with ARM LSE 1
    #elif __ARM_FEATURE_ATOMICS

      // use casp
      T out = value;
      while (!atomic_compare_exchange_strong(target, out, value, order)) { }

    #else
      // if we don't have casp atomics use a cas loop
      // not redirecting this to atomic_compare_exchange_strong
      // because it's more effient to rewrite the algo

      uint128__ out;
      bool success;

      #define SATOMI_ATOMIC_ASM_LOOP(store_order)                                   \
        __asm__ __volatile__                                                        \
        (                                                                           \
          "1:\n\t"                                                                  \
          "ldxp %x[out_0], %x[out_1], %[target]\n\t"                                \
          "st" store_order "xp %w[success], %x[value_0], %x[value_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                \
          : [success] "=&r" (success), [target] "+Q" (*(struct uint128__ *)&target),\
            [out_0] "=&r" (out.v[0u]), [out_1] "=&r" (out.v[1u])                    \
          : [value_0] "r" (v.v[0u]), [value_1] "r" (v.v[1u])                        \
          : "memory"                                                                \
        )

      if (order == memory_order_relaxed)
        SATOMI_ATOMIC_ASM_LOOP("");
      else
        SATOMI_ATOMIC_ASM_LOOP("l");

      #undef SATOMI_ATOMIC_ASM_LOOP
    #endif
    }

  #endif
  }


  // only available for integral/pointer types and the operand is either the same type or ptrdiff_t
  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_add(volatile T &target, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = target;
      const_cast<T &>(target) += operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, operand))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedExchangeAdd,
      return SATOMI_BIT_CAST(T, out);
    )
    #undef SATOMI_HELPER

    else if constexpr (sizeof(T) == 16)
    {
      T ret = atomic_load(target, memory_order_relaxed);
      while (!atomic_compare_exchange_strong(target, ret, ret + operand, order)) {}
    }

  #else

    #if defined(__x86_64__)

      #define SATOMI_ATOMIC_ASM(type, affix)                    \
        type o = SATOMI_BIT_CAST(type, operand);                \
        __asm__ __volatile__                                    \
        (                                                       \
          "lock; xadd" affix " %[operand], %[target]\n\t"       \
          : [target] "+m" (*(type *)&target), [operand] "+r" (o)\
          :                                                     \
          : "memory"                                            \
        );                                                      \
        return SATOMI_BIT_CAST(T, o);

      if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b"); }
      else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w"); }
      else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l"); }
      else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q"); }

      #undef SATOMI_ATOMIC_ASM

    #elif defined(__aarch64__)

        // builtin CAS support with ARM LSE 1
      #ifdef __ARM_FEATURE_ATOMICS

        #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier)                    \
          type original;                                                                                          \
          auto o = SATOMI_BIT_CAST(type, operand);                                                                \
          __asm__ __volatile__                                                                                    \
          (                                                                                                       \
            "ldadd" load_order store_order suffix " " modifier "[operand], " modifier "[original], %[target]\n\t" \
            : [target] "+Q" (*(type *)&target), [original] "=&r" (original)                                       \
            : [operand] "r" (o)                                                                                   \
            : "memory"                                                                                            \
          );                                                                                                      \
          return SATOMI_BIT_CAST(T, original);

      #else

        bool success;
        #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier)\
          type temp, original;                                                                \
          auto o = SATOMI_BIT_CAST(type, operand);                                            \
          __asm__ __volatile__                                                                \
          (                                                                                   \
            "1:\n\t"                                                                          \
            "ld" load_order "xr" suffix " " modifier "[original], %[target]\n\t"              \
            "add " modifier "[temp], " modifier "[original], " modifier "[operand]\n\t"       \
            "st" store_order "xr" suffix " %w[success], " modifier "[temp], %[target]\n\t"    \
            "cbnz %w[success], 1b\n\t"                                                        \
            msvc_fence                                                                        \
            : [target] "+Q" (*(type *)&target), [success] "=&r" (success),                    \
              [temp] "=&r" (temp), [original] "=&r" (original)                                \
            : [operand] "r" (o)                                                               \
            : "memory"                                                                        \
          );                                                                                  \
          return SATOMI_BIT_CAST(T, original);

      #endif

      if constexpr (sizeof(T) == 1) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT8_TYPE__, "b", "%w"); }
      else if constexpr (sizeof(T) == 2) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT16_TYPE__, "h", "%w"); }
      else if constexpr (sizeof(T) == 4) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT32_TYPE__, "", "%w"); }
      else if constexpr (sizeof(T) == 8) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT64_TYPE__, "", "%x"); }

      #undef SATOMI_ATOMIC_ASM

    #endif

    else if constexpr (sizeof(T) == 16)
    {
      T intermediate = operand;
      // relaxed because we ONLY care about the value
      T ret = atomic_load(target, memory_order_relaxed);

      // NOTE: this generates slightly subpar code on armv8-a (>= armv8.1-a with casp is fine though)
      // because it can't interleave the addition inside the cas loop but it isn't too bad
      do
      {
        intermediate = ret + operand;
      } while (!atomic_compare_exchange_strong(target, ret, intermediate, order));

      return ret;
    }
  #endif
  }

  // only available for pointer types
  template<typename T> requires SATOMI_IS_POINTER(T)
  SATOMI_INLINE constexpr T atomic_fetch_add(volatile T &target, detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = target;
      const_cast<T &>(target) += operand;
      return current;
    }

    return (T)atomic_fetch_add(*(SATOMI_U64 *)&target, (SATOMI_U64)operand, order);
  }

  #if defined(_MSC_VER) && ! (__clang__)
    #pragma warning(push)
    #pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
  #endif

  // only available for integral types
  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_sub(volatile T &target, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    return atomic_fetch_add(target, (T)(-operand), order);
  }

  // only available for pointer types
  template<typename T> requires SATOMI_IS_POINTER(T)
  SATOMI_INLINE constexpr T atomic_fetch_sub(volatile T &target, detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept
  {
    return atomic_fetch_sub(target, -operand, order);
  }


  #if defined(__x86_64__)

    #define SATOMI_ATOMIC_ASM(type, affix, a_register)          \
      type ret = 0, temp;                                       \
      auto o = SATOMI_BIT_CAST(type, operand);                  \
      __asm__ __volatile__                                      \
      (                                                         \
        "mov" affix " %[target], " a_register "\n\t"            \
        "1: mov" affix " %[operand], %[temp]\n\t"               \
        SATOMI_ATOMIC_OP affix " " a_register ", %[temp]\n\t"   \
        "lock; cmpxchg" affix " %[temp], %[target]\n\t"         \
        "jne 1b\n\t"                                            \
        : [target] "+m" (*(type *)&target), [temp] "=&r" (temp),\
          [original] "+&a" (ret)                                \
        : [operand] "r" (o)                                     \
        : "cc", "memory"                                        \
      );                                                        \
      return SATOMI_BIT_CAST(T, ret);

  #elif defined(__aarch64__)

    #ifdef __ARM_FEATURE_ATOMICS

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, ...)                       \
        type ret;                                                                                                       \
        auto o = SATOMI_BIT_CAST(type, operand);                                                                        \
        __VA_ARGS__                                                                                                     \
        __asm__ __volatile__                                                                                            \
        (                                                                                                               \
          SATOMI_ATOMIC_OP load_order store_order suffix " " modifier "[operand], " modifier "[original], %[target]\n\t"\
          : [target] "+Q" (*(type *)&target), [original] "=&r" (ret)                                                     \
          : [operand] "r" (o)                                                                                           \
          : "memory"                                                                                                    \
        );                                                                                                              \
        return SATOMI_BIT_CAST(T, ret);

    #else

      #define SATOMI_ATOMIC_ASM(load_order, store_order, msvc_fence, type, suffix, modifier, ...)   \
        bool success;                                                                               \
        type ret, temp;                                                                             \
        auto o = SATOMI_BIT_CAST(type, operand);                                                    \
        __VA_ARGS__                                                                                 \
        __asm__ __volatile__                                                                        \
        (                                                                                           \
          "1:\n\t"                                                                                  \
          "ld" load_order "xr" suffix " " modifier "[original], %[target]\n\t"                      \
          SATOMI_ATOMIC_OP " " modifier "[temp], " modifier "[original], " modifier "[operand]\n\t" \
          "st" store_order "xr" suffix " %w[success], " modifier "[temp], %[target]\n\t"            \
          "cbnz %w[success], 1b\n\t"                                                                \
          msvc_fence                                                                                \
          : [target] "+Q" (*(type *)&target), [success] "=&r" (success),                            \
            [temp] "=&r" (temp), [original] "=&r" (ret)                                             \
          : [operand] "r" (o)                                                                       \
          : "memory"                                                                                \
        );                                                                                          \
      return SATOMI_BIT_CAST(T, ret);

    #endif

  #endif

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_and(volatile T &target, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = target;
      const_cast<T &>(target) &= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, operand))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedAnd,
      return SATOMI_BIT_CAST(T, out);
    )
    #undef SATOMI_HELPER

  #elif defined(__x86_64__)

    #define SATOMI_ATOMIC_OP "and"

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b", "%%al"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w", "%%ax"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l", "%%eax"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q", "%%rax"); }

    #undef SATOMI_ATOMIC_OP

  #elif defined(__aarch64__)

    // builtin CAS support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS
      #define SATOMI_ATOMIC_OP "ldclr"
      #define SATOMI_ATOMIC_EXTRA o = ~o;
    #else
      #define SATOMI_ATOMIC_OP "and"
      #define SATOMI_ATOMIC_EXTRA
    #endif

    if constexpr (sizeof(T) == 1) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT8_TYPE__, "b", "%w", SATOMI_ATOMIC_EXTRA); }
    else if constexpr (sizeof(T) == 2) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT16_TYPE__, "h", "%w", SATOMI_ATOMIC_EXTRA); }
    else if constexpr (sizeof(T) == 4) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT32_TYPE__, "", "%w", SATOMI_ATOMIC_EXTRA); }
    else if constexpr (sizeof(T) == 8) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT64_TYPE__, "", "%x", SATOMI_ATOMIC_EXTRA); }

    #undef SATOMI_ATOMIC_OP
    #undef SATOMI_ATOMIC_EXTRA

  #endif

    else if constexpr (sizeof(T) == 16)
    {
      // relaxed because we ONLY care about the value
      T current = atomic_load(target, memory_order_relaxed);
      while (!atomic_compare_exchange_strong(target, current, current & operand, order)) {}
      return current;
    }
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_or(volatile T &target, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = target;
      const_cast<T &>(target) |= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, operand))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedOr,
      return SATOMI_BIT_CAST(T, out);
    )
    #undef SATOMI_HELPER

  #elif defined(__x86_64__)

    #define SATOMI_ATOMIC_OP "or"

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b", "%%al"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w", "%%ax"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l", "%%eax"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q", "%%rax"); }

    #undef SATOMI_ATOMIC_OP

  #elif defined(__aarch64__)

    // builtin CAS support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS
      #define SATOMI_ATOMIC_OP "ldset"
    #else
      #define SATOMI_ATOMIC_OP "orr"
    #endif

    if constexpr (sizeof(T) == 1) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT8_TYPE__, "b", "%w"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT16_TYPE__, "h", "%w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT32_TYPE__, "", "%w"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT64_TYPE__, "", "%x"); }

    #undef SATOMI_ATOMIC_OP

  #endif

    else if constexpr (sizeof(T) == 16)
    {
      // relaxed because we ONLY care about the value
      T ret = atomic_load(target, memory_order_relaxed);
      while (!atomic_compare_exchange_strong(target, ret, ret | operand, order)) {}
      return ret;
    }

  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_xor(volatile T &target, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = target;
      const_cast<T &>(target) ^= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), target);

  #if defined(_MSC_VER) && ! (__clang__)

    #define SATOMI_HELPER(cast) (volatile cast *)&target, (SATOMI_BIT_CAST(cast, operand))
    SATOMI_CHOOSE_SIZE(SATOMI_HELPER, sizeof(T), _InterlockedXor,
      return SATOMI_BIT_CAST(T, out);
    )
    #undef SATOMI_HELPER

  #elif defined(__x86_64__)

    #define SATOMI_ATOMIC_OP "xor"

    if constexpr (sizeof(T) == 1) { SATOMI_ATOMIC_ASM(__UINT8_TYPE__, "b", "%%al"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_ATOMIC_ASM(__UINT16_TYPE__, "w", "%%ax"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_ATOMIC_ASM(__UINT32_TYPE__, "l", "%%eax"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_ATOMIC_ASM(__UINT64_TYPE__, "q", "%%rax"); }

    #undef SATOMI_ATOMIC_OP

  #elif defined(__aarch64__)

    // builtin CAS support with ARM LSE 1
    #ifdef __ARM_FEATURE_ATOMICS
      #define SATOMI_ATOMIC_OP "ldeor"
    #else
      #define SATOMI_ATOMIC_OP "eor"
    #endif

    if constexpr (sizeof(T) == 1) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT8_TYPE__, "b", "%w"); }
    else if constexpr (sizeof(T) == 2) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT16_TYPE__, "h", "%w"); }
    else if constexpr (sizeof(T) == 4) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT32_TYPE__, "", "%w"); }
    else if constexpr (sizeof(T) == 8) { SATOMI_CHOOSE_MEMORY_ORDER_ASM(order, __UINT64_TYPE__, "", "%x"); }

    #undef SATOMI_ATOMIC_OP

  #endif

    else if constexpr (sizeof(T) == 16)
    {
      // relaxed because we ONLY care about the value
      T ret = atomic_load(target, memory_order_relaxed);
      while (!atomic_compare_exchange_strong(target, ret, ret ^ operand, order)) {}
      return ret;
    }
  }

  #if (defined (LINUX) || defined (__linux__)) && defined(__x86_64__)

    #define SATOMI_SYS_FUTEX 202
    #define SATOMI_WAKE_SYSCALL(address, waiters_to_wake_up)         \
      __asm__ __volatile__                                           \
      (                                                              \
        "mov %[syscall_number], %%rax\n\t"                           \
        "mov %[a], %%rdi\n\t"                                        \
        "mov %[futex_op], %%rsi\n\t"                                 \
        "mov %[count], %%edx\n\t"                                    \
        "syscall\n\t"                                                \
        :                                                            \
        : [syscall_number] "Z" (SATOMI_SYS_FUTEX), [a] "r" (address),\
          [futex_op] "Z" (1 /*wake op*/ | 128 /*private flag*/),     \
          [count] "r" (waiters_to_wake_up)                           \
        : "rax", "rdi", "rsi", "rdx", "r10"                          \
      );

  #elif (defined (LINUX) || defined (__linux__)) && defined(__aarch64__)

    #define SATOMI_SYS_FUTEX 98
    #define SATOMI_WAKE_SYSCALL(address, waiters_to_wake_up)         \
      __asm__ __volatile__                                           \
      (                                                              \
        "mov w8, %x[syscall_number]\n\t"                             \
        "mov x0, %x[a]\n\t"                                          \
        "mov x1, %x[futex_op]\n\t"                                   \
        "mov w2, %w[count]\n\t"                                      \
        "sxtw x2, w2\n\t"                                            \
        "svc #0\n\t"                                                 \
        :                                                            \
        : [syscall_number] "M" (SATOMI_SYS_FUTEX), [a] "r" (address),\
          [futex_op] "N" (1 /*wake op*/ | 128 /*private flag*/),     \
          [count] "r" (waiters_to_wake_up)                           \
        : "w8", "x0", "x1", "x2"                                     \
      );

  #endif

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_wait(volatile T &object, T old, memory_order order = memory_order_seq_cst) noexcept
  {
    if (order == memory_order_release)
      order = memory_order_acquire;
    else if (order == memory_order_acq_rel)
      order = memory_order_seq_cst;

    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = const_cast<T &>(object);
      while (current == old) { current = const_cast<T &>(object); }
      return current;
    }

    static_assert(sizeof(T) <= 16, "We can't handle object larger than 16 bytes");

  #if defined (_WIN64)

    if constexpr (sizeof(T) <= 8)
    {
      auto value = atomic_load(object, order);
      while (value == old)
      {
        WaitOnAddress(&object, &old, sizeof(T), 0xFFFF'FFFF /*No timeout*/);
        value = atomic_load(object, order);
      }
      return value;
    }
    else if constexpr (sizeof(T) == 16)
    {
      auto value = atomic_load(object, order);
      while (value == old)
      {
        WaitOnAddress(&object, &value, 8, 0xFFFF'FFFF /*No timeout*/);
        value = atomic_load(object, order);
      }
      return value;
    }

  #else

    // assumes linux has futexes, kernel versions must be >= 2.6.22

    // assumes macOS has __ulock_wait and __ulock_wake, >= Darwin 16 (macOS 10.12)

    T current = atomic_load(object, order);

    auto short_spin = [&]()
    {
      constexpr auto spin_count = 16;
      for (int i = 0; i < spin_count; ++i)
      {
        current = atomic_load(object, order);
        if (current != old)
          return true;

      #if defined(__x86_64__)
        __asm__ __volatile__("pause");
      #elif defined(__aarch64__)
        __asm__ __volatile__("yield");
      #endif
      }

      return false;
    };

    auto &slot = detail::get_waiting_slot(&object);
    (void)__atomic_fetch_add(&slot.wait_count, 1, __ATOMIC_SEQ_CST);

    int *address = nullptr;
    int compare;

    if (sizeof(T) >= sizeof(detail::waiting_slot::version) &&
      (((__UINTPTR_TYPE__)&object) % sizeof(int)) == 0)
    {
      address = (int *)&object;
      __builtin_memcpy(&compare, &old, sizeof(compare));
    }
    else
    {
      address = &slot.version;
      compare = __atomic_load_n(&slot.version, __ATOMIC_RELAXED);
    }

    while(current == old)
    {
      if (short_spin())
        break;

    #if defined (LINUX) || defined (__linux__)
      __INT64_TYPE__ result;
      #define SATOMI_WAIT_OP (0 /*wait op*/ | 128 /*private flag*/)

    #if defined(__x86_64__)

      __asm__ __volatile__
      (
        "mov %[syscall_number], %%rax\n\t"
        "mov %[address], %%rdi\n\t"
        "mov %[futex_op], %%rsi\n\t"
        "mov %[compare], %%edx\n\t"
        "mov %[timeout], %%r10\n\t"
        "syscall\n\t"
        "mov %%rax, %[result]"
        : [result] "=r" (result)
        : [syscall_number] "Z" (SATOMI_SYS_FUTEX), [address] "r" (address),
          [futex_op] "Z" (SATOMI_WAIT_OP), [compare] "r" (compare), [timeout] "Z" (nullptr)
        : "rax", "rdi", "rsi", "rdx", "r10"
      );

    #elif defined(__aarch64__)

      __asm__ __volatile__
      (
        "mov w8, %x[syscall_number]\n\t"
        "mov x0, %x[address]\n\t"
        "mov x1, %x[futex_op]\n\t"
        "mov w2, %w[compare]\n\t"
        "sxtw x2, w2\n\t"
        "mov x3, %x[timeout]\n\t"
        "svc #0\n\t"
        "mov %x[result], x0"
        : [result] "=r" (result)
        : [syscall_number] "M" (SATOMI_SYS_FUTEX), [address] "r" (address),
          [futex_op] "N" (SATOMI_WAIT_OP), [compare] "r" (compare), [timeout] "N" (nullptr)
        : "w8", "x0", "x1", "x2", "x3"
      );

    #endif

      #undef SATOMI_WAIT_OP

      if (!result && (-result) != 11 /*EAGAIN*/)
        __builtin_trap();

    #elif defined (__APPLE__)

      __ulock_wait(SATOMI_UL_COMPARE_AND_WAIT, address, (__UINT64_TYPE__)compare, 0);

    #endif

      current = atomic_load(object, order);
    }

    (void)__atomic_fetch_sub(&slot.wait_count, 1, __ATOMIC_RELEASE);

    return current;
  #endif
  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr void atomic_notify_one(volatile T &object) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
      return;

  #if defined (_WIN64)

    WakeByAddressSingle(const_cast<T *>(&object));

  #else

    // assumes linux has futexes, kernel versions must be >= 2.6.22

    // assumes macOS has __ulock_wait and __ulock_wake, >= Darwin 16 (macOS 10.12)

    auto &slot = detail::get_waiting_slot(&object);
    bool is_anyone_waiting = __atomic_load_n(&slot.wait_count, __ATOMIC_RELAXED) != 0;
    if (!is_anyone_waiting)
      return;

    int *address = nullptr;
    int waiters_to_wake_up = 1;

    if (sizeof(T) >= sizeof(detail::waiting_slot::version) &&
      (((__UINTPTR_TYPE__)&object) % sizeof(int)) == 0)
    {
      address = (int *)&object;
    }
    else
    {
      (void)__atomic_fetch_add(&slot.version, 1, __ATOMIC_SEQ_CST);
      // waking up everyone because a different atomic might have the same hash
      // so we can't guarantee waking up threads only on OUR atomic with notify_one
      waiters_to_wake_up = __INT_MAX__;
      address = &slot.version;
    }

    #if defined (LINUX) || defined (__linux__)

      SATOMI_WAKE_SYSCALL(address, waiters_to_wake_up)

    #elif defined (__APPLE__)

      int extra = waiters_to_wake_up == __INT_MAX__ ? SATOMI_ULF_WAKE_ALL : 0;
      __ulock_wake(SATOMI_UL_COMPARE_AND_WAIT | extra, address, 0);

    #endif

  #endif
  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr void atomic_notify_all(volatile T &object) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
      return;

  #if defined (_WIN64)

    WakeByAddressAll(const_cast<T *>(&object));

  #else

    // assumes linux has futexes, kernel versions must be >= 2.6.22

    // assumes macOS has __ulock_wait and __ulock_wake, >= Darwin 16 (macOS 10.12)

    auto &slot = detail::get_waiting_slot(&object);
    bool is_anyone_waiting = __atomic_load_n(&slot.wait_count, __ATOMIC_RELAXED) != 0;
    if (!is_anyone_waiting)
        return;

    int *address = nullptr;

    if (sizeof(T) >= sizeof(detail::waiting_slot::version) &&
      (((__UINTPTR_TYPE__)&object) % sizeof(int)) == 0)
    {
      address = (int *)&object;
    }
    else
    {
      (void)__atomic_fetch_add(&slot.version, 1, __ATOMIC_SEQ_CST);
      address = &slot.version;
    }

    #if defined (LINUX) || defined (__linux__)

      int waiters_to_wake_up = __INT_MAX__;

      SATOMI_WAKE_SYSCALL(address, waiters_to_wake_up)

      #undef SATOMI_WAKE_SYSCALL
      #undef SATOMI_SYS_FUTEX

    #elif defined (__APPLE__)

      __ulock_wake(SATOMI_UL_COMPARE_AND_WAIT | SATOMI_ULF_WAKE_ALL, address, 0);

      #undef SATOMI_UL_COMPARE_AND_WAIT
      #undef SATOMI_ULF_WAKE_ALL

    #endif

  #endif
  }

  #define SATOMI_CONDITIONAL(Test, True, False) decltype([]([[maybe_unused]] True *t___, \
    [[maybe_unused]] False *f___) { if constexpr (Test) return *t___; else return *f___; }(nullptr, nullptr))

  template<typename U>
  class atomic
  {
    using T = typename detail::remove_reference<U>::type;
    static constexpr bool owns_data = !detail::remove_reference<U>::is_ref;
    struct data_holder
    {
      alignas(sizeof(T)) T object{};
      T &operator*() { return object; }
      const T &operator*() const { return object; }
    };
    using storage_type = SATOMI_CONDITIONAL(owns_data, data_holder, T *);

    static_assert(__is_trivially_copyable(T), "Type must be trivially copyable");
    static_assert(!SATOMI_IS_SAME(T, const T) && !SATOMI_IS_SAME(T, volatile T), "Type must NOT be const or volatile");
    static_assert(requires(const T &v) { T(v); }, "Type must be copy-constructible");
    static_assert(requires(const T &v, T u) { u = v; }, "Type must be copy-assignable");
    static_assert(requires(T &&v) { T(v); }, "Type must be move-constructible");
    static_assert(requires(T &&v, T u) { u = v; }, "Type must be move-assignable");
    static_assert((sizeof(T) & (sizeof(T) - 1)) == 0, "Type must be a power-of-2");

  #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
  #endif

    static constexpr bool is_integral = detail::type_list<bool, char, signed char, unsigned char, wchar_t,
    #ifdef __cpp_char8_t
      char8_t,
    #endif
    #ifdef __SIZEOF_INT128__
      __int128, unsigned __int128,
    #endif
      char16_t, char32_t, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long>::any_of<T>();
    static constexpr bool is_pointer = SATOMI_IS_POINTER(T);
    static constexpr bool is_floating_point = detail::type_list<float, double, long double>::any_of<T>();

  #ifdef __GNUC__
    #pragma GCC diagnostic pop
  #endif

  public:
    static constexpr bool is_always_lock_free = sizeof(T) <= 16;

    constexpr atomic() noexcept = default;
    constexpr atomic(T value) noexcept requires owns_data : object{ value }
    {
      if constexpr (SATOMI_HAS_PADDING_BITS(T))
        SATOMI_CLEAR_PADDING_BITS(&object);
    }
    explicit constexpr atomic(T &reference) noexcept requires (!owns_data) { object = &reference; }

    constexpr atomic(const atomic &) noexcept requires (!owns_data) = default;
    constexpr atomic(atomic &&) noexcept requires (!owns_data) = default;
    constexpr atomic &operator=(const atomic &) noexcept requires (!owns_data) = default;
    constexpr atomic &operator=(atomic &&) noexcept requires (!owns_data) = default;

    constexpr T load(memory_order order = memory_order_seq_cst) const noexcept
    { return atomic_load(*object, order); }

    constexpr void store(T desired, memory_order order = memory_order_seq_cst) noexcept
    { atomic_store(*object, desired, order); }

    constexpr T exchange(T desired, memory_order order = memory_order_seq_cst) noexcept
    { return atomic_exchange(*object, desired, order); }

    constexpr bool compare_exchange_strong(T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
    { return atomic_compare_exchange_strong(*object, expected, desired, order); }

    constexpr bool compare_exchange_weak(T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
    { return atomic_compare_exchange_weak(*object, expected, desired, order); }

    constexpr T fetch_add(T operand, memory_order order = memory_order_seq_cst) noexcept requires is_integral || is_floating_point
    {
      if constexpr (is_integral)
        return atomic_fetch_add(*object, operand, order);
      else
      {
        auto current = load(order);
        while (!compare_exchange_weak(current, current + operand, order)) {}
        return current;
      }
    }

    constexpr T fetch_add(detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept requires is_pointer
    { return atomic_fetch_add(*object, operand, order); }

    constexpr T fetch_sub(T operand, memory_order order = memory_order_seq_cst) noexcept requires is_integral || is_floating_point
    { return atomic_fetch_add(*object, (T)(-operand), order); }

    constexpr T fetch_sub(detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept requires is_pointer
    { return fetch_add(-operand, order); }

    constexpr T fetch_and(T operand, memory_order order = memory_order_seq_cst) noexcept requires is_integral
    { return atomic_fetch_and(*object, operand, order); }

    constexpr T fetch_or(T operand, memory_order order = memory_order_seq_cst) noexcept requires is_integral
    { return atomic_fetch_or(*object, operand, order); }

    constexpr T fetch_xor(T operand, memory_order order = memory_order_seq_cst) noexcept requires is_integral
    { return atomic_fetch_xor(*object, operand, order); }


    constexpr T wait(T old, memory_order order = memory_order_seq_cst) noexcept
    { return atomic_wait(*object, old, order); }

    constexpr void notify_one() noexcept { atomic_notify_one(*object); }
    constexpr void notify_all() noexcept { atomic_notify_all(*object); }

    static constexpr bool is_lock_free() noexcept { return is_always_lock_free; }

    constexpr T *address() const noexcept requires (!owns_data) { return object; }

  private:
    storage_type object{};
  };

  template<typename T>
  atomic(T) -> atomic<T>;

  template<typename T>
  using atomic_ref = atomic<T &>;
}

#if defined(_MSC_VER) && ! (__clang__)
  #pragma warning(pop)
#endif

#undef SATOMI_INLINE
#undef SATOMI_IS_POINTER
#undef SATOMI_CONDITIONAL
#undef SATOMI_IS_SAME
#undef SATOMI_IS_CONSTANT_EVALUATED
#undef SATOMI_BIT_CAST
#undef SATOMI_IS_ATOMIC_READY
#undef SATOMI_CHOOSE_MEMORY_ORDER
#undef SATOMI_CHOOSE_MEMORY_ORDER_ASM
#undef SATOMI_CHECK_ALIGNMENT
#undef SATOMI_CHOOSE_SIZE
#undef SATOMI_MEMORY_LOAD_ACQUIRE_BARRIER
#undef SATOMI_COMPILER_OR_MEMORY_BARRIER
#undef SATOMI_COMPILER_BARRIER
#undef SATOMI_HAS_PADDING_BITS
#undef SATOMI_CLEAR_PADDING_BITS
