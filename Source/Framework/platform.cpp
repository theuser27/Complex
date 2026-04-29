
// Created: 2024-03-10 02:06:32

#include "utils.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include "stl_utils.hpp"
#include "memory.hpp"
#include "simd_math.hpp"
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
      auto buffer__ = arranew(localScratch, char, size__); \
      ::stbsp_snprintf(buffer__, (int)size__, message, __VA_ARGS__); \
      ::OutputDebugStringA(buffer__); \
      ::utils::bumpArena::remove(buffer__); \
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
  char *buffer = arranew(localScratch, char, size);
  va_end(argsCopy);

  ::stbsp_vsnprintf(buffer, (int)size, format, args);
  //PRINT_SIMPLE("\"");
  PRINT_SIMPLE(buffer);
  //PRINT_SIMPLE("\"\n\n");

  ::utils::bumpArena::remove(buffer);
}

void common::complexLogMessage(const char *fileName,
  const char *functionName, int line, const char *format, ...)
{
  if (fileName && functionName)
    PRINT_MESSAGE("\nLog: %s, #%d, %s: ", fileName, line, functionName);

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
    if (SUCCEEDED(GetDpiForMonitor((HMONITOR)ret.nativeHandle, MDT_Effective_DPI, &dpiX, &dpiY)))
      dpi = (dpiX + dpiY) / 2.0;

    ret.dpiScale = (float)dpi / 96.0f;
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

  float sin(float arg) { return sin(simd_float{ arg })[0]; }
  float cos(float arg) { return cos(simd_float{ arg })[0]; }

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

  void shrinkWorkingSet()
  {
  #if COMPLEX_WINDOWS
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
  #else

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
    localScratch = utils::bumpArena::create(COMPLEX_MB(4), COMPLEX_KB(128));

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
    static_assert(sizeof(::DWORD) == sizeof(threadId.nativeId));
    if (!::CreateThread(nullptr, 0, threadEntry, (LPVOID)procedure, 0, (::DWORD *)&threadId.nativeId))
  #else
    static_assert(sizeof(::pthread_t) == sizeof(threadId.nativeId));
    if (pthread_create((::pthread_t *)&threadId.nativeId, nullptr, threadEntry, (void *)procedure) != 0)
  #endif
    {
      threadId = {};
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
    auto handle = ::OpenThread(THREAD_ALL_ACCESS, false, (::DWORD)threadId.nativeId);

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
    auto handle = ::OpenThread(THREAD_ALL_ACCESS, false, (::DWORD)threadId.nativeId);
    // https://stackoverflow.com/questions/12744324/how-to-detach-a-thread-on-windows-c#answer-12746081
    return ::CloseHandle(handle) != 0;
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
    if (arena->threadSafe)
      utils::ScopedLock g{ arena->lock, utils::WaitMechanism::Spin };
    // TODO:
    return usize();
  }

  static void insertNewFreeNode(bumpArena *arena, byte *newFreeNodeAdress, usize newFreeNodeSize)
  {
    bumpArena::node *currentNode{}, *previousNode{};
    u32 nodeShift = arena->freeNodeStart;
    u32 shiftToCurrent = (u32)(newFreeNodeAdress - (byte *)arena);

    while (nodeShift)
    {
      if (shiftToCurrent < nodeShift)
        break;

      currentNode = utils::launder((bumpArena::node *)((byte *)arena + nodeShift));
      nodeShift = currentNode->next;
      previousNode = currentNode;
    }

    ((previousNode) ? previousNode->next : arena->freeNodeStart) = shiftToCurrent;

    COMPLEX_ASSERT(nodeShift != (u32)(newFreeNodeAdress - (byte *)arena));
    (void)new(newFreeNodeAdress) bumpArena::node{ .size = (u32)newFreeNodeSize, .next = nodeShift };
  }

  byte *
  bumpArena::insert(bumpArena *arena, usize size, usize alignment, bool clean)
  {
    COMPLEX_HARD_ASSERT(arena);
    COMPLEX_HARD_ASSERT(utils::isPowerOfTwo(alignment));
 
    size = utils::roundUpToMultiple(size, sizeof(bumpArena::node));
    alignment = utils::max(alignment, alignof(bumpArena::node));

    COMPLEX_HARD_ASSERT(utils::roundUpToMultiple(sizeof(bumpArena) + size, alignment) <= usize(u32(-1)) + 1, 
      "Allocations larger than 4GB cannot fit inside this arena");

    utils::ScopedLock g{};
    if (arena->threadSafe)
      g = ScopedLock{ arena->lock, utils::WaitMechanism::Spin };
    
    byte *memory{};
    bumpArena::node *currentNode{}, *previousNode{};
    u32 nodeShift = arena->freeNodeStart;

    // try to find a free node to fit the allocation
    while (nodeShift)
    {
      previousNode = currentNode;
      currentNode = utils::launder((bumpArena::node *)((byte *)arena + nodeShift));

      // collapsing nearby free nodes
      for (; currentNode->next;)
      {
        auto *nextNode = utils::launder((bumpArena::node *)((byte *)arena + currentNode->next));
        if ((byte *)currentNode + currentNode->size < (byte *)nextNode)
          break;
        // free nodes form a contiguous block of memory, combine them
        currentNode->size += nextNode->size;
        currentNode->next = nextNode->next;
      }

      // align for object placement
      byte *test = (byte *)utils::roundUpToMultiple((usize)(currentNode) + sizeof(bumpArena::node), alignment);
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
      memory = (byte *)utils::roundUpToMultiple((usize)arena + committedSize + sizeof(bumpArena::node), alignment);

      // have we reached the end of our reserved space?
      if (committedSize != reservedSize)
      {
        // commit more pages based on desired allocation

        usize availableSize = reservedSize - committedSize;
        bool isLastFreeNodeAtEnd = currentNode && (byte *)currentNode + currentNode->size == (byte *)arena + committedSize;
        if (isLastFreeNodeAtEnd)
        {
          availableSize += currentNode->size;
          memory = (byte *)utils::roundUpToMultiple((usize)currentNode + sizeof(bumpArena::node), alignment);
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
          currentNode = new((byte *)arena + committedSize) bumpArena::node{ .size = (u32)commitSize };
          ((previousNode) ? previousNode->next : arena->freeNodeStart) = (u32)committedSize;
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
            arena->nextArena = bumpArena::createNested(bumpArena::fromAllocation(arena), nextCommittedSize);
            break;

          case AllocatorType::General:
            arena->nextArena = bumpArena::create(nextReservedSize, nextCommittedSize);
            break;
          }
        }
      
        auto *nextArena = arena->nextArena;
        g.~ScopedLock();
        return bumpArena::insert(nextArena, size, alignment, clean);
      }
    }

    byte *nodeAddress = memory - sizeof(bumpArena::node);

    // if we can insert a new free node at the end of the range
    if (memory + size + sizeof(bumpArena::node) < (byte *)currentNode + currentNode->size)
    {
      // add the free node at the end
      auto newSize = ((byte *)currentNode + currentNode->size) - (memory + size);
      auto newFreeNodeOffset = (u32)((memory + size) - (byte *)arena);

      COMPLEX_ASSERT(currentNode->next != (u32)(memory + size - (byte *)arena));
      (void)new(memory + size) bumpArena::node{ .size = (u32)newSize, .next = currentNode->next };

      // does the allocation have such high alignment that it leaves enough space for a free node?
      if ((byte *)currentNode + sizeof(bumpArena::node) <= nodeAddress)
      {
        currentNode->size = (u32)(nodeAddress - (byte *)currentNode);
        previousNode = currentNode;
      }

      ((previousNode) ? previousNode->next : arena->freeNodeStart) = newFreeNodeOffset;
    }
    else
    {
      // does the allocation have such high alignment that it leaves enough space for a free node?
      if ((byte *)currentNode + sizeof(bumpArena::node) <= nodeAddress)
        currentNode->size = (u32)(nodeAddress - (byte *)currentNode);
      else
      {
        // the current node will be used fully, link the previous node with the next node
        ((previousNode) ? previousNode->next : arena->freeNodeStart) = currentNode->next;
      }
    }

    (void)new(nodeAddress) bumpArena::node{ .size = (u32)(size + sizeof(bumpArena::node)),
      .offsetToArena = (u32)(nodeAddress - (byte *)arena) };

    if (clean)
      ::zeroset(memory, size);

  #if COMPLEX_DEBUG
    if (memoryLogger.contains(arena))
      logBlocks(arena);
  #endif

    return memory;
  }

  byte *
  bumpArena::resize(const void *data, usize newSize, bool cleanNewSpace)
  {
    auto *node = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node)));
    auto *arena = utils::launder((bumpArena *)((byte *)node - node->offsetToArena));

    newSize = utils::roundUpToMultiple(newSize, sizeof(bumpArena::node));

    if (newSize + sizeof(bumpArena::node) == node->size)
      return (byte *)data;
    else if (newSize + sizeof(bumpArena::node) < node->size)
    {
      auto fullSize = newSize + sizeof(bumpArena::node);
      if (arena->threadSafe)
        utils::ScopedLock g{ arena->lock, utils::WaitMechanism::Spin };

      insertNewFreeNode(arena, (byte *)node + fullSize, (usize)node->size - fullSize);
      node->size = (u32)fullSize;

      return (byte *)data;
    }
    else
    {
      auto *newAllocation = bumpArena::insert(arena, newSize, utils::getAlignment(data));
      usize copiedBytes = node->size - sizeof(bumpArena::node);
      valcpy(newAllocation, (byte *)data, copiedBytes);
      if (cleanNewSpace)
        zeroset(newAllocation + copiedBytes, newSize - copiedBytes);
      bumpArena::remove(data);

      return newAllocation;
    }
  }

  void bumpArena::remove(const void *data)
  {
    auto *toRemove = utils::launder((bumpArena::node *)((byte *)data - sizeof(bumpArena::node))); 
    auto *arena = utils::launder((bumpArena *)((byte *)toRemove - toRemove->offsetToArena));

    utils::ScopedLock g{};
    if (arena->threadSafe)
      g = { arena->lock, utils::WaitMechanism::Spin };

    insertNewFreeNode(arena, (byte *)toRemove, toRemove->size);

  #if COMPLEX_DEBUG
    if (memoryLogger.contains(arena))
      logBlocks(arena);
  #endif
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

  #if COMPLEX_DEBUG
    memoryLogger.erase(arena);
  #endif

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

  void bumpArena::logBlocks(bumpArena *arena)
  {
    bumpArena::node *currentNode{};
    usize nodeShift = arena->freeNodeStart;

    usize i = 0;
    utils::string string{ globalArena, 128 };
    string.appendFormat("bumpArena @ %p\n", arena);

    // try to find a free node to fit the allocation
    while (nodeShift)
    {
      currentNode = utils::launder((bumpArena::node *)((byte *)arena + nodeShift));
      nodeShift = currentNode->next;

      string.appendFormat("free list [%zu]{ .address = %zu, .size = %zu, .next = %zu }\n",
        i, (usize)((byte *)currentNode - (byte *)arena), (usize)currentNode->size, nodeShift);

      ++i;
    }

    if (string.size())
      COMPLEX_LOG("%v", utils::string_view{ string });
  }

  void MemoryLogger::add(void *pointer)
  {
    utils::ScopedLock g{ writeLock, true, utils::WaitMechanism::Spin };
    memoryLogger.toLog.emplaceBack(pointer);
  }

  void MemoryLogger::erase(void *pointer)
  {
    utils::ScopedLock g{ writeLock, false, utils::WaitMechanism::Spin };
    memoryLogger.toLog.erase(pointer);
  }

  bool
  MemoryLogger::contains(void *pointer) const
  {
    utils::ScopedLock g{ writeLock, false, utils::WaitMechanism::Spin };
    auto iter = utils::find_if(memoryLogger.toLog, [pointer](const auto &item) { return item == pointer; });
    return iter != memoryLogger.toLog.end();
  }
}
