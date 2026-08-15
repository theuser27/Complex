#include <stddef.h>

#if defined (_WIN32) || defined (_WIN64)
  #define COMPLEX_WINDOWS 1
#elif defined (LINUX) || defined (__linux__)
  #define COMPLEX_LINUX 1
#elif defined (__APPLE__ ) || defined (__MACH__)
  #define COMPLEX_MAC 1
#else
  #error Unsupported Platform
#endif

#ifdef COMPLEX_MAC
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _MSC_VER
  #pragma warning (push, 2)
#else
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wmultichar"
  #pragma GCC diagnostic ignored "-Wnullability-completeness"
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

#ifdef __cplusplus
extern "C" {
#endif
void *global_malloc(size_t size);
void *global_calloc(size_t num, size_t size);
void *global_realloc(void *pointer, size_t newSize);
void  global_free(void *pointer);

void *arena_malloc(size_t size);
void *arena_calloc(size_t num, size_t size);
void *arena_realloc(void *pointer, size_t newSize);
void  arena_free(void *pointer);
#ifdef __cplusplus
}
#endif

#define CPLUG_MALLOC global_malloc
#define CPLUG_CALLOC global_calloc
#define CPLUG_REALLOC global_realloc
#define CPLUG_FREE global_free

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

#define XFILES_MALLOC(size)       global_malloc(size)
#define XFILES_REALLOC(ptr, size) global_realloc(ptr, size)
#define XFILES_FREE(ptr)          global_free(ptr)
#define XHL_FILES_IMPL
#include "Third Party/xhl/xhl_files.h"

#ifdef _MSC_VER
  #pragma warning (pop)
#else
  #pragma GCC diagnostic pop
#endif

#define GLAD_MALLOC global_malloc
#define GLAD_FREE global_free
#include "Third Party/glad/glad.c"

#define NVG_MALLOC global_malloc
#define NVG_REALLOC global_realloc
#define NVG_FREE global_free
#include "Third Party/nanovg/nanovg.c"
#define NANOVG_GL3_IMPLEMENTATION
#include "Third Party/nanovg/nanovg_gl.h"
#include "Third Party/nanovg/nanovg_gl_utils.h"

#define NSVG_MALLOC arena_malloc
#define NSVG_FREE arena_free
#define NSVG_REALLOC arena_realloc
#define NANOSVG_IMPLEMENTATION
#include "Third Party/nanovg/nanosvg.h"

#define PUGL_CALLOC global_calloc
#define PUGL_REALLOC global_realloc
#define PUGL_FREE global_free
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

#define STB_SPRINTF_IMPLEMENTATION
#include "Third Party/stb/stb_sprintf.h"

#include "Third Party/cjson/cjson.c"
#include "Third Party/pffft/pffft.c"

#ifdef COMPLEX_MAC
  #pragma GCC diagnostic pop
#endif
