
#if defined (_WIN32) || defined (_WIN64)
  #define COMPLEX_WINDOWS 1
#elif defined (LINUX) || defined (__linux__)
  #define COMPLEX_LINUX 1
#elif defined (__APPLE__ ) || defined (__MACH__)
  #define COMPLEX_MAC 1
#else
  #error Unsupported Platform
#endif

#ifdef _MSC_VER
  #pragma warning (push, 2)
#else

#endif

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
  #define NORASTEROPS
  #define NOSYSCOMMANDS
  #define NOTEXTMETRIC
  #define NODEFERWINDOWPOS
  #define NOGDICAPMASKS
  #define NOSYSMETRICS
#endif

#include "Third Party/cplug/config.h"

#ifdef COMPLEX_STANDALONE
  #if COMPLEX_WINDOWS
    #include "Third Party/cplug/cplug_standalone_win.c"
  #elif COMPLEX_LINUX
    #error No Standalone Version for Linux
  #else
    #include "Third Party/cplug/cplug_standalone_osx.m"
  #endif
#elif COMPLEX_CLAP
  #include "Third Party/cplug/cplug_clap.c"
#else
  #include "Third Party/cplug/cplug_vst3.c"
#endif

#ifdef _MSC_VER
  #pragma warning (pop)
#else

#endif

#include "Third Party/glad/glad.c"

#include "Third Party/nanovg/nanovg.c"
#define NANOVG_GL3_IMPLEMENTATION
#include "Third Party/nanovg/nanovg_gl.h"
#include "Third Party/nanovg/nanovg_gl_utils.h"
#define NANOSVG_IMPLEMENTATION
#include "Third Party/nanovg/nanosvg.h"

#include "Third Party/pugl/src/common.c"
#include "Third Party/pugl/src/internal.c"
#if COMPLEX_WINDOWS
  #include "Third Party/pugl/src/win.c"
  #include "Third Party/pugl/src/win_gl.c"
#elif COMPLEX_LINUX
  #include "Third Party/pugl/src/x11.c"
  #include "Third Party/pugl/src/x11_gl.c"
#else
  #include "Third Party/pugl/src/mac.m"
  #include "Third Party/pugl/src/mac_gl.m"
#endif

#define XHL_FILES_IMPL
#include "Third Party/xhl/xhl_files.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "Third Party/stb/stb_sprintf.h"

#include "Third Party/cjson/cjson.c"
#include "Third Party/pffft/pffft.c"
