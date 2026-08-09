// Copyright 2012-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef PUGL_ATTRIBUTES_H
#define PUGL_ATTRIBUTES_H

// Public declaration scope
#ifdef __cplusplus
#  define PUGL_BEGIN_DECLS extern "C" {
#  define PUGL_END_DECLS }
#else
#  define PUGL_BEGIN_DECLS ///< Begin public API definitions
#  define PUGL_END_DECLS   ///< End public API definitions
#endif

// Symbol exposed in the public API
#ifndef PUGL_API
#  if defined(_WIN32) && !defined(PUGL_STATIC) && defined(PUGL_INTERNAL)
#    define PUGL_API __declspec(dllexport)
#  elif defined(_WIN32) && !defined(PUGL_STATIC)
#    define PUGL_API __declspec(dllimport)
#  elif defined(__GNUC__)
#    define PUGL_API __attribute__((visibility("default")))
#  else
#    define PUGL_API
#  endif
#endif

// GCC function attributes
#if defined(__GNUC__)
#  define PUGL_CONST_FUNC __attribute__((const))
#  define PUGL_MALLOC_FUNC __attribute__((malloc))
#else
#  define PUGL_CONST_FUNC  ///< Only reads its parameters
#  define PUGL_MALLOC_FUNC ///< Allocates memory
#endif

/// A const function in the public API that only reads parameters
#define PUGL_CONST_API PUGL_API PUGL_CONST_FUNC

/// A malloc function in the public API that returns allocated memory
#define PUGL_MALLOC_API PUGL_API PUGL_MALLOC_FUNC

#if !defined(PUGL_CALLOC) && !defined(PUGL_REALLOC) && !defined(PUGL_FREE)
#	 define PUGL_CALLOC calloc
#  define PUGL_REALLOC realloc
#  define PUGL_FREE free
#endif

// Unused parameter macro to suppresses warnings and make it impossible to use
#if defined(__cplusplus)
#  define PUGL_UNUSED(name) name
#elif defined(__GNUC__) || defined(__clang__)
#  define PUGL_UNUSED(name) name##_unused __attribute__((__unused__))
#else
#  define PUGL_UNUSED(name) name
#endif

// Unused result macro to warn when an important return status is ignored
#ifndef _MSC_VER
#  define PUGL_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#  define PUGL_WARN_UNUSED_RESULT
#endif

#endif // PUGL_ATTRIBUTES_H
