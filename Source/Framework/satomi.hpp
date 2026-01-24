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

#ifndef SATOMI_HPP
#define SATOMI_HPP

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

  #if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)

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
    #define SATOMI_COMPILER_OR_MEMORY_BARRIER() __dmb(0xB)
    #define SATOMI_MEMORY_LOAD_ACQUIRE_BARRIER() __dmb(0x9)

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

  static_assert(sizeof(__UINT64_TYPE__) == sizeof(void *));

  // currently clang doesn't implement either functions
  // but when/if it decides to it can choose
  #if __has_builtin(__builtin_clear_padding)
    #define SATOMI_CLEAR_PADDING_BITS(x) __builtin_clear_padding(x)
  #elif __has_builtin(__builtin_clear_padding)
    #define SATOMI_CLEAR_PADDING_BITS(x) __builtin_zero_non_value_bits(x)
  #else
    #define SATOMI_CLEAR_PADDING_BITS(x) (void)(x)
  #endif

  #define SATOMI_INLINE inline __attribute__((always_inline))
  #define SATOMI_TRAP() __builtin_trap()
  #define SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)\
    if (order == memory_order_relaxed) { SATOMI_ATOMIC_ASM("", "") } \
    else if (order == memory_order_consume || order == memory_order_acquire) { SATOMI_ATOMIC_ASM("a", "") } \
    else if (order == memory_order_release) { SATOMI_ATOMIC_ASM("", "l") } \
    else if (order == memory_order_acq_rel || order == memory_order_seq_cst) { SATOMI_ATOMIC_ASM("a", "l") } \
    else { __builtin_trap(); }
  #define SATOMI_CHOOSE_SIZE(T, macro)                            \
    if constexpr (sizeof(T) == 1) { macro(__UINT8_TYPE__); }      \
    else if constexpr (sizeof(T) == 2) { macro(__UINT16_TYPE__); }\
    else if constexpr (sizeof(T) == 4) { macro(__UINT32_TYPE__); }\
    else if constexpr (sizeof(T) == 8) { macro(__UINT64_TYPE__); }

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

    inline waiting_slot &get_waiting_slot(const volatile void *address)
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

  #if defined(_MSC_VER) && ! (__clang__)
    SATOMI_COMPILER_BARRIER();
    #if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
      if (order == memory_order_acquire || order == memory_order_consume)
        SATOMI_MEMORY_LOAD_ACQUIRE_BARRIER();
      else
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
    #else
      if (order == memory_order_seq_cst)
      {
      #pragma warning(push)
      #pragma warning(disable : 6001)  // "Using uninitialized memory 'guard'"
      #pragma warning(disable : 28113) // "Accessing a local variable guard via an Interlocked function: This is an unusual
                                       // usage which could be reconsidered."
        volatile long guard;
        (void)_InterlockedIncrement(&guard);
        SATOMI_COMPILER_BARRIER();
      #pragma warning(pop)
      }
    #endif
  #else
    #if defined (__x86_64__)
      if (order == memory_order_seq_cst)
      {
        unsigned char dummy = 0u;
        __asm__ __volatile__ ("lock; notb %0" : "+m" (dummy) : : "memory");
      }
      else if (order != memory_order_relaxed)
        __asm__ __volatile__ ("" ::: "memory");
    #else
      if (order != memory_order_relaxed)
      {
        if (order == memory_order_consume || order == memory_order_acquire)
          __asm__ __volatile__ ("dmb ishld\n\t" ::: "memory");
        else
          __asm__ __volatile__ ("dmb ish\n\t" ::: "memory");
      }
    #endif
  #endif
  }

  SATOMI_INLINE constexpr void atomic_signal_fence(memory_order order = memory_order_seq_cst)
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
      return;
  #if defined(_MSC_VER) && ! (__clang__)
    if (order != memory_order_relaxed)
      SATOMI_COMPILER_BARRIER();
  #else
    if (order != memory_order_relaxed)
      __asm__ __volatile__ ("" ::: "memory");
  #endif
  }

  template<typename T>
  SATOMI_INLINE constexpr T kill_dependency(T t) noexcept { return t; }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr bool atomic_compare_exchange_strong(volatile T &object, T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      if (const_cast<T &>(object) == expected)
      {
        const_cast<T &>(object) = desired;
        return true;
      }
      expected = const_cast<T &>(object);
      return false;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&desired);

  #if defined(_MSC_VER) && ! (__clang__)

    (void)order;
    if constexpr (sizeof(T) == 1)
    {
      __int8 ret = 0, d = SATOMI_BIT_CAST(__int8, desired), e = SATOMI_BIT_CAST(__int8, expected);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedCompareExchange8, ((volatile __int8 *)&object, d, e))
      if (ret == e) return true;
      expected = SATOMI_BIT_CAST(T, ret);
      return false;
    }
    else if constexpr (sizeof(T) == 2)
    {
      __int16 ret = 0, d = SATOMI_BIT_CAST(__int16, desired), e = SATOMI_BIT_CAST(__int16, expected);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedCompareExchange16, ((volatile __int16 *)&object, d, e))
      if (ret == e) return true;
      expected = SATOMI_BIT_CAST(T, ret);
      return false;
    }
    else if constexpr (sizeof(T) == 4)
    {
      __int32 ret = 0, d = SATOMI_BIT_CAST(__int32, desired), e = SATOMI_BIT_CAST(__int32, expected);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedCompareExchange, ((volatile long *)&object, d, e))
      if (ret == e) return true;
      expected = SATOMI_BIT_CAST(T, ret);
      return false;
    }
    else if constexpr (sizeof(T) == 8)
    {
      __int64 ret = 0, d = SATOMI_BIT_CAST(__int64, desired), e = SATOMI_BIT_CAST(__int64, expected);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedCompareExchange64, ((volatile __int64 *)&object, d, e))
      if (ret == e) return true;
      expected = SATOMI_BIT_CAST(T, ret);
      return false;
    }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) int128__ { __int64 v[2]; } d;
      
      d = SATOMI_BIT_CAST(int128__, desired);

      unsigned char result = 0;
      SATOMI_CHOOSE_MEMORY_ORDER(order, result = _InterlockedCompareExchange128, ((volatile __int64 *)&object, 
        d.v[1], d.v[0], (__int64 *)&expected))
      
      return result != 0;
    }

  #else

    #define SATOMI_ATOMIC_OP(INT) return __atomic_compare_exchange_n((volatile INT *)&object, (INT *)&expected, SATOMI_BIT_CAST(INT, desired),\
      0, (int)order, (int)(order == memory_order_acq_rel || order == memory_order_seq_cst ? memory_order_acquire : \
      order == memory_order_release ?  memory_order_relaxed : order));
    SATOMI_CHOOSE_SIZE(T, SATOMI_ATOMIC_OP)
    #undef SATOMI_ATOMIC_OP
    
    else if constexpr (sizeof(T) == 16)
    {
    #if defined (__x86_64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; };
      auto e = SATOMI_BIT_CAST(uint128__, expected);
      auto d = SATOMI_BIT_CAST(uint128__, desired);

      bool success;
      __asm__ __volatile__
      (
        "lock; cmpxchg16b %[target]\n\t"
        "sete %[success]\n\t"
        : [target] "+m" (object), "+a" (e.v[0]), "+d" (e.v[1]), [success] "=q" (success)
        : "b" (d.v[0]), "c" (d.v[1])
        : "cc", "memory"
      );

      expected = SATOMI_BIT_CAST(T, e);
      return success;

    #elif defined (__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original;
      auto d = SATOMI_BIT_CAST(uint128__, desired);
      auto e = SATOMI_BIT_CAST(uint128__, expected);
      bool success;
      
      #define SATOMI_ATOMIC_ASM(load_order, store_order)                                \
        __asm__ __volatile__                                                            \
        (                                                                               \
          "1:\n\t"                                                                      \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"            \
          "cmp %x[original_0], %x[expected_0]\n\t"                                      \
          "ccmp %x[original_1], %x[expected_1], #0, eq\n\t"                             \
          "b.ne 2f\n\t"                                                                 \
          "st" store_order "xp %w[success], %x[desired_0], %x[desired_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                    \
          "2:\n\t"                                                                      \
          "cset %w[success], eq\n\t"                                                    \
          : [success] "=&r" (success), [target] "+Q" (object),                          \
            [original_0] "=&r" (original.v[0u]), [original_1] "=&r" (original.v[1u])    \
          : [desired_0] "r" (d.v[0u]), [desired_1] "r" (d.v[1u]),                       \
            [expected_0] "r" (e.v[0u]), [expected_1] "r" (e.v[1u])                      \
          : "cc", "memory"                                                              \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      expected = SATOMI_BIT_CAST(T, original);
      return success;

    #endif
    }
      
  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr bool atomic_compare_exchange_weak(volatile T &object, T &expected, T desired, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      if (const_cast<T &>(object) == expected)
      {
        const_cast<T &>(object) = desired;
        return true;
      }
      expected = const_cast<T &>(object);
      return false;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    return atomic_compare_exchange_strong(object, expected, desired, order);

  #else
    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&desired);

    #define SATOMI_ATOMIC_OP(INT) return __atomic_compare_exchange_n((volatile INT *)&object, (INT *)&expected, SATOMI_BIT_CAST(INT, desired),\
      1, (int)order, (int)(order == memory_order_acq_rel || order == memory_order_seq_cst ? memory_order_acquire : memory_order_relaxed));
    SATOMI_CHOOSE_SIZE(T, SATOMI_ATOMIC_OP)
    #undef SATOMI_ATOMIC_OP

    else if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)
      return atomic_compare_exchange_strong(object, expected, desired, order);
    #elif defined(__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original;
      auto d = SATOMI_BIT_CAST(uint128__, desired);
      auto e = SATOMI_BIT_CAST(uint128__, expected);
      bool success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order)                                \
        __asm__ __volatile__                                                            \
        (                                                                               \
          "mov %w[success], #0\n\t"                                                     \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"            \
          "cmp %x[original_0], %x[expected_0]\n\t"                                      \
          "ccmp %x[original_1], %x[expected_1], #0, eq\n\t"                             \
          "b.ne 1f\n\t"                                                                 \
          "st" store_order "xp %w[success], %x[desired_0], %x[desired_1], %[target]\n\t"\
          "eor %w[success], %w[success], #1\n\t"                                        \
          "1:\n\t"                                                                      \
          : [success] "=&r" (success), [target] "+Q" (object),                          \
            [original_0] "=&r" (original.v[0u]), [original_1] "=&r" (original.v[1u])    \
          : [desired_0] "r" (d.v[0u]), [desired_1] "r" (d.v[1u]),                       \
            [expected_0] "r" (e.v[0u]), [expected_1] "r" (e.v[1u])                      \
          : "cc", "memory"                                                              \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      expected = SATOMI_BIT_CAST(T, original);
      return success;

    #endif
    }

  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_exchange(volatile T &object, T value, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T result = const_cast<T &>(object);
      const_cast<T &>(object) = value;
      return result;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&value);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      __int8 ret, v = SATOMI_BIT_CAST(__int8, value);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedExchange8, ((volatile __int8 *)&object, v))
      return SATOMI_BIT_CAST(T, ret);
    }
    else if constexpr (sizeof(T) == 2)
    {
      __int16 ret, v = SATOMI_BIT_CAST(__int16, value);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedExchange16, ((volatile __int16 *)&object, v))
      return SATOMI_BIT_CAST(T, ret);
    }
    else if constexpr (sizeof(T) == 4)
    {
      __int32 ret, v = SATOMI_BIT_CAST(__int32, value);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedExchange, ((volatile long *)&object, v))
      return SATOMI_BIT_CAST(T, ret);
    }
    else if constexpr (sizeof(T) == 8)
    {
      __int64 ret, v = SATOMI_BIT_CAST(__int64, value);
      SATOMI_CHOOSE_MEMORY_ORDER(order, ret = _InterlockedExchange64, ((volatile __int64 *)&object, v))
      return SATOMI_BIT_CAST(T, ret);
    }
    else if constexpr (sizeof(T) == 16)
    {
      T result = value;
      while (!atomic_compare_exchange_strong(object, result, value, order)) {}
      return result;
    }

  #else

    #define SATOMI_ATOMIC_OP(INT) return SATOMI_BIT_CAST(T, __atomic_exchange_n((volatile INT *)&object, SATOMI_BIT_CAST(INT, value), (int)order))
    SATOMI_CHOOSE_SIZE(T, SATOMI_ATOMIC_OP)
    #undef SATOMI_ATOMIC_OP

    else if constexpr (sizeof(T) == 16)
    {
      // shamelessly stolen from boost.atomic
    #if defined (__x86_64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } old;
      auto d = SATOMI_BIT_CAST(uint128__, value);

      __asm__ __volatile__
      (
        // the load needs to be done in assembly because movq is guaranteed to be atomic
        "movq %[target_lo], %%rax\n\t"
        "movq %[target_hi], %%rdx\n\t"
        ".align 16\n\t"
        "1: lock; cmpxchg16b %[target_lo]\n\t"
        "jne 1b\n\t"
        : [target_lo] "+m" (object), [target_hi] "+m" (reinterpret_cast<volatile __UINT64_TYPE__ *>(&object)[1]), 
          "=&a" (old.v[0]), "=&d" (old.v[1])
        : "b" (d.v[0]), "c" (d.v[1])
        : "cc", "memory"
      );

      return SATOMI_BIT_CAST(T, old);

    #elif defined (__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original;
      auto v = SATOMI_BIT_CAST(uint128__, value);
      unsigned int success;
      
      #define SATOMI_ATOMIC_ASM(load_order, store_order)                            \
        __asm__ __volatile__                                                        \
        (                                                                           \
          "1:\n\t"                                                                  \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"        \
          "st" store_order "xp %w[success], %x[value_0], %x[value_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                \
          : [success] "=&r" (success), [target] "+Q" (object),                      \
            [original_0] "=&r" (original.v[0u]), [original_1] "=&r" (original.v[1u])\
          : [value_0] "r" (v.v[0u]), [value_1] "r" (v.v[1u])                        \
          : "memory"                                                                \
        );

      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      return SATOMI_BIT_CAST(T, original);

    #endif
    }
  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_load(const volatile T &object, memory_order order = memory_order_seq_cst) noexcept
  {
    if (order == memory_order_release)
      order = memory_order_acquire;
    else if (order == memory_order_acq_rel)
      order = memory_order_seq_cst;
      
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T ret = const_cast<T &>(object);
      return ret;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      auto v = __iso_volatile_load8((const volatile __int8 *)&object);
      if (order != memory_order_relaxed)
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
      return SATOMI_BIT_CAST(T, v);
    }
    else if constexpr (sizeof(T) == 2)
    {
      auto v = __iso_volatile_load16((const volatile __int16 *)&object);
      if (order != memory_order_relaxed)
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
      return SATOMI_BIT_CAST(T, v);
    }
    else if constexpr (sizeof(T) == 4)
    {
      auto v = __iso_volatile_load32((const volatile __int32 *)&object);
      if (order != memory_order_relaxed)
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
      return SATOMI_BIT_CAST(T, v);
    }
    else if constexpr (sizeof(T) == 8)
    {
    #ifdef _M_ARM
      auto v = __ldrexd((const volatile __int64 *)&object);
    #else
      auto v = __iso_volatile_load64((const volatile __int64 *)&object);
    #endif
      if (order != memory_order_relaxed)
        SATOMI_COMPILER_OR_MEMORY_BARRIER();
      return SATOMI_BIT_CAST(T, v);
    }
    else if constexpr (sizeof(T) == 16)
    {
      struct alignas(16) int128__ { __int64 v[2]{}; } v{};

      SATOMI_CHOOSE_MEMORY_ORDER(order, (void)_InterlockedCompareExchange128, ((volatile __int64 *)&object, 0, 0, v.v))
      
      return SATOMI_BIT_CAST(T, v);
    }

  #else

    #define SATOMI_ATOMIC_OP(INT) return SATOMI_BIT_CAST(T, __atomic_load_n((volatile INT *)&object, (int)order))
    SATOMI_CHOOSE_SIZE(T, SATOMI_ATOMIC_OP)
    #undef SATOMI_ATOMIC_OP

    else if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      #if defined(__AVX__)

        // Intel Software Developer Manual Volume 3, Guaranteed Atomic Operations 
        // Processors supporting AVX guarantee aligned vector moves to be atomic.
        if ((((__UINTPTR_TYPE__)&object) & 15) == 0)
        {
          using uint128__ = __UINT64_TYPE__ __attribute__((__vector_size__(16)));
          uint128__ v;
          __asm__ __volatile__
          (
            "vmovdqa %[target], %[value]\n\t"
            : [value] "=x" (v)
            : [target] "m" (object)
            : "memory"
          );

          return SATOMI_BIT_CAST(T, v);
        }

      #endif

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } v;

      __asm__ __volatile__
      (
        // store whatever is rbx/rcx in rax/rdx so that 
        // even if we succeed to exchange we already have the value in rax/rdx
        "movq %%rbx, %%rax\n\t"
        "movq %%rcx, %%rdx\n\t"
        "lock; cmpxchg16b %[target]\n\t"
        : "=&a" (v.v[0]), "=&d" (v.v[1])
        : [target] "m" (object)
        : "cc", "memory"
      );

      return SATOMI_BIT_CAST(T, v);

    #elif defined(__aarch64__)
        
      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } value;
      unsigned success;

      #define SATOMI_DEFINE_LOAD_MEMORY_ORDERS(acquire_order)            \
        __asm__ __volatile__                                             \
        (                                                                \
          "1:\n\t"                                                       \
          "ld" acquire_order "xp %x[value_0], %x[value_1], %[target]\n\t"\
          "stxp %w[success], %x[value_0], %x[value_1], %[target]\n\t"    \
          "cbnz %w[success], 1b\n\t"                                     \
          : [success] "=&r" (success),                                   \
            [value_0] "=&r" (value.v[0]), [value_1] "=&r" (value.v[1])   \
          : [target] "Q" (object)                                        \
          : "memory"                                                     \
        );

      if (order == memory_order_relaxed)
        SATOMI_DEFINE_LOAD_MEMORY_ORDERS()
      else
        SATOMI_DEFINE_LOAD_MEMORY_ORDERS("a")
      
      #undef SATOMI_DEFINE_LOAD_MEMORY_ORDERS

      return SATOMI_BIT_CAST(T, value);

    #endif
    }
  #endif
  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr void atomic_store(volatile T &object, T value, memory_order order = memory_order_seq_cst) noexcept
  {
    if (order == memory_order_acquire || order == memory_order_consume)
      order = memory_order_release;
    else if (order == memory_order_acq_rel)
      order = memory_order_seq_cst;
          
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      const_cast<T &>(object) = value;
      return;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);    

    if constexpr (SATOMI_HAS_PADDING_BITS(T))
      SATOMI_CLEAR_PADDING_BITS(&value);

  #if defined(_MSC_VER) && ! (__clang__)

    #if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
      #define SATOMI_SEQ_CST_STORE(iso_suffix, ...) SATOMI_COMPILER_OR_MEMORY_BARRIER(); __iso_volatile_store##iso_suffix(memory, v); SATOMI_COMPILER_OR_MEMORY_BARRIER();
    #else
      #define SATOMI_SEQ_CST_STORE(iso_suffix, interlocked_suffix, ...) (void)_InterlockedExchange##interlocked_suffix(__VA_ARGS__ memory, v);
    #endif

    #define SATOMI_DEFINE_STORE_MEMORY_ORDERS(iso_suffix, interlocked_suffix, ...)\
      auto memory = (volatile __int##iso_suffix *)&object;                        \
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
      while (!atomic_compare_exchange_strong(object, result, value, order)) {}
    }

  #undef SATOMI_DEFINE_STORE_MEMORY_ORDERS
  #undef SATOMI_SEQ_CST_STORE

  #else

    #define SATOMI_ATOMIC_OP(INT) __atomic_store_n((volatile INT *)&object, SATOMI_BIT_CAST(INT, value), (int)order)
    SATOMI_CHOOSE_SIZE(T, SATOMI_ATOMIC_OP)
    #undef SATOMI_ATOMIC_OP

    else if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      #if defined(__AVX__)

        // Intel Software Developer Manual Volume 3, Guaranteed Atomic Operations 
        // Processors supporting AVX guarantee aligned vector moves to be atomic.
        using uint128__ = __UINT64_TYPE__ __attribute__((__vector_size__(16)));
        if ((((__UINTPTR_TYPE__)&object) & 15) == 0)
        {
          auto v = SATOMI_BIT_CAST(uint128__, value);
          __asm__ __volatile__
          (
            "vmovdqa %[value], %[storage]\n\t"
            : [storage] "=m" (object)
            : [value] "x" (v)
            : "memory"
          );

          return;
        }

      #endif

        struct alignas(16) uint128___ { __UINT64_TYPE__ v[2]; };
        auto v = SATOMI_BIT_CAST(uint128___, value);

        __asm__ __volatile__
        (
          "movq %[target_lo], %%rax\n\t"
          "movq %[target_hi], %%rdx\n\t"
          ".align 16\n\t"
          "1: lock; cmpxchg16b %[target_lo]\n\t"
          "jne 1b\n\t"
          : [target_lo] "=m" (object), [target_hi] "=m" (reinterpret_cast<volatile __UINT64_TYPE__ *>(&object)[1])
          : "b" (v.v[0]), "c" (v.v[1])
          : "cc", "rax", "rdx", "memory"
        );

    #elif defined(__aarch64__)
        
      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original;
      auto v = SATOMI_BIT_CAST(uint128__, value);
      unsigned success;

      #define SATOMI_DEFINE_STORE_MEMORY_ORDERS(store_order)                        \
        __asm__ __volatile__                                                        \
        (                                                                           \
          "1:\n\t"                                                                  \
          "ldxp %x[original_0], %x[original_1], %[target]\n\t"                      \
          "st" store_order "xp %w[success], %x[value_0], %x[value_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                \
          : [success] "=&r" (success), [target] "+Q" (object),                      \
            [original_0] "=&r" (original.v[0u]), [original_1] "=&r" (original.v[1u])\
          : [value_0] "r" (v.v[0u]), [value_1] "r" (v.v[1u])                        \
          : "memory"                                                                \
        );

      if (order == memory_order_relaxed)
        SATOMI_DEFINE_STORE_MEMORY_ORDERS()
      else
        SATOMI_DEFINE_STORE_MEMORY_ORDERS("l")
      
      #undef SATOMI_DEFINE_STORE_MEMORY_ORDERS

    #endif
    }

  #endif
  }


  // only available for integral types
  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_add(volatile T &object, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = object;
      const_cast<T &>(object) += operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd8, ((volatile __int8 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 2)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd16, ((volatile __int16 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 4)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd, ((volatile long *)&object, (long)operand))
    }
    else if constexpr (sizeof(T) == 8)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd64, ((volatile __int64 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 16)
    {
      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current + operand, order)) {}
      return current;
    }

    // only to satisfy the arm warnings since msvc doesn't recognise __fastfail being noreturn
    #if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
      return {};
    #endif

  #else

    if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current + operand, order)) {}
      return current;       

    #elif defined(__aarch64__)

      #if defined(__AARCH64EL__) || \
        (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
        (defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__))

        // little endian
        #define SATOMI_ARG_LO "0"
        #define SATOMI_ARG_HI "1"

      #else

        // big endian
        #define SATOMI_ARG_LO "1"
        #define SATOMI_ARG_HI "0"

      #endif

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original, result;
      auto o = SATOMI_BIT_CAST(uint128__, operand);
      unsigned success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order)                                                        \
        __asm__ __volatile__                                                                                    \
        (                                                                                                       \
          "1:\n\t"                                                                                              \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"                                    \
          "adds %x[result_" SATOMI_ARG_LO "], %x[original_" SATOMI_ARG_LO "], %x[operand_" SATOMI_ARG_LO "]\n\t"\
          "adc %x[result_" SATOMI_ARG_HI "], %x[original_" SATOMI_ARG_HI "], %x[operand_" SATOMI_ARG_HI "]\n\t" \
          "st" store_order "xp %w[success], %x[result_0], %x[result_1], %[target]\n\t"                          \
          "cbnz %w[success], 1b\n\t"                                                                            \
          : [success] "=&r" (success), [target] "+Q" (object),                                                  \
            [original_0] "=&r" (original.v[0]), [original_1] "=&r" (original.v[1]),                             \
            [result_0] "=&r" (result.v[0]), [result_1] "=&r" (result.v[1])                                      \
          : [operand_0] "Lr" (o.v[0]), [operand_1] "Lr" (o.v[1])                                                \
          : "cc", "memory"                                                                                      \
        );
      
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ARG_LO
      #undef SATOMI_ARG_HI
      #undef SATOMI_ATOMIC_ASM

      return SATOMI_BIT_CAST(T, original);

    #endif
    }
    else
    {
      return __atomic_fetch_add(&object, operand, (int)order);
    }

  #endif
  }


  // only available for pointer types
  template<typename T> requires SATOMI_IS_POINTER(T)
  SATOMI_INLINE constexpr T atomic_fetch_add(volatile T &object, detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = object;
      const_cast<T &>(object) += operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 4)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd, ((volatile __int32 *)&object, (long)operand))
    }
    else if constexpr (sizeof(T) == 8)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedExchangeAdd64, ((volatile __int64 *)&object, operand))
    }

  #else

    return __atomic_fetch_add(&object, operand, (int)order);

  #endif
  }


  #if defined(_MSC_VER) && ! (__clang__)
    #pragma warning(push)
    #pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
  #endif

  // only available for integral types
  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_sub(volatile T &object, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    return atomic_fetch_add(object, (T)(-operand), order);
  }


  // only available for pointer types
  template<typename T> requires SATOMI_IS_POINTER(T)
  SATOMI_INLINE constexpr T atomic_fetch_sub(volatile T &object, detail::ptrdiff_t operand, memory_order order = memory_order_seq_cst) noexcept
  {
    return atomic_fetch_sub(object, -operand, order);
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_and(volatile T &object, T operand, memory_order order = memory_order_seq_cst) noexcept
  {    
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = object;
      const_cast<T &>(object) &= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedAnd8, ((volatile __int8 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 2)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedAnd16, ((volatile __int16 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 4)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedAnd, ((volatile long *)&object, (long)operand))
    }
    else if constexpr (sizeof(T) == 8)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedAnd64, ((volatile __int64 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 16)
    {
      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current & operand, order)) {}
      return current;
    }

  #else

    if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current & operand, order)) {}
      return current;

    #elif defined(__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original, result;
      auto o = SATOMI_BIT_CAST(uint128__, operand);
      unsigned success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order)                              \
        __asm__ __volatile__                                                          \
        (                                                                             \
          "1:\n\t"                                                                    \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"          \
          "and %x[result_0], %x[original_0], %x[operand_0]\n\t"                       \
          "and %x[result_1], %x[original_1], %x[operand_1]\n\t"                       \
          "st" store_order "xp %w[success], %x[result_0], %x[result_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                  \
          : [success] "=&r" (success), [target] "+Q" (object),                        \
            [original_0] "=&r" (original.v[0]), [original_1] "=&r" (original.v[1]),   \
            [result_0] "=&r" (result.v[0]), [result_1] "=&r" (result.v[1])            \
          : [operand_0] "Lr" (o.v[0]),                                                \
            [operand_1] "Lr" (o.v[1])                                                 \
          : "cc", "memory"                                                            \
        );
      
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      return SATOMI_BIT_CAST(T, original);

    #endif
    }
    else
    {
      return __atomic_fetch_and(&object, operand, (int)order);
    }

  #endif
  }


  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_or(volatile T &object, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = object;
      const_cast<T &>(object) |= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedOr8, ((volatile __int8 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 2)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedOr16, ((volatile __int16 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 4)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedOr, ((volatile long *)&object, (long)operand))
    }
    else if constexpr (sizeof(T) == 8)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedOr64, ((volatile __int64 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 16)
    {
      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current | operand, order)) {}
      return current;
    }

  #else

    if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current | operand, order)) {}
      return current;

    #elif defined(__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original, result;
      auto o = SATOMI_BIT_CAST(uint128__, operand);
      unsigned success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order)                              \
        __asm__ __volatile__                                                          \
        (                                                                             \
          "1:\n\t"                                                                    \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"          \
          "orr %x[result_0], %x[original_0], %x[operand_0]\n\t"                       \
          "orr %x[result_1], %x[original_1], %x[operand_1]\n\t"                       \
          "st" store_order "xp %w[success], %x[result_0], %x[result_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                  \
          : [success] "=&r" (success), [target] "+Q" (object),                        \
            [original_0] "=&r" (original.v[0]), [original_1] "=&r" (original.v[1]),   \
            [result_0] "=&r" (result.v[0]), [result_1] "=&r" (result.v[1])            \
          : [operand_0] "Lr" (o.v[0]),                                                \
            [operand_1] "Lr" (o.v[1])                                                 \
          : "cc", "memory"                                                            \
        );
      
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      return SATOMI_BIT_CAST(T, original);

    #endif
    }
    else
    {
      return __atomic_fetch_or(&object, operand, (int)order);
    }

  #endif
  }

  template<typename T> requires SATOMI_IS_ATOMIC_READY(T)
  SATOMI_INLINE constexpr T atomic_fetch_xor(volatile T &object, T operand, memory_order order = memory_order_seq_cst) noexcept
  {
    if (SATOMI_IS_CONSTANT_EVALUATED())
    {
      T current = object;
      const_cast<T &>(object) ^= operand;
      return current;
    }

    SATOMI_CHECK_ALIGNMENT(sizeof(T), object);

  #if defined(_MSC_VER) && ! (__clang__)

    if constexpr (sizeof(T) == 1)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedXor8, ((volatile __int8 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 2)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedXor16, ((volatile __int16 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 4)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedXor, ((volatile long *)&object, (long)operand))
    }
    else if constexpr (sizeof(T) == 8)
    {
      SATOMI_CHOOSE_MEMORY_ORDER(order, return _InterlockedXor64, ((volatile __int64 *)&object, operand))
    }
    else if constexpr (sizeof(T) == 16)
    {
      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current ^ operand, order)) {}
      return current;
    }

  #else

    if constexpr (sizeof(T) == 16)
    {
    #if defined(__x86_64__)

      auto current = atomic_load(object, order);
      while (!atomic_compare_exchange_strong(object, current, current ^ operand, order)) {}
      return current;

    #elif defined(__aarch64__)

      struct alignas(16) uint128__ { __UINT64_TYPE__ v[2]; } original, result;
      auto o = SATOMI_BIT_CAST(uint128__, operand);
      unsigned success;

      #define SATOMI_ATOMIC_ASM(load_order, store_order)                              \
        __asm__ __volatile__                                                          \
        (                                                                             \
          "1:\n\t"                                                                    \
          "ld" load_order "xp %x[original_0], %x[original_1], %[target]\n\t"          \
          "eor %x[result_0], %x[original_0], %x[operand_0]\n\t"                       \
          "eor %x[result_1], %x[original_1], %x[operand_1]\n\t"                       \
          "st" store_order "xp %w[success], %x[result_0], %x[result_1], %[target]\n\t"\
          "cbnz %w[success], 1b\n\t"                                                  \
          : [success] "=&r" (success), [target] "+Q" (object),                        \
            [original_0] "=&r" (original.v[0]), [original_1] "=&r" (original.v[1]),   \
            [result_0] "=&r" (result.v[0]), [result_1] "=&r" (result.v[1])            \
          : [operand_0] "Lr" (o.v[0]),                                                \
            [operand_1] "Lr" (o.v[1])                                                 \
          : "cc", "memory"                                                            \
        );
      
      SATOMI_CHOOSE_MEMORY_ORDER_ASM(order)
      #undef SATOMI_ATOMIC_ASM

      return SATOMI_BIT_CAST(T, original);

    #endif
    }
    else
    {
      return __atomic_fetch_xor(&object, operand, (int)order);
    }

  #endif
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

  protected:
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

#endif
