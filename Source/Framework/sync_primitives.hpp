/*
  ==============================================================================

    sync_primitives.hpp
    Created: 10 Mar 2024 2:06:32am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <atomic>
#ifdef COMPLEX_ARM
  #include <arm_acle.h>
#endif
#include "Third Party/clog/small_function.hpp"

#include "stl_utils.hpp"
#include "simd_values.hpp"
#include <cstring>

namespace utils
{
  strict_inline void pause() noexcept
  {
  #if COMPLEX_SSE4_1
    _mm_pause();
  #elif COMPLEX_NEON
    #if COMPLEX_MSVC
      __yield();
    #else
      asm volatile ("yield");
    #endif
  #endif
  }

  template<usize Iterations>
  strict_inline void longPause() noexcept
  {
    unroll<Iterations>([]()
      {
        pause();
        pause();
        pause();
        pause();
        pause();
      });
  }

  void millisleep() noexcept;
  void millisleep(const clg::small_fn<bool()> &shouldWaitFn);

  // Spin - spinlock (use on realtime threads)
  // Wait - waits for signal from locking site (use for unknown sleep length)
  // Sleep - millisleep (use if you expect something reasonably soon)
  // 
  // all notify variants signal waiting sites upon unlock (REALTIME-UNSAFE)
  enum class WaitMechanism : u32 { Spin = 0, Wait, Sleep, SpinNotify = 4, WaitNotify, SleepNotify };

  // git blame for deadlocks
  template<typename T>
  struct LockBlame
  {
    std::atomic<T> lock{};
    std::atomic<usize> lastLockId{};
  };

  template<typename T>
  struct ReentrantLock
  {
    std::atomic<T> lock{};
    std::atomic<usize> lastLockId{};
  };

  void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
  void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
  i32 lockAtomic(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism,
    const clg::small_fn<void()> &lambda = [](){}) noexcept;
  void unlockAtomic(LockBlame<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept;

  class ScopedLock
  {
  public:
    ScopedLock(ReentrantLock<bool> &reentrantLock, WaitMechanism mechanism, bool expected = false) noexcept;

    ScopedLock(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(BoolEnum), mechanism_(mechanism), bool_{ &atomic, expected }
    { lockAtomic(atomic, mechanism, expected); }

    ScopedLock(LockBlame<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
      const clg::small_fn<void()> &lambda = [](){}) noexcept : type_(I32Enum), mechanism_(mechanism),
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

      if (type_ == BoolEnum)
        unlockAtomic(*bool_.atomic, mechanism_, bool_.expected);
      else if (type_ == I32Enum)
        unlockAtomic(*i32_.atomic, i32_.isExclusive, mechanism_);
      else if (type_ == ReentrantBoolEnum && !reentrantBool_.wasLocked)
      {
        unlockAtomic(reentrantBool_.atomic->lock, mechanism_, reentrantBool_.expected);
        reentrantBool_.atomic->lastLockId.store({}, std::memory_order_relaxed);
      }

      type_ = Empty;
    }

  private:
    enum { Empty, BoolEnum, I32Enum, ReentrantBoolEnum } type_;
    WaitMechanism mechanism_;
    struct ReentrantBoolType { ReentrantLock<bool> *atomic; bool wasLocked; bool expected; };
    struct BoolType { std::atomic<bool> *atomic; bool expected; };
    struct I32Type { LockBlame<i32> *atomic; bool isExclusive; };
    union
    {
      ReentrantBoolType reentrantBool_;
      BoolType bool_;
      I32Type i32_;
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
        value = COMPLEX_MOVE(newValue);
      }

      T value;
      mutable std::atomic<bool> guard = false;
    };

    // if the type cannot fit into an atomic we can have an atomic_bool to guard it
    using holder = utils::conditional_t<sizeof(T) <= sizeof(std::uintptr_t), atomic_holder, guard_holder>;
  public:
    shared_value() = default;
    shared_value(const shared_value &other) noexcept : value{ other.value.load() } { }
    shared_value(T newValue) noexcept : value{ COMPLEX_MOVE(newValue) } { }
    shared_value &operator=(const shared_value &other) noexcept { return shared_value::operator=(other.value.load()); }
    shared_value &operator=(T newValue) noexcept { value.store(COMPLEX_MOVE(newValue)); return *this; }
    operator T() const noexcept { return get(); }
    T operator->() const noexcept requires utils::is_pointer_v<T> { return get(); }

    [[nodiscard]] T get() const noexcept { return value.load(); }
  private:
    holder value{};
  };

  // intended for quick writes and frequent reads
  template<typename T>
  class shared_value<T[]>
  {
  private:
    enum Flag : u8 { Unused, Updated, Using };
    up<T[]> data_{};
    usize size_{};
    mutable std::atomic<Flag> flag_{ Unused };

    void changeFlag(Flag newFlag) noexcept
    {
      Flag flag = Unused;
      while (!flag_.compare_exchange_weak(flag, newFlag, std::memory_order_acquire))
      {
        while (flag == Using)
        {
          millisleep();
          flag = flag_.load(std::memory_order_relaxed);
        }
      }
    }
  public:
    class span : public utils::span<T>
    {
      shared_value *holder_ = nullptr;
      bool isWriting_ = false;
      span(T *data, usize size, shared_value *holder, bool isWriting) :
        utils::span<T>{ data, size }, holder_{ holder }, isWriting_{ isWriting } { }
    public:
      span(const span &) = delete;
      ~span() noexcept
      {
        if (!holder_)
          return;
        
        COMPLEX_ASSERT(holder_->flag_.load(std::memory_order_relaxed) == Using, 
          "A span is given out only if it is the exclusive user of the data");
        auto newFlag = isWriting_ ? Updated : Unused;
        holder_->flag_.store(newFlag, std::memory_order_release);

        holder_ = nullptr;
      }
      friend class shared_value;
    };

    shared_value() = default;
    shared_value(const shared_value &) noexcept = delete;
    shared_value(shared_value &&) noexcept = default;
    shared_value(usize size) : data_{ up<T[]>::create(size) }, size_{ size } { }
    shared_value &operator=(const shared_value &) noexcept = delete;
    shared_value &operator=(shared_value &&other) noexcept
    {
      if (this != &other)
      {
        data_ = COMPLEX_MOVE(other.data_);
        size_ = other.size_;
      }
      return *this;
    }

    void update() noexcept { changeFlag(Updated); }
    bool hasUpdate() const noexcept
    { return flag_.load(std::memory_order_relaxed) == Updated; }

    void resize(usize size)
    {
      usize copySize = min(size, size_);
      auto newData = up<T[]>::create(size);
      
      changeFlag(Using);

      if (copySize != 0)
        memcpy(newData.get(), data_.get(), copySize);
      data_ = COMPLEX_MOVE(newData);
      size_ = size;

      flag_.store(Unused, std::memory_order_release);
    }

    void copy(T *writee) noexcept
    {
      changeFlag(Using);

      memcpy(writee, data_.get(), size_ * sizeof(T));

      flag_.store(Unused, std::memory_order_release);
    }
    pair<up<T[]>, usize> copy() noexcept
    {
      changeFlag(Using);

      usize size = size_;
      auto newData = up<T[]>::create(size);
      memcpy(newData.get(), data_.get(), size_ * sizeof(T));

      flag_.store(Unused, std::memory_order_release);
      return { COMPLEX_MOVE(newData), size };
    }

    // locks the array and returns a span that will set update flag automatically

    span read() noexcept
    {
      changeFlag(Using);
      return { data_.get(), size_, this, false };
    }
    span write() noexcept
    {
      changeFlag(Using);
      return { data_.get(), size_, this, true };
    }
  };
}
