
// Created: 2024-03-10 02:06:32

#include "utils.hpp"

// interferes with things defined in Appkit's NSWindow.h
#undef defer

#include <stdarg.h> // va_list, va_arg, va_start, va_copy, va_end
#include <stddef.h> // offsetof

#include "stl_utils.hpp"
#include "simd_math.hpp"
#include "Interface/gui_utils.hpp"
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
  #include <sys/syscall.h>
  // to get the actual MAP_ANONYMOUS definition and also get all clock flag definitions (i.e. CLOCK_MONOTONIC_RAW)
  // https://catfox.life/2015/09/04/the-joys-of-unix-programming-map_anonymous/
  #define _DARWIN_C_SOURCE
  #include <sys/mman.h>
  #include <dlfcn.h>
  #include <time.h>
  #include <pthread.h>
  #include <unistd.h>

  #include <CoreFoundation/CoreFoundation.h>
  #include <AppKit/AppKit.h>

#endif

#if COMPLEX_WINDOWS
  #define PRINT_SIMPLE(message) OutputDebugStringA(message)
#else
  #define PRINT_SIMPLE(message) fputs(message, stdout)
#endif

#define PRINT_MESSAGE(message, ...) do { \
    usize size__ = 1 + stbsp_snprintf(nullptr, 0, message, __VA_ARGS__); \
    auto buffer__ = arranew(globalPool, char, size__); \
    stbsp_snprintf(buffer__, (int)size__, message, __VA_ARGS__); \
    PRINT_SIMPLE(buffer__); \
    utils::Pool::remove(buffer__); \
  } while(false)

int
printVariadic(const char *format, va_list args)
{
  va_list argsCopy;
  va_copy(argsCopy, args);
  usize size = stbsp_vsnprintf(nullptr, 0, format, argsCopy) + 1;
  char *buffer = arranew(globalPool, char, size);
  va_end(argsCopy);

  stbsp_vsnprintf(buffer, (int)size, format, args);
  //PRINT_SIMPLE("\"");
  PRINT_SIMPLE(buffer);
  //PRINT_SIMPLE("\"\n\n");

  utils::Pool::remove(buffer);

  return (int)size;
}

void complexLogMessage(const char *fileName,
  const char *functionName, int line, const char *format, ...)
{
  if (fileName && functionName)
    PRINT_MESSAGE("\nLog: %s, #%d, %s: ", fileName, line, functionName);

  va_list args;
  va_start(args, format);

  printVariadic(format, args);

  va_end(args);
}

void complexPrintAssertMessage(const char *conditionString, const char *fileName,
  const char *functionName, int line, int hasMoreArgs, ...)
{
  char errorBuffer[256];
  stbsp_snprintf(errorBuffer, (int)countof(errorBuffer), "\nError: %s, #%d, %s", fileName, line, functionName);
  PRINT_SIMPLE(errorBuffer);
  if (conditionString)
  {
    PRINT_SIMPLE("\nCondition not met: ");
    PRINT_SIMPLE(conditionString);
  }
  PRINT_SIMPLE("\n\n");

  // errors with further messages will require allocations,
  // so don't provide any if inside allocator code

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

MonitorInfo
getCurrentMonitorInfo(void *nativeHandle)
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

#elif COMPLEX_MAC

MonitorInfo
getCurrentMonitorInfo(void *nativeHandle)
{
  MonitorInfo ret{};

  NSView *view = (__bridge NSView *)nativeHandle;
  NSScreen *screen = view.window && view.window.screen ? view.window.screen : NSScreen.mainScreen;

  ret.nativeHandle = (__bridge void *)screen;

  // if it's anything other than (0,0) we have a big problem
  COMPLEX_ASSERT(screen.frame.origin.x == 0);
  COMPLEX_ASSERT(screen.frame.origin.y == 0);
  ret.totalArea = { .w = (i32)screen.frame.size.width, .h = (i32)screen.frame.size.height };

  ret.workArea = {
      (i32)screen.visibleFrame.origin.x,
      (i32)screen.visibleFrame.origin.y,
      (i32)screen.visibleFrame.size.width,
      (i32)screen.visibleFrame.size.height
  };
  // macOS's origin is in the bottom-left, we need to fix it to top-left
  ret.workArea.y = ret.totalArea.h - (ret.workArea.y + ret.workArea.h);
  ret.isPrimary = (screen == NSScreen.screens.firstObject);
  ret.dpiScale = (float)screen.backingScaleFactor;

  return ret;
}

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

#elif COMPLEX_LINUX

#endif


namespace utils
{
  // keeping struct layout the same so that utils::string_view can be passed the stb_printf routines safely
  static_assert(sizeof(String_View) == sizeof(utils::string_view));
  static_assert(offsetof(String_View, data) == offsetof(utils::string_view, data_));
  static_assert(offsetof(String_View, size) == offsetof(utils::string_view, size_));

  float sin(float arg) { return sin(f4{ arg })[0]; }
  float cos(float arg) { return cos(f4{ arg })[0]; }

  byte *
  allocate(usize size, usize alignment, bool clean)
  {
    return utils::Pool::insert(globalPool, size, alignment, clean);
  }

  void deallocate(const void *memory)
  {
    return utils::Pool::remove(memory);
  }

  static constinit usize pageSize = 0;

  byte *
  reserveMemory(usize &size)
  {
    size = utils::roundUpToMultiple(size, pageSize);
  #if COMPLEX_WINDOWS
    return (byte *)VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
  #else
    return (byte *)mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  #endif
  }

  void commitMemory(void *memory, usize &size)
  {
    usize allocatedSize;
    void *begin;
    {
      usize newStart = utils::roundUpToMultiple((usize)memory, pageSize);
      if ((newStart - (usize)memory) >= size)
      {
        size = newStart - (usize)memory;
        return;
      }

      usize newEnd = utils::roundUpToMultiple((usize)memory + size, pageSize);
      size = newEnd - (usize)memory;
      begin = (void *)newStart;
      allocatedSize = newEnd - newStart;
    }

    while (true)
    {
    #if COMPLEX_WINDOWS
      auto result = VirtualAlloc(begin, allocatedSize, MEM_COMMIT, PAGE_READWRITE);
      if (result != nullptr)
        return;
    #else
      auto result = mprotect(begin, allocatedSize, PROT_READ | PROT_WRITE);
      if (result == 0)
        return;
    #endif

      millisleep();
    }
  }

  void decommitMemory(void *memory, usize size)
  {
    {
      usize newStart = utils::roundUpToMultiple((usize)memory, pageSize);
      if ((newStart - (usize)memory) >= size)
        return;

      usize newEnd = utils::roundUpToMultiple((usize)memory + size, pageSize);
      memory = (void *)newStart;
      size = newEnd - newStart;
    }

  #if COMPLEX_WINDOWS
    [[maybe_unused]] auto result = VirtualFree(memory, size, MEM_DECOMMIT);
    COMPLEX_ASSERT(result != 0);
  #else
    madvise(memory, size, MADV_DONTNEED);
    [[maybe_unused]] auto result = mprotect(memory, size, PROT_NONE);
    COMPLEX_ASSERT(result == 0);
  #endif
  }

  void releaseMemory(void *memory, [[maybe_unused]] usize size)
  {
  #if COMPLEX_WINDOWS
    [[maybe_unused]] auto result = VirtualFree(memory, 0, MEM_RELEASE);
    COMPLEX_ASSERT(result != 0);
  #else
    [[maybe_unused]] auto result = munmap(memory, size);
    COMPLEX_ASSERT(result == 0);
  #endif
  }

  // unlike the global allocation functions
  // you can set the pool for these with `getSettableMallocPool() = pool;`
  extern "C" void *arena_malloc(size_t size)
  {
    return utils::Pool::insert(getSettableMallocPool(), size, alignof(void *));
  }
  extern "C" void *arena_calloc(size_t num, size_t size)
  {
    return utils::Pool::insert(getSettableMallocPool(), num * size, alignof(void *), true);
  }
  extern "C" void *arena_realloc(void *pointer, size_t newSize)
  {
    if (pointer)
      return utils::Pool::resize(pointer, newSize);
    else
      return arena_malloc(newSize);
  }
  extern "C" void arena_free(void *pointer)
  {
    if (pointer)
      utils::Pool::remove(pointer);
  }

  extern "C" void *global_malloc(size_t size)
  {
    return utils::Pool::insert(globalPool, size, alignof(void *));
  }
  extern "C" void *global_calloc(size_t num, size_t size)
  {
    return utils::Pool::insert(globalPool, num * size, alignof(void *), true);
  }
  extern "C" void *global_realloc(void *pointer, size_t newSize)
  {
    if (pointer)
      return utils::Pool::resize(pointer, newSize);
    else
      return global_malloc(newSize);
  }
  extern "C" void global_free(void *pointer)
  {
    if (pointer)
      utils::Pool::remove(pointer);
  }

  ScopedNoDenormals::ScopedNoDenormals()
  {
  #if COMPLEX_X64
    u32 mask = 0x8040;
    flags_ = (u64)_mm_getcsr();
    _mm_setcsr((u32)flags_ | mask);
  #elif COMPLEX_ARM
    u64 fpcr;
    u64 mask = (1 << 24 /* FZ */);
    __asm__ volatile("mrs %0, fpcr"
      : "=r" (fpcr));
    flags_ = fpcr;
    __asm__ volatile("msr fpcr, %0"
      :
      : "r" (fpcr | mask));
  #endif
  }

  ScopedNoDenormals::~ScopedNoDenormals()
  {
  #if COMPLEX_X64
    _mm_setcsr((u32)flags_);
  #elif COMPLEX_ARM
    __asm__ volatile("msr fpcr, %0"
      :
      : "r" (flags_));
  #endif
  }

  Dylib::Dylib(const char *fullPath) : path{ fullPath }
  {
  #if COMPLEX_WINDOWS
    // spinning when loading a library
    // since it might not have finished being built
    for (usize i = 0; i < 100; ++i) // up to ~10 seconds
    {
      handle = LoadLibraryA(fullPath);
      if (handle)
        break;

      if (GetLastError() != ERROR_SHARING_VIOLATION)
        break;

      Sleep(100);
    }

    COMPLEX_ASSERT(handle, "Couldn't load library");
  #else
    handle = dlopen(fullPath, RTLD_NOW);
  #endif
  }

  Dylib::~Dylib()
  {
  #if COMPLEX_WINDOWS
    if (handle)
      FreeLibrary((HMODULE)handle);
  #else
    if (handle)
      dlclose(handle);
  #endif
  }

  void *
  Dylib::getSymbol(utils::string_view decoratedName) const
  {
  #if COMPLEX_WINDOWS
    if (handle)
      return GetProcAddress((HMODULE)handle, decoratedName.data());
  #else
    if (handle)
      return dlsym(handle, decoratedName.data());
  #endif

    return nullptr;
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
    LARGE_INTEGER delay = { .QuadPart = ((LONGLONG)sleepUs_ - correctionUs_) * -10 };
    timeBeginPeriod(1);
    NtDelayExecution(FALSE, &delay);
    timeEndPeriod(1);
  #elif COMPLEX_MAC
    timespec delay =
    {
      .tv_sec = (time_t)((sleepUs_ - correctionUs_) / 1'000'000),
      .tv_nsec = (long)(sleepUs_ - correctionUs_) * 1000
    };
    nanosleep(&delay, nullptr);
  #else
    timespec delay =
    {
      .tv_sec = (time_t)((sleepUs_ - correctionUs_) / 1'000'000),
      .tv_nsec = (long)(sleepUs_ - correctionUs_) * 1000
    };
    clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, nullptr);
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
      LARGE_INTEGER largeInt;
      QueryPerformanceCounter(&largeInt);
      timestamp = (u64)largeInt.QuadPart;
    #else
      timespec time;
      clock_gettime(CLOCK_MONOTONIC_RAW, &time);
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

  u64 
  getTimestamp()
  {
  #if COMPLEX_MSVC

    #if COMPLEX_X64
      return __rdtsc();
    #elif COMPLEX_ARM
      #error Not Implemented
    #endif

  #else

    #if COMPLEX_X64
      u32 hi, lo;
      __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
      return ((u64)lo) | (((u64)hi) << 32);
    #elif COMPLEX_ARM
      u64 cntvct;
      __asm__ __volatile__("mrs %0, cntvct_el0; " : "=r"(cntvct) :: "memory");
      return cntvct;
    #endif

  #endif
  }

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
  #elif COMPLEX_MAC
    timespec delay = { .tv_sec = 0, .tv_nsec = 900 };
    nanosleep(&delay, nullptr);
  #else
    timespec delay = { .tv_sec = 0, .tv_nsec = 900 };
    clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, nullptr);
  #endif
  }

  void setHighResolutionClock(bool isHighResolution)
  {
  #if COMPLEX_WINDOWS
    if (isHighResolution)
      timeBeginPeriod(1);
    else
      timeEndPeriod(1);
  #else
    (void)isHighResolution;
  #endif
  }

  i32
  lockAtomic(satomi::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism) noexcept
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

        while ((state = atomic.load(satomi::memory_order_relaxed)) != expected)
        {
          if (mechanism == WaitMechanism::Spin || mechanism == WaitMechanism::SpinNotify)
            COMPLEX_PAUSE();
          else if (mechanism == WaitMechanism::Sleep || mechanism == WaitMechanism::SleepNotify)
            millisleep();
          else if (mechanism == WaitMechanism::Sleep || mechanism == WaitMechanism::SleepNotify)
            atomic.wait(state, satomi::memory_order_relaxed);
          else
            COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
        }
      }
    }
    else
    {
      state = atomic.load(satomi::memory_order_relaxed);

      while (true)
      {
        while (state < 0)
        {
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

  static void destroyTlsIndex();

  static constinit bool gfreeTlsIndex = true;
#if COMPLEX_WINDOWS
  constinit DWORD gDwTlsIndex = TLS_OUT_OF_INDEXES;
  extern "C" __declspec(dllexport) void
  initialiseHotreloadDylib(DWORD tlsIndex, utils::Pool *global)
#else
  constinit pthread_key_t gTlsKey{};
  extern "C" __attribute__((visibility("default"))) void
  initialiseHotreloadDylib(pthread_key_t tlsKey, utils::Pool *global)
#endif
  {
    destroyTlsIndex();
    if (globalPool)
      utils::Pool::destroy(globalPool);
    globalPool = global;

    gfreeTlsIndex = false;
  #if COMPLEX_WINDOWS
    gDwTlsIndex = tlsIndex;
  #else
    gTlsKey = tlsKey;
  #endif
  }

  void initialiseHotreload(utils::Dylib *hotreload)
  {
    auto *fn = (decltype(initialiseHotreloadDylib) *)hotreload->getSymbol("initialiseHotreloadDylib");
  #if COMPLEX_WINDOWS
    fn(gDwTlsIndex, globalPool);
  #else
    fn(gTlsKey, globalPool);
  #endif
  }

  void *
  getTls()
  {
  #if COMPLEX_WINDOWS
    if (gDwTlsIndex != TLS_OUT_OF_INDEXES)
      return TlsGetValue(gDwTlsIndex);
  #else
    if (gTlsKey)
      return pthread_getspecific(gTlsKey);
  #endif
    return nullptr;
  }

  void setTls(void *context)
  {
  #if COMPLEX_WINDOWS
    if (gDwTlsIndex != TLS_OUT_OF_INDEXES)
      TlsSetValue(gDwTlsIndex, context);
  #else
    if (gTlsKey)
      pthread_setspecific(gTlsKey, context);
  #endif
  }

  bool
  thread::id::operator==(const id &other) const
  {
  #ifdef COMPLEX_WINDOWS
    return nativeId == other.nativeId;
  #else
    auto ret = nativeId == other.nativeId;
    // on linux this should just compare the values directly inside the function
    COMPLEX_ASSERT(pthread_equal((pthread_t)nativeId, (pthread_t)other.nativeId) == ret);
    return ret;
  #endif
  }

  static thread::id
  getCurrentThreadId()
  {
  #if COMPLEX_WINDOWS
    return { (decltype(thread::id::nativeId))GetCurrentThreadId() };
  //#elif COMPLEX_LINUX
  //  auto threadId_ = gettid();
  //#elif COMPLEX_MAC
  //  // https://elliotth.blogspot.com/2012/04/gettid-on-mac-os.html
  //  auto threadId_ = syscall(SYS_thread_selfid);
  #else
    return { (decltype(thread::id::nativeId))pthread_self() };
  #endif
  }

  thread::id
  thread::getCurrentId()
  {
    return getCurrentThreadId();
  }

  void thread::exit(int result)
  {
  #ifdef COMPLEX_WINDOWS
    ExitThread((DWORD)result);
  #else
    pthread_exit((void *)(intptr_t)result);
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

    (void)thread::getCurrentId();
    setTls(createTlsContext());

    int result = function();

    if (auto *tls = getTls())
      destroyTlsContext(tls);

  #ifdef COMPLEX_WINDOWS
    return (DWORD)result;
  #else
    return (void *)(usize)result;
  #endif
  }

  void thread::createThread(utils::dynFn<int()> *procedure)
  {
  #ifdef COMPLEX_WINDOWS
    static_assert(sizeof(DWORD) == sizeof(threadId.nativeId));
    if (!CreateThread(nullptr, 0, threadEntry, (LPVOID)procedure, 0, (DWORD *)&threadId.nativeId))
  #else
    static_assert(sizeof(pthread_t) == sizeof(threadId.nativeId));
    if (pthread_create((pthread_t *)&threadId.nativeId, nullptr, threadEntry, (void *)procedure) != 0)
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
    DWORD dwRes;
    auto handle = OpenThread(THREAD_ALL_ACCESS, false, (DWORD)threadId.nativeId);

    if (WaitForSingleObject(handle, INFINITE) == WAIT_FAILED)
      return false;
    if (exitCode != nullptr)
    {
      if (GetExitCodeThread(handle, &dwRes) != 0)
        *exitCode = (int)dwRes;
      else
        return false;
    }
    CloseHandle(handle);
  #else
    void *result;
    auto handle = (pthread_t)threadId.nativeId;
    if (pthread_join(handle, &result) != 0)
      return false;
    if (exitCode != nullptr)
      *exitCode = (int)(usize)result;
  #endif
    return true;
  }

  bool thread::detach()
  {
  #ifdef COMPLEX_WINDOWS
    auto handle = OpenThread(THREAD_ALL_ACCESS, false, (DWORD)threadId.nativeId);
    // https://stackoverflow.com/questions/12744324/how-to-detach-a-thread-on-windows-c#answer-12746081
    return CloseHandle(handle) != 0;
  #else
    return pthread_detach((pthread_t)threadId.nativeId) == 0;
  #endif
  }

  void thread::yield()
  {
  #ifdef COMPLEX_WINDOWS
    Sleep(0);
  #else
    sched_yield();
  #endif
  }


  void atLoad()
  {
  #if COMPLEX_WINDOWS
    SYSTEM_INFO sysinfo{};
    GetSystemInfo(&sysinfo);
    pageSize = sysinfo.dwPageSize;

    LARGE_INTEGER largeInt;
    QueryPerformanceFrequency(&largeInt);
    systemFrequency = (u64)largeInt.QuadPart;

    gDwTlsIndex = TlsAlloc();
  #else
    pageSize = sysconf(_SC_PAGE_SIZE);
    pthread_key_create(&gTlsKey, nullptr);
  #endif

    globalPool = utils::Pool::create(COMPLEX_MB(128), COMPLEX_MB(1));
    setTls(createTlsContext());
    (void)utils::thread::getCurrentId();
  }

  void destroyTlsIndex()
  {
    if (auto tls = getTls())
      destroyTlsContext(tls);

    if (gfreeTlsIndex)
    {
    #if COMPLEX_WINDOWS
      if (gDwTlsIndex != TLS_OUT_OF_INDEXES)
        TlsFree(gDwTlsIndex);
    #else
      if (gTlsKey)
        pthread_key_delete(gTlsKey);
    #endif
    }
  }

  void atUnload()
  {
    destroyTlsIndex();
    if (gfreeTlsIndex)
      utils::Pool::destroy(globalPool);
    globalPool = nullptr;
  }

  usize
  Pool::getUsedSize(Pool *pool)
  {
    return (usize)pool->committedSize + 1 - Pool::getUnusedSize(pool);
  }

  usize
  Pool::getUnusedSize(Pool *pool)
  {
    if (pool->threadSafe)
      utils::ScopedLock g{ pool->lock, utils::WaitMechanism::Spin };

    u32 nodeShift = pool->freeNodeStart;
    u32 sizeUnused = 0;

    while (nodeShift)
    {
      auto *currentNode = utils::launder((Pool::node *)((byte *)pool + nodeShift));
      sizeUnused += currentNode->size;
      nodeShift = currentNode->next;
    }

    return sizeUnused;
  }

  static void insertNewFreeNode(Pool *pool, byte *newFreeNodeAdress, usize newFreeNodeSize)
  {
    Pool::node *currentNode{}, *previousNode{};
    u32 nodeShift = pool->freeNodeStart;
    u32 shiftToCurrent = (u32)(newFreeNodeAdress - (byte *)pool);

    while (nodeShift)
    {
      if (shiftToCurrent < nodeShift)
        break;

      currentNode = utils::launder((Pool::node *)((byte *)pool + nodeShift));
      nodeShift = currentNode->next;
      previousNode = currentNode;
    }

    ((previousNode) ? previousNode->next : pool->freeNodeStart) = shiftToCurrent;

    COMPLEX_ASSERT(nodeShift != (u32)(newFreeNodeAdress - (byte *)pool));
    (void)new(newFreeNodeAdress) Pool::node{ .size = (u32)newFreeNodeSize, .next = nodeShift };
  }

  static void combineAdjacentFreeNodes(Pool *pool, Pool::node *currentNode)
  {
    for (; currentNode->next;)
    {
      auto *nextNode = utils::launder((Pool::node *)((byte *)pool + currentNode->next));
      if ((byte *)currentNode + currentNode->size < (byte *)nextNode)
        break;
      // free nodes form a contiguous block of memory, combine them
      currentNode->size += nextNode->size;
      currentNode->next = nextNode->next;
    }
  }

  byte *
  Pool::insert(Pool *pool, usize size, usize alignment, bool clean)
  {
    COMPLEX_HARD_ASSERT(pool);
    COMPLEX_HARD_ASSERT(utils::isPowerOfTwo(alignment));

    size = utils::roundUpToMultiple(size, sizeof(Pool::node));
    alignment = utils::max(alignment, alignof(Pool::node));

    COMPLEX_HARD_ASSERT(utils::roundUpToMultiple(sizeof(Pool) + size, alignment) <= usize(u32(-1)) + 1,
      "Allocations larger than 4GB cannot fit inside this pool");

    utils::ScopedLock g{};
    if (pool->threadSafe)
      g = ScopedLock{ pool->lock, utils::WaitMechanism::Spin };

    byte *memory{};
    Pool::node *currentNode{}, *previousNode{};

    // try to find a free node to fit the allocation
    for (u32 nodeShift = pool->freeNodeStart; nodeShift; nodeShift = currentNode->next)
    {
      previousNode = currentNode;
      currentNode = utils::launder((Pool::node *)((byte *)pool + nodeShift));

      combineAdjacentFreeNodes(pool, currentNode);

      // align for object placement
      byte *test = (byte *)utils::roundUpToMultiple((usize)(currentNode) + sizeof(Pool::node), alignment);
      if (test + size <= (byte *)currentNode + currentNode->size)
      {
        memory = test;
        break;
      }
    }

    COMPLEX_INVARIANT_ASSERT(
      (
        return !previousNode || previousNode->next == (u32)((byte *)currentNode - (byte *)pool);
      ),
      (
        if (!memory)
          return true;

        byte *nodeAddress = memory - sizeof(Pool::node);
        return (byte *)currentNode <= nodeAddress && (byte *)currentNode > nodeAddress - alignment;
      )
    );

    if (!memory)
    {
      // try to commit more pages

      // incrementing to get actual sizes
      usize committedSize = (usize)pool->committedSize + 1;
      usize reservedSize = (usize)pool->reservedSize + 1;
      memory = (byte *)utils::roundUpToMultiple((usize)pool + committedSize + sizeof(Pool::node), alignment);

      // have we reached the end of our reserved space?
      if (committedSize != reservedSize)
      {
        // commit more pages based on desired allocation

        bool isLastFreeNodeAtEnd = currentNode && (byte *)currentNode + currentNode->size == (byte *)pool + committedSize;
        if (isLastFreeNodeAtEnd)
        {
          memory = (byte *)utils::roundUpToMultiple((usize)currentNode + sizeof(Pool::node), alignment);
        }

        usize commitSize = utils::min(reservedSize - committedSize,
          (usize)((memory + size) - ((byte *)pool + committedSize)));

        utils::commitMemory((byte *)pool + committedSize, commitSize);
        pool->committedSize += (u32)commitSize;

        if (isLastFreeNodeAtEnd)
          currentNode->size += (u32)commitSize;
        else
        {
          previousNode = currentNode;
          currentNode = new((byte *)pool + committedSize) Pool::node{ .size = (u32)commitSize };
          ((previousNode) ? previousNode->next : pool->freeNodeStart) = (u32)committedSize;
        }
      }

      // object is still too big to fit in the current pool, insert into the next pool
      if ((byte *)pool + reservedSize < memory + size)
      {
        // create next pool if it doesn't exist
        if (!pool->nextArena)
        {
          // calculate sizes based on current pool size and desired allocation size
          usize nextReservedSize = 4 * utils::max(size, reservedSize);
          usize nextCommittedSize = 2 * utils::max(size, committedSize);

          switch ((AllocatorType)pool->flags)
          {
          //case AllocatorType::Arena:
          //case AllocatorType::ArenaNode:
          //  pool->nextArena = createNested(nextReservedSize, pool::fromAllocation(pool));
          //  break;

          case AllocatorType::Pool:
            pool->nextArena = Pool::createNested(Pool::fromAllocation(pool), nextCommittedSize);
            break;

          case AllocatorType::General:
            pool->nextArena = Pool::create(nextReservedSize, nextCommittedSize);
            break;
          }
        }

        auto *nextArena = pool->nextArena;
        g.~ScopedLock();
        return Pool::insert(nextArena, size, alignment, clean);
      }
    }

    byte *nodeAddress = memory - sizeof(Pool::node);

    COMPLEX_INVARIANT_ASSERT(
      (
        return memory;
      ),
      (
        return !previousNode || previousNode->next == (u32)((byte *)currentNode - (byte *)pool);
      ),
      (
        return (byte *)currentNode <= nodeAddress && (byte *)currentNode > nodeAddress - alignment;
      )
    );

    // if we can insert a new free node at the end of the range
    if (memory + size + sizeof(Pool::node) <= (byte *)currentNode + currentNode->size)
    {
      // add the free node at the end
      auto newSize = ((byte *)currentNode + currentNode->size) - (memory + size);
      auto newFreeNodeOffset = (u32)((memory + size) - (byte *)pool);

      COMPLEX_ASSERT(currentNode->next != (u32)(memory + size - (byte *)pool));
      (void)new(memory + size) Pool::node{ .size = (u32)newSize, .next = currentNode->next };

      // does the allocation have such high alignment that it leaves enough space for a free node?
      if ((byte *)currentNode + sizeof(Pool::node) <= nodeAddress)
      {
        currentNode->size = (u32)(nodeAddress - (byte *)currentNode);
        previousNode = currentNode;
      }

      ((previousNode) ? previousNode->next : pool->freeNodeStart) = newFreeNodeOffset;
    }
    else
    {
      // does the allocation have such high alignment that it leaves enough space for a free node?
      if ((byte *)currentNode + sizeof(Pool::node) <= nodeAddress)
        currentNode->size = (u32)(nodeAddress - (byte *)currentNode);
      else
      {
        // the current node will be used fully, link the previous node with the next node
        ((previousNode) ? previousNode->next : pool->freeNodeStart) = currentNode->next;
      }
    }

    pool->lastUsedNode = utils::max(pool->lastUsedNode, (u32)((byte *)nodeAddress - (byte *)pool));

    (void)new(nodeAddress) Pool::node{ .size = (u32)(size + sizeof(Pool::node)),
      .offsetToArena = (u32)(nodeAddress - (byte *)pool) };

    if (clean)
      zeroset(memory, size);

    return memory;
  }

  byte *
  Pool::resize(const void *data, usize newSize, bool cleanNewSpace)
  {
    auto *node = utils::launder((Pool::node *)((byte *)data - sizeof(Pool::node)));
    auto *pool = utils::launder((Pool *)((byte *)node - node->offsetToArena));

    newSize = utils::roundUpToMultiple(newSize, sizeof(Pool::node));
    auto fullSize = newSize + sizeof(Pool::node);

    if (newSize + sizeof(Pool::node) == node->size)
      return (byte *)data;
    else if (newSize + sizeof(Pool::node) < node->size)
    {
      if (pool->threadSafe)
        utils::ScopedLock g{ pool->lock, utils::WaitMechanism::Spin };

      insertNewFreeNode(pool, (byte *)node + fullSize, (usize)node->size - fullSize);
      node->size = (u32)fullSize;

      return (byte *)data;
    }
    else
    {
      // if we're the last node we can just expand (assuming we have enough reserved space to do so)
      usize reservedSize = (usize)pool->reservedSize + 1;
      usize committedSize = (usize)pool->committedSize + 1;
      byte *endOfAllocation = (byte *)data + newSize;
      byte *endOfCommitted = (byte *)pool + committedSize;
      if (pool->lastUsedNode == node->offsetToArena &&
        endOfAllocation <= ((byte *)pool + reservedSize))
      {
        if (endOfAllocation > endOfCommitted)
        {
          usize commitSize = utils::min(reservedSize - committedSize,
            (usize)(endOfAllocation - endOfCommitted));
          utils::commitMemory(endOfCommitted, commitSize);

          pool->committedSize += (u32)commitSize;
          committedSize += commitSize;
          endOfCommitted += commitSize;
        }

        // find last/2nd to last free node and
        // change what it's pointing to inside the newly "allocated" region
        Pool::node *currentNode{}, *previousNode{};
        for (u32 nodeShift = pool->freeNodeStart; nodeShift; nodeShift = currentNode->next)
        {
          previousNode = currentNode;
          currentNode = utils::launder((Pool::node *)((byte *)pool + nodeShift));

          combineAdjacentFreeNodes(pool, currentNode);

          if ((byte *)currentNode >= (byte *)data && (byte *)currentNode < endOfAllocation)
            break;
        }

        // is there space left to insert a free node at the end
        u32 newFreeNodeOffset = 0;
        u32 spaceLeft = (u32)(endOfCommitted - endOfAllocation);
        if (spaceLeft >= sizeof(Pool::node))
        {
          (void)new(endOfAllocation) Pool::node{ .size = (u32)spaceLeft, .next = 0 };
          newFreeNodeOffset = (u32)(endOfAllocation - (byte *)pool);
        }

        ((previousNode) ? previousNode->next : pool->freeNodeStart) = newFreeNodeOffset;

        node->size = (u32)fullSize;
        return (byte *)data;
      }

      // allocation cannot be expanded in-place, allocate again and copy data over
      auto *newAllocation = Pool::insert(pool, newSize, utils::getAlignment(data));
      usize copiedBytes = node->size - sizeof(Pool::node);
      valcpy(newAllocation, (byte *)data, copiedBytes);
      if (cleanNewSpace)
        zeroset(newAllocation + copiedBytes, newSize - copiedBytes);
      Pool::remove(data);

      return newAllocation;
    }
  }

  void Pool::remove(const void *data)
  {
    auto *toRemove = utils::launder((Pool::node *)((byte *)data - sizeof(Pool::node)));
    auto *pool = utils::launder((Pool *)((byte *)toRemove - toRemove->offsetToArena));

    utils::ScopedLock g{};
    if (pool->threadSafe)
      g = { pool->lock, utils::WaitMechanism::Spin };

    insertNewFreeNode(pool, (byte *)toRemove, toRemove->size);
  }

  void Pool::shrinkToFit(Pool *pool, bool shrinkToCommitted)
  {
    {
      utils::ScopedLock g{};
      if (pool->threadSafe)
        g = { pool->lock, utils::WaitMechanism::Spin };

      usize committedSize = (usize)pool->committedSize + 1;
      usize reservedSize = (usize)pool->reservedSize + 1;

      node *lastNode = (pool->freeNodeStart) ? utils::launder((node *)((byte *)pool + pool->freeNodeStart)) : nullptr;
      while (lastNode->next)
        lastNode = utils::launder((node *)((byte *)pool + lastNode->next));

      // shrink to the last allocation or to committed size
      usize end = committedSize;
      if (!shrinkToCommitted && lastNode && ((byte *)lastNode + lastNode->size) == (byte *)pool + committedSize)
        end = (usize)((byte *)lastNode - (byte *)pool);

      COMPLEX_ASSERT(end <= reservedSize);

      switch ((AllocatorType)pool->flags)
      {
      //case AllocatorType::Arena:
      //case AllocatorType::ArenaNode:
      //  (void)pool::resizeNode(pool::fromAllocation(pool), end);
      //  pool->reservedSize = (u32)(end - 1);
      //  pool->committedSize = (u32)(end - 1);
      //  break;

      case AllocatorType::Pool:
        break;

      case AllocatorType::General:
        // only decommitting because of releaseMemory requirements
        utils::decommitMemory((byte *)pool + end, reservedSize - end);
        pool->committedSize = (u32)(end - 1);
        break;
      }
    }

    if (pool->nextArena)
      shrinkToFit(pool->nextArena, shrinkToCommitted);
  }

  void Pool::clear(Pool *pool)
  {
    {
      utils::ScopedLock g{};
      if (pool->threadSafe)
        g = { pool->lock, utils::WaitMechanism::Spin };

      pool->freeNodeStart = (u32)sizeof(Pool);
      (void)new((byte *)pool + sizeof(Pool))
        node{ .size = (u32)(pool->committedSize - sizeof(Pool)) };
    }

    if (pool->nextArena)
      clear(pool->nextArena);
  }

  void Pool::destroy(Pool *pool)
  {
    // it is assumed no one else has access to the pool
    // or the underlying memory anymore, therefore **no contention**

    auto *nextArena = pool->nextArena;

    // if pool is nested, we need to free the node
    switch ((AllocatorType)pool->flags)
    {
    //case AllocatorType::Arena:
    //case AllocatorType::ArenaNode:
    //  pool::remove(pool);
    //  break;

    case AllocatorType::Pool:
      Pool::remove(pool);
      break;

    case AllocatorType::General:
      releaseMemory(pool, pool->reservedSize);
      break;
    }

    if (nextArena)
      destroy(nextArena);
  }
}
