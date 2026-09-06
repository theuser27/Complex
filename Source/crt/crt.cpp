
#include "Framework/stl_utils.hpp"
#include "Framework/memory.hpp"
#include "Framework/simd_values.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/simd_math.hpp"

namespace utils
{
  // malloc replacement
  byte *allocate(usize size, usize alignment, bool clean);
  // free replacement
  void deallocate(const void *memory);
}

#ifdef COMPLEX_WINDOWS
  #define CALL_CONV __cdecl
#else
  #define CALL_CONV
#endif

namespace std
{
  enum class align_val_t : usize { };
}

extern "C++"
{
  [[nodiscard]] void *CALL_CONV operator new(usize size)
  {
    COMPLEX_ASSERT_FALSE("Do not use allocating operator new, use arenas instead");
    return utils::allocate(size);
  }
  [[nodiscard]] void *CALL_CONV operator new(usize size, std::align_val_t alignment)
  {
    COMPLEX_ASSERT_FALSE("Do not use allocating operator new, use arenas instead");
    return utils::allocate(size, (usize)alignment);
  }
  [[nodiscard]] void *CALL_CONV operator new[](usize size) 
  {
    COMPLEX_ASSERT_FALSE("Do not use allocating operator new, use arenas instead");
    return utils::allocate(size);
  }
  [[nodiscard]] void *CALL_CONV operator new[](usize size, std::align_val_t alignment)
  {
    COMPLEX_ASSERT_FALSE("Do not use allocating operator new, use arenas instead");
    return utils::allocate(size, (usize)alignment);
  }

  void CALL_CONV operator delete(void *pointer) noexcept 
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
  void CALL_CONV operator delete[](void *pointer) noexcept
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
  void CALL_CONV operator delete(void *pointer, usize) noexcept
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
  void CALL_CONV operator delete[](void *pointer, usize) noexcept
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
  void CALL_CONV operator delete(void *pointer, std::align_val_t) noexcept
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
  void CALL_CONV operator delete[](void *pointer, std::align_val_t) noexcept
  {
    COMPLEX_ASSERT_FALSE("Do not use deallocating operator delete, use arenas instead");
    utils::deallocate(pointer);
  }
}

int printVariadic(const char *format, va_list args);

extern "C"
{
  __declspec(allocator) __declspec(restrict) void *CALL_CONV malloc(usize size) { return utils::bumpArena::insert(globalArena, size, alignof(void *)); }
  __declspec(allocator) __declspec(restrict) void *CALL_CONV calloc(usize count, usize size) { return utils::bumpArena::insert(globalArena, count * size, alignof(void *), true); }
  __declspec(allocator) __declspec(restrict) void *CALL_CONV realloc(void *pointer, usize newSize) { return utils::bumpArena::resize(pointer, newSize); }
  void CALL_CONV free(void *pointer) { utils::bumpArena::remove(pointer); }

  int CALL_CONV abs(int x) { return utils::abs(x); }

  float CALL_CONV floorf(float x) { return simd_float::floor(x)[0]; }
  float CALL_CONV ceilf(float x) { return simd_float::ceil(x)[0]; }
  float CALL_CONV roundf(float x) { return simd_float::round(x)[0]; }

  float CALL_CONV sinf(float x) { return utils::cis(x)[1]; }
  float CALL_CONV cosf(float x) { return utils::cis(x)[0]; }
  float CALL_CONV tanf(float x) { return utils::tan(x)[0]; }
  float CALL_CONV atan2f(float y, float x) { return utils::atan2(y, x)[0]; }

  float CALL_CONV sqrtf(float x) { return simd_float::sqrt(x)[0]; }
  float CALL_CONV expf(float x) { return utils::exp(x); }
  float CALL_CONV powf(float x, float y) { return utils::pow(x, y); }
  float CALL_CONV log10f(float x) { return utils::log10(x); }

  double CALL_CONV 
  floor(double value)
  {
  #if COMPLEX_SSE4_1
    return _mm_round_pd(_mm_set1_pd(value), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC).m128d_f64[0];
  #elif COMPLEX_NEON
    
  #endif
  }

  double CALL_CONV
  ceil(double value)
  {
  #if COMPLEX_SSE4_1
    return _mm_round_pd(_mm_set1_pd(value), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC).m128d_f64[0];
  #elif COMPLEX_NEON

  #endif
  }

  double CALL_CONV
  round(double value)
  {
  #if COMPLEX_SSE4_1
    return _mm_round_pd(_mm_set1_pd(value), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC).m128d_f64[0];
  #elif COMPLEX_NEON

  #endif
  }

  double CALL_CONV
  sqrt(double value)
  {
  #if COMPLEX_SSE4_1
    return _mm_sqrt_pd(_mm_set1_pd(value)).m128d_f64[0];
  #elif COMPLEX_NEON

  #endif
  }

  double CALL_CONV
  sin(double angle)
  {
    return utils::cis((float)angle)[1];
  }

  double CALL_CONV
  cos(double angle)
  {
    return utils::cis((float)angle)[0];
  }

  double CALL_CONV
  acos(double x)
  {
    static constexpr double c0 = 1.0 / 6.0;
    static constexpr double c1 = 3.0 / 40.0;
    static constexpr double c2 = 5.0 / 112.0;
    static constexpr double c3 = 35.0 / 1152.0;
    static constexpr double c4 = 63.0 / 2816.0;
    static constexpr double c5 = 231.0 / 13312.0;
    static constexpr double c6 = 143.0 / 10240.0;

    static constexpr double pi = 3.14159265358979323846264338327950288;
    static constexpr double halfPi = 1.57079632679489661923132169163975144;

    if (x < -1.0 || x > 1.0)
      return const_math::quiet_nan<double>();

    double ax = fabs(x);

    if (ax < 0.5)
    {
      const double z = x * x;

      // R(z) = minimax approximation to
      //
      // (asin(x) - x) / x^3
      //
      // on [0, 0.25]

      double r = c6;
      r = r * z + c5;
      r = r * z + c4;
      r = r * z + c3;
      r = r * z + c2;
      r = r * z + c1;
      r = r * z + c0;

      return halfPi - (x + x * z * r);
    }

    double z = 0.5 * (1.0 - ax);
    double s = sqrt(z);

    // R(z) = minimax approximation to
    //
    // (asin(s) - s) / s^3
    //
    // on [0, 0.25]

    double r = c6;
    r = r * z + c5;
    r = r * z + c4;
    r = r * z + c3;
    r = r * z + c2;
    r = r * z + c1;
    r = r * z + c0;

    double a = s + s * z * r;
    double result = 2.0 * a;

    return x >= 0.0 ? result : pi - result;
  }

  float CALL_CONV acosf(float x) { return (float)acos(x); }
  
  double CALL_CONV
  log2(double x)
  {
    return utils::log2((float)x);
  }

  double CALL_CONV 
  pow(double base, double exponent)
  {
    return utils::exp2((float)(utils::log2((float)base) * exponent));
  }

  double CALL_CONV
  fabs(double value)
  {
    static constexpr u64 kNotSignMask = (1ULL << 63) - 1;
    return utils::bit_cast<double>(utils::bit_cast<u64>(value) & kNotSignMask);
  }

  float CALL_CONV
  fmodf(float x, float y)
  {
    float c = fabsf(x / y);
    c = (c - floorf(c)) * fabsf(y);
    return (x < 0) ? -c : c;
  }

  double CALL_CONV
  fmod(double x, double y)
  {
    double c = fabs(x / y);
    c = (c - floor(c)) * fabs(y);
    return (x < 0) ? -c : c;
  }

  int CALL_CONV
  isspace(int c)
  {
    return (unsigned)(c - 9) < 5u || c == ' ';
  }

  int CALL_CONV 
  isdigit(int c)
  {
    return c >= '0' && c <= '9';
  }

  int CALL_CONV 
  isxdigit(int c)
  {
    return (bool)isdigit(c) || 
      (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
  }

  int CALL_CONV 
  tolower(int c)
  {
    if (c >= 'A' && c <= 'Z')
      return c + 'a' - 'A';
    return c;
  }

  int CALL_CONV 
  strcmp(const char *one, const char *two)
  {
    COMPLEX_ASSERT(one);
    COMPLEX_ASSERT(two);

    int result;
    while ((result = *one - *two++) == 0 && *one++) { }
    return result;
  }

  size_t CALL_CONV
  strlen(const char *string)
  {
    size_t size = 0;
    usize address = utils::roundUpToMultiple((usize)string, sizeof(simd_int));
    for (usize i = address - (usize)string; i; (--i), (++size))
      if (!string[size])
        return size;

    while (true)
    {
    #if COMPLEX_SSE4_1

      auto test = _mm_load_si128((const __m128i *)(string + size));
      auto mask = _mm_movemask_epi8(_mm_cmpeq_epi8(test, _mm_setzero_si128()));
      if (!mask)
      {
        size += sizeof(__m128i);
        continue;
      }

      unsigned long increment;
      _BitScanForward(&increment, (u32)mask);

      size += increment;

      break;

    #elif COMPLEX_NEON

    #endif
    }
    return size;
  }

  char *CALL_CONV
  strcpy(char *destination, const char *source)
  {
    auto size = strlen(source);
    memmove(destination, source, size + 1);
    return destination;
  }

  char *CALL_CONV
  strncpy(char *destination, const char *source, size_t num)
  {
    auto size = strlen(source);
    memmove(destination, source, utils::min(size + 1, num));
    if (size + 1 < num)
      memset(destination + size + 1, 0, num - size - 1);

    return destination;
  }

  char *CALL_CONV
  strstr(const char *string, const char *substring)
  {
    auto size = strlen(substring);
    usize j = 0;
    bool isMatched = false;

    for (; !isMatched; ++j)
    {
      for (usize i = 0; i < size; ++i)
      {
        if (string[j + i] == '\0')
          return nullptr;

        isMatched = string[j + i] == substring[i];
        if (!isMatched)
          break;
      }
    }

    return (isMatched) ? (char *)string + j : nullptr;
  }

  int CALL_CONV
  strncmp(const char *lhs, const char *rhs, size_t count)
  {
    int diff = 0;
    for (usize i = 0; i < count && !diff; ++i)
      diff = lhs[i] - rhs[i];
    return diff;
  }

  wchar_t *CALL_CONV 
  wcsncat(wchar_t *dest, const wchar_t *src, size_t count)
  {
    usize i = 0;
    for (; i < count && src[i]; ++i)
      dest[i] = src[i];
    dest[i] = 0;
    return dest;
  }

  int CALL_CONV
  wcsncmp(const wchar_t *lhs, const wchar_t *rhs, size_t count)
  {
    int diff = 0;
    for (usize i = 0; i < count && !diff; ++i)
      diff = lhs[i] - rhs[i];
    return diff;
  }

  wchar_t *CALL_CONV
  wcsncpy(wchar_t *dest, const wchar_t *src, size_t count)
  {
    return (wchar_t *)strncpy((char *)dest, (const char *)src, count * sizeof(wchar_t));
  }

  int CALL_CONV
  _stricmp(char const *one, char const *two)
  {
    int diff = 0;
    for (usize i = 0; !diff && one[i] && two[i]; ++i)
      diff = tolower(one[i]) - tolower(two[i]);
    return diff;
  }

  // the following string to integer conversion functions are modified versions from
  // https://github.com/aligrudi/neatlibc/blob/93643b23cdbd185fc86b27302806cf09e2d3decb/atoi.c#L6
  //
  // LICENCE
  // =======
  // 
  // NEATLIBC C STANDARD LIBRARY
  // 
  // Copyright (C) 2010-2020 Ali Gholami Rudi <ali at rudi dot ir>
  // 
  // Permission to use, copy, modify, and/or distribute this software for any
  // purpose with or without fee is hereby granted, provided that the above
  // copyright notice and this permission notice appear in all copies.
  // 
  // THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  // WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  // MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  // ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  // WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  // ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  // OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

  long CALL_CONV
  atol(const char *s)
  {
    long num = 0;
    int neg = 0;
    while (isspace(*s))
      s++;
    if (*s == '-' || *s == '+')
      neg = *s++ == '-';
    while ((unsigned)(*s - '0') <= 9u)
      num = num * 10 + *s++ - '0';
    return neg ? -num : num;
  }

  int atoi(const char *s) { return (int)atol(s); }

  static int 
  digit(char c, int base)
  {
    int d;
    if (c <= '9')
      d = c - '0';
    else if (c <= 'Z')
      d = 10 + c - 'A';
    else
      d = 10 + c - 'a';
    return d < base ? d : -1;
  }

  long long CALL_CONV
  strtoll(const char *s, char **endptr, int base)
  {
    int sgn = 1;
    int overflow = 0;
    long long num;
    int dig;
    while (isspace(*s))
      s++;
    if (*s == '-' || *s == '+')
      sgn = ',' - *s++;
    if (base == 0)
    {
      if (*s == '0')
      {
        if (s[1] == 'x' || s[1] == 'X')
          base = 16;
        else
          base = 8;
      }
      else
      {
        base = 10;
      }
    }
    if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X'))
      s += 2;
    for (num = 0; (dig = digit(*s, base)) >= 0; s++)
    {
      if (num > utils::int_max<long long> / base)
        overflow = 1;
      num *= base;
      if (num > utils::int_max<long long> - dig)
        overflow = 1;
      num += dig;
    }
    if (endptr)
      *endptr = (char *)s;
    if (overflow)
    {
      num = sgn > 0 ? utils::int_max<long long> : utils::int_min<long long>;
      //errno = ERANGE;
    }
    else
    {
      num *= sgn;
    }
    return num;
  }

  unsigned long long CALL_CONV
  strtoull(const char *s, char **endptr, int base)
  {
    int sgn = 1;
    int overflow = 0;
    unsigned long long num;
    int dig;
    while (isspace(*s))
      s++;
    if (*s == '-' || *s == '+')
      sgn = ',' - *s++;
    if (base == 0)
    {
      if (*s == '0')
      {
        if (s[1] == 'x' || s[1] == 'X')
          base = 16;
        else
          base = 8;
      }
      else
      {
        base = 10;
      }
    }
    if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X'))
      s += 2;
    for (num = 0; (dig = digit(*s, base)) >= 0; s++)
    {
      if (num > utils::int_max<unsigned long long> / base)
        overflow = 1;
      num *= base;
      if (num > utils::int_max<unsigned long long> - dig)
        overflow = 1;
      num += dig;
    }
    if (endptr)
      *endptr = (char *)s;
    if (overflow)
    {
      num = utils::int_max<unsigned long long>;
      //errno = ERANGE;
    }
    else
    {
      num *= sgn;
    }
    return num;
  }

  long CALL_CONV strtol(const char *s, char **endptr, int base) { return (long)strtoll(s, endptr, base); }
  unsigned long CALL_CONV strtoul(const char *s, char **endptr, int base) { return (long)strtoull(s, endptr, base); }
  
  double
  strtod(const char *string, char **stringEnd)
  {
    const char *p = string;

    // skip whitespace
    while (isspace(*p))
      ++p;

    bool sign = false;
    if (*p == '+' || *p == '-')
    {
      sign = (*p == '-');
      ++p;
    }

    // handle infinity and nan
    if (tolower(p[0]) == 'i' && tolower(p[1]) == 'n' && tolower(p[2]) == 'f')
    {
      p += 3;

      if (tolower(p[0]) == 'i' && tolower(p[1]) == 'n' &&
        tolower(p[2]) == 'i' && tolower(p[3]) == 't' && tolower(p[4]) == 'y')
      {
        p += 5;
      }

      if (stringEnd)
        *stringEnd = (char *)p;

      return sign ? -const_math::infinity<double>() : const_math::infinity<double>();
    }
    if (tolower(p[0]) == 'n' && tolower(p[1]) == 'a' && tolower(p[2]) == 'n')
    {
      p += 3;

      // optional nan payload: nan(...)
      if (*p == '(')
      {
        while (*p && *p != ')')
          ++p;

        if (*p == ')')
          ++p;
      }

      if (stringEnd)
        *stringEnd = (char *)p;

      return sign ? -const_math::quiet_nan<double>() : const_math::quiet_nan<double>();
    }

    double value = 0.0;
    bool hasDigits = false;
    bool isHex = p[0] == '0' && tolower(p[1]) == 'x';
    if (isHex)
      p += 2;
    
    // integer part
    if (isHex)
    {
      while (isxdigit(*p))
      {
        hasDigits = true;
        int digit = (*p >= '0' && *p <= '9') ? *p - '0' :
          tolower(*p) - 'a' + 10;
        value = value * 16.0 + digit;
        ++p;
      }
    }
    else
    {
      while (isdigit(*p))
      {
        hasDigits = true;
        value = value * 10.0 + (*p - '0');
        ++p;
      }
    }

    // fractional part
    if (*p == '.')
    {
      ++p;

      if (isHex)
      {
        for (double scale = 1.0 / 16.0; isxdigit(*p); (scale /= 16.0), (++p))
        {
          hasDigits = true;
          int digit = (*p >= '0' && *p <= '9') ? *p - '0' :
            tolower(*p) - 'a' + 10;
          value = value + digit * scale;
        }

      }
      else
      {
        for (double scale = 0.1; isdigit(*p); (scale *= 0.1), (++p))
        {
          hasDigits = true;
          value += (*p - '0') * scale;
        }
      }
    }

    // No number was found
    if (!hasDigits)
    {
      if (stringEnd)
        *stringEnd = (char *)string;

      return 0.0;
    }

    // exponent part
    if (isHex && tolower(*p) == 'p')
    {
      const char *exponentStart = p;
      ++exponentStart;

      bool exponentSign = false;
      if (*exponentStart == '+' || *exponentStart == '-')
      {
        exponentSign = (*exponentStart == '-');
        ++exponentStart;
      }

      // if the next character isn't a digit then we've misparsed an exponent
      // otherwise continue parsing
      if (isdigit(*exponentStart))
      {
        i64 exponent = 0;
        for (; isdigit(*exponentStart); ++exponentStart)
          exponent = exponent * 10 + (*exponentStart - '0');
        
        exponent = (exponentSign) ? -exponent : exponent;
        exponent += (utils::bit_cast<u64>(value) >> 52) & 0x7ff;
        if (exponent > 2046)
          value = const_math::infinity<double>();
        else
        {
          auto mantissa = utils::bit_cast<u64>(value) & kDoubleMantissaMask;
          if (exponent <= 0 && utils::bit_cast<u64>(value) != 0)
            value = utils::bit_cast<double>((mantissa | (u64(1) << 51)) >> -exponent);
          else
            value = utils::bit_cast<double>(exponent << 52 | mantissa);
        }

        p = exponentStart;
      }
    }
    else if (tolower(*p) == 'e')
    {
      const char *exponentStart = p;
      ++exponentStart;

      bool exponentSign = false;
      if (*exponentStart == '+' || *exponentStart == '-')
      {
        exponentSign = (*exponentStart == '-');
        ++exponentStart;
      }

      // if the next character isn't a digit then we've misparsed an exponent
      // otherwise continue parsing
      if (isdigit(*exponentStart))
      {
        int exponent = 0;
        for (; isdigit(*exponentStart); ++exponentStart)
          exponent = exponent * 10 + (*exponentStart - '0');

        double scale = (exponentSign) ? 0.1 : 10.0;
        for (int i = exponent % 10; exponent; i = exponent % 10)
        {
          for (int j = 0; j < i; ++j)
            value *= scale;

          exponent /= 10;

          double multiplier = scale;
          // 9 more times to reach scale^10
          for (int j = 0; j < 9; ++j)
            scale *= multiplier;
        }

        p = exponentStart;
      }
    }

    if (stringEnd)
      *stringEnd = (char *)p;

    return (sign) ? -value : value;
  }

  float CALL_CONV strtof(const char *string, char **stringEnd) { return (float)strtod(string, stringEnd); }

  // msvc specific definitions to make the compiler work without UCRT
#if COMPLEX_MSVC

  constinit int _fltused = 0;

  int __cdecl _purecall() { COMPLEX_TRAP(); }

  constinit __int64 __memset_nt_threshold = 33554432;
  constinit __int64 __memset_fast_string_threshold = 524288;
  constinit __int8 __favor = 1; // __FAVOR_ENFSTRG

  enum ISA_AVAILABILITY
  {
    __ISA_AVAILABLE_X86 = 0,
    __ISA_AVAILABLE_SSE2 = 1,
    __ISA_AVAILABLE_SSE42 = 2,
    __ISA_AVAILABLE_AVX = 3,
    __ISA_AVAILABLE_ENFSTRG = 4,
    __ISA_AVAILABLE_AVX2 = 5,
    __ISA_AVAILABLE_AVX512 = 6,

    __ISA_AVAILABLE_ARMNT = 0,   // minimum Win8 ARM support (but w/o NEON)
    __ISA_AVAILABLE_NEON = 1,   // support for 128-bit NEON instructions
    __ISA_AVAILABLE_NEON_ARM64 = 2,// support for 128-bit NEON instructions for ARM64. The distinction between ARM32 and
    // ARM64 NEON is temporary. They may eventually be merged.
  };

  constinit __int32 __isa_available = __ISA_AVAILABLE_SSE42;

  #include <corecrt_math.h>

  short __cdecl 
  _fdtest(float *x)
  {
    auto value = utils::bit_cast<u32>(*x);
    bool isInf = (value & kFloatExponentMask) == kFloatExponentMask;
    bool isSubnormal = (value & kFloatExponentMask) == 0;
    bool hasMantissaBits = (value & kFloatMantissaMask) > 0;
    return (isSubnormal && !hasMantissaBits) ? FP_ZERO : 
           (isSubnormal                    ) ? FP_SUBNORMAL :
           (isInf && hasMantissaBits       ) ? FP_NAN : 
           (isInf                          ) ? FP_INFINITE : 
                                               FP_NORMAL;
  }
  short __cdecl 
  _dtest(double *x)
  {
    auto value = utils::bit_cast<u64>(*x);
    bool isInf = (value & kDoubleExponentMask) == kDoubleExponentMask;
    bool isSubnormal = (value & kDoubleExponentMask) == 0;
    bool hasMantissaBits = (value & kDoubleMantissaMask) > 0;
    return (isSubnormal && !hasMantissaBits) ? FP_ZERO : 
           (isSubnormal                    ) ? FP_SUBNORMAL :
           (isInf && hasMantissaBits       ) ? FP_NAN : 
           (isInf                          ) ? FP_INFINITE : 
                                               FP_NORMAL;
  }
  short __cdecl _ldtest(long double *x) { return _dtest((double *)x); }

  short __cdecl _fdclass(float x) { return _fdtest(&x); }
  short __cdecl _dclass(double x) { return _dtest(&x); }
  short __cdecl _ldclass(long double x) { return _dclass((double)x); }

  __declspec(dllimport) void _stdcall OutputDebugStringA(const char *lpOutputString);
  __declspec(dllimport) void _stdcall OutputDebugStringW(const wchar_t *lpOutputString);
  void __cdecl _wassert(wchar_t const *message, wchar_t const *file, unsigned line)
  {
    OutputDebugStringW(file);
    char string[64];
    stbsp_snprintf(string, countof(string), ": line %d", line);
    OutputDebugStringA(string);
    OutputDebugStringW(L": Assertion fail: ");
    OutputDebugStringW(message);
    OutputDebugStringA("\n\n");

    __debugbreak();
  }

  #include <stdio.h>

  static constinit FILE stdInput = { ._Placeholder = 0 };
  static constinit FILE stdOutput = { ._Placeholder = (void *)1 };
  static constinit FILE stdError = { ._Placeholder = (void *)2 };

  FILE *__cdecl 
  __acrt_iob_func(unsigned index)
  {
    return (index == 0) ? &stdInput : (index == 1) ? &stdOutput : &stdError;
  }

  int __cdecl 
  __stdio_common_vfprintf(unsigned __int64, FILE *,
    char const *format, _locale_t, va_list args)
  {
    return printVariadic(format, args);
  }

  int stbsp_vsnprintf(char *buf, int count, char const *fmt, va_list va);

  int __cdecl 
  __stdio_common_vsprintf(unsigned __int64, char *buffer, size_t size, 
    char const *format, _locale_t, va_list args)
  {
    return stbsp_vsnprintf(buffer, (int)size, format, args);
  }



  struct image_dos_header { // DOS .EXE header
    u16 e_magic;            // Magic number
    u16 e_cblp;             // Bytes on last page of file
    u16 e_cp;               // Pages in file
    u16 e_crlc;             // Relocations
    u16 e_cparhdr;          // Size of header in paragraphs
    u16 e_minalloc;         // Minimum extra paragraphs needed
    u16 e_maxalloc;         // Maximum extra paragraphs needed
    u16 e_ss;               // Initial (relative) SS value
    u16 e_sp;               // Initial SP value
    u16 e_csum;             // Checksum
    u16 e_ip;               // Initial IP value
    u16 e_cs;               // Initial (relative) CS value
    u16 e_lfarlc;           // File address of relocation table
    u16 e_ovno;             // Overlay number
    u16 e_res[4];           // Reserved words
    u16 e_oemid;            // OEM identifier (for e_oeminfo)
    u16 e_oeminfo;          // OEM information; e_oemid specific
    u16 e_res2[10];         // Reserved words
    i32 e_lfanew;           // File address of new exe header
  };

  extern "C" image_dos_header __ImageBase;
  
  struct startupinfo
  {
    u32      cb;
    wchar_t *lpReserved;
    wchar_t *lpDesktop;
    wchar_t *lpTitle;
    u32      dwX;
    u32      dwY;
    u32      dwXSize;
    u32      dwYSize;
    u32      dwXCountChars;
    u32      dwYCountChars;
    u32      dwFillAttribute;
    u32      dwFlags;
    u16      wShowWindow;
    u16      cbReserved2;
    byte *   lpReserved2;
    void *   hStdInput;
    void *   hStdOutput;
    void *   hStdError;
  };
  __declspec(dllimport) void __stdcall GetStartupInfoW(startupinfo *lpStartupInfo);

  int _DllMainCRTStartup() { return 1; }

#if COMPLEX_STANDALONE

  __declspec(dllimport) void __stdcall ExitProcess(u32 uExitCode);
  int __stdcall WinMain(void *hInst, void *hPrevInst, char *cmdline, int cmdshow);
  int 
  WinMainCRTStartup(void *)
  {
    startupinfo info{};
    GetStartupInfoW(&info);

    int windowMode = info.dwFlags & 0x00000001 /* STARTF_USESHOWWINDOW */
      ? info.wShowWindow : 10 /* SW_SHOWDEFAULT */;

    ExitProcess(WinMain((void *)(&__ImageBase), nullptr, nullptr, windowMode));

    return 0;
  }

#endif

#endif
}

#undef CALL_CONV
