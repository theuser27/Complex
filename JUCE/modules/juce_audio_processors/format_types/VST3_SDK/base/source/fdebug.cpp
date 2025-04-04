//------------------------------------------------------------------------
// Project     : SDK Base
// Version     : 1.0
//
// Category    : Helpers
// Filename    : base/source/fdebug.cpp
// Created by  : Steinberg, 1995
// Description : There are 2 levels of debugging messages:
//	             DEVELOPMENT               During development
//	             RELEASE                   Program is shipping.
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2021, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "base/source/fdebug.h"

#if SMTG_OS_WINDOWS
#include <windows.h>

bool AmIBeingDebugged ()
{
  return IsDebuggerPresent ();
}
#endif

#if SMTG_OS_LINUX
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
//--------------------------------------------------------------------------
bool AmIBeingDebugged ()
{
  // TODO: check if GDB or LLDB is attached
  return true;
}
#endif

#if SMTG_OS_MACOS

#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

//------------------------------------------------------------------------
// from Technical Q&A QA1361 (http://developer.apple.com/qa/qa2004/qa1361.html)
//------------------------------------------------------------------------
bool AmIBeingDebugged ()
// Returns true if the current process is being debugged (either
// running under the debugger or has a debugger attached post facto).
{
  int mib[4];
  struct kinfo_proc info;
  size_t size;

  // Initialize the flags so that, if sysctl fails for some bizarre
  // reason, we get a predictable result.

  info.kp_proc.p_flag = 0;

  // Initialize mib, which tells sysctl the info we want, in this case
  // we're looking for information about a specific process ID.

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid ();

  // Call sysctl.

  size = sizeof (info);
  sysctl (mib, sizeof (mib) / sizeof (*mib), &info, &size, NULL, 0);

  // We're being debugged if the P_TRACED flag is set.
  return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

#endif // SMTG_OS_MACOS

#if DEVELOPMENT

#include <cassert>
#include <cstdarg>
#include <cstdio>

#if SMTG_OS_WINDOWS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#if _MSC_VER
#include <intrin.h>
#endif
#define vsnprintf _vsnprintf
#define snprintf _snprintf

#elif SMTG_OS_MACOS
#include <errno.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <new>
#include <signal.h>

#define THREAD_ALLOC_WATCH 0 // check allocations on specific threads

#if THREAD_ALLOC_WATCH
mach_port_t watchThreadID = 0;
#endif

#endif

AssertionHandler gAssertionHandler = nullptr;
AssertionHandler gPreAssertionHook = nullptr;
DebugPrintLogger gDebugPrintLogger = nullptr;

//--------------------------------------------------------------------------
static const int kDebugPrintfBufferSize = 10000;
static bool neverDebugger = false; // so I can switch it off in the debugger...

//--------------------------------------------------------------------------
static void printDebugString (const char* string)
{
  if (!string)
    return;

  if (gDebugPrintLogger)
  {
    gDebugPrintLogger (string);
  }
  else
  {
#if SMTG_OS_MACOS || defined(__MINGW32__)
    fprintf (stderr, "%s", string);
#elif SMTG_OS_WINDOWS
    OutputDebugStringA (string);
#endif
  }
}

//--------------------------------------------------------------------------
//	printf style debugging output
//--------------------------------------------------------------------------
void FDebugPrint (const char* format, ...)
{
  char string[kDebugPrintfBufferSize];
  va_list marker;
  va_start (marker, format);
  vsnprintf (string, kDebugPrintfBufferSize, format, marker);

  printDebugString (string);
}

//--------------------------------------------------------------------------
//	printf style debugging output
//--------------------------------------------------------------------------
void FDebugBreak (const char* format, ...)
{
  char string[kDebugPrintfBufferSize];
  va_list marker;
  va_start (marker, format);
  vsnprintf (string, kDebugPrintfBufferSize, format, marker);

  printDebugString (string);

  // The Pre-assertion hook is always called, even if we're not running in the debugger,
  // so that we can log asserts without displaying them
  if (gPreAssertionHook)
  {
    gPreAssertionHook (string);
  }

  if (neverDebugger)
    return;
  if (AmIBeingDebugged ())
  {
    // do not crash if no debugger present
    // If there  is an assertion handler defined then let this override the UI
    // and tell us whether we want to break into the debugger
    bool breakIntoDebugger = true;
    if (gAssertionHandler && gAssertionHandler (string) == false)
    {
      breakIntoDebugger = false;
    }

    if (breakIntoDebugger)
    {
#if SMTG_OS_WINDOWS	&& _MSC_VER
      __debugbreak (); // intrinsic version of DebugBreak()
#elif SMTG_OS_MACOS && __arm64__
      raise (SIGSTOP);

#elif __ppc64__ || __ppc__ || __arm__
      kill (getpid (), SIGINT);
#elif __i386__ || __x86_64__
      {
        __asm__ volatile ("int3");
      }
#endif
    }
  }
}

//--------------------------------------------------------------------------
void FPrintLastError (const char* file, int line)
{
#if SMTG_OS_WINDOWS
  LPVOID lpMessageBuffer;
  FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
                  GetLastError (), MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&lpMessageBuffer, 0, nullptr);
  FDebugPrint ("%s(%d) : %s\n", file, line, lpMessageBuffer);
  LocalFree (lpMessageBuffer);
#endif

#if SMTG_OS_MACOS
#if !__MACH__
  extern int errno;
#endif
  FDebugPrint ("%s(%d) : Errno %d\n", file, line, errno);
#endif
}

#if SMTG_OS_MACOS

//------------------------------------------------------------------------
void* operator new (size_t size, int, const char* file, int line)
{
#if THREAD_ALLOC_WATCH
  mach_port_t threadID = mach_thread_self ();
  if (watchThreadID == threadID)
  {
    FDebugPrint ("Watched Thread Allocation : %s (Line:%d)\n", file ? file : "Unknown", line);
  }
#endif
  try
  {
    return ::operator new (size);
  }
  catch (std::bad_alloc exception)
  {
    FDebugPrint ("bad_alloc exception : %s (Line:%d)", file ? file : "Unknown", line);
  }
  return (void*)-1;
}

//------------------------------------------------------------------------
void* operator new[] (size_t size, int, const char* file, int line)
{
#if THREAD_ALLOC_WATCH
  mach_port_t threadID = mach_thread_self ();
  if (watchThreadID == threadID)
  {
    FDebugPrint ("Watched Thread Allocation : %s (Line:%d)\n", file ? file : "Unknown", line);
  }
#endif
  try
  {
    return ::operator new[] (size);
  }
  catch (std::bad_alloc exception)
  {
    FDebugPrint ("bad_alloc exception : %s (Line:%d)", file ? file : "Unknown", line);
  }
  return (void*)-1;
}

//------------------------------------------------------------------------
void operator delete (void* p, int, const char* file, int line)
{
  ::operator delete (p);
}

//------------------------------------------------------------------------
void operator delete[] (void* p, int, const char* file, int line)
{
  ::operator delete[] (p);
}

#endif // SMTG_OS_MACOS

#endif // DEVELOPMENT

static bool smtg_unit_testing_active = false; // ugly hack to unit testing ...

//------------------------------------------------------------------------
bool isSmtgUnitTesting ()
{
  return smtg_unit_testing_active;
}

//------------------------------------------------------------------------
void setSmtgUnitTesting ()
{
  smtg_unit_testing_active = true;
}
