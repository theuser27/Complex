
#include "Framework/platform_definitions.hpp"
#include "Framework/simd_values.hpp"
#include "Framework/simd_utils.hpp"

namespace utils
{
  // malloc replacement
  byte *allocate(usize size, usize alignment);
  // free replacement
  void deallocate(const void *memory);
}

namespace std
{
  enum class align_val_t : usize { };
}

extern "C++"
{
  [[nodiscard]] void *__cdecl operator new(usize size) { return utils::allocate(size); }
  [[nodiscard]] void *__cdecl operator new(usize size, std::align_val_t alignment)
  { return utils::allocate(size, (usize)alignment); }
  [[nodiscard]] void *__cdecl operator new[](usize size) { return utils::allocate(size); }
  [[nodiscard]] void *__cdecl operator new[](usize size, std::align_val_t alignment)
  { return utils::allocate(size, (usize)alignment); }

  void __cdecl operator delete(void *pointer) noexcept { utils::deallocate(pointer); }
  void __cdecl operator delete[](void *pointer) noexcept { utils::deallocate(pointer); }
  void __cdecl operator delete(void *pointer, usize) noexcept { utils::deallocate(pointer); }
  void __cdecl operator delete[](void *pointer, usize) noexcept { utils::deallocate(pointer); }
  void __cdecl operator delete(void *pointer, std::align_val_t) noexcept { utils::deallocate(pointer); }
  void __cdecl operator delete[](void *pointer, std::align_val_t) noexcept { utils::deallocate(pointer); }
}



extern "C"
{
  void *memset(void *dest, int ch, usize count);

  __declspec(restrict) void *malloc(usize size) { return utils::allocate(size); }
  __declspec(restrict) void *calloc(usize num, usize size) { return memset(utils::allocate(size * num), 0, size * num); }
  __declspec(restrict) void *realloc(void *pointer, usize new_size)
  {
    return nullptr;
    //return memset(utils::allocate(size * num), 0, size * num);
  }
  void free(void *pointer) { utils::deallocate(pointer); }

  int abs(int x) { return x & utils::max_limit<int>; }

  float floorf(float x) { return simd_float::floor(x)[0]; }
  float ceilf(float x) { return simd_float::ceil(x)[0]; }
  float roundf(float x) { return simd_float::round(x)[0]; }

  float sinf(float x) { return utils::cis(x)[1]; }
  float cosf(float x) { return utils::cis(x)[0]; }
  float tanf(float x) { return utils::tan(x)[0]; }
  float atan2f(float y, float x) { return utils::atan2(y, x)[0]; }

  float sqrtf(float x) { return simd_float::sqrt(x)[0]; }
  float expf(float x) { return utils::exp(x); }
  float powf(float x, float y) { return utils::pow(x, y); }
  float log10f(float x) { return utils::log10(x); }

  int tolower(int c)
  {
    if (c >= 'A' && c <= 'Z')
      return c + 'a' - 'A';
    return c;
  }


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
#endif
}


