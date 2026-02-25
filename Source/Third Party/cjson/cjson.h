/*
  Copyright (c) 2009-2017 Dave Gamble and cjson contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* project version */
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 19

#include <stddef.h>
#include <stdbool.h>

/* cjson Types: */
enum cjson_Types
{
  cjson_Invalid       = 0,
  cjson_Bool          = 1 << 0,
  cjson_NULL          = 1 << 1,
  cjson_Float        = 1 << 2,
  cjson_Integer       = 1 << 3,
  cjson_Unsigned      = 1 << 4,
  cjson_String        = 1 << 5,
  cjson_Array         = 1 << 6,
  cjson_Object        = 1 << 7,
  cjson_Raw           = 1 << 8, /* raw json */
  cjson_TypeMask      = (1 << 9) - 1,
  
  cjson_IsReference   = 1 << 9,
  cjson_StringIsConst = 1 << 10,
};

/* The cjson structure: */
typedef struct cjson
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct cjson *next;
    struct cjson *prev;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *name;
    /* The type of the item, as above. */
    int type;

    union
    {
      /* if type == cjson_Object and type == cjson_Array. */
      struct cjson *child;
      /* if type == cjson_String and type == cjson_Raw */
      char *vstring;
      /* if type == cjson_Unsigned */
      unsigned long long vuint;
      /* if type == cjson_Integer */
      long long vint;
      /* if type == cjson_Float */
      double vdouble;
      /* if type == cjson_Bool */
      bool vbool;
    };

} cjson;

typedef struct cjson_Hooks
{
  void *(*malloc_fn)(size_t sz);
  void (*free_fn)(void *ptr);
} cjson_Hooks;

/* Limits how deeply nested arrays/objects can be before cjson rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* Limits the length of circular references can be before cjson rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_CIRCULAR_LIMIT
#define CJSON_CIRCULAR_LIMIT 10000
#endif

/* Supply malloc, realloc and free functions to cjson */
void cjson_InitHooks(cjson_Hooks* hooks);

/* Memory Management: the caller is always responsible to free the results from all variants of 
   cjson_Parse (with cjson_Delete) and cjson_Print (with stdlib free, cjson_Hooks.free_fn, or cjson_free as appropriate). 
   The exception is cjson_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a cjson object you can interrogate. */
cjson *cjson_Parse(const char *value, size_t buffer_length);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match cjson_GetErrorPtr(). */
cjson *cjson_ParseWithOpts(const char *value, size_t buffer_length, const char **return_parse_end, bool require_null_terminated);

/* Render a cjson entity to text for transfer/storage. */
char *cjson_Print(const cjson *item, size_t *size);
/* Render a cjson entity to text for transfer/storage without any formatting. */
char *cjson_PrintUnformatted(const cjson *item, size_t *size);
/* Render a cjson entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
char *cjson_PrintBuffered(const cjson *item, int prebuffer, bool fmt);
/* Render a cjson entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: cjson is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
bool cjson_PrintPreallocated(cjson *item, char *buffer, const int length, const bool format);


/* Returns the number of items in an array (or object). */
size_t cjson_GetSize(const cjson *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
cjson *cjson_GetArrayItem(const cjson *array, size_t index);
/* Get item "string" from object. Case insensitive. */
cjson *cjson_GetObjectItem(const cjson * const object, const char * const string);
cjson *cjson_GetObjectItemCaseSensitive(const cjson * const object, const char * const string);

/* These calls create a cjson item of the appropriate type. */
cjson *cjson_Create(int type, ...);
#define cjson_Create(type, ...) (cjson_Create)(type, (0LL, ##__VA_ARGS__))

#define _cjson_IsStringLiteral_(x) ((#x)[0] == '"' || (sizeof(#x) > 2 && (#x)[1] == '"') || (sizeof(#x) > 3 && (#x)[2] == '"') || (sizeof(#x) > 4 && (#x)[3] == '"'))
#define _cjson_IsStringLiteral(x) _cjson_IsStringLiteral_(x)

/* Append item to the specified array/object. If you know the object is an array, string can be NULL */
cjson *cjson_AddExistingTo(cjson *parent, const char *name, cjson *item, bool delete_on_fail, bool is_name_constant);
#define cjson_AddExistingTo(parent, name, item) cjson_AddExistingTo(parent, name, item, false, _cjson_IsStringLiteral(name))

/* Remove/Detach items from Arrays/Objects. */
void cjson_Delete(cjson *item);
cjson *cjson_DetachItemViaPointer(cjson *parent, cjson * const item);
#define cjson_DetachItemFromArray(array, index) cjson_DetachItemViaPointer(array, cjson_GetArrayItem(array, index))
#define cjson_DetachItemFromObject(object, string) cjson_DetachItemViaPointer(object, cjson_GetObjectItem(object, string))
#define cjson_DetachItemFromObjectCaseSensitive(object, string) cjson_DetachItemViaPointer(object, cjson_GetObjectItemCaseSensitive(object, string))
#define cjson_DeleteItemFromArray(array, index) cjson_Delete(cjson_DetachItemFromArray(array, index))
#define cjson_DeleteItemFromObject(object, string) cjson_Delete(cjson_DetachItemFromObject(object, string))
#define cjson_DeleteItemFromObjectCaseSensitive(object, string) cjson_Delete(cjson_DetachItemFromObjectCaseSensitive(object, string))

/* Update array items. */
bool cjson_InsertItemInArray(cjson *array, int which, cjson *newitem); /* Shifts pre-existing items to the right. */
bool cjson_ReplaceItemViaPointer(cjson * const parent, cjson * const item, cjson * replacement);
#define cjson_ReplaceItemInArray(array, index, new_item) cjson_ReplaceItemViaPointer(array, cjson_GetArrayItem(array, index), new_item)
bool cjson_ReplaceItemInObject(cjson *object,const char *string,cjson *newitem);
bool cjson_ReplaceItemInObjectCaseSensitive(cjson *object,const char *string,cjson *newitem);

/* Duplicate a cjson item */
/* Duplicate will create a new, identical cjson item to the one you pass, in new memory that will
 * need to be released. With recurse!=0, it will duplicate any children connected to the item.
 * The item->next and ->prev pointers are always zero on return from Duplicate. */
cjson *cjson_Duplicate(const cjson *item, bool recurse);
/* Recursively compare two cjson items for equality. If either a or b is NULL or invalid, they will be considered unequal.
 * case_sensitive determines if object keys are treated case sensitive (1) or case insensitive (0) */
bool cjson_Compare(const cjson * a, const cjson * b, bool case_sensitive);

/* Minify a strings, remove blank characters(such as ' ', '\t', '\r', '\n') from strings.
 * The input pointer json cannot point to a read-only address area, such as a string constant,
 * but should point to a readable and writable address area. */
void cjson_Minify(char *json);

/* Helper function for creating and adding items to an object/array at the same time.
 * Returns the added item or NULL on failure. */
#define cjson_AddTo(parent, name, type, /*args*/ ...) (cjson_AddExistingTo)(parent, name, (cjson_Create)(type, (0LL, ##__VA_ARGS__)), true, _cjson_IsStringLiteral(name))

bool cjson_Set(cjson *item, int type, ...);

/* Macro for iterating over an array or object */
#define cjson_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* malloc/free objects using the malloc/free functions that have been set with cjson_InitHooks */
void *cjson_malloc(size_t size);
void cjson_free(void *object);

#ifdef __cplusplus
}
#endif
