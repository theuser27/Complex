
// Created: 2021-05-22 18:22:20

#pragma once

#include "platform.hpp"
#include "stl_utils.hpp"
#include "constants.hpp"

// the project cannot run without vectorisation (either x86 SSE4.1 or ARM NEON, however it can run without FMA)
#if COMPLEX_SSE4_1
  #include <smmintrin.h>

  extern "C"
  {
    __m128 _mm_fmadd_ps(__m128, __m128, __m128);
    __m128 _mm_fmsub_ps(__m128, __m128, __m128);
  }
#elif COMPLEX_NEON
  #include <arm_neon.h>
#else
  #error Either SSE4.1 or ARM NEON is needed for this program to work
#endif



#if defined(COMPLEX_MSVC) && !defined(_mm_undefined_si128)
  #define _mm_undefined_si128 _mm_setzero_si128
#endif

namespace utils
{
  inline constexpr u32 kFullMask = u32(-1);
  inline constexpr u32 kNoChangeMask = u32(-1);
  inline constexpr u32 kSignMask = 1U << 31;
  inline constexpr u32 kNotSignMask = kSignMask - 1;

  // u32/i32 x 4
  struct alignas(COMPLEX_SIMD_ALIGNMENT) i4
  {
  #if COMPLEX_SSE4_1
    using simdtype = __m128i;
    static constexpr usize size = 4;
  #elif COMPLEX_NEON
    using simdtype = uint32x4_t;
    static constexpr usize size = 4;
  #endif
    using array_t = utils::array<u32, size>;

    simdtype value;


    ////////////////////////////
    // Method Implementations //
    ////////////////////////////

    static forceinline simdtype vectorcall init(u32 scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_set1_epi32((i32)scalar);
    #elif COMPLEX_NEON
      return vdupq_n_u32(scalar);
    #endif
    }

    static forceinline simdtype vectorcall load(const u32 *memory)
    {
    #if COMPLEX_SSE4_1
      return _mm_loadu_si128((const __m128i *)memory);
    #elif COMPLEX_NEON
      return vld1q_u32(memory);
    #endif
    }

    static forceinline simdtype vectorcall add(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_add_epi32(one, two);
    #elif COMPLEX_NEON
      return vaddq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall sub(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_epi32(one, two);
    #elif COMPLEX_NEON
      return vsubq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall neg(simdtype value)
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_epi32(_mm_setzero_si128(), value);
    #elif COMPLEX_NEON
      return vmulq_n_u32(value, (u32)(-1));
    #endif
    }

    static forceinline simdtype vectorcall mul(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_mullo_epi32(one, two);
    #elif COMPLEX_NEON
      return vmulq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall bitAnd(simdtype value, simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_and_si128(value, mask);
    #elif COMPLEX_NEON
      return vandq_u32(value, mask);
    #endif
    }

    static forceinline simdtype vectorcall bitOr(simdtype value, simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_or_si128(value, mask);
    #elif COMPLEX_NEON
      return vorrq_u32(value, mask);
    #endif
    }

    static forceinline simdtype vectorcall bitXor(simdtype value, simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_si128(value, mask);
    #elif COMPLEX_NEON
      return veorq_u32(value, mask);
    #endif
    }

    static forceinline simdtype vectorcall bitNot(simdtype value)
    {
    #if COMPLEX_SSE4_1
      auto dummy = _mm_undefined_si128();
      return _mm_xor_si128(value, _mm_cmpeq_epi32(dummy, dummy));
    #elif COMPLEX_NEON
      return vmvnq_u32(value);
    #endif
    }

    static forceinline simdtype vectorcall equal(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_cmpeq_epi32(one, two);
    #elif COMPLEX_NEON
      return vceqq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall maxSigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_epi32(one, two);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_s32(vmaxq_s32(vreinterpretq_s32_u32(one), vreinterpretq_s32_u32(two)));
    #endif
    }

    static forceinline simdtype vectorcall maxUnsigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_epu32(one, two);
    #elif COMPLEX_NEON
      return vmaxq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall minSigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_epi32(one, two);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_s32(vminq_s32(vreinterpretq_s32_u32(one), vreinterpretq_s32_u32(two)));
    #endif
    }

    static forceinline simdtype vectorcall minUnsigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_epu32(one, two);
    #elif COMPLEX_NEON
      return vminq_u32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall greaterThanSigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_cmpgt_epi32(one, two);
    #elif COMPLEX_NEON
      return vcgtq_s32(vreinterpretq_s32_u32(one), vreinterpretq_s32_u32(two));
    #endif
    }

    static forceinline simdtype vectorcall greaterThanUnsigned(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return equal(maxUnsigned(one, two), one);
    #elif COMPLEX_NEON
      return vcgtq_u32(one, two);
    #endif
    }

    static forceinline u32 vectorcall sum(simdtype value) {
    #if COMPLEX_SSE4_1
      simdtype flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(1, 0, 3, 2));
      value = _mm_add_epi32(value, flip);
      flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(2, 3, 0, 1));
      return _mm_cvtsi128_si32(_mm_add_epi32(value, flip));
    #elif COMPLEX_NEON
      uint32x2_t sum = vpadd_u32(vget_low_u32(value), vget_high_u32(value));
      sum = vpadd_u32(sum, sum);
      return vget_lane_u32(sum, 0);
    #endif
    }

    static forceinline u32 vectorcall anyMask(simdtype value)
    {
    #if COMPLEX_SSE4_1
      return _mm_movemask_epi8(value);
    #elif COMPLEX_NEON
      uint32x2_t max = vpmax_u32(vget_low_u32(value), vget_high_u32(value));
      max = vpmax_u32(max, max);
      return vget_lane_u32(max, 0);
    #endif
    }


    //////////////////
    // Method Calls //
    //////////////////

    static forceinline i4 vectorcall maxSigned(i4 one, i4 two)
    {	return maxSigned(one.value, two.value); }

    static forceinline i4 vectorcall maxUnsigned(i4 one, i4 two)
    {	return maxUnsigned(one.value, two.value); }

    static forceinline i4 vectorcall minSigned(i4 one, i4 two)
    { return minSigned(one.value, two.value); }

    static forceinline i4 vectorcall minUnsigned(i4 one, i4 two)
    { return minUnsigned(one.value, two.value); }

    static forceinline i4 vectorcall clampSigned(i4 low, i4 high, i4 value)
    { return maxSigned(minSigned(value.value, high.value), low.value); }

    static forceinline i4 vectorcall clampUnsigned(i4 low, i4 high, i4 value)
    { return maxUnsigned(minUnsigned(value.value, high.value), low.value); }

    static forceinline i4 vectorcall equal(i4 one, i4 two)
    {	return equal(one.value, two.value); }

    static forceinline i4 vectorcall notEqual(i4 one, i4 two)
    {
      // DO NOT try to optimise this by using the fp version on intel
      // because that one is subject to FTZ and DAZ flags
      return ~equal(one, two);
    }

    static forceinline i4 vectorcall greaterThanSigned(i4 one, i4 two)
    {	return greaterThanSigned(one.value, two.value); }

    static forceinline i4 vectorcall lessThanSigned(i4 one, i4 two)
    {	return greaterThanSigned(two.value, one.value); }

    static forceinline i4 vectorcall greaterThanOrEqualSigned(i4 one, i4 two)
    {	return ~lessThanSigned(one, two); }

    static forceinline i4 vectorcall lessThanOrEqualSigned(i4 one, i4 two)
    {	return ~greaterThanSigned(one, two); }

    static forceinline u32 vectorcall sum(i4 value)
    { return sum(value.value); }

    static forceinline u32 vectorcall anyMask(i4 value)
    { return anyMask(value.value); }

    static forceinline bool vectorcall allSame(i4 value)
    {
    #if COMPLEX_SSE4_1
      simdtype mask = equal(value.value, _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 3, 0, 1)));
      mask = bitAnd(mask, equal(value.value, _mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3))));
      return anyMask(mask);
    #elif COMPLEX_NEON
      return vaddvq_u32(notEqual(value.value, vdupq_laneq_u32(value.value, 0)).value) == 0U;
    #endif
    }


    //////////////////
    // Constructors //
    //////////////////

  #ifdef COMPLEX_MSVC
  private:
    // i hate unions
    template<usize ... N>
    static constexpr auto getInternalValue(const auto &scalars, utils::index_sequence<N...>) noexcept
    { return simdtype{ .m128i_u32 = { scalars[N]... } }; }
  public:
  #endif

    constexpr forceinline i4() noexcept : i4{ 0 } { }
    constexpr forceinline i4(u32 initialValue) noexcept
    {
      if (utils::is_constant_evaluated())
      {
        array_t values{};
        values.fill(initialValue);
      #ifdef COMPLEX_MSVC
        value = getInternalValue(values, utils::make_index_sequence<size>());
      #else
        value = utils::bit_cast<simdtype>(values);
      #endif
      }
      else
        value = init(initialValue);
    }
    constexpr forceinline i4(simdtype initialValue) noexcept : value(initialValue) { }
    constexpr forceinline i4(const array_t &scalars) noexcept :
  #ifdef COMPLEX_MSVC
      value(getInternalValue(scalars, utils::make_index_sequence<size>())) { }
  #else
      value(utils::bit_cast<simdtype>(scalars)) { }
  #endif

    constexpr forceinline i4(u32 even, u32 odd) noexcept
    {
      array_t values{};
      for (usize i = 0; i < size; i += 2)
      {
        values[i] = even;
        values[i + 1] = odd;
      }

    #ifdef COMPLEX_MSVC
      value = getInternalValue(values, utils::make_index_sequence<size>());
    #else
      value = utils::bit_cast<simdtype>(values);
    #endif
    }


    forceinline void vectorcall set(usize index, u32 newValue) noexcept
    {
      auto scalars = utils::bit_cast<array_t>(value);
      scalars[index] = newValue;
      value = utils::bit_cast<simdtype>(scalars);
    }

    constexpr forceinline array_t getArrayOfValues() const noexcept
    {
    #ifdef COMPLEX_MSVC
      if (utils::is_constant_evaluated())
      {
        array_t array;
        for (usize i = 0; i < size; ++i)
          array[i] = value.m128i_u32[i];
        return array;
      }
    #endif
      return utils::bit_cast<array_t>(value);
    }

    template<typename T> requires ((sizeof(u32) * size) % sizeof(T) == 0)
    forceinline auto getArrayOfValues() const noexcept
    {	return utils::bit_cast<utils::array<T, ((sizeof(u32) * size) / sizeof(T))>>(value); }

    ///////////////
    // Operators //
    ///////////////

    //forceinline bool vectorcall operator==(i4 other) const noexcept
    //{ return anyMask(notEqual(*this, other)); }

    constexpr forceinline u32 vectorcall operator[](usize index) const noexcept
    { return getArrayOfValues()[index]; }

    forceinline i4 vectorcall operator+(i4 other) const noexcept
    { return add(value, other.value); }

    friend forceinline i4 vectorcall operator+(u32 one, i4 two) noexcept
    { return add(init(one), two.value); }

    forceinline i4 vectorcall operator-(i4 other) const noexcept
    { return sub(value, other.value); }

    friend forceinline i4 vectorcall operator-(u32 one, i4 two) noexcept
    { return sub(init(one), two.value); }

    forceinline i4 vectorcall operator*(i4 other) const noexcept
    { return mul(value, other.value); }

    friend forceinline i4 vectorcall operator*(u32 one, i4 two) noexcept
    { return mul(init(one), two.value); }

    forceinline i4 vectorcall operator&(i4 other) const noexcept
    { return bitAnd(value, other.value); }

    friend forceinline i4 vectorcall operator&(u32 one, i4 two) noexcept
    { return bitAnd(init(one), two.value); }

    forceinline i4 vectorcall operator|(i4 other) const noexcept
    { return bitOr(value, other.value); }

    friend forceinline i4 vectorcall operator|(u32 one, i4 two) noexcept
    { return bitOr(init(one), two.value); }

    forceinline i4 vectorcall operator^(i4 other) const noexcept
    { return bitXor(value, other.value); }

    friend forceinline i4 vectorcall operator^(u32 one, i4 two) noexcept
    { return bitXor(init(one), two.value); }

    forceinline i4 vectorcall operator-() const noexcept
    { return neg(value); }

    forceinline i4 vectorcall operator~() const noexcept
    { return bitNot(value); }

    forceinline i4 &vectorcall operator+=(i4 other) noexcept
    { value = add(value, other.value); return *this; }

    forceinline i4 &vectorcall operator-=(i4 other) noexcept
    { value = sub(value, other.value); return *this; }

    forceinline i4 &vectorcall operator*=(i4 other) noexcept
    { value = mul(value, other.value); return *this; }

    forceinline i4 &vectorcall operator&=(i4 other) noexcept
    { value = bitAnd(value, other.value); return *this; }

    forceinline i4 &vectorcall operator|=(i4 other) noexcept
    { value = bitOr(value, other.value); return *this; }

    forceinline i4 &vectorcall operator^=(i4 other) noexcept
    { value = bitXor(value, other.value); return *this; }
  };

  // u32/i32 x 4 mask type
  using i4mask = i4;

  // f32 x 4
  struct alignas(COMPLEX_SIMD_ALIGNMENT) f4
  {
  #if COMPLEX_SSE4_1
    using simdtype = __m128;
    using mask_simdtype = __m128i;
  #elif COMPLEX_NEON
    using simdtype = float32x4_t;
    using mask_simdtype = uint32x4_t;
  #endif
    static constexpr usize size = sizeof(simdtype) / sizeof(float);
    static constexpr usize complexSize = size / 2;
    using array_t = utils::array<float, size>;

    simdtype value;


    ////////////////////////////
    // Methods Implementation //
    ////////////////////////////

    static forceinline mask_simdtype vectorcall toMask(simdtype value)
    {
    #if COMPLEX_SSE4_1
      return _mm_castps_si128(value);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_f32(value);
    #endif
    }

    static forceinline simdtype vectorcall toSimd(mask_simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_castsi128_ps(mask);
    #elif COMPLEX_NEON
      return vreinterpretq_f32_u32(mask);
    #endif
    }

    static forceinline simdtype vectorcall init(float scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_set1_ps(scalar);
    #elif COMPLEX_NEON
      return vdupq_n_f32(scalar);
    #endif
    }

    static forceinline simdtype vectorcall load(const float *memory)
    {
    #if COMPLEX_SSE4_1
      return _mm_loadu_ps(memory);
    #elif COMPLEX_NEON
      return vld1q_f32(memory);
    #endif
    }

    static forceinline simdtype vectorcall add(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_add_ps(one, two);
    #elif COMPLEX_NEON
      return vaddq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall sub(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_ps(one, two);
    #elif COMPLEX_NEON
      return vsubq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall neg(simdtype value)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
    #elif COMPLEX_NEON
      return vnegq_f32(value);
    #endif
    }

    static forceinline simdtype vectorcall mul(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_mul_ps(one, two);
    #elif COMPLEX_NEON
      return vmulq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall mulScalar(simdtype value, float scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_mul_ps(value, _mm_set1_ps(scalar));
    #elif COMPLEX_NEON
      return vmulq_n_f32(value, scalar);
    #endif
    }

    static forceinline simdtype vectorcall mulAdd(simdtype add, simdtype mulOne, simdtype mulTwo)
    {
    #if COMPLEX_SSE4_1
    #if COMPLEX_FMA
      return _mm_fmadd_ps(mulOne, mulTwo, add);
    #else
      return _mm_add_ps(add, _mm_mul_ps(mulOne, mulTwo));
    #endif
    #elif COMPLEX_NEON
      return vfmaq_f32(add, mulOne, mulTwo);
    #endif
    }

    static forceinline simdtype vectorcall mulSub(simdtype sub, simdtype mulOne, simdtype mulTwo)
    {
    #if COMPLEX_SSE4_1
    #if COMPLEX_FMA
      return _mm_fmsub_ps(mulOne, mulTwo, sub);
    #else
      return _mm_sub_ps(_mm_mul_ps(mulOne, mulTwo), sub);
    #endif
    #elif COMPLEX_NEON
      return vfmaq_f32(neg(sub), mulOne, mulTwo);
    #endif
    }

    static forceinline simdtype vectorcall div(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_div_ps(one, two);
    #elif COMPLEX_NEON
      return vdivq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall sqrt(simdtype value) noexcept
    {
    #if COMPLEX_SSE4_1
      return _mm_sqrt_ps(value);
    #elif COMPLEX_NEON
      return vsqrtq_f32(value);
    #endif
    }

    static forceinline simdtype vectorcall invSqrt(simdtype value) noexcept
    {
    #if COMPLEX_SSE4_1
      return _mm_rsqrt_ps(value);
    #elif COMPLEX_NEON
      return vrsqrteq_f32(value);
    #endif
    }

    static forceinline simdtype vectorcall bitAnd(simdtype value, mask_simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_and_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(vandq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simdtype vectorcall bitOr(simdtype value, mask_simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_or_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(vorrq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simdtype vectorcall bitXor(simdtype value, mask_simdtype mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(veorq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simdtype vectorcall bitNot(simdtype value)
    {
    #if COMPLEX_SSE4_1
      auto dummy = _mm_undefined_si128();
      return _mm_xor_ps(value, toSimd(_mm_cmpeq_epi32(dummy, dummy)));
    #elif COMPLEX_NEON
      return toSimd(vmvnq_u32(toMask(value)));
    #endif
    }

    static forceinline simdtype vectorcall max(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_ps(one, two);
    #elif COMPLEX_NEON
      return vmaxq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall min(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_ps(one, two);
    #elif COMPLEX_NEON
      return vminq_f32(one, two);
    #endif
    }

    static forceinline simdtype vectorcall truncate(simdtype values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndq_f32(values);
    #endif
    }

    static forceinline simdtype vectorcall floor(simdtype values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndmq_f32(values);
    #endif
    }

    static forceinline simdtype vectorcall ceil(simdtype values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndpq_f32(values);
    #endif
    }

    static forceinline simdtype vectorcall round(simdtype values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndnq_f32(values);
    #endif
    }

    static forceinline simdtype vectorcall abs(simdtype value)
    {
      static constexpr i4mask mask = kNotSignMask;
    #if COMPLEX_SSE4_1
      return _mm_and_ps(value, toSimd(mask.value));
    #elif COMPLEX_NEON
      return toSimd(vandq_u32(toMask(value), mask.value));
    #endif
    }

    static forceinline mask_simdtype vectorcall signMask(simdtype value)
    { return toMask(bitAnd(value, i4mask::init(kSignMask))); }

    static forceinline mask_simdtype vectorcall equal(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpeq_ps(one, two));
    #elif COMPLEX_NEON
      return vceqq_f32(one, two);
    #endif
    }

    static forceinline mask_simdtype vectorcall greaterThan(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpgt_ps(one, two));
    #elif COMPLEX_NEON
      return vcgtq_f32(one, two);
    #endif
    }

    static forceinline mask_simdtype vectorcall greaterThanOrEqual(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpge_ps(one, two));
    #elif COMPLEX_NEON
      return vcgeq_f32(one, two);
    #endif
    }

    static forceinline mask_simdtype vectorcall notEqual(simdtype one, simdtype two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpneq_ps(one, two));
    #elif COMPLEX_NEON
      return i4mask::bitNot(equal(one, two));
    #endif
    }

    static forceinline float vectorcall sum(simdtype value)
    {
    #if COMPLEX_SSE4_1
      simdtype flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2));
      value = _mm_add_ps(value, flip);
      flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(2, 3, 0, 1));
      return _mm_cvtss_f32(_mm_add_ps(value, flip));
    #elif COMPLEX_NEON
      float32x2_t sum = vpadd_f32(vget_low_f32(value), vget_high_f32(value));
      sum = vpadd_f32(sum, sum);
      return vget_lane_f32(sum, 0);
    #endif
    }

    static forceinline mask_simdtype vectorcall isNan(simdtype value)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpunord_ps(value, value));
    #elif COMPLEX_NEON
      return i4mask::bitNot(vceqq_f32(value, value));
    #endif
    }


    //////////////////
    // Method Calls //
    //////////////////

    static forceinline i4mask vectorcall toMask(f4 value)
    {
    #if COMPLEX_SSE4_1
      return _mm_castps_si128(value.value);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_f32(value.value);
    #endif
    }

    static forceinline f4 vectorcall mulAdd(f4 add, f4 mulOne, f4 mulTwo)
    { return mulAdd(add.value, mulOne.value, mulTwo.value);	}

    static forceinline f4 vectorcall mulSub(f4 sub, f4 mulOne, f4 mulTwo)
    { return mulSub(sub.value, mulOne.value, mulTwo.value); }

    static forceinline f4 vectorcall sqrt(f4 value)
    { return sqrt(value.value); }

    static forceinline f4 vectorcall invSqrt(f4 value)
    { return invSqrt(value.value); }

    static forceinline f4 vectorcall max(f4 one, f4 two)
    { return max(one.value, two.value); }

    static forceinline f4 vectorcall min(f4 one, f4 two)
    { return min(one.value, two.value);	}

    static forceinline f4 vectorcall clamp(f4 value, f4 low, f4 high)
    { return max(min(value.value, high.value), low.value); }

    static forceinline f4 vectorcall truncate(f4 value)
    { return truncate(value.value); }

    static forceinline f4 vectorcall floor(f4 value)
    { return floor(value.value); }

    static forceinline f4 vectorcall ceil(f4 value)
    { return ceil(value.value); }

    static forceinline f4 vectorcall round(f4 value)
    { return round(value.value); }

    static forceinline f4 vectorcall abs(f4 value)
    { return abs(value.value); }

    static forceinline i4mask vectorcall signMask(f4 value)
    { return signMask(value.value); }

    static forceinline i4mask vectorcall equal(f4 one, f4 two)
    { return equal(one.value, two.value);	}

    static forceinline i4mask vectorcall notEqual(f4 one, f4 two)
    { return notEqual(one.value, two.value); }

    static forceinline i4mask vectorcall greaterThan(f4 one, f4 two)
    { return greaterThan(one.value, two.value);	}

    static forceinline i4mask vectorcall greaterThanOrEqual(f4 one, f4 two)
    { return greaterThanOrEqual(one.value, two.value); }

    static forceinline i4mask vectorcall lessThan(f4 one, f4 two)
    { return greaterThan(two.value, one.value); }

    static forceinline i4mask vectorcall lessThanOrEqual(f4 one, f4 two)
    { return greaterThanOrEqual(two.value, one.value); }

    static forceinline float vectorcall sum(f4 value)
    { return sum(value.value); }

    static forceinline bool vectorcall allSame(f4 value)
    { return i4::allSame(toMask(value)); }

    static forceinline i4mask vectorcall isNan(f4 value)
    { return isNan(value.value); }

    static forceinline bool vectorcall allEqual(f4 one, f4 two)
    { return i4mask::anyMask(notEqual(one, two)) == 0; }


    //////////////////////////////////
    // Constructors and Destructors //
    //////////////////////////////////

  #ifdef COMPLEX_MSVC
  private:
    // i hate unions
    template<usize ... N>
    static constexpr auto getInternalValue(const auto &scalars, utils::index_sequence<N...>) noexcept
    { return simdtype{ .m128_f32 = { scalars[N]... } }; }
  public:
  #endif

    constexpr forceinline f4() noexcept : f4{ 0.0f } { }
    constexpr forceinline f4(float initialValue) noexcept
    {
      if (utils::is_constant_evaluated())
      {
        array_t values{};
        values.fill(initialValue);
      #ifdef COMPLEX_MSVC
        value = getInternalValue(values, utils::make_index_sequence<size>());
      #else
        value = utils::bit_cast<simdtype>(values);
      #endif
      }
      else
        value = init(initialValue);
    }
    constexpr forceinline f4(simdtype initialValue) noexcept : value(initialValue) { }
    constexpr forceinline f4(const array_t &scalars) noexcept :
    #ifdef COMPLEX_MSVC
      value(getInternalValue(scalars, utils::make_index_sequence<size>())) { }
    #else
      value(utils::bit_cast<simdtype>(scalars)) { }
    #endif

    template<typename T, usize N> requires (sizeof(simdtype) == sizeof(T) * N)
    forceinline f4(const utils::array<T, N> &scalars) noexcept :
      value(utils::bit_cast<simdtype>(scalars)) { }

    constexpr forceinline f4(float even, float odd) noexcept
    {
      array_t values{};
      for (usize i = 0; i < size; i += 2)
      {
        values[i] = even;
        values[i + 1] = odd;
      }

    #ifdef COMPLEX_MSVC
      value = getInternalValue(values, utils::make_index_sequence<size>());
    #else
      value = utils::bit_cast<simdtype>(values);
    #endif
    }

    forceinline void vectorcall set(usize index, float newValue) noexcept
    {
      auto scalars = utils::bit_cast<array_t>(value);
      scalars[index] = newValue;
      value = utils::bit_cast<simdtype>(scalars);
    }

    constexpr forceinline array_t vectorcall getArrayOfValues() const noexcept
    {
    #ifdef COMPLEX_MSVC
      array_t array;
      array.fill(value.m128_f32);
      return array;
    #else
      return utils::bit_cast<array_t>(value);
    #endif
    }

    template<typename T> requires ((sizeof(float) * size) % sizeof(T) == 0)
    forceinline auto vectorcall getArrayOfValues() const noexcept
    { return utils::bit_cast<utils::array<T, ((sizeof(float) * size) / sizeof(T))>>(value); }

    ///////////////
    // Operators //
    ///////////////

    forceinline bool vectorcall operator==(f4 other) const noexcept
    { return i4mask::anyMask(notEqual(*this, other)) == 0; }

    constexpr forceinline float vectorcall operator[](usize index) const noexcept
    {
    #ifdef COMPLEX_MSVC
      return value.m128_f32[index];
    #else
      return utils::bit_cast<array_t>(value)[index];
    #endif
    }

    forceinline f4& vectorcall operator+=(f4 other) noexcept
    { value = add(value, other.value); return *this; }

    forceinline f4& vectorcall operator-=(f4 other) noexcept
    { value = sub(value, other.value); return *this; }

    forceinline f4& vectorcall operator*=(f4 other) noexcept
    { value = mul(value, other.value); return *this; }

    forceinline f4& vectorcall operator*=(float scalar) noexcept
    { value = mulScalar(value, scalar); return *this; }

    forceinline f4& vectorcall operator/=(f4 other) noexcept
    { value = div(value, other.value); return *this; }


    forceinline f4& vectorcall operator&=(i4mask other) noexcept
    { value = bitAnd(value, other.value); return *this; }

    forceinline f4& vectorcall operator|=(i4mask other) noexcept
    { value = bitOr(value, other.value); return *this; }

    forceinline f4& vectorcall operator^=(i4mask other) noexcept
    { value = bitXor(value, other.value); return *this; }

    forceinline f4& vectorcall operator&=(f4 other) noexcept
    { value = bitAnd(value, toMask(other.value)); return *this; }

    forceinline f4& vectorcall operator|=(f4 other) noexcept
    { value = bitOr(value, toMask(other.value)); return *this; }

    forceinline f4& vectorcall operator^=(f4 other) noexcept
    { value = bitXor(value, toMask(other.value)); return *this; }


    forceinline f4 vectorcall operator+(f4 other) const noexcept
    { return add(value, other.value); }

    forceinline friend f4 vectorcall operator+(float one, f4 two) noexcept
    { return add(init(one), two.value); }

    forceinline f4 vectorcall operator-(f4 other) const noexcept
    { return sub(value, other.value); }

    forceinline friend f4 vectorcall operator-(float one, f4 two) noexcept
    { return sub(init(one), two.value); }

    forceinline f4 vectorcall operator*(f4 other) const noexcept
    { return mul(value, other.value); }

    forceinline f4 vectorcall operator*(float scalar) const noexcept
    { return mulScalar(value, scalar); }

    forceinline friend f4 vectorcall operator*(float one, f4 two) noexcept
    { return mulScalar(two.value, one); }

    forceinline f4 vectorcall operator/(f4 other) const noexcept
    { return div(value, other.value); }

    forceinline friend f4 vectorcall operator/(float one, f4 two) noexcept
    { return div(init(one), two.value); }


    forceinline f4 vectorcall operator&(i4mask other) const noexcept
    { return bitAnd(value, other.value); }

    forceinline f4 vectorcall operator|(i4mask other) const noexcept
    { return bitOr(value, other.value); }

    forceinline f4 vectorcall operator^(i4mask other) const noexcept
    { return bitXor(value, other.value); }

    forceinline f4 vectorcall operator&(f4 other) const noexcept
    { return bitAnd(value, toMask(other.value)); }

    forceinline f4 vectorcall operator|(f4 other) const noexcept
    { return bitOr(value, toMask(other.value)); }

    forceinline f4 vectorcall operator^(f4 other) const noexcept
    { return bitXor(value, toMask(other.value)); }

    forceinline f4 vectorcall operator-() const noexcept
    { return neg(value); }

    forceinline f4 vectorcall operator~() const noexcept
    { return bitNot(value); }
  };

  template<typename T>
  concept SimdValue = utils::is_same_v<T, f4> || utils::is_same_v<T, i4>;

  // an in/output needs to be contained in a single simd
  // effectively an alias of complexSize
  inline constexpr u32 kChannelsPerInOut = f4::complexSize;

  inline constexpr auto kChannelMasks = []()
  {
    utils::array<i4mask, kChannelsPerInOut> channelMasks{};
    for (usize i = 0; i < channelMasks.size(); ++i)
    {
      utils::array<u32, i4mask::size> mask{};
      mask[2 * i] = kFullMask;
      mask[2 * i + 1] = kFullMask;
      channelMasks[i] = mask;
    }
    return channelMasks;
  }();
  inline constexpr i4mask kRealMask = { kFullMask, 0U };
  inline constexpr i4mask kImaginaryMask = { 0U, kFullMask };
  inline constexpr i4mask kMagnitudeMask = kRealMask;
  inline constexpr i4mask kPhaseMask = kImaginaryMask;
}

using utils::f4;
using utils::i4;
using utils::i4mask;
