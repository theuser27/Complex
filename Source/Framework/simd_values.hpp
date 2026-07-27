
// Created: 2021-05-22 18:22:20

#pragma once

#include "platform.hpp"
#include "stl_utils.hpp"
#include "constants.hpp"

// the project cannot run without vectorisation (either x86 SSE4.1 or ARM NEON, however it can run without FMA)
#if COMPLEX_SSE4_1
  #include <smmintrin.h>
#elif COMPLEX_NEON
  #include <arm_neon.h>
#else
  #error Either SSE4.1 or ARM NEON is needed for this program to work
#endif

extern "C"
{
  __m128 _mm_fmadd_ps(__m128, __m128, __m128);
  __m128 _mm_fmsub_ps(__m128, __m128, __m128);
}

#if defined(COMPLEX_MSVC) && !defined(_mm_undefined_si128)
  #define _mm_undefined_si128 _mm_setzero_si128
#endif

namespace utils
{
  inline constexpr u32 kFullMask = u32(-1);
  inline constexpr u32 kNoChangeMask = u32(-1);
  inline constexpr u32 kSignMask = 1U << 31;
  inline constexpr u32 kNotSignMask = kSignMask - 1;

  struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_int
  {
  #if COMPLEX_SSE4_1
    using simd_type = __m128i;
    static constexpr usize size = 4;
  #elif COMPLEX_NEON
    using simd_type = uint32x4_t;
    static constexpr usize size = 4;
  #endif
    using array_t = utils::array<u32, size>;

    simd_type value;


    ////////////////////////////
    // Method Implementations //
    ////////////////////////////

    static forceinline simd_type vectorcall init(u32 scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_set1_epi32((i32)scalar);
    #elif COMPLEX_NEON
      return vdupq_n_u32(scalar);
    #endif 
    }

    static forceinline simd_type vectorcall load(const u32 *memory)
    {
    #if COMPLEX_SSE4_1
      return _mm_loadu_si128((const __m128i *)memory);
    #elif COMPLEX_NEON
      return vld1q_u32(memory);
    #endif 
    }

    static forceinline simd_type vectorcall add(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_add_epi32(one, two);
    #elif COMPLEX_NEON
      return vaddq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall sub(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_epi32(one, two);
    #elif COMPLEX_NEON
      return vsubq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall neg(simd_type value)
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_epi32(_mm_setzero_si128(), value);
    #elif COMPLEX_NEON
      return vmulq_n_u32(value, (u32)(-1));
    #endif
    }

    static forceinline simd_type vectorcall mul(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_mullo_epi32(one, two);
    #elif COMPLEX_NEON
      return vmulq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall bitAnd(simd_type value, simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_and_si128(value, mask);
    #elif COMPLEX_NEON
      return vandq_u32(value, mask);
    #endif
    }

    static forceinline simd_type vectorcall bitOr(simd_type value, simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_or_si128(value, mask);
    #elif COMPLEX_NEON
      return vorrq_u32(value, mask);
    #endif
    }

    static forceinline simd_type vectorcall bitXor(simd_type value, simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_si128(value, mask);
    #elif COMPLEX_NEON
      return veorq_u32(value, mask);
    #endif
    }

    static forceinline simd_type vectorcall bitNot(simd_type value)
    {
    #if COMPLEX_SSE4_1
      auto dummy = _mm_undefined_si128();
      return _mm_xor_si128(value, _mm_cmpeq_epi32(dummy, dummy));
    #elif COMPLEX_NEON
      return vmvnq_u32(value);
    #endif
    }

    static forceinline simd_type vectorcall equal(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_cmpeq_epi32(one, two);
    #elif COMPLEX_NEON
      return vceqq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall maxSigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_epi32(one, two);
    #elif COMPLEX_NEON
      return vmaxq_s32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall maxUnsigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_epu32(one, two);
    #elif COMPLEX_NEON
      return vmaxq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall minSigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_epi32(one, two);
    #elif COMPLEX_NEON
      return vminq_s32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall minUnsigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_epu32(one, two);
    #elif COMPLEX_NEON
      return vminq_u32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall greaterThanSigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_cmpgt_epi32(one, two);
    #elif COMPLEX_NEON
      return vcgtq_s32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall greaterThanUnsigned(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return equal(maxUnsigned(one, two), one);
    #elif COMPLEX_NEON
      return vcgtq_u32(one, two);
    #endif
    }

    static forceinline u32 vectorcall sum(simd_type value) {
    #if COMPLEX_SSE4_1
      simd_type flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(1, 0, 3, 2));
      value = _mm_add_epi32(value, flip);
      flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(2, 3, 0, 1));
      return _mm_cvtsi128_si32(_mm_add_epi32(value, flip));
    #elif COMPLEX_NEON
      uint32x2_t sum = vpadd_u32(vget_low_u32(value), vget_high_u32(value));
      sum = vpadd_u32(sum, sum);
      return vget_lane_u32(sum, 0);
    #endif
    }

    static forceinline u32 vectorcall anyMask(simd_type value)
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

    static forceinline simd_int vectorcall maxSigned(simd_int one, simd_int two)
    {	return maxSigned(one.value, two.value); }

    static forceinline simd_int vectorcall maxUnsigned(simd_int one, simd_int two)
    {	return maxUnsigned(one.value, two.value); }

    static forceinline simd_int vectorcall minSigned(simd_int one, simd_int two)
    { return minSigned(one.value, two.value); }

    static forceinline simd_int vectorcall minUnsigned(simd_int one, simd_int two)
    { return minUnsigned(one.value, two.value); }

    static forceinline simd_int vectorcall clampSigned(simd_int low, simd_int high, simd_int value)
    { return maxSigned(minSigned(value.value, high.value), low.value); }

    static forceinline simd_int vectorcall clampUnsigned(simd_int low, simd_int high, simd_int value)
    { return maxUnsigned(minUnsigned(value.value, high.value), low.value); }

    static forceinline simd_int vectorcall equal(simd_int one, simd_int two)
    {	return equal(one.value, two.value); }

    static forceinline simd_int vectorcall notEqual(simd_int one, simd_int two)
    {
      // DO NOT try to optimise this by using the fp version on intel
      // because that one is subject to FTZ and DAZ flags
      return ~equal(one, two);
    }

    static forceinline simd_int vectorcall greaterThanSigned(simd_int one, simd_int two)
    {	return greaterThanSigned(one.value, two.value); }

    static forceinline simd_int vectorcall lessThanSigned(simd_int one, simd_int two)
    {	return greaterThanSigned(two.value, one.value); }

    static forceinline simd_int vectorcall greaterThanOrEqualSigned(simd_int one, simd_int two)
    {	return ~lessThanSigned(one, two); }

    static forceinline simd_int vectorcall lessThanOrEqualSigned(simd_int one, simd_int two)
    {	return ~greaterThanSigned(one, two); }

    static forceinline u32 vectorcall sum(simd_int value)
    { return sum(value.value); }

    static forceinline u32 vectorcall anyMask(simd_int value)
    { return anyMask(value.value); }

    static forceinline bool vectorcall allSame(simd_int value)
    {
    #if COMPLEX_SSE4_1
      simd_type mask = equal(value.value, _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 3, 0, 1)));
      mask = bitAnd(mask, equal(value.value, _mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3))));
      return anyMask(mask);
    #elif COMPLEX_NEON
      return vaddvq_u32(notEqual(value.value, vdupq_laneq_u32(value.value, 0)).value.value) == 0U;
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
    { return simd_type{ .m128i_u32 = { scalars[N]... } }; }
  public:
  #endif

    constexpr forceinline simd_int() noexcept : simd_int{ 0 } { }
    constexpr forceinline simd_int(u32 initialValue) noexcept
    {
      if (utils::is_constant_evaluated())
      {
        array_t values{};
        values.fill(initialValue);
      #ifdef COMPLEX_MSVC
        value = getInternalValue(values, utils::make_index_sequence<size>());
      #else
        value = utils::bit_cast<simd_type>(values);
      #endif
      }
      else
        value = init(initialValue);
    }
    constexpr forceinline simd_int(simd_type initialValue) noexcept : value(initialValue) { }
    constexpr forceinline simd_int(const array_t &scalars) noexcept :
  #ifdef COMPLEX_MSVC
      value(getInternalValue(scalars, utils::make_index_sequence<size>())) { }
  #else
      value(utils::bit_cast<simd_type>(scalars)) { }
  #endif

    constexpr forceinline simd_int(u32 even, u32 odd) noexcept
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
      value = utils::bit_cast<simd_type>(values);
    #endif
    }


    forceinline void vectorcall set(usize index, u32 newValue) noexcept
    {
      auto scalars = utils::bit_cast<array_t>(value);
      scalars[index] = newValue;
      value = utils::bit_cast<simd_type>(scalars);
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

    //forceinline bool vectorcall operator==(simd_int other) const noexcept
    //{ return anyMask(notEqual(*this, other)); }

    constexpr forceinline u32 vectorcall operator[](usize index) const noexcept
    { return getArrayOfValues()[index]; }

    forceinline simd_int vectorcall operator+(simd_int other) const noexcept
    { return add(value, other.value); }

    friend forceinline simd_int vectorcall operator+(u32 one, simd_int two) noexcept
    { return add(init(one), two.value); }

    forceinline simd_int vectorcall operator-(simd_int other) const noexcept
    { return sub(value, other.value); }

    friend forceinline simd_int vectorcall operator-(u32 one, simd_int two) noexcept
    { return sub(init(one), two.value); }

    forceinline simd_int vectorcall operator*(simd_int other) const noexcept
    { return mul(value, other.value); }

    friend forceinline simd_int vectorcall operator*(u32 one, simd_int two) noexcept
    { return mul(init(one), two.value); }

    forceinline simd_int vectorcall operator&(simd_int other) const noexcept
    { return bitAnd(value, other.value); }

    friend forceinline simd_int vectorcall operator&(u32 one, simd_int two) noexcept
    { return bitAnd(init(one), two.value); }

    forceinline simd_int vectorcall operator|(simd_int other) const noexcept
    { return bitOr(value, other.value); }

    friend forceinline simd_int vectorcall operator|(u32 one, simd_int two) noexcept
    { return bitOr(init(one), two.value); }

    forceinline simd_int vectorcall operator^(simd_int other) const noexcept
    { return bitXor(value, other.value); }

    friend forceinline simd_int vectorcall operator^(u32 one, simd_int two) noexcept
    { return bitXor(init(one), two.value); }

    forceinline simd_int vectorcall operator-() const noexcept
    { return neg(value); }

    forceinline simd_int vectorcall operator~() const noexcept
    { return bitNot(value); }

    forceinline simd_int &vectorcall operator+=(simd_int other) noexcept
    { value = add(value, other.value); return *this; }

    forceinline simd_int &vectorcall operator-=(simd_int other) noexcept
    { value = sub(value, other.value); return *this; }

    forceinline simd_int &vectorcall operator*=(simd_int other) noexcept
    { value = mul(value, other.value); return *this; }

    forceinline simd_int &vectorcall operator&=(simd_int other) noexcept
    { value = bitAnd(value, other.value); return *this; }

    forceinline simd_int &vectorcall operator|=(simd_int other) noexcept
    { value = bitOr(value, other.value); return *this; }

    forceinline simd_int &vectorcall operator^=(simd_int other) noexcept
    { value = bitXor(value, other.value); return *this; }
  };

  using simd_mask = simd_int;

  struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_float
  {
  #if COMPLEX_SSE4_1
    using simd_type = __m128;
    using mask_simd_type = __m128i;
  #elif COMPLEX_NEON
    using simd_type = float32x4_t;
    using mask_simd_type = uint32x4_t;
  #endif
    static constexpr usize size = sizeof(simd_type) / sizeof(float);
    static constexpr usize complexSize = size / 2;
    using array_t = utils::array<float, size>;

    simd_type value;


    ////////////////////////////
    // Methods Implementation //
    ////////////////////////////

    static forceinline mask_simd_type vectorcall toMask(simd_type value)
    {
    #if COMPLEX_SSE4_1
      return _mm_castps_si128(value);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_f32(value);
    #endif
    }

    static forceinline simd_type vectorcall toSimd(mask_simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_castsi128_ps(mask);
    #elif COMPLEX_NEON
      return vreinterpretq_f32_u32(mask);
    #endif
    }

    static forceinline simd_type vectorcall init(float scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_set1_ps(scalar);
    #elif COMPLEX_NEON
      return vdupq_n_f32(scalar);
    #endif
    }

    static forceinline simd_type vectorcall load(const float *memory)
    {
    #if COMPLEX_SSE4_1
      return _mm_loadu_ps(memory);
    #elif COMPLEX_NEON
      return vld1q_f32(memory);
    #endif
    }

    static forceinline simd_type vectorcall add(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_add_ps(one, two);
    #elif COMPLEX_NEON
      return vaddq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall sub(simd_type one, simd_type two) 
    {
    #if COMPLEX_SSE4_1
      return _mm_sub_ps(one, two);
    #elif COMPLEX_NEON
      return vsubq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall neg(simd_type value)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
    #elif COMPLEX_NEON
      return vnegq_f32(value);
    #endif
    }

    static forceinline simd_type vectorcall mul(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_mul_ps(one, two);
    #elif COMPLEX_NEON
      return vmulq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall mulScalar(simd_type value, float scalar)
    {
    #if COMPLEX_SSE4_1
      return _mm_mul_ps(value, _mm_set1_ps(scalar));
    #elif COMPLEX_NEON
      return vmulq_n_f32(value, scalar);
    #endif
    }

    static forceinline simd_type vectorcall mulAdd(simd_type add, simd_type mulOne, simd_type mulTwo)
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

    static forceinline simd_type vectorcall mulSub(simd_type sub, simd_type mulOne, simd_type mulTwo)
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

    static forceinline simd_type vectorcall div(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_div_ps(one, two);
    #elif COMPLEX_NEON
      return vdivq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall sqrt(simd_type value) noexcept
    {
    #if COMPLEX_SSE4_1
      return _mm_sqrt_ps(value);
    #elif COMPLEX_NEON
      return vsqrtq_f32(value);
    #endif
    }

    static forceinline simd_type vectorcall invSqrt(simd_type value) noexcept
    {
    #if COMPLEX_SSE4_1
      return _mm_rsqrt_ps(value);
    #elif COMPLEX_NEON
      return vrsqrteq_f32(value);
    #endif
    }

    static forceinline simd_type vectorcall bitAnd(simd_type value, mask_simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_and_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(vandq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simd_type vectorcall bitOr(simd_type value, mask_simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_or_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(vorrq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simd_type vectorcall bitXor(simd_type value, mask_simd_type mask)
    {
    #if COMPLEX_SSE4_1
      return _mm_xor_ps(value, toSimd(mask));
    #elif COMPLEX_NEON
      return toSimd(veorq_u32(toMask(value), mask));
    #endif
    }

    static forceinline simd_type vectorcall bitNot(simd_type value)
    {
    #if COMPLEX_SSE4_1
      auto dummy = _mm_undefined_si128();
      return _mm_xor_ps(value, toSimd(_mm_cmpeq_epi32(dummy, dummy)));
    #elif COMPLEX_NEON
      return toSimd(vmvnq_u32(toMask(value)));
    #endif
    }

    static forceinline simd_type vectorcall max(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_max_ps(one, two);
    #elif COMPLEX_NEON
      return vmaxq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall min(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return _mm_min_ps(one, two);
    #elif COMPLEX_NEON
      return vminq_f32(one, two);
    #endif
    }

    static forceinline simd_type vectorcall truncate(simd_type values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndq_f32(values);
    #endif
    }

    static forceinline simd_type vectorcall floor(simd_type values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndmq_f32(values);
    #endif
    }

    static forceinline simd_type vectorcall ceil(simd_type values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndpq_f32(values);
    #endif
    }

    static forceinline simd_type vectorcall round(simd_type values)
    {
    #if COMPLEX_SSE4_1
      return _mm_round_ps(values, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    #elif COMPLEX_NEON
      return vrndnq_f32(values);
    #endif
    }

    static forceinline simd_type vectorcall abs(simd_type value)
    {
      static constexpr simd_mask mask = kNotSignMask;
    #if COMPLEX_SSE4_1
      return _mm_and_ps(value, toSimd(mask.value));
    #elif COMPLEX_NEON
      return toSimd(vandq_u32(toMask(value), mask.value));
    #endif
    }

    static forceinline mask_simd_type vectorcall signMask(simd_type value)
    { return toMask(bitAnd(value, simd_mask::init(kSignMask))); }

    static forceinline mask_simd_type vectorcall equal(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpeq_ps(one, two));
    #elif COMPLEX_NEON
      return vceqq_f32(one, two);
    #endif
    }

    static forceinline mask_simd_type vectorcall greaterThan(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpgt_ps(one, two));
    #elif COMPLEX_NEON
      return vcgtq_f32(one, two);
    #endif
    }

    static forceinline mask_simd_type vectorcall greaterThanOrEqual(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpge_ps(one, two));
    #elif COMPLEX_NEON
      return vcgeq_f32(one, two);
    #endif
    }

    static forceinline mask_simd_type vectorcall notEqual(simd_type one, simd_type two)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpneq_ps(one, two));
    #elif COMPLEX_NEON
      return simd_mask::bitNot(equal(one.value, two.value));
    #endif
    }

    static forceinline float vectorcall sum(simd_type value)
    {
    #if COMPLEX_SSE4_1
      simd_type flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2));
      value = _mm_add_ps(value, flip);
      flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(2, 3, 0, 1));
      return _mm_cvtss_f32(_mm_add_ps(value, flip));
    #elif COMPLEX_NEON
      float32x2_t sum = vpadd_f32(vget_low_f32(value), vget_high_f32(value));
      sum = vpadd_f32(sum, sum);
      return vget_lane_f32(sum, 0);
    #endif
    }

    static forceinline mask_simd_type vectorcall isNan(simd_type value)
    {
    #if COMPLEX_SSE4_1
      return toMask(_mm_cmpunord_ps(value, value));
    #elif COMPLEX_NEON
      return simd_mask::bitNot(vceqq_f32(value, value));
    #endif
    }


    //////////////////
    // Method Calls //
    //////////////////

    static forceinline simd_mask vectorcall toMask(simd_float value)
    {
    #if COMPLEX_SSE4_1
      return _mm_castps_si128(value.value);
    #elif COMPLEX_NEON
      return vreinterpretq_u32_f32(value.value);
    #endif
    }

    static forceinline simd_float vectorcall mulAdd(simd_float add, simd_float mulOne, simd_float mulTwo)
    { return mulAdd(add.value, mulOne.value, mulTwo.value);	}

    static forceinline simd_float vectorcall mulSub(simd_float sub, simd_float mulOne, simd_float mulTwo)
    { return mulSub(sub.value, mulOne.value, mulTwo.value); }

    static forceinline simd_float vectorcall sqrt(simd_float value)
    { return sqrt(value.value); }

    static forceinline simd_float vectorcall invSqrt(simd_float value)
    { return invSqrt(value.value); }

    static forceinline simd_float vectorcall max(simd_float one, simd_float two)
    { return max(one.value, two.value); }

    static forceinline simd_float vectorcall min(simd_float one, simd_float two)
    { return min(one.value, two.value);	}

    static forceinline simd_float vectorcall clamp(simd_float value, simd_float low, simd_float high)
    { return max(min(value.value, high.value), low.value); }

    static forceinline simd_float vectorcall truncate(simd_float value)
    { return truncate(value.value); }

    static forceinline simd_float vectorcall floor(simd_float value)
    { return floor(value.value); }

    static forceinline simd_float vectorcall ceil(simd_float value)
    { return ceil(value.value); }

    static forceinline simd_float vectorcall round(simd_float value)
    { return round(value.value); }

    static forceinline simd_float vectorcall abs(simd_float value)
    { return abs(value.value); }

    static forceinline simd_mask vectorcall signMask(simd_float value)
    { return signMask(value.value); }

    static forceinline simd_mask vectorcall equal(simd_float one, simd_float two)
    { return equal(one.value, two.value);	}

    static forceinline simd_mask vectorcall notEqual(simd_float one, simd_float two)
    { return notEqual(one.value, two.value); }

    static forceinline simd_mask vectorcall greaterThan(simd_float one, simd_float two)
    { return greaterThan(one.value, two.value);	}

    static forceinline simd_mask vectorcall greaterThanOrEqual(simd_float one, simd_float two)
    { return greaterThanOrEqual(one.value, two.value); }

    static forceinline simd_mask vectorcall lessThan(simd_float one, simd_float two)
    { return greaterThan(two.value, one.value); }

    static forceinline simd_mask vectorcall lessThanOrEqual(simd_float one, simd_float two)
    { return greaterThanOrEqual(two.value, one.value); }
    
    static forceinline float vectorcall sum(simd_float value)
    { return sum(value.value); }

    static forceinline bool vectorcall allSame(simd_float value)
    { return simd_int::allSame(toMask(value)); }

    static forceinline simd_mask vectorcall isNan(simd_float value)
    { return isNan(value.value); }

    static forceinline bool vectorcall allEqual(simd_float one, simd_float two)
    { return simd_mask::anyMask(notEqual(one, two)) == 0; }


    //////////////////////////////////
    // Constructors and Destructors //
    //////////////////////////////////

  #ifdef COMPLEX_MSVC
  private:
    // i hate unions
    template<usize ... N>
    static constexpr auto getInternalValue(const auto &scalars, utils::index_sequence<N...>) noexcept
    { return simd_type{ .m128_f32 = { scalars[N]... } }; }
  public:
  #endif

    constexpr forceinline simd_float() noexcept : simd_float{ 0.0f } { }
    constexpr forceinline simd_float(float initialValue) noexcept
    {
      if (utils::is_constant_evaluated())
      {
        array_t values{};
        values.fill(initialValue);
      #ifdef COMPLEX_MSVC
        value = getInternalValue(values, utils::make_index_sequence<size>());
      #else
        value = utils::bit_cast<simd_type>(values);
      #endif
      }
      else
        value = init(initialValue);
    }
    constexpr forceinline simd_float(simd_type initialValue) noexcept : value(initialValue) { }
    constexpr forceinline simd_float(const array_t &scalars) noexcept :
    #ifdef COMPLEX_MSVC
      value(getInternalValue(scalars, utils::make_index_sequence<size>())) { }
    #else
      value(utils::bit_cast<simd_type>(scalars)) { }
    #endif
    
    template<typename T, usize N> requires (sizeof(simd_type) == sizeof(T) * N)
    forceinline simd_float(const utils::array<T, N> &scalars) noexcept :
      value(utils::bit_cast<simd_type>(scalars)) { }

    constexpr forceinline simd_float(float even, float odd) noexcept
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
      value = utils::bit_cast<simd_type>(values);
    #endif
    }

    forceinline void vectorcall set(usize index, float newValue) noexcept
    {
      auto scalars = utils::bit_cast<array_t>(value);
      scalars[index] = newValue;
      value = utils::bit_cast<simd_type>(scalars);
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

    forceinline bool vectorcall operator==(simd_float other) const noexcept
    { return simd_mask::anyMask(notEqual(*this, other)) == 0; }

    constexpr forceinline float vectorcall operator[](usize index) const noexcept
    {
    #ifdef COMPLEX_MSVC
      return value.m128_f32[index];
    #else
      return utils::bit_cast<array_t>(value)[index];
    #endif
    }

    forceinline simd_float& vectorcall operator+=(simd_float other) noexcept
    { value = add(value, other.value); return *this; }

    forceinline simd_float& vectorcall operator-=(simd_float other) noexcept
    { value = sub(value, other.value); return *this; }

    forceinline simd_float& vectorcall operator*=(simd_float other) noexcept
    { value = mul(value, other.value); return *this; }

    forceinline simd_float& vectorcall operator*=(float scalar) noexcept
    { value = mulScalar(value, scalar); return *this; }

    forceinline simd_float& vectorcall operator/=(simd_float other) noexcept
    { value = div(value, other.value); return *this; }


    forceinline simd_float& vectorcall operator&=(simd_mask other) noexcept
    { value = bitAnd(value, other.value); return *this; }

    forceinline simd_float& vectorcall operator|=(simd_mask other) noexcept
    { value = bitOr(value, other.value); return *this; }

    forceinline simd_float& vectorcall operator^=(simd_mask other) noexcept
    { value = bitXor(value, other.value); return *this; }
    
    forceinline simd_float& vectorcall operator&=(simd_float other) noexcept
    { value = bitAnd(value, toMask(other.value)); return *this; }

    forceinline simd_float& vectorcall operator|=(simd_float other) noexcept
    { value = bitOr(value, toMask(other.value)); return *this; }
    
    forceinline simd_float& vectorcall operator^=(simd_float other) noexcept
    { value = bitXor(value, toMask(other.value)); return *this; }


    forceinline simd_float vectorcall operator+(simd_float other) const noexcept
    { return add(value, other.value); }

    forceinline friend simd_float vectorcall operator+(float one, simd_float two) noexcept
    { return add(init(one), two.value); }

    forceinline simd_float vectorcall operator-(simd_float other) const noexcept
    { return sub(value, other.value); }

    forceinline friend simd_float vectorcall operator-(float one, simd_float two) noexcept
    { return sub(init(one), two.value); }

    forceinline simd_float vectorcall operator*(simd_float other) const noexcept
    { return mul(value, other.value); }

    forceinline simd_float vectorcall operator*(float scalar) const noexcept
    { return mulScalar(value, scalar); }

    forceinline friend simd_float vectorcall operator*(float one, simd_float two) noexcept
    { return mulScalar(two.value, one); }

    forceinline simd_float vectorcall operator/(simd_float other) const noexcept
    { return div(value, other.value); }

    forceinline friend simd_float vectorcall operator/(float one, simd_float two) noexcept
    { return div(init(one), two.value); }


    forceinline simd_float vectorcall operator&(simd_mask other) const noexcept
    { return bitAnd(value, other.value); }

    forceinline simd_float vectorcall operator|(simd_mask other) const noexcept
    { return bitOr(value, other.value); }

    forceinline simd_float vectorcall operator^(simd_mask other) const noexcept
    { return bitXor(value, other.value); }
    
    forceinline simd_float vectorcall operator&(simd_float other) const noexcept
    { return bitAnd(value, toMask(other.value)); }

    forceinline simd_float vectorcall operator|(simd_float other) const noexcept
    { return bitOr(value, toMask(other.value)); }

    forceinline simd_float vectorcall operator^(simd_float other) const noexcept
    { return bitXor(value, toMask(other.value)); }

    forceinline simd_float vectorcall operator-() const noexcept
    { return neg(value); }

    forceinline simd_float vectorcall operator~() const noexcept
    { return bitNot(value); }
  };

  template<typename T>
  concept SimdValue = utils::is_same_v<T, simd_float> || utils::is_same_v<T, simd_int>;

  // an in/output needs to be contained in a single simd
  // effectively an alias of complexSize
  inline constexpr u32 kChannelsPerInOut = simd_float::complexSize;

  inline constexpr auto kChannelMasks = []()
  {
    utils::array<simd_mask, kChannelsPerInOut> channelMasks{};
    for (usize i = 0; i < channelMasks.size(); ++i)
    {
      utils::array<u32, simd_mask::size> mask{};
      mask[2 * i] = kFullMask;
      mask[2 * i + 1] = kFullMask;
      channelMasks[i] = mask;
    }
    return channelMasks;
  }();
  inline constexpr simd_mask kRealMask = { kFullMask, 0U };
  inline constexpr simd_mask kImaginaryMask = { 0U, kFullMask };
  inline constexpr simd_mask kMagnitudeMask = kRealMask;
  inline constexpr simd_mask kPhaseMask = kImaginaryMask;
}

using utils::simd_float;
using utils::simd_int;
using utils::simd_mask;

