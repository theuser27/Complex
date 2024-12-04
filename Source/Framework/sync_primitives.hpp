/*
  ==============================================================================

    sync_primitives.hpp
    Created: 10 Mar 2024 2:06:32am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <atomic>
#include <thread>
#include "Third Party/clog/small_function.hpp"

#include "stl_utils.hpp"

namespace utils
{
  strict_inline void pause() noexcept
  {
  #if COMPLEX_SSE4_1
    _mm_pause();
  #elif COMPLEX_NEON
    __yield();
  #endif
  }

  template<usize Iterations>
  strict_inline void longPause() noexcept
  {
    unroll<Iterations>([]()
      {
      #if COMPLEX_SSE4_1
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
      #elif COMPLEX_NEON
        __yield();
        __yield();
        __yield();
        __yield();
        __yield();
      #endif
      });
  }

  void loadLibraries();
  void unloadLibraries();

  void millisleep() noexcept;

  enum class WaitMechanism : u32 { Spin = 0, Wait, Sleep, SpinNotify = 4, WaitNotify, SleepNotify };

  // git blame for deadlocks
  // versionFlag is incremented on writes to the data being protected
  template<typename T>
  struct LockBlame
  {
    std::atomic<T> lock{};
    std::atomic<std::thread::id> lastLockId{};
    std::atomic<u64> versionFlag{};
  };

  template<typename T>
  struct ReentrantLock
  {
    std::atomic<T> lock{};
    std::atomic<std::thread::id> lastLockId{};
  };

  void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
  void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
  i32 lockAtomic(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism,
    const clg::small_fn<void()> &lambda = [](){}) noexcept;
  void unlockAtomic(LockBlame<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept;

  class ScopedLock
  {
  public:
    ScopedLock(ReentrantLock<bool> &reentrantLock, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(ReentrantBoolType), mechanism_(mechanism), reentrantBool_{ &reentrantLock, false, expected }
    {
      reentrantBool_.wasLocked = std::this_thread::get_id() == 
        reentrantLock.lastLockId.load(std::memory_order_relaxed);

      if (!reentrantBool_.wasLocked)
      {
        lockAtomic(reentrantLock.lock, mechanism, expected);
        reentrantLock.lastLockId.store(std::this_thread::get_id(), std::memory_order_relaxed);
      }
    }

    ScopedLock(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(BoolType), mechanism_(mechanism), bool_{ &atomic, expected }
    { lockAtomic(atomic, mechanism, expected); }

    ScopedLock(LockBlame<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
      const clg::small_fn<void()> &lambda = [](){}) noexcept : type_(I32Type), mechanism_(mechanism),
      i32_{ &atomic, isExclusive }
    { lockAtomic(atomic, isExclusive, mechanism, lambda); }

    ScopedLock(ScopedLock &&other) noexcept = default;
    ScopedLock &operator=(ScopedLock &&other) noexcept = default;
    ScopedLock(const ScopedLock &) = delete;
    ScopedLock operator=(const ScopedLock &) = delete;

    ~ScopedLock() noexcept
    {
      if (type_ == Empty)
        return;

      if (type_ == BoolType)
        unlockAtomic(*bool_.atomic, mechanism_, bool_.expected);
      else if (type_ == I32Type)
        unlockAtomic(*i32_.atomic, i32_.isExclusive, mechanism_);
      else if (type_ == ReentrantBoolType && !reentrantBool_.wasLocked)
      {
        unlockAtomic(reentrantBool_.atomic->lock, mechanism_, reentrantBool_.expected);
        reentrantBool_.atomic->lastLockId.store({}, std::memory_order_relaxed);
      }

      type_ = Empty;
    }

  private:
    enum { Empty, BoolType, I32Type, ReentrantBoolType } type_;
    WaitMechanism mechanism_;
    union
    {
      struct { ReentrantLock<bool> *atomic; bool wasLocked; bool expected; } reentrantBool_;
      struct { std::atomic<bool> *atomic; bool expected; } bool_;
      struct { LockBlame<i32> *atomic; bool isExclusive; } i32_;
    };
  };

  template<typename T>
  class shared_value
  {
    struct atomic_holder
    {
      T load() const noexcept { return value.load(std::memory_order_relaxed); }
      void store(T newValue) noexcept { value.store(newValue, std::memory_order_relaxed); }

      std::atomic<T> value;
    };

    struct guard_holder
    {
      T load() const noexcept
      {
        ScopedLock g{ guard, WaitMechanism::Spin };
        return value;
      }
      void store(T newValue) noexcept
      {
        ScopedLock g{ guard, WaitMechanism::Spin };
        value = std::move(newValue);
      }

      T value;
      mutable std::atomic<bool> guard = false;
    };

    // if the type cannot fit into an atomic we can have an atomic_bool to guard it
    using holder = std::conditional_t<sizeof(T) <= sizeof(std::uintptr_t), atomic_holder, guard_holder>;  // NOLINT(bugprone-sizeof-expression)
  public:
    shared_value() = default;
    shared_value(const shared_value &other) noexcept : value{ other.value.load() } { }
    shared_value(T newValue) noexcept : value{ std::move(newValue) } { }
    shared_value &operator=(const shared_value &other) noexcept { return shared_value::operator=(other.value.load()); }
    shared_value &operator=(T newValue) noexcept { value.store(std::move(newValue)); return *this; }
    operator T() const noexcept { return get(); }
    T operator->() const noexcept requires std::is_pointer_v<T> { return get(); }

    [[nodiscard]] T get() const noexcept { return value.load(); }
  private:
    holder value{};
  };
}
