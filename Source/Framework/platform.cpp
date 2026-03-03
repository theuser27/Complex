
// Created: 2024-03-10 02:06:32

#include "utils.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include "stl_utils.hpp"
#include "sync_primitives.hpp"
#include "memory.hpp"
#include "Interface/LookAndFeel/gui_utils.hpp"
#include "Third Party/stb/stb_sprintf.h"

#ifdef COMPLEX_WINDOWS

  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #define NOMCX
  #define NOIME
  #define NOSERVICE
  #define NOCRYPT
  #define NOOPENFILE
  #define NOSCROLL
  #define NOSOUND
  #define NOCOMM
  #define NOKANJI
  #define NORPC
  #define NOPROXYSTUB
  #define NOIMAGE
  #define NOTAPE
  #define NONLS
  #define NOSHOWWINDOW
  #define NORASTEROPS
  #define NOSYSCOMMANDS
  #define NOMSG
  #define NOTEXTMETRIC
  #define NOWINOFFSETS
  #define NODEFERWINDOWPOS
  #define NOGDICAPMASKS
  #define NOVIRTUALKEYCODES
  #define NOWINMESSAGES
  #define NOWINSTYLES
  #define NOSYSMETRICS
  #include <windows.h>
  #include <timeapi.h>
  #pragma comment(lib, "winmm.lib")
  #pragma comment(lib, "ntdll.lib")

  extern "C"
  {
    typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
    NTSYSAPI NTSTATUS NTAPI NtDelayExecution(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
  }

#elif COMPLEX_LINUX

  #include <stdio.h>
  #include <time.h>
  #define _GNU_SOURCE
  #include <unistd.h>

#elif COMPLEX_MAC

  #include <stdio.h>
  // to get correct nanosecond values in timespec.tv_nsec and not microseconds
  // https://stackoverflow.com/a/53708448
  #define _POSIX_C_SOURCE 200809L
  #include <time.h>
  #include <sys/syscall.h>

#endif

#if COMPLEX_WINDOWS
  #define PRINT_MESSAGE(message, ...) { \
      usize size__ = 1 + ::stbsp_snprintf(nullptr, 0, message, __VA_ARGS__); \
      auto buffer__ = (char *)::utils::allocate(size__); \
      ::stbsp_snprintf(buffer__, (int)size__, message, __VA_ARGS__); \
      ::OutputDebugStringA(buffer__); \
      ::utils::deallocate(buffer__); \
    }
  #define PRINT_SIMPLE(message) ::OutputDebugStringA(message);
#else
  #define PRINT_MESSAGE(message, ...) ::fprintf(stdout, message __VA_OPT__(,) __VA_ARGS__);
  #define PRINT_SIMPLE(...) PRINT_MESSAGE("%s", __VA_ARGS__)
#endif

static void printVariadic(const char *format, va_list args)
{
  va_list argsCopy;
  va_copy(argsCopy, args);
  usize size = ::stbsp_vsnprintf(nullptr, 0, format, argsCopy) + 1;
  char *buffer = (char *)::utils::allocate(size);
  va_end(argsCopy);

  ::stbsp_vsnprintf(buffer, (int)size, format, args);
  PRINT_SIMPLE("\"");
  PRINT_SIMPLE(buffer);
  PRINT_SIMPLE("\"\n\n");

  ::utils::deallocate(buffer);
}

void common::complexLogMessage(const char *fileName,
  const char *functionName, int line, const char *format, ...)
{
  PRINT_MESSAGE("\nLog: %s, #%d, %s\n", fileName, line, functionName);

  va_list args;
  va_start(args, format);

  printVariadic(format, args);

  va_end(args);
}

void common::complexPrintAssertMessage(const char *conditionString, const char *fileName,
  const char *functionName, int line, int hasMoreArgs, ...)
{
  PRINT_MESSAGE("\nError: %s, #%d, %s\n", fileName, line, functionName);
  if (conditionString)
    PRINT_MESSAGE("Condition not met: %s\n", conditionString);
  PRINT_SIMPLE("\n");

  if (!hasMoreArgs)
    return;

  va_list args;
  va_start(args, hasMoreArgs);

  const char *format = va_arg(args, const char *);
  printVariadic(format, args);

  va_end(args);
}

#undef PRINT_MESSAGE
#undef PRINT_SIMPLE

#if COMPLEX_WINDOWS

extern "C"
{
  enum Monitor_DPI_Type
  {
    MDT_Effective_DPI = 0,
    MDT_Angular_DPI = 1,
    MDT_Raw_DPI = 2,
    MDT_Default = MDT_Effective_DPI
  };

  STDAPI GetDpiForMonitor(HMONITOR hmonitor,
    Monitor_DPI_Type dpiType, UINT *dpiX, UINT *dpiY);

#pragma comment(lib, "Shcore.lib")
}

namespace Interface
{
  MonitorInfo getCurrentMonitorInfo(void *nativeHandle)
  {
    MonitorInfo ret;
    ret.nativeHandle = MonitorFromWindow((HWND)nativeHandle, 2 /* MONITOR_DEFAULTTONEAREST */);

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    GetMonitorInfoA((HMONITOR)ret.nativeHandle, &info);

    ret.isPrimary = (info.dwFlags & 1 /* MONITORINFOF_PRIMARY */) != 0;
    ret.totalArea = { info.rcMonitor.left, info.rcMonitor.top,
      info.rcMonitor.right - info.rcMonitor.left,
      info.rcMonitor.bottom - info.rcMonitor.top };
    ret.totalArea = { info.rcWork.left, info.rcWork.top,
      info.rcWork.right - info.rcWork.left,
      info.rcWork.bottom - info.rcWork.top };

    auto dpi = 0.0;
    UINT dpiX = 0, dpiY = 0;
    if (SUCCEEDED(GetDpiForMonitor((HMONITOR)ret.nativeHandle, MDT_Default, &dpiX, &dpiY)))
      dpi = (dpiX + dpiY) / 2.0;

    ret.dpiScale = (float)dpi;
    return ret;
  }

  bool showNativeMessageBox(const char *title, const char *message, MessageBoxType type)
  {
    int iconFlag;

    switch (type)
    {
    case MessageBoxType::Info:
      iconFlag = MB_ICONINFORMATION;
      break;
    case MessageBoxType::Warning:
      iconFlag = MB_ICONWARNING;
      break;
    default:
    case MessageBoxType::Error:
      iconFlag = MB_ICONERROR;
      break;
    }

    int result = MessageBoxA(nullptr, message, title, MB_OKCANCEL | MB_SYSTEMMODAL | iconFlag);
    return result == IDOK;
  }
}

#elif COMPLEX_MACOS
#include <CoreFoundation/CoreFoundation.h>

namespace Interface
{
  bool showNativeMessageBox(const char *title, const char *message, MessageBoxType type)
  {
    CFOptionFlags cfAlertIcon;

    switch (type)
    {
    case MessageBoxType::Info:
      cfAlertIcon = kCFUserNotificationNoteAlertLevel;
      break;
    case MessageBoxType::Warning:
      cfAlertIcon = kCFUserNotificationCautionAlertLevel;
      break;
    default:
    case MessageBoxType::Error:
      cfAlertIcon = kCFUserNotificationStopAlertLevel;
      break;
    }

    CFStringRef cfTitle = CFStringCreateWithCString(kCFAllocatorDefault, title, kCFStringEncodingUTF8);
    CFStringRef cfMessage = CFStringCreateWithCString(kCFAllocatorDefault, message, kCFStringEncodingUTF8);

    CFOptionFlags result;

    CFUserNotificationDisplayAlert(0, cfAlertIcon, nullptr, nullptr, nullptr, cfTitle, cfMessage, CFSTR("OK"), CFSTR("Cancel"), nullptr, &result);

    CFRelease(cfTitle);
    CFRelease(cfMessage);

    return result == kCFUserNotificationDefaultResponse;
  }
}

#elif COMPLEX_LINUX

#endif


namespace utils
{
  // keeping struct layout the same so that utils::string_view can be passed the stb_printf routines safely
  static_assert(sizeof(::String_View) == sizeof(utils::string_view));
  static_assert(offsetof(::String_View, data) == offsetof(utils::string_view, data_));
  static_assert(offsetof(::String_View, size) == offsetof(utils::string_view, size_));

  byte *
  allocate(usize size, usize alignment, bool clean)
  {
    return utils::bumpArena::insert(globalArena, size, alignment, clean);
  }

  void deallocate(const void *memory)
  {
    return utils::bumpArena::remove(memory);
  }

  static constinit satomi::atomic<int> pageSize_ = 0;

  byte *
  reserveMemory(usize &size)
  {
    size = utils::roundUpToMultiple(size, (usize)pageSize_.load(satomi::memory_order_relaxed));
  #if COMPLEX_WINDOWS
    return (byte *)::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
  #else
    return (byte *)::mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  #endif
  }

  void commitMemory(void *memory, usize &size)
  {
    usize allocatedSize;
    void *begin;
    {
      auto pageSize = pageSize_.load(satomi::memory_order_relaxed);
      usize newStart = utils::roundUpToMultiple((usize)memory, (usize)pageSize);
      if ((newStart - (usize)memory) >= size)
      {
        size = newStart - (usize)memory;
        return;
      }

      usize newEnd = utils::roundUpToMultiple((usize)memory + size, (usize)pageSize);
      size = newEnd - (usize)memory;
      begin = (void *)newStart;
      allocatedSize = newEnd - newStart;
    }

    while (true)
    {
    #if COMPLEX_WINDOWS
      auto result = ::VirtualAlloc(begin, allocatedSize, MEM_COMMIT, PAGE_READWRITE);
      if (result != nullptr)
        return;
    #else
      auto result = ::mprotect(begin, allocatedSize, PROT_READ | PROT_WRITE);
      if (result == 0)
        return;
    #endif

      millisleep();
    }
  }

  void decommitMemory(void *memory, usize size)
  {
    {
      auto pageSize = pageSize_.load(satomi::memory_order_relaxed);
      usize newStart = utils::roundUpToMultiple((usize)memory, (usize)pageSize);
      if ((newStart - (usize)memory) >= size)
        return;

      usize newEnd = utils::roundUpToMultiple((usize)memory + size, (usize)pageSize);
      memory = (void *)newStart;
      size = newEnd - newStart;
    }

  #if COMPLEX_WINDOWS
    [[maybe_unused]] auto result = ::VirtualFree(memory, size, MEM_DECOMMIT);
    COMPLEX_ASSERT(result != 0);
  #else
    ::madvise(memory, size, MADV_DONTNEED);
    [[maybe_unused]] auto result = ::mprotect(memory, size, PROT_NONE);
    COMPLEX_ASSERT(result == 0);
  #endif
  }

  void releaseMemory(void *memory, [[maybe_unused]] usize size)
  {
  #if COMPLEX_WINDOWS
    [[maybe_unused]] auto result = ::VirtualFree(memory, 0, MEM_RELEASE);
    COMPLEX_ASSERT(result != 0);
  #else
    [[maybe_unused]] auto result = ::munmap(memory, size);
    COMPLEX_ASSERT(result == 0);
  #endif
  }

  ScopedNoDenormals::ScopedNoDenormals()
  {
  #if COMPLEX_X64
    u32 mask = 0x8040;
    flags_ = (u64)_mm_getcsr();
    _mm_setcsr((u32)flags_ | mask);
  #elif COMPLEX_ARM
    u64 fpsr;
    u64 mask = (1 << 24 /* FZ */);
    asm volatile("vmrs %0, fpscr"
      : "=r"(fpsr));
    flags_ = fpsr;
    asm volatile("vmsr fpscr, %0"
      :
      : "ri"(fpsr | mask));
  #endif
  }

  ScopedNoDenormals::~ScopedNoDenormals()
  {
  #if COMPLEX_X64
    _mm_setcsr((u32)flags_);
  #elif COMPLEX_ARM
    asm volatile("vmsr fpscr, %0"
      :
      : "ri"(flags_));
  #endif
  }

  Dylib::Dylib(const char *fullPath)
  {
  #if COMPLEX_WINDOWS
    handle = ::LoadLibraryA(fullPath);
    COMPLEX_ASSERT(handle, "Couldn't load library");
  #else

  #endif
  }

  Dylib::~Dylib()
  {
  #if COMPLEX_WINDOWS
    if (handle)
      ::FreeLibrary((::HMODULE)handle);
  #else

  #endif
  }

  static constinit u64 systemFrequency = 10'000'000;

  Timer::Timer() : correctionPeriod_{ systemFrequency } { }

  void Timer::setCorrectionPeriod(double seconds)
  {
    correctionPeriod_ = (u64)((double)systemFrequency * seconds);
    correctionStart_ = 0;
    correctionUs_ = 500;
    counter_ = 0;
  }

  void Timer::sleep(bool correctAutomatically)
  {
    COMPLEX_ASSERT(sleepUs_ != 0);

  #if COMPLEX_WINDOWS
    // negative multiplier to signalise relative value
    // 10 in order to input microsecond delay
    ::LARGE_INTEGER delay = { .QuadPart = ((LONGLONG)sleepUs_ - correctionUs_) * -10 };
    ::timeBeginPeriod(1);
    ::NtDelayExecution(FALSE, &delay);
    ::timeEndPeriod(1);
  #else
    ::timespec delay =
    { 
      .tv_sec = (::time_t)((sleepUs_ - correctionUs_) / 1'000'000), 
      .tv_nsec = (long)(sleepUs_ - correctionUs_) * 1000
    };
    ::clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, nullptr);
  #endif

    ++counter_;

    if (correctAutomatically)
      correctSleep();
  }

  void Timer::correctSleep()
  {
    u64 timestamp;
    {
    #if COMPLEX_WINDOWS
      ::LARGE_INTEGER largeInt;
      ::QueryPerformanceCounter(&largeInt);
      timestamp = (u64)largeInt.QuadPart;
    #else
      ::timespec time;
      ::clock_gettime(CLOCK_MONOTONIC_RAW, &time);
      timestamp = (u64)time.tv_nsec / 100 + (u64)time.tv_sec * 10'000'000;
    #endif
    }

    // set an actual value once timer is in use
    if (correctionStart_ == 0)
    {
      correctionStart_ = timestamp;
      return;
    }

    if ((timestamp - correctionStart_) < correctionPeriod_)
      return;

    double elapsedTime = (double)(timestamp - correctionStart_);
    double observedPeriod = (kMicro / (double)(counter_ * systemFrequency)) * elapsedTime;
    correctionUs_ += (i32)observedPeriod - sleepUs_;

    counter_ = 0;
    correctionStart_ = timestamp;
  }

  void millisleep() noexcept
  {
  #ifdef COMPLEX_WINDOWS
    // taking advantage of undocumented ntdll function in combination with timeBegin/timeEndPeriod to reach ~1ms sleep
    // https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FThread%2FNtDelayExecution.html
    // after some experimentation i discovered that 600us most consistently yielded a real delay of ~1ms **on my machine**
    ::LONGLONG delay = 600 * -10;
    ::timeBeginPeriod(1);
    ::NtDelayExecution(0, (PLARGE_INTEGER)&delay);
    ::timeEndPeriod(1);
  #else
    ::timespec delay = { .tv_sec = 0, .tv_nsec = 900 };
    ::clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, nullptr);
  #endif
  }

  void setHighResolutionClock(bool isHighResolution)
  {
    if (isHighResolution)
      ::timeBeginPeriod(1);
    else
      ::timeEndPeriod(1);
  }

  i32 
  lockAtomic(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
    const utils::smallFn<void()> &lambda) noexcept
  {
    i32 state, desired;
    if (isExclusive)
    {
      static constexpr i32 expected = 0;
      state = expected;
      desired = state - 1;

      while (!atomic.compare_exchange_weak(state, desired, satomi::memory_order_acq_rel))
      {
        // protecting against spurious failures
        if (state == expected)
          continue;

        lambda();

        if (mechanism == WaitMechanism::Spin || mechanism == WaitMechanism::SpinNotify)
          while (atomic.load(satomi::memory_order_relaxed) != expected) { COMPLEX_PAUSE(); }
        else
        {
          do
          {
            if (mechanism == WaitMechanism::Sleep || mechanism == WaitMechanism::SleepNotify)
              millisleep();
            else
              atomic.wait(state, satomi::memory_order_relaxed);
            state = atomic.load(satomi::memory_order_relaxed);
          } while (state != expected);
        }
      }
    }
    else
    {
      state = atomic.load(satomi::memory_order_relaxed);
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
            while (atomic.load(satomi::memory_order_relaxed) < 0) { COMPLEX_PAUSE(); }
            break;
          case WaitMechanism::Wait:
          case WaitMechanism::WaitNotify:
            atomic.wait(state, satomi::memory_order_relaxed);
            break;
          case WaitMechanism::Sleep:
          case WaitMechanism::SleepNotify:
            while (atomic.load(satomi::memory_order_relaxed) < 0)
              millisleep();
            break;
          default:
            COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
            break;
          }

          state = atomic.load(satomi::memory_order_relaxed);
        }

        desired = state + 1;
        
        if (atomic.compare_exchange_weak(state, desired, satomi::memory_order_acq_rel))
          break;        
      }
    }

    return state;
  }

  void unlockAtomic(satomi::atomic<i32> &atomic, bool wasExclusive, WaitMechanism mechanism) noexcept
  {
    if (wasExclusive)
    {
      [[maybe_unused]] auto value = atomic.fetch_add(1, satomi::memory_order_release);
      COMPLEX_ASSERT(value == -1, "Current value is %d", value);
    }
    else
    {
      [[maybe_unused]] auto value = atomic.fetch_sub(1, satomi::memory_order_release);
      COMPLEX_ASSERT(value > 0, "Current value is %d", value);
    }

    if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
      atomic.notify_all();
  }

  bool 
  thread::id::operator==(const id &other) const
  {
  #ifdef COMPLEX_WINDOWS
    return nativeId == other.nativeId;
  #else
    auto ret = nativeId == other.nativeId;
    // on linux this should just compare the values directly inside the function
    COMPLEX_ASSERT(::pthread_equal((::pthread_t)nativeId, (::pthread_t)other.nativeId) == ret);
    return ret;
  #endif
  }
  
  static thread::id
  getCurrentThreadId()
  {
  #if COMPLEX_WINDOWS
    return { (decltype(thread::id::nativeId))::GetCurrentThreadId() };
  //#elif COMPLEX_LINUX
  //  auto threadId_ = ::gettid();
  //#elif COMPLEX_MAC
  //  // https://elliotth.blogspot.com/2012/04/gettid-on-mac-os.html
  //  auto threadId_ = ::syscall(SYS_thread_selfid);
  #else
    return { (decltype(thread::id::nativeId))::pthread_self() };
  #endif
  }

  void thread::exit(int result)
  {
  #ifdef COMPLEX_WINDOWS
    ::ExitThread((DWORD)result);
  #else
    ::pthread_exit((void *)(intptr_t)result);
  #endif
  }

#ifdef COMPLEX_WINDOWS
  static DWORD WINAPI threadEntry(LPVOID argument)
#else
  static void *threadEntry(void *argument)
#endif
  {
    utils::dynFn<int()> function = COMPLEX_MOVE(*(utils::dynFn<int()> *)argument);
    utils::deallocate(argument);

    thread::currentId = getCurrentThreadId();

    int result = function();

  #ifdef COMPLEX_WINDOWS
    return (::DWORD)result;
  #else
    return (void *)(usize)result;
  #endif
  }

  void thread::createThread(utils::dynFn<int()> *procedure)
  {
  #ifdef COMPLEX_WINDOWS
    static_assert(sizeof(::HANDLE) == sizeof(handle_));
    handle_ = (decltype(handle_))::CreateThread(
      nullptr, 0, threadEntry, (LPVOID)procedure, 0, nullptr);
  #else
    static_assert(sizeof(::pthread_t) == sizeof(threadId.nativeId));
    if (pthread_create((::pthread_t *)&threadId.nativeId, nullptr, threadEntry, (void *)procedure) != 0)
      threadId = {};
  #endif

    if (!threadId)
    {
      utils::deallocate(procedure);
    }
  }

  thread::~thread()
  {
    if (threadId)
    {
      join();
      threadId = {};
    }
  }

  bool thread::join(int *exitCode)
  {
  #ifdef COMPLEX_WINDOWS
    ::DWORD dwRes;
    auto handle = (::HANDLE)handle_;

    if (::WaitForSingleObject(handle, INFINITE) == WAIT_FAILED)
      return false;
    if (exitCode != nullptr)
    {
      if (::GetExitCodeThread(handle, &dwRes) != 0)
        *exitCode = (int)dwRes;
      else
        return false;
    }
    ::CloseHandle(handle);
  #else
    void *result;
    auto handle = (::pthread_t)threadId.nativeId;
    if (::pthread_join(handle, &result) != 0)
      return false;
    if (exitCode != nullptr)
      *exitCode = (int)(usize)result;
  #endif
    return true;
  }

  bool thread::detach()
  {
  #ifdef COMPLEX_WINDOWS
    // https://stackoverflow.com/questions/12744324/how-to-detach-a-thread-on-windows-c#answer-12746081
    return ::CloseHandle((::HANDLE)handle_) != 0;
  #else
    return ::pthread_detach((::pthread_t)threadId.nativeId) == 0;
  #endif
  }

  void thread::yield()
  {
  #ifdef COMPLEX_WINDOWS
    ::Sleep(0);
  #else
    ::sched_yield();
  #endif
  }

  static constinit auto placeholderNode = arena::node{};


  // 1. set last added deallocation's previousFree to &placeholderNode to
  //    so it can't be potentially unlinked while we're adding
  // 2. set last added deallocation to our new resource
  // 3. link the old deallocation with the new one
  void arena::linkDeallocation(const void *parentResource, const void *childResource)
  {
    COMPLEX_ASSERT(parentResource);
    COMPLEX_ASSERT(childResource);

    // function assumes no other thread is trying to add the child resource to the same or different parent resource
    
    auto *parent = utils::launder((arena::node *)((byte *)parentResource - sizeof(arena::node)));
    auto *newChild = utils::launder((arena::node *)((byte *)childResource - sizeof(arena::node)));

    COMPLEX_ASSERT(newChild->previousFree.load(satomi::memory_order_relaxed) == nullptr &&
      newChild->nextFree.load(satomi::memory_order_relaxed) == nullptr);

    newChild->previousFree.store(parent, satomi::memory_order_relaxed);
    auto *oldChild = parent->childrenFree.load(satomi::memory_order_relaxed);
    outer_loop:
    {
      // loop tries to push (LIFO) the new child resource as a child 
      // and link the previous child (if it exists) with the new child

      // signal to other linkers we're intending to link
      // SYNCHRONISES WITH childrenFree.compare_exchange_weak in arenaUnlinkDeallocation
      if (!parent->childrenFree.compare_exchange_weak(oldChild, &placeholderNode, satomi::memory_order_acquire))
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        goto outer_loop;
      }

      newChild->nextFree.store(oldChild, satomi::memory_order_relaxed);
      if (!oldChild)
      {
        parent->childrenFree.store(newChild, satomi::memory_order_release);
        return;
      }

      arena::node *parentFromOldChild = oldChild->previousFree.load(satomi::memory_order_relaxed);
      while (true)
      {
        // see if any unlinkers are intending to unlink

        if (parentFromOldChild == &placeholderNode)
        {
          // oldChild is being unlinked, we must wait until that is done
          parent->childrenFree.store(oldChild, satomi::memory_order_relaxed);

          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          goto outer_loop;
        }

        COMPLEX_ASSERT(parentFromOldChild == parent);

        if (oldChild->previousFree.compare_exchange_weak(
          parentFromOldChild, newChild, satomi::memory_order_release))
          break;
      }

      // let other linkers continue
      parent->childrenFree.store(newChild, satomi::memory_order_release);
    }
  }

  static void arenaUnlinkDeallocation(arena::node *node, arena::node *newNode)
  {
    COMPLEX_HARD_ASSERT(node != &placeholderNode);
    COMPLEX_HARD_ASSERT(newNode != &placeholderNode);

    if (newNode)
    {
      newNode->previousFree.store(&placeholderNode, satomi::memory_order_relaxed);
      newNode->nextFree.store(&placeholderNode, satomi::memory_order_relaxed);
    }

    auto *next = node->nextFree.load(satomi::memory_order_relaxed);
    while (true)
    {
      if (!next)
        break;

      if (next == &placeholderNode)
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();

        next = node->nextFree.load(satomi::memory_order_relaxed);
        continue;
      }

      if (!node->nextFree.compare_exchange_weak(next, &placeholderNode, satomi::memory_order_acquire))
        continue;

      arena::node *nextPrevious = next->previousFree.load(satomi::memory_order_relaxed);

      while (true)
      {
        if (nextPrevious == &placeholderNode)
        {
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
              
          // next node is expected to give way to previous
          nextPrevious = next->previousFree.load(satomi::memory_order_relaxed);
          continue;
        }

        COMPLEX_ASSERT(nextPrevious == node);
        
        if (next->previousFree.compare_exchange_weak(nextPrevious,
          &placeholderNode, satomi::memory_order_release))
          break;
      }

      break;
    }

    arena::node *newNext = newNode ? newNode : next;
    auto *previous = node->previousFree.load(satomi::memory_order_relaxed);
    // node wasn't even in a linked list to begin with
    if (!previous)
    {
      COMPLEX_HARD_ASSERT(!next);
      return;
    }

    while (true)
    {
      if (previous == &placeholderNode)
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();

        previous = node->nextFree.load(satomi::memory_order_relaxed);
        continue;
      }

      if (!node->previousFree.compare_exchange_weak(previous, &placeholderNode, satomi::memory_order_acquire))
        continue;

      inner_loop:
      {
        // we don't know if the previous node is our parent or just another node in the linked list
        auto previousNext = previous->nextFree.load(satomi::memory_order_relaxed);
        auto previousChildren = previous->childrenFree.load(satomi::memory_order_relaxed);

        if (previousChildren == node)
        {
          // previous == parent

          // wait until a linker is backs off and lets us continue
          // SYNCHRONISES WITH childrenFree.compare_exchange_weak in arenaLinkDeallocation
          for (auto expected = node; 
            !previous->childrenFree.compare_exchange_weak(expected, newNext, satomi::memory_order_release);
            expected = node) { COMPLEX_PAUSE(); }
          break;
        }
        else if (previousNext == node)
        {
          // previous == child
          // all other children should back off if the previous is trying to get unlinked
          if (previous->nextFree.compare_exchange_strong(previousNext, newNext, satomi::memory_order_release))
            break;
        
          // the previous node is getting unlinked as well
          // we need to wait until that's done
          node->previousFree.store(previous, satomi::memory_order_relaxed);

          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();

          previous = node->previousFree.load(satomi::memory_order_relaxed);
        }
        else
        {
          // someone is linking or unlinking and we don't know whether we're a child or a sibling
          // wait a little until they are finished

          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          goto inner_loop;
        }
      }
    }

    arena::node *newPrevious = previous;
    if (newNode)
    {
      newPrevious = newNode;
      newNode->previousFree.store(previous, satomi::memory_order_release);
      newNode->nextFree.store(next, satomi::memory_order_relaxed);
    }

    if (next)
      next->previousFree.store(newPrevious, satomi::memory_order_release);
  }

  void arena::unlinkDeallocation(const void *childResource)
  {
    arenaUnlinkDeallocation(utils::launder((arena::node *)((byte *)childResource - sizeof(arena::node))), nullptr);
  }

  void arena::relinkDeallocation(const void *oldChildResource, const void *newChildResource)
  {
    arenaUnlinkDeallocation(utils::launder((arena::node *)((byte *)oldChildResource - sizeof(arena::node))),
      utils::launder((arena::node *)((byte *)newChildResource - sizeof(arena::node))));
  }

#define GET_READ_ACCESS(node)                                         \
  for (i16 expected = node->lock.load(satomi::memory_order_relaxed);;)\
  {                                                                   \
    if (expected == -1)                                               \
    {                                                                 \
      COMPLEX_PAUSE();                                                \
      expected = node->lock.load(satomi::memory_order_relaxed);       \
      continue;                                                       \
    }                                                                 \
                                                                      \
    if (node->lock.compare_exchange_weak(expected, expected + 1,      \
      satomi::memory_order_acquire))                                  \
      break;                                                          \
  }
#define GET_ARENA_NODE_FROM_END(name, lastNode) utils::arena *name; ::memcpy(&name, utils::launder((byte *)lastNode) + \
  sizeof(arena::node) + lastNode->size - sizeof(utils::arena *), sizeof(utils::arena *))

  static byte *
  arenaFindFreeSpace(arena::node *node, usize size, usize alignment, bool insertAtEnd, bool threadSafe);

  static byte *
  arenaInsertAtEnd(arena *arena, usize size, usize alignment, 
    bool searchForFreeSpace, bool unlockArenaAfterInsert, bool threadSafe)
  {
    arena::node *lastNode;
    byte *memory = nullptr;
    
    // after the last node we store a pointer to the arena to use to loop back
    usize adjustedSize = size + sizeof(utils::arena *);
    // no need to get read access lock for the arena node
    // because we assume that the arena will remain alive

    if (threadSafe)
    {
      lock_last_node:
      {
        // (re)load the last node and reserve for insertion
        lastNode = arena->data.lastNode.load(satomi::memory_order_relaxed);
      
        for (; lastNode == &placeholderNode || !arena->data.lastNode
          .compare_exchange_weak(lastNode, &placeholderNode, satomi::memory_order_acquire);)
        {
          COMPLEX_ASSERT(lastNode != nullptr);
          if (lastNode == &placeholderNode)
            COMPLEX_PAUSE();
        }

        if (i16 expected = lastNode->lock.load(satomi::memory_order_relaxed); expected == -1 ||
          !lastNode->lock.compare_exchange_strong(expected, expected + 1, satomi::memory_order_acquire))
        {
          arena->data.lastNode.store(lastNode, satomi::memory_order_relaxed);
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          goto lock_last_node;
        }

        if (bool expected = false; !lastNode->reservedForInsertion
          .compare_exchange_strong(expected, true, satomi::memory_order_relaxed))
        {
          lastNode->lock.fetch_sub(1, satomi::memory_order_relaxed);
          arena->data.lastNode.store(lastNode, satomi::memory_order_relaxed);

          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          goto lock_last_node;
        }
      }
    }
    else
      lastNode = arena->data.lastNode.load(satomi::memory_order_relaxed);

    while (true)
    {
      usize committedSize = arena->data.committedSize.load(satomi::memory_order_relaxed);
      byte *committedEnd = (byte *)arena + committedSize;
      // starting point for the buffer allocation
      usize startingPoint = utils::roundUpToMultiple((usize)lastNode +
        sizeof(arena::node) +                   // end of current last node block 
        lastNode->size +                        // size of last node allocation
        sizeof(arena::node),                    // end of the new last node block
        alignment);

      if ((startingPoint + adjustedSize) <= (usize)committedEnd)
      {
        // copy pointer to the beginning of the arena 
        memory = (byte *)startingPoint - sizeof(arena::node);
        ::memcpy(memory + sizeof(arena::node) + size, &arena, sizeof(utils::arena *));
        break;
      }

      if (searchForFreeSpace)
      {
        if (threadSafe)
        {
          lastNode->lock.fetch_sub(1, satomi::memory_order_relaxed);
          lastNode->reservedForInsertion.store(false, satomi::memory_order_relaxed);
          arena->data.lastNode.store(lastNode, satomi::memory_order_release);
        }

        if (auto *slot = arenaFindFreeSpace(&arena->firstNode, size, alignment, false, threadSafe))
          return slot;
        searchForFreeSpace = false;

        goto lock_last_node;
      }

      if (threadSafe)
      {
        if (committedSize == 0 || !arena->data.committedSize
          .compare_exchange_strong(committedSize, 0, satomi::memory_order_release))
        {
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();

          continue;
        }
      }

      // double or at least commit enough memory to fit this allocation
      usize commitSize = startingPoint + adjustedSize - (usize)committedEnd;
      commitSize = utils::max(committedSize, commitSize);

      utils::commitMemory(committedEnd, commitSize);
      committedSize = (usize)(committedEnd + commitSize - (byte *)arena);
      arena->data.committedSize.store(committedSize, satomi::memory_order_relaxed);

      if (((byte *)startingPoint + adjustedSize) > (byte *)arena + arena->data.reservedSize)
      {
        auto *nextArena = arena->data.nextArena.load(satomi::memory_order_acquire);

        if (!nextArena)
        {
          // calculate sizes based on current arena size and desired allocation size
          usize nextReservedSize = 4 * utils::max(size, arena->data.reservedSize);
          usize nextCommittedSize = 2 * utils::max(size, committedSize);

          switch ((AllocatorType)arena->firstNode.alignment)
          {
          //case AllocatorType::Arena:
          //case AllocatorType::ArenaNode:
          //  nextArena = utils::arena::createNested(nextReservedSize, arena::fromAllocation(arena));
          //  break;

          case AllocatorType::BumpArena:
            nextArena = utils::arena::createNested(bumpArena::fromAllocation(arena), nextReservedSize);
            break;

          case AllocatorType::General:
            nextArena = utils::arena::create(nextReservedSize, nextCommittedSize);
            break;
          }

          arena->data.nextArena.store(nextArena, satomi::memory_order_release);
        }

        if (threadSafe)
        {
          lastNode->lock.fetch_sub(1, satomi::memory_order_relaxed);
          lastNode->reservedForInsertion.store(false, satomi::memory_order_relaxed);
          arena->data.lastNode.store(lastNode, satomi::memory_order_release);
        }

        return utils::arena::insert(nextArena, size, alignment, false, threadSafe);
      }
    }

    auto distanceFromPrevious = (u32)(memory - (byte *)lastNode);
    // do not change, read comment below
    i8 lockValue = (threadSafe) ? -1 : 0;

    auto *newLastNode = new(memory) arena::node
    {
      .size = (u32)adjustedSize,
      .previous = distanceFromPrevious,
      .next = 0,
      .lock = lockValue,
      .alignment = (u8)alignment
    };
    lastNode->next = distanceFromPrevious;

    if (threadSafe && unlockArenaAfterInsert)
    {
      // this store might seem redundant but 
      // it is necessary for other threads to see the changes made by this one
      // since every thread MUST acquire read access lock before doing anything
      newLastNode->lock.store(0, satomi::memory_order_release);
      lastNode->reservedForInsertion.store(false, satomi::memory_order_relaxed);
      lastNode->lock.store(0, satomi::memory_order_release);
    }

    arena->data.lastNode.store(newLastNode, satomi::memory_order_release);

    return memory + sizeof(arena::node);
  }
  
  static byte *
  arenaFindFreeSpace(arena::node *node, usize size, usize alignment, bool insertAtEnd, bool threadSafe)
  {
    arena::node *nextNode = node;
    node = nullptr;
    byte *memory = nullptr;

    while (true)
    {
      if (threadSafe)
      {
        GET_READ_ACCESS(nextNode);

        if (node)
          node->lock.fetch_sub(1, satomi::memory_order_relaxed);
      }

      if (nextNode->next == 0)
      {
        if (!insertAtEnd)
          return nullptr;

        GET_ARENA_NODE_FROM_END(arena, nextNode);

        if (threadSafe)
          nextNode->lock.fetch_sub(1, satomi::memory_order_relaxed);

        return arenaInsertAtEnd(arena, size, alignment, false, true, threadSafe);
      }

      node = nextNode;
      nextNode = utils::launder((arena::node *)((byte *)nextNode + sizeof(arena::node) + nextNode->next));

      // starting point for the object, not the node
      auto startingPoint = utils::roundUpToMultiple(
        (usize)node + sizeof(arena::node) + node->size, alignment);

      if ((usize)nextNode < startingPoint + size)
        continue;

      if (threadSafe)
      {
        if (bool expected = false; !node->reservedForInsertion
          .compare_exchange_strong(expected, true, satomi::memory_order_relaxed))
          continue;

        // get exclusive access to previous and next nodes
        for (i16 expected = 1; !node->lock
          .compare_exchange_weak(expected, -1, satomi::memory_order_acquire);
          expected = 1) { COMPLEX_PAUSE(); }

        for (i16 expected = 0; !nextNode->lock
          .compare_exchange_weak(expected, -1, satomi::memory_order_acquire);
          expected = 0) { COMPLEX_PAUSE(); }
      }

      memory = (byte *)startingPoint - sizeof(arena::node);

      break;
    }

    auto distanceFromPrevious = (u32)(memory - (byte *)node);
    auto distanceFromNext = (u32)((byte *)nextNode - memory);
    (void)new(memory) arena::node
    {
      .size = (u32)size,
      .previous = distanceFromPrevious,
      .next = distanceFromNext,
      .alignment = (u8)alignment
    };

    node->next = distanceFromPrevious;
    nextNode->previous = distanceFromNext;

    if (threadSafe)
    {
      node->reservedForInsertion.store(false, satomi::memory_order_relaxed);
      node->lock.store(0, satomi::memory_order_release);
      nextNode->lock.store(0, satomi::memory_order_release);
    }

    return memory + sizeof(arena::node);
  }

  byte *
  arena::insert(arena::node *node, usize size, usize alignment, bool clean, bool threadSafe)
  {
    COMPLEX_ASSERT(node);
    COMPLEX_ASSERT(alignment > 0 && utils::isPowerOfTwo(alignment));

    alignment = utils::max(alignof(arena::node), alignment);
    size = utils::roundUpToMultiple(size, alignof(arena::node));

    byte *slot = (byte *)arenaFindFreeSpace(node, size, alignment, true, threadSafe);
    if (clean)
      ::zeroset(slot, size);

    return slot;
  }

  byte *
  arena::insert(arena *arena, usize size, usize alignment, bool clean, bool threadSafe)
  {
    COMPLEX_ASSERT(alignment > 0 && utils::isPowerOfTwo(alignment));
    alignment = utils::max(alignof(arena::node), alignment);
    size = utils::roundUpToMultiple(size, alignof(arena::node));

    byte *slot = (byte *)arenaInsertAtEnd(arena, size, alignment, true, true, threadSafe);
    if (clean)
      ::zeroset(slot, size);

    return slot;
  }

  void arena::removeNode(arena::node *node, bool threadSafe)
  {
    // detaching node from the linked frees
    arenaUnlinkDeallocation(node, nullptr);

    auto child = node->childrenFree.load(satomi::memory_order_relaxed);

    arena::node *previous;

    if (!threadSafe)
    {
      previous = utils::launder((arena::node *)((byte *)node - node->previous));
      if (node->next)
      {
        arena::node *next = utils::launder((arena::node *)((byte *)node + node->next));
        previous->next += node->next;
        next->previous += node->previous;
      }
      else
      {
        // this was the last node we need to set up the previous node 
        GET_ARENA_NODE_FROM_END(arena, node);
        // put arena pointer at the end
        ::memcpy((byte *)previous + previous->size, &arena, sizeof(arena));
        previous->size += sizeof(arena);
        previous->next = 0;
      }
    }
    else
    {
      outer_lock_previous:
      {
        GET_READ_ACCESS(node);

        // we're dealing with a whole arena, call respective function
        if (node->previous == 0)
        {
          arena::destroy((utils::arena *)node);
          return;
        }

        previous = utils::launder((arena::node *)((byte *)node - node->previous));

        // we always have a previous node, even if it's the master arena node
        for (i16 expected = 0; !previous->reservedForInsertion.load(satomi::memory_order_relaxed) &&
          !previous->lock.compare_exchange_weak(expected, -1, satomi::memory_order_acq_rel);
          expected = 0)
        {
          if (expected == -1)
          {
            node->lock.fetch_sub(1, satomi::memory_order_relaxed);
            COMPLEX_PAUSE();
            COMPLEX_PAUSE();
            COMPLEX_PAUSE();

            goto outer_lock_previous;
          }
        }
      }

      // making sure there is NO ONE accessing this node
      node->lock.fetch_sub(1, satomi::memory_order_relaxed);
      for (i16 expected = 0; !node->reservedForInsertion.load(satomi::memory_order_relaxed) &&
        !node->lock.compare_exchange_strong(expected, -1, satomi::memory_order_acq_rel);
        expected = 0)
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
      }
    
      if (node->next)
      {
        arena::node *next = utils::launder((arena::node *)((byte *)node + node->next));
        for (i16 expected = 0; !next->lock
          .compare_exchange_weak(expected, -1, satomi::memory_order_acq_rel);
          expected = 0)
        {
          if (expected == -1)
          {
            COMPLEX_PAUSE();
            COMPLEX_PAUSE();
            COMPLEX_PAUSE();
          }
        }

        previous->next += node->next;
        next->previous += node->previous;
        previous->lock.store(0, satomi::memory_order_release);
        next->lock.store(0, satomi::memory_order_release);
      }
      else
      {
        // this was the last node we need to set up the previous node 
        GET_ARENA_NODE_FROM_END(arena, node);

        // this is safe because i still hold the lock to the previous node
        for (auto *expected = node; !arena->data.lastNode
          .compare_exchange_weak(expected, previous, satomi::memory_order_release);
          expected = previous) { if (expected != previous) COMPLEX_PAUSE(); }

        // put arena pointer at the end
        ::memcpy((byte *)previous + previous->size, &arena, sizeof(arena));
        previous->size += sizeof(arena);
        previous->next = 0;
        previous->lock.store(0, satomi::memory_order_release);
      }
    }

    // freeing children
    while (child)
    {
      // it is assumed that linked frees are only accessible through their parents
      // and no other locations hold a pointer to these memory locations,
      // that means that we do not expect other threads to have access to the child nodes (let alone try to free them)
      child->previousFree.store(nullptr, satomi::memory_order_relaxed);
      auto childNextFree = child->nextFree.exchange(nullptr, satomi::memory_order_acq_rel);

      // potentially dangerous, switch to thread-safe
      removeNode(child);

      child = childNextFree;
    }
  }

  // if newSize <= oldSize, resizing is guaranteed to succeed
  byte *
  arena::resizeNode(arena::node *node, usize newSize, bool threadSafe)
  {
    if (threadSafe)
    {
      while (true)
      {
        GET_READ_ACCESS(node)

        if (bool expected = false; node->reservedForInsertion
          .compare_exchange_strong(expected, true, satomi::memory_order_relaxed))
          break;

        node->lock.fetch_sub(1, satomi::memory_order_relaxed);

        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
      }

      for (i16 expected = 1; !node->lock
        .compare_exchange_weak(expected, -1, satomi::memory_order_acquire);
        expected = 1) { COMPLEX_PAUSE(); }
    }

    byte *result = (byte *)node + sizeof(arena::node);
    if (newSize <= node->size || (node->next != 0 && newSize <= node->next))
      node->size = (u32)newSize;
    else if (node->next == 0)
    {
      // if we're the end node, we need to check how much committed memory the arena has
      newSize += sizeof(utils::arena *);

      GET_ARENA_NODE_FROM_END(arena, node);

      usize committedSize = arena->data.committedSize.load(satomi::memory_order_relaxed);
      if (threadSafe)
      {
        while (committedSize == 0 || !arena->data.committedSize
          .compare_exchange_strong(committedSize, 0, satomi::memory_order_relaxed))
        {
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
          COMPLEX_PAUSE();
        }
      }

      auto *committedEnd = (byte *)arena + committedSize;
      auto *commitEnd = (byte *)node + newSize;

      if (commitEnd > committedEnd)
      {
        // double or at least commit enough memory to fit this allocation
        usize commitSize = (usize)commitEnd - (usize)committedEnd;
        commitSize = (utils::max(committedSize, commitSize));

        if (arena->data.reservedSize < (committedSize + commitSize))
        {
          // desired size goes out of bounds of the reserved space, return nullptr
          commitEnd = nullptr;
          result = nullptr;
        }
        else
        {
          utils::commitMemory(committedEnd, commitSize);

          committedSize += commitSize;
          arena->data.committedSize.store(committedSize, satomi::memory_order_relaxed);
        }
      }

      if (commitEnd)
      {
        ::memcpy(commitEnd - sizeof(arena), &arena, sizeof(arena));
        node->size = (u32)newSize;
      }
    }
    else
      result = nullptr;

    if (threadSafe)
    {
      node->reservedForInsertion.store(false, satomi::memory_order_release);
      node->lock.store(0, satomi::memory_order_release);
    }

    return result;
  }

  void arena::destroy(arena *arena)
  {
    // it is assumed no one else has access to the arena 
    // or the underlying memory anymore, therefore **no contention**

    // do initial memory synchronisation and load next arena if it exists
    auto *nextArena = (utils::arena *)arena->firstNode.nextFree.load(satomi::memory_order_acquire);

    auto *node = &arena->firstNode;
    auto nextOffset = node->next;
    while (nextOffset)
    {
      node = utils::launder((arena::node *)((byte *)node + nextOffset));

      arenaUnlinkDeallocation(node, nullptr);

      auto child = node->childrenFree.load(satomi::memory_order_relaxed);

      // freeing children
      while (child)
      {
        // it is assumed that a linked free are only accessible through its parent (object)
        // and no other locations hold a pointer to this memory,
        // that means that we do not expect other threads to have access to the child nodes (let alone try to free them)
        child->previousFree.store(nullptr, satomi::memory_order_relaxed);
        auto childNextFree = child->nextFree.exchange(nullptr, satomi::memory_order_acq_rel);

        arena::removeNode(child);

        child = childNextFree;
      }

      nextOffset = node->next;
    }

    if (nextArena)
      arena::destroy(nextArena);

    // if arena is nested, we need to free the node
    switch ((AllocatorType)arena->firstNode.alignment)
    {
    //case AllocatorType::Arena:
    //case AllocatorType::ArenaNode:
    //  arena::remove(arena);
    //  break;

    case AllocatorType::BumpArena:
      bumpArena::remove(arena);
      break;

    case AllocatorType::General:
      releaseMemory(arena, arena->data.reservedSize);
      break;
    }
  }

#undef GET_READ_ACCESS
#undef GET_ARENA_NODE_FROM_END

  void atLoad()
  {
  #if COMPLEX_WINDOWS
    ::SYSTEM_INFO sysinfo{};
    ::GetSystemInfo(&sysinfo);
    pageSize_.store(sysinfo.dwPageSize, satomi::memory_order_relaxed);

    ::LARGE_INTEGER largeInt;
    ::QueryPerformanceFrequency(&largeInt);
    systemFrequency = (u64)largeInt.QuadPart;
  #else
    pageSize_.store(::sysconf(_SC_PAGE_SIZE), satomi::memory_order_relaxed);
  #endif

    utils::thread::currentId = getCurrentThreadId();
  }
  

  usize 
  bumpArena::getUsedSize(bumpArena *arena)
  {
    utils::ScopedLock g{ arena->lock, utils::WaitMechanism::Spin };
    // TODO:
    return usize();
  }

  byte *
  bumpArena::insert(bumpArena *arena, usize size, usize alignment, bool clean)
  {
    COMPLEX_HARD_ASSERT(arena);
    COMPLEX_HARD_ASSERT(utils::isPowerOfTwo(alignment));

    size = utils::roundUpToMultiple(size, sizeof(node));
    alignment = utils::max(alignment, alignof(node));

    COMPLEX_HARD_ASSERT(utils::roundUpToMultiple(sizeof(bumpArena) + size, alignment) <= usize(u32(-1)) + 1, 
      "Allocations larger than 4GB cannot fit inside this arena");

    utils::ScopedLock g{};
    if (arena->threadSafe)
      g = ScopedLock{ arena->lock, utils::WaitMechanism::Spin };

    byte *memory{};
    node *currentNode{}, *previousNode{};
    u32 nodeShift = arena->freeNodeStart;

    while (nodeShift)
    {
      previousNode = currentNode;
      currentNode = utils::launder((node *)((byte *)arena + nodeShift));

      // collapsing nearby free nodes
      for (auto *nextNode = utils::launder((node *)((byte *)arena + currentNode->next)); currentNode->next;
        nextNode = utils::launder((node *)((byte *)arena + currentNode->next)))
      {
        if ((byte *)currentNode + currentNode->size + sizeof(node) < (byte *)nextNode)
          break;
        currentNode->size = (u32)(((byte *)nextNode + nextNode->next) - (byte *)currentNode);
        currentNode->next = nextNode->next;
      }

      // align for object placement
      byte *test = (byte *)utils::roundUpToMultiple((usize)(currentNode) + sizeof(node), alignment);
      if (test + size <= (byte *)currentNode + currentNode->size)
      {
        memory = test;
        break;
      }

      nodeShift = currentNode->next;
    }

    
    if (!memory)
    {
      // try to commit more pages

      // incrementing to get actual sizes
      usize committedSize = (usize)arena->committedSize + 1;
      usize reservedSize = (usize)arena->reservedSize + 1;
      memory = (byte *)utils::roundUpToMultiple((usize)arena + committedSize + sizeof(node), alignment);

      // have we reached the end of our reserved space?
      if (committedSize != reservedSize)
      {
        // commit more pages based on desired allocation

        usize availableSize = reservedSize - committedSize;
        bool isLastFreeNodeAtEnd = currentNode && (byte *)currentNode + currentNode->size == (byte *)arena + committedSize;
        if (isLastFreeNodeAtEnd)
        {
          availableSize += currentNode->size;
          memory = (byte *)utils::roundUpToMultiple((usize)currentNode + sizeof(node), alignment);
        }

        usize commitSize = utils::min(reservedSize - committedSize, 
          (usize)((memory + size) - ((byte *)arena + committedSize)));

        utils::commitMemory((byte *)arena + committedSize, commitSize);
        arena->committedSize += (u32)commitSize;

        if (isLastFreeNodeAtEnd)
          currentNode->size += (u32)commitSize;
        else
        {
          previousNode = currentNode;
          currentNode = new((byte *)arena + committedSize) node{ .size = (u32)commitSize };

          if (previousNode)
            previousNode->next = (u32)committedSize;
          else
            arena->freeNodeStart = (u32)committedSize;
        }
      }

      // object is still too big to fit in the current arena, insert into the next arena
      if ((byte *)arena + reservedSize < memory + size)
      {
        // create next arena if it doesn't exist
        if (!arena->nextArena)
        {
          // calculate sizes based on current arena size and desired allocation size
          usize nextReservedSize = 4 * utils::max(size, reservedSize);
          usize nextCommittedSize = 2 * utils::max(size, committedSize);

          switch ((AllocatorType)arena->flags)
          {
          //case AllocatorType::Arena:
          //case AllocatorType::ArenaNode:
          //  arena->nextArena = createNested(nextReservedSize, arena::fromAllocation(arena));
          //  break;

          case AllocatorType::BumpArena:
            arena->nextArena = createNested(bumpArena::fromAllocation(arena), nextCommittedSize);
            break;

          case AllocatorType::General:
            arena->nextArena = create(nextReservedSize, nextCommittedSize);
            break;
          }
        }
      
        auto *nextArena = arena->nextArena;
        g.~ScopedLock();
        return insert(nextArena, size, alignment, clean);
      }
    }

    // if we can insert a new free node at the end of the range
    if (memory + size + sizeof(node) < (byte *)currentNode + currentNode->size)
    {
      if ((byte *)currentNode + 2 * sizeof(node) < memory)
      {
        currentNode->size = (u32)(memory - (byte *)currentNode - sizeof(node));
        previousNode = currentNode;
      }

      // shrink node and re-add it in the free list
      auto newSize = ((byte *)currentNode + currentNode->size) - (memory + size);
      auto newFreeNodeOffset = (u32)((memory + size) - (byte *)arena);

      // yes you can do this apparently
      ((previousNode) ? previousNode->next : arena->freeNodeStart) = newFreeNodeOffset;

      (void)new(memory + size) node{ .size = (u32)newSize, .next = currentNode->next };
    }
    else
    {
      if ((byte *)currentNode + sizeof(node) < memory)
        currentNode->size = (u32)(memory - (byte *)currentNode - sizeof(node));
      else
        ((previousNode) ? previousNode->next : arena->freeNodeStart) = currentNode->next;
    }

    byte *nodeAddress = memory - sizeof(bumpArena::node);
    (void)new(nodeAddress) bumpArena::node{ .size = (u32)size, .offsetToArena = (u32)(nodeAddress - (byte *)arena) };
    return memory;
  }

  void bumpArena::remove(const void *data)
  {
    auto *toRemove = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node))); 
    auto *arena = utils::launder((bumpArena *)((byte *)toRemove - toRemove->offsetToArena));

    utils::ScopedLock g{};
    if (arena->threadSafe)
      g = { arena->lock, utils::WaitMechanism::Spin };

    node *currentNode{}, *previousNode{};
    u32 nodeShift = arena->freeNodeStart;

    while (nodeShift)
    {
      previousNode = currentNode;
      currentNode = utils::launder((node *)((byte *)arena + nodeShift));

      if ((byte *)toRemove < (byte *)currentNode)
      {
        toRemove->next = (u32)((byte *)currentNode - (byte *)arena);
        ((previousNode) ? previousNode->next : arena->freeNodeStart) = toRemove->offsetToArena;

        return;
      }

      nodeShift = currentNode->next;
    }

    toRemove->next = 0;
    ((currentNode) ? currentNode->next : arena->freeNodeStart) = toRemove->offsetToArena;
  }

  void bumpArena::shrinkToFit(bumpArena *arena, bool shrinkToCommitted)
  {
    {
      utils::ScopedLock g{};
      if (arena->threadSafe)
        g = { arena->lock, utils::WaitMechanism::Spin };

      usize committedSize = (usize)arena->committedSize + 1;
      usize reservedSize = (usize)arena->reservedSize + 1;

      node *lastNode = (arena->freeNodeStart) ? utils::launder((node *)((byte *)arena + arena->freeNodeStart)) : nullptr;
      while (lastNode->next)
        lastNode = utils::launder((node *)((byte *)arena + lastNode->next));

      // shrink to the last allocation or to committed size
      usize end = committedSize;
      if (!shrinkToCommitted && lastNode && ((byte *)lastNode + lastNode->size) == (byte *)arena + committedSize)
        end = (usize)((byte *)lastNode - (byte *)arena);

      COMPLEX_ASSERT(end <= reservedSize);

      switch ((AllocatorType)arena->flags)
      {
      //case AllocatorType::Arena:
      //case AllocatorType::ArenaNode:
      //  (void)arena::resizeNode(arena::fromAllocation(arena), end);
      //  arena->reservedSize = (u32)(end - 1);
      //  arena->committedSize = (u32)(end - 1);
      //  break;
      
      case AllocatorType::BumpArena:
        break;

      case AllocatorType::General:
        // only decommitting because of releaseMemory requirements
        utils::decommitMemory((byte *)arena + end, reservedSize - end);
        arena->committedSize = (u32)(end - 1);
        break;
      }
    }

    if (arena->nextArena)
      shrinkToFit(arena->nextArena, shrinkToCommitted);
  }

  void bumpArena::clear(bumpArena *arena)
  {
    {
      utils::ScopedLock g{};
      if (arena->threadSafe)
        g = { arena->lock, utils::WaitMechanism::Spin };

      arena->freeNodeStart = (u32)sizeof(bumpArena);
      (void)new((byte *)arena + sizeof(bumpArena)) 
        node{ .size = (u32)(arena->committedSize - sizeof(bumpArena)) };
    }

    if (arena->nextArena)
      clear(arena->nextArena);
  }

  void bumpArena::destroy(bumpArena *arena)
  {
    // it is assumed no one else has access to the arena 
    // or the underlying memory anymore, therefore **no contention**

    auto *nextArena = arena->nextArena;

    // if arena is nested, we need to free the node
    switch ((AllocatorType)arena->flags)
    {
    //case AllocatorType::Arena:
    //case AllocatorType::ArenaNode:
    //  arena::remove(arena);
    //  break;

    case AllocatorType::BumpArena:
      bumpArena::remove(arena);
      break;

    case AllocatorType::General:
      releaseMemory(arena, arena->reservedSize);
      break;
    }      

    if (nextArena)
      destroy(nextArena);
  }

}

