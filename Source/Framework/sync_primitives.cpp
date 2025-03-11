/*
  ==============================================================================

    sync_primitives.cpp
    Created: 10 Mar 2024 2:06:32am
    Author:  theuser27

  ==============================================================================
*/

#include "platform_definitions.hpp"

#if COMPLEX_DEBUG
#include <cstdarg>
#include <vector>
#include <cstdio>
#include <cassert>

#ifdef COMPLEX_WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #undef WIN32_LEAN_AND_MEAN
#endif

namespace common
{
  void complexPrintAssertMessage(const char *conditionString, const char *fileName, 
    const char *functionName, int line, int hasMoreArgs, ...)
  {
  #if COMPLEX_WINDOWS
    #define PRINT_MESSAGE(message, ...) { \
        std::vector<char> buffer(1 + std::snprintf(nullptr, 0, message, __VA_ARGS__)); \
        std::snprintf(buffer.data(), buffer.size(), message, __VA_ARGS__); \
        OutputDebugStringA(buffer.data()); \
      }
  #define PRINT_SIMPLE(message) OutputDebugStringA(message);
    #if 0
      FILE *errorFile = nullptr;
      void (*closeConsole)() = [](){};
      if (GetConsoleWindow() == nullptr)
      {
        if (AllocConsole())
        {
          errorFile = fopen("CONOUT$", "w");
          closeConsole = []() { FreeConsole(); };

          SetFocus(GetConsoleWindow());
        }
      }

      if (errorFile == nullptr)
      {
        closeConsole();
        return;
      }
    #endif
  #else
    #define PRINT_MESSAGE(message, ...) fprintf(stdout, message __VA_OPT__(,) __VA_ARGS__);
    #define PRINT_SIMPLE PRINT_MESSAGE
  #endif

    PRINT_MESSAGE("\nError in file: %s\nat line: %d\ninside function: %s\n", fileName, line, functionName);
    if (conditionString)
      PRINT_MESSAGE("Condition not met: %s\n", conditionString);

    if (!hasMoreArgs)
    {
    #if 0
      fclose(errorFile);
    #endif
      return;
    }

    va_list args;
    va_start(args, hasMoreArgs);

    const char *string = va_arg(args, const char *);

    va_list argsCopy;
    va_copy(argsCopy, args);
    std::vector<char> buffer(usize(std::vsnprintf(nullptr, 0, string, argsCopy) + 1));
    va_end(argsCopy);

    std::vsnprintf(buffer.data(), buffer.size(), string, args);
    PRINT_SIMPLE("\"");
    PRINT_SIMPLE("%s", buffer.data());
    PRINT_SIMPLE("\"\n\n");

    va_end(args);

  #if 0
    fclose(errorFile);
  #endif
  #undef PRINT_MESSAGE
  #undef PRINT_SIMPLE
  }
}
#endif

#include "sync_primitives.hpp"

#include <thread>

#ifdef COMPLEX_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ntdll.lib")

extern "C"
{
  typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
  NTSYSAPI NTSTATUS NTAPI NtDelayExecution(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
}
#endif

namespace utils
{
  void millisleep() noexcept
  {
  #ifdef COMPLEX_WINDOWS
    // taking advantage of undocumented ntdll function in combination with timeBegin/timeEndPeriod to reach ~1ms sleep
    // https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FThread%2FNtDelayExecution.html
    // after some experimentation i discovered that 600us most consistently yielded a real delay of ~1ms **on my machine**
    LONGLONG delay = 600 * -10;
    timeBeginPeriod(1);
    NtDelayExecution(0, (PLARGE_INTEGER)&delay);
    timeEndPeriod(1);
  #else
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  #endif
  }

  void millisleep(const clg::small_fn<bool()> &shouldWaitFn)
  {
  #ifdef COMPLEX_WINDOWS
    // initial check if we should even wait
    if (!shouldWaitFn())
      return;

    // taking advantage of undocumented ntdll function in combination with timeBegin/timeEndPeriod to reach ~1ms sleep
    // https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FThread%2FNtDelayExecution.html
    // after some experimentation i discovered that 600us most consistently yielded a real delay of ~1ms **on my machine**
    LONGLONG delay = 600 * -10;
    timeBeginPeriod(1);
    do
    {
      NtDelayExecution(0, (PLARGE_INTEGER)&delay);
    } while (shouldWaitFn());
    timeEndPeriod(1);
  #else
    while (shouldWaitFn())
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  #endif
  }

  static strict_inline auto getThreadId() noexcept -> usize
  {
    usize threadId{};
    auto threadId_ = std::this_thread::get_id();
    static_assert(sizeof(usize) >= sizeof(threadId_));
    std::memcpy(&threadId, &threadId_, sizeof(threadId_));
    return threadId;
  }

  void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept
  {
    bool state = expected;
    while (!atomic.compare_exchange_weak(state, !expected, std::memory_order_acq_rel, std::memory_order_relaxed))
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
        while (atomic.load(std::memory_order_relaxed) != expected) { pause(); }
        break;
      case WaitMechanism::Wait:
      case WaitMechanism::WaitNotify:
        atomic.wait(state, std::memory_order_relaxed);
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

  i32 lockAtomic(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism, const clg::small_fn<void()> &lambda) noexcept
  {
    i32 state, desired;
    auto threadId = getThreadId();
    if (isExclusive)
    {
      static constexpr i32 expected = 0;
      state = expected;
      desired = state - 1;
      while (!lock.lock.compare_exchange_weak(state, desired, std::memory_order_acq_rel, std::memory_order_acquire))
      {
        // protecting against spurious failures
        if (state == expected)
          continue;

        if (state <= expected)
        {          
          COMPLEX_ASSERT(lock.lastLockId.load(std::memory_order_relaxed) != threadId,
            "Guess who forgot to unlock this atomic");
        }

        lambda();

        if (mechanism == WaitMechanism::Spin || mechanism == WaitMechanism::SpinNotify)
          while (lock.lock.load(std::memory_order_relaxed) != expected) { pause(); }
        else
        {
          do
          {
            if (mechanism == WaitMechanism::Sleep || mechanism == WaitMechanism::SleepNotify)
              millisleep();
            else
              lock.lock.wait(state, std::memory_order_relaxed);
            state = lock.lock.load(std::memory_order_relaxed);
          } while (state != expected);
        }
      }

      lock.lastLockId.store(threadId, std::memory_order_relaxed) ;
    }
    else
    {
      state = lock.lock.load(std::memory_order_relaxed);
      bool isLambdaRun = false;

      while (true)
      {
        while (state < 0)
        {
          if (!isLambdaRun)
          {
            lambda();
            isLambdaRun = true;
          }

          switch (mechanism)
          {
          case WaitMechanism::Spin:
          case WaitMechanism::SpinNotify:
            while (lock.lock.load(std::memory_order_relaxed) < 0) { pause(); }
            break;
          case WaitMechanism::Wait:
          case WaitMechanism::WaitNotify:
            lock.lock.wait(state, std::memory_order_relaxed);
            break;
          case WaitMechanism::Sleep:
          case WaitMechanism::SleepNotify:
            millisleep([&lock](){ return lock.lock.load(std::memory_order_relaxed) < 0; });
            break;
          default:
            COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
            break;
          }

          state = lock.lock.load(std::memory_order_relaxed);
        }

        desired = state + 1;
        
        if (lock.lock.compare_exchange_weak(state, desired, std::memory_order_acq_rel, std::memory_order_relaxed))
        {
          lock.lastLockId.store({}, std::memory_order_relaxed);
          break;
        }
      }
    }

    return state;
  }

  void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept
  {
    atomic.store(expected, std::memory_order_release);
    if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
      atomic.notify_all();
  }

  void unlockAtomic(LockBlame<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept
  {
    if (wasExclusive)
    {
      [[maybe_unused]] auto value = atomic.lock.fetch_add(1, std::memory_order_release);
      COMPLEX_ASSERT(value == -1, "Current value is %d", value);
    }
    else
    {
      [[maybe_unused]] auto value = atomic.lock.fetch_sub(1, std::memory_order_release);
      COMPLEX_ASSERT(value > 0, "Current value is %d", value);
    }

    if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
      atomic.lock.notify_all();
  }

  ScopedLock::ScopedLock(ReentrantLock<bool> &reentrantLock, WaitMechanism mechanism, bool expected) noexcept :
    type_(ReentrantBoolEnum), mechanism_(mechanism), reentrantBool_{ &reentrantLock, false, expected }
  {
    auto threadId = getThreadId();
    reentrantBool_.wasLocked = threadId == reentrantLock.lastLockId.load(std::memory_order_relaxed);

    if (!reentrantBool_.wasLocked)
    {
      lockAtomic(reentrantLock.lock, mechanism, expected);
      reentrantLock.lastLockId.store(threadId, std::memory_order_relaxed);
    }
  }
}
