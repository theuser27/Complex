
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
#endif

#include "cplug/config.h"

#ifdef COMPLEX_STANDALONE
  #if COMPLEX_WINDOWS
    #include "cplug/cplug_standalone_win.c"
  #elif COMPLEX_LINUX
    #error No Standalone Version for Linux
  #else
    #include "cplug/cplug_standalone_osx.m"
  #endif
#elif COMPLEX_CLAP
  #include "cplug/cplug_clap.c"
#else
  #include "cplug/cplug_vst3.c"
#endif

#ifdef _MSC_VER
  #pragma warning (pop)
#else

#endif

#include "glad/glad.c"

#include "nanovg/nanovg.c"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"
#define NANOSVG_IMPLEMENTATION
#include "nanovg/nanosvg.h"

#include "pugl/src/common.c"
#include "pugl/src/internal.c"
#if COMPLEX_WINDOWS
  #include "pugl/src/win.c"
  #include "pugl/src/win_gl.c"
#elif COMPLEX_LINUX
  #include "pugl/src/x11.c"
  #include "pugl/src/x11_gl.c"
#else
  #include "pugl/src/mac.m"
  #include "pugl/src/mac_gl.m"
#endif

#define XHL_FILES_IMPL
#include "Third Party/xhl/files.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "Third Party/stb/stb_sprintf.h"

#include "Third Party/cjson/cjson.c"
#include "Third Party/pffft/pffft.c"
