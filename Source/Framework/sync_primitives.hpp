/*
  ==============================================================================

    sync_primitives.hpp
    Created: 10 Mar 2024 02:06:32
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "satomi.hpp"
#include "utils.hpp"

namespace utils
{
  template<usize Iterations>
  strict_inline void longPause() noexcept
  {
    unroll<Iterations>([]()
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
      });
  }

  void millisleep() noexcept;

  /// this timer is intended to be used inside a running loop
  /// for accurate dispatch at regular intervals
  /// sleep of <1ms is not guaranteed
  class Timer
  {
    static constexpr double kMicro = 1'000'000.0;

    /// static state
    //
    i32 sleepUs_ = 0;
    u64 correctionPeriod_; // corrects every second by default

    /// runtime state
    //
    i32 correctionUs_ = 500; // start at some reasonable correction
    u32 counter_ = 0;
    u64 correctionStart_ = 0;

  public:
    Timer();
    void setPeriod(double microseconds)
    {
      sleepUs_ = (i32)microseconds;
      correctionStart_ = 0;
      correctionUs_ = 500;
      counter_ = 0;
    }
    void setFrequency(double hz) { setPeriod(kMicro / hz); }

    /// how often to check correct sleep time to more closely match desired loop period
    /// can also be set to 0 and manually call correctSleep()
    /// @see Timer::correctSleep()
    void setCorrectionPeriod(double seconds);

    void sleep(bool correctAutomatically = true);

    /// corrects sleep time to more closely match desired loop period
    /// can be used without the provided automatic correction
    /// so long as setCorrectionPeriod(0.0)
    /// this is intended when user wants to meet a certain threshold of accuracy
    /// and then stop looking for a better sleep correction offset to minimise jitter
    void correctSleep();
  };

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
    satomi::atomic<T> lock{};
    satomi::atomic<utils::thread::id> lastLockId{};
  };

  template<typename T>
  struct ReentrantLock : LockBlame<T> { };

  strict_inline void 
  lockAtomic(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false)
  {
    bool state = expected;
    while (!atomic.compare_exchange_weak(state, !expected, satomi::memory_order_acq_rel))
    {
      // protecting against spurious failures
      if (state == expected)
        continue;

      switch (mechanism)
      {
      case WaitMechanism::Spin:
      case WaitMechanism::SpinNotify:
        // taking advantage of the MESI protocol where we only want shared access to read until the value is changed,
        // instead of read/write with exclusive access thereby slowing down other threads trying to read as well
        while (atomic.load(satomi::memory_order_relaxed) != expected) { COMPLEX_PAUSE(); }
        break;
      case WaitMechanism::Wait:
      case WaitMechanism::WaitNotify:
        atomic.wait(state, satomi::memory_order_relaxed);
        break;
      case WaitMechanism::Sleep:
      case WaitMechanism::SleepNotify:
        // sleeping should take ideally around a milisecond, may change later
        millisleep();
        break;
      default:
        COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
        break;
      }

      state = expected;
    }
  }
  strict_inline void unlockAtomic(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false)
  {
    atomic.store(expected, satomi::memory_order_release);
    if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
      atomic.notify_all();
  }
  i32 lockAtomic(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
    const utils::smallFn<void()> &lambda = [](){}) noexcept;
  strict_inline i32 
  lockAtomic(LockBlame<i32> &lock, bool isReentrant, bool isExclusive, WaitMechanism mechanism,
    const utils::smallFn<void()> &lambda = [](){}) noexcept
  {
    if (!isExclusive)
      return lockAtomic(lock.lock, isExclusive, mechanism, lambda);

    auto threadId = utils::thread::getCurrentId();
    if (lock.lastLockId.load(satomi::memory_order_relaxed) == threadId)
    {
      if (isReentrant)
        return -1;
      else
        COMPLEX_ASSERT_FALSE("Guess who forgot to unlock this atomic");
    }

    auto ret = lockAtomic(lock.lock, isExclusive, mechanism, lambda);

    if (ret == 0)
      lock.lastLockId.store(threadId, satomi::memory_order_relaxed);
    return ret;
  }
  void unlockAtomic(satomi::atomic<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept;
  strict_inline void
  unlockAtomic(LockBlame<i32> &lock, bool wasExclusive, 
    WaitMechanism mechanism, i32 previousValue) noexcept
  {
    if (wasExclusive && previousValue == -1)
      return;

    unlockAtomic(lock.lock, wasExclusive, mechanism);

    if (wasExclusive)
      lock.lastLockId.store({}, satomi::memory_order_relaxed);
  }

  class ScopedLock
  {
  public:
    ScopedLock() : type_{ Empty }, mechanism_{ WaitMechanism::Spin }, bool_{} { }
    
    ScopedLock(satomi::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(BoolEnum), mechanism_(mechanism), bool_{ &atomic, expected }
    { lockAtomic(atomic, mechanism, expected); }

    ScopedLock(ReentrantLock<bool> &reentrantLock, WaitMechanism mechanism, bool expected = false) noexcept :
      type_(ReentrantBoolEnum), mechanism_(mechanism), reentrantBool_{ &reentrantLock, false, expected }
    {
      auto threadId = utils::thread::getCurrentId();
      reentrantBool_.wasLocked = threadId == reentrantLock.lastLockId.load(satomi::memory_order_relaxed);

      if (!reentrantBool_.wasLocked)
      {
        lockAtomic(reentrantLock.lock, mechanism, expected);
        reentrantLock.lastLockId.store(threadId, satomi::memory_order_relaxed);
      }
    }
    
    ScopedLock(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism) noexcept :
      type_{ I32LockEnum }, mechanism_{ mechanism }, i32_{ &atomic, isExclusive }
    { lockAtomic(atomic, isExclusive, mechanism); }
    
    ScopedLock(ReentrantLock<i32> &reentrantLock, bool isExclusive, WaitMechanism mechanism) noexcept :
      type_{ I32LockEnum }, mechanism_{ mechanism }, i32Lock_{ &reentrantLock, isExclusive }
    { i32Lock_.previousValue = lockAtomic(reentrantLock, true, isExclusive, mechanism); }

    ScopedLock(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism,
      const utils::smallFn<void()> &lambda = [](){}) noexcept : type_{ I32LockEnum },
      mechanism_{ mechanism }, i32Lock_{ &lock, isExclusive }
    { i32Lock_.previousValue = lockAtomic(lock, false, isExclusive, mechanism, lambda); }

    ScopedLock(ScopedLock &&other) noexcept = delete;
    ScopedLock &operator=(ScopedLock &&other) noexcept
    {
      ::valcpy(this, &other);
      other.type_ = Empty;
      return *this;
    }
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
      else if (type_ == I32LockEnum)
        unlockAtomic(*i32Lock_.atomic, i32Lock_.isExclusive, mechanism_, i32Lock_.previousValue);
      else if (type_ == ReentrantBoolEnum && !reentrantBool_.wasLocked)
      {
        unlockAtomic(reentrantBool_.atomic->lock, mechanism_, reentrantBool_.expected);
        reentrantBool_.atomic->lastLockId.store({}, satomi::memory_order_relaxed);
      }

      type_ = Empty;
    }

  private:
    enum { Empty, BoolEnum, I32Enum, I32LockEnum, ReentrantBoolEnum } type_;
    WaitMechanism mechanism_;
    struct BoolType { satomi::atomic<bool> *atomic; bool expected; };
    struct I32Type { satomi::atomic<i32> *atomic; bool isExclusive; };
    struct I32LockType { LockBlame<i32> *atomic; bool isExclusive; i32 previousValue; };
    struct ReentrantBoolType { ReentrantLock<bool> *atomic; bool wasLocked; bool expected; };
    union
    {
      BoolType bool_;
      I32Type i32_;
      I32LockType i32Lock_;
      ReentrantBoolType reentrantBool_;
    };
  };

  template<typename T>
  class shared_value
  {
    struct atomic_holder
    {
      T load() const noexcept { return value.load(satomi::memory_order_relaxed); }
      void store(T newValue) noexcept { value.store(newValue, satomi::memory_order_relaxed); }

      satomi::atomic<T> value;
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
      mutable satomi::atomic<bool> guard = false;
    };

    // if the type cannot fit into an atomic we can have an atomic_bool to guard it
    using holder = utils::conditional_t<satomi::atomic<T>::is_always_lock_free, atomic_holder, guard_holder>;
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
    mutable satomi::atomic<Flag> flag_{ Unused };

    void changeFlag(Flag newFlag) noexcept
    {
      Flag flag = Unused;
      while (!flag_.compare_exchange_weak(flag, newFlag, satomi::memory_order_acquire))
      {
        while (flag == Using)
        {
          millisleep();
          flag = flag_.load(satomi::memory_order_relaxed);
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
        
        COMPLEX_ASSERT(holder_->flag_.load(satomi::memory_order_relaxed) == Using,
          "A span is given out only if it is the exclusive user of the data");
        auto newFlag = isWriting_ ? Updated : Unused;
        holder_->flag_.store(newFlag, satomi::memory_order_release);

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
    { return flag_.load(satomi::memory_order_relaxed) == Updated; }

    void resize(usize size)
    {
      usize copySize = min(size, size_);
      auto newData = up<T[]>::create(size);
      
      changeFlag(Using);

      if (copySize != 0)
        memcpy(newData.get(), data_.get(), copySize);
      data_ = COMPLEX_MOVE(newData);
      size_ = size;

      flag_.store(Unused, satomi::memory_order_release);
    }

    void copy(T *writee) noexcept
    {
      changeFlag(Using);

      memcpy(writee, data_.get(), size_ * sizeof(T));

      flag_.store(Unused, satomi::memory_order_release);
    }
    pair<up<T[]>, usize> copy() noexcept
    {
      changeFlag(Using);

      usize size = size_;
      auto newData = up<T[]>::create(size);
      memcpy(newData.get(), data_.get(), size_ * sizeof(T));

      flag_.store(Unused, satomi::memory_order_release);
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

  template<typename T>
  class shared_value_block
  {
  public:
    shared_value_block() = default;
    explicit shared_value_block(T &&value) noexcept { this->value = COMPLEX_MOVE(value); }
    shared_value_block(shared_value_block &&other) noexcept
    {
      ScopedLock g{ other.guard, WaitMechanism::Sleep };
      value = COMPLEX_MOVE(other.value);
    }
    shared_value_block &operator=(shared_value_block &&other) noexcept
    {
      ScopedLock g{ other.guard, WaitMechanism::Sleep };
      return shared_value_block::operator=(COMPLEX_MOVE(other.value));
    }
    shared_value_block &operator=(T &&newValue) noexcept
    {
      ScopedLock g{ guard, WaitMechanism::WaitNotify };
      value = COMPLEX_MOVE(newValue);
      return *this;
    }

    [[nodiscard]] T &lock() noexcept
    {
      lockAtomic(guard, WaitMechanism::WaitNotify, false);
      return value;
    }

    [[nodiscard]] const T &lock() const noexcept
    {
      lockAtomic(guard, WaitMechanism::WaitNotify, false);
      return value;
    }

    void unlock() const noexcept
    {
      guard.store(false, satomi::memory_order_release);
      guard.notify_one();
    }
  private:
    mutable satomi::atomic<bool> guard = false;
    T value{};
  };
}
