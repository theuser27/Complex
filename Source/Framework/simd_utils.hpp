/*
  ==============================================================================

    simd_utils.hpp
    Created: 26 Aug 2021 3:53:04am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "constants.hpp"
#include "simd_values.hpp"

namespace utils
{
  strict_inline simd_float vector_call toFloat(simd_int value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtepi32_ps(value.value);
  #elif COMPLEX_NEON
    return vcvtq_f32_s32(vreinterpretq_s32_u32(value.value));
  #endif
  }
  constexpr strict_inline simd_float vector_call toFloat(simd_float value) noexcept { return value; }

  strict_inline simd_int vector_call toInt(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtps_epi32(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_s32(vcvtq_s32_f32(value.value));
  #endif
  }
  constexpr strict_inline simd_int vector_call toInt(simd_int value) noexcept { return value; }

  strict_inline simd_float vector_call reinterpretToFloat(simd_int value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_castsi128_ps(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_f32_u32(value.value);
  #endif
  }
  constexpr strict_inline simd_float vector_call reinterpretToFloat(simd_float value) noexcept { return value; }

  strict_inline simd_int vector_call reinterpretToInt(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_castps_si128(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_f32(value.value);
  #endif
  }
  constexpr strict_inline simd_int vector_call reinterpretToInt(simd_int value) noexcept { return value; }

  strict_inline simd_float vector_call toSimdFloatFromUnaligned(const float *unaligned) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_loadu_ps(unaligned);
  #elif COMPLEX_NEON
    return vld1q_f32(unaligned);
  #endif
  }

  strict_inline void vector_call transpose(utils::array<simd_float, simd_float::size> &rows)
  {
  #if COMPLEX_SSE4_1
    auto low0 = _mm_unpacklo_ps(rows[0].value, rows[1].value);
    auto low1 = _mm_unpacklo_ps(rows[2].value, rows[3].value);
    auto high0 = _mm_unpackhi_ps(rows[0].value, rows[1].value);
    auto high1 = _mm_unpackhi_ps(rows[2].value, rows[3].value);
    rows[0].value = _mm_movelh_ps(low0, low1);
    rows[1].value = _mm_movehl_ps(low1, low0);
    rows[2].value = _mm_movelh_ps(high0, high1);
    rows[3].value = _mm_movehl_ps(high1, high0);
  #elif COMPLEX_NEON
    auto swapLow = vtrnq_f32(rows[0].value, rows[1].value);
    auto swapHigh = vtrnq_f32(rows[2].value, rows[3].value);
    rows[0].value = vextq_f32(vextq_f32(swapLow.val[0], swapLow.val[0], 2), swapHigh.val[0], 2);
    rows[1].value = vextq_f32(vextq_f32(swapLow.val[1], swapLow.val[1], 2), swapHigh.val[1], 2);
    rows[2].value = vextq_f32(swapLow.val[0], vextq_f32(swapHigh.val[0], swapHigh.val[0], 2), 2);
    rows[3].value = vextq_f32(swapLow.val[1], vextq_f32(swapHigh.val[1], swapHigh.val[1], 2), 2);
  #endif
  }

  strict_inline void vector_call complexTranspose(utils::array<simd_float, simd_float::complexSize> &rows)
  {
  #if COMPLEX_SSE4_1
    auto low = _mm_movelh_ps(rows[0].value, rows[1].value);
    auto high = _mm_movehl_ps(rows[1].value, rows[0].value);
  #elif COMPLEX_NEON
    auto low = vreinterpretq_f32_f64(vzip1q_f64(vreinterpretq_f64_f32(rows[0].value),
      vreinterpretq_f64_f32(rows[1].value)));
    auto high = vreinterpretq_f32_f64(vzip2q_f64(vreinterpretq_f64_f32(rows[0].value), 
      vreinterpretq_f64_f32(rows[1].value)));
  #endif
    rows[0].value = low;
    rows[1].value = high;
  }

  strict_inline simd_float vector_call merge(simd_float falseValue, simd_float trueValue, simd_mask mask) noexcept
  {
  #ifdef COMPLEX_SSE4_1
    return _mm_blendv_ps(falseValue.value, trueValue.value, reinterpretToFloat(mask).value);
  #elif COMPLEX_NEON
    return vbslq_f32(mask.value, reinterpretToFloat(trueValue).value, reinterpretToFloat(falseValue).value);
  #endif
  }

  strict_inline simd_int vector_call merge(simd_int falseValue, simd_int trueValue, simd_mask mask) noexcept
  {
  #ifdef COMPLEX_SSE4_1
    return reinterpretToInt(_mm_blendv_ps(reinterpretToFloat(falseValue).value, 
      reinterpretToFloat(trueValue).value, reinterpretToFloat(mask).value));
  #elif COMPLEX_NEON
    return reinterpretToInt(vbslq_f32(mask.value, 
      reinterpretToFloat(trueValue).value, reinterpretToFloat(falseValue).value));
  #endif
  }

  strict_inline simd_float vector_call lerp(simd_float from, simd_float to, simd_float t) noexcept
  { return simd_float::mulAdd(from, to - from, t); }

  strict_inline simd_float vector_call getDecimalPlaces(simd_float value) noexcept
  { return value - simd_float::floor(value); }

  strict_inline simd_mask vector_call getSign(simd_int value) noexcept
  { return value & kSignMask; }

  strict_inline simd_mask vector_call getSign(simd_float value) noexcept
  { return reinterpretToInt(value) & kSignMask; }

  // lerps between the closest range of from and to inside [0; range]
  strict_inline simd_float vector_call circularLerp(simd_float from, simd_float to, simd_float t, simd_float range) noexcept
  {
    simd_float fromTo = to - from;
    simd_float toFrom = (range ^ getSign(fromTo)) - fromTo;

    simd_float result = merge(
      from - simd_float::mulAdd(range, t, toFrom),
      simd_float::mulAdd(from, t, fromTo),
      simd_float::lessThan(simd_float::abs(fromTo), simd_float::abs(toFrom)));

    return result - simd_float::floor(result / range) * range;
  }

  // lerps between the closest range of from and to inside +/- range
  strict_inline simd_float vector_call circularLerpSymmetric(simd_float from, 
    simd_float to, simd_float t, simd_float range) noexcept
  { return circularLerp(from + range, to + range, t, range * 2.0f) - range; }

  strict_inline auto vector_call getLinearInterpolationMatrix(simd_float t) noexcept
  { return utils::array{ simd_float{ 0.0f }, simd_float{ 1.0f } - t, t, simd_float{ 0.0f } }; }

  strict_inline auto vector_call getCatmullInterpolationMatrix(simd_float t) noexcept
  {
    simd_float halfT = t * 0.5f;
    simd_float halfT2 = t * halfT;
    simd_float halfT3 = t * halfT2;
    simd_float halfThreeT3 = halfT3 * 3.0f;

    return utils::array{
      simd_float::mulAdd(-halfT3, halfT2, 2.0f) - halfT,
      simd_float::mulSub(halfThreeT3, halfT2, 5.0f) + 1.0f,
      simd_float::mulAdd(halfT, halfT3, 4.0f) - halfThreeT3,
      halfT3 - halfT2
    };
  }

  strict_inline auto vector_call getValueMatrix(const float *buffer, simd_int indices) noexcept
  {
    utils::array<simd_float, simd_float::size> values;
    for (u32 i = 0; i < values.size(); i++)
      values[i] = toSimdFloatFromUnaligned(buffer + indices[i]);
    return values;
  }

  template<auto N>
  strict_inline simd_float multiplyAndSumRows(const utils::array<simd_float, N> &one,
    const utils::array<simd_float, N> &two) noexcept
  {
    simd_float summedVector = 0;
    for (usize i = 0; i < N; ++i)
      summedVector = simd_float::mulAdd(summedVector, one[i], two[i]);
    return summedVector;
  }

  strict_inline simd_float vector_call copyFromEven(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    float32x2_t a00 = vdup_laneq_f32(value.value, 0);
    float32x2_t b22 = vdup_laneq_f32(value.value, 2);
    return vcombine_f32(a00, b22);
  #endif
  }

  strict_inline simd_int vector_call copyFromEven(simd_int value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    uint32x2_t a00 = vdup_laneq_u32(value.value, 0);
    uint32x2_t b22 = vdup_laneq_u32(value.value, 2);
    return vcombine_u32(a00, b22);
  #endif
  }

  strict_inline simd_float vector_call copyFromOdd(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    float32x2_t a11 = vdup_laneq_f32(value.value, 1);
    float32x2_t b33 = vdup_laneq_f32(value.value, 3);
    return vcombine_f32(a11, b33);
  #endif
  }

  strict_inline simd_int vector_call copyFromOdd(simd_int value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    uint32x2_t a11 = vdup_laneq_u32(value.value, 1);
    uint32x2_t b33 = vdup_laneq_u32(value.value, 3);
    return vcombine_u32(a11, b33);
  #endif
  }

  strict_inline simd_float vector_call groupEven(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 1, 2, 0));
  #elif COMPLEX_NEON
    // 2 positions are already correct so just insert the 2 in the middle lmao
    return vcopyq_laneq_f32(vcopyq_laneq_f32(value.value, 1, value.value, 2), 2, value.value, 1);
  #endif
  }

  strict_inline simd_float vector_call groupEvenReverse(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(1, 3, 0, 2));
  #elif COMPLEX_NEON
    // [3,2,1,0] > [1,0,3,2]
    //     v           |
    // [|3,2|,1,0]     |
    // [|1,0|,3,2] <---|
    //    v
    // [1,3,0,2]
    auto switched = vextq_f32(value.value, value.value, 2);
    return vzip2q_f32(value.value, switched);
  #endif
  }

  strict_inline simd_float vector_call groupOdd(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 0, 3, 1));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEvenReverse(value).value);
  #endif
  }

  strict_inline simd_float vector_call groupOddReverse(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(0, 2, 1, 3));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEven(value).value);
  #endif
  }

  strict_inline simd_float vector_call switchInner(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 3, 0, 1));
  #elif COMPLEX_NEON
    value.value = vrev64q_f32(value.value);
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }

  strict_inline simd_float vector_call switchOuter(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(1, 0, 3, 2));
  #elif COMPLEX_NEON
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }


  template<SimdValue SIMD>
  strict_inline SIMD vector_call gather(const SIMD *values, simd_int indices) noexcept
  {
    auto array = indices.getArrayOfValues();
  #if COMPLEX_SSE4_1
    auto one = reinterpretToFloat(values[array[0]]).value;
    auto two = reinterpretToFloat(values[array[1]]).value;
    auto three = reinterpretToFloat(values[array[2]]).value;
    auto four = reinterpretToFloat(values[array[3]]).value;

    one = _mm_shuffle_ps(one, two, _MM_SHUFFLE(1, 1, 0, 0));
    two = _mm_shuffle_ps(three, four, _MM_SHUFFLE(3, 3, 2, 2));
    one = _mm_shuffle_ps(one, two, _MM_SHUFFLE(2, 0, 2, 0));

    if constexpr (utils::is_same_v<SIMD, simd_int>)
      return reinterpretToInt(one);
    else
      return one;
  #elif COMPLEX_NEON
    static constexpr simd_mask one = utils::array{ kFullMask, 0U, 0U, 0U };
    static constexpr simd_mask two = utils::array{ 0U, kFullMask, 0U, 0U };
    static constexpr simd_mask three = utils::array{ 0U, 0U, kFullMask, 0U };
    static constexpr simd_mask four = utils::array{ 0U, 0U, 0U, kFullMask };

    return (values[array[0]] & one  ) | (values[array[1]] & two) |
           (values[array[2]] & three) | (values[array[3]] & four);
  #endif
  }

  template<SimdValue SIMD>
  strict_inline void vector_call scatter(SIMD *values, SIMD value, simd_int indices) noexcept
  {
    auto array = indices.getArrayOfValues();
  #if COMPLEX_SSE4_1
    auto valueArray = value.getArrayOfValues();

    auto one = reinterpretToInt(values[array[0]]).value;
    auto two = reinterpretToInt(values[array[1]]).value;
    auto three = reinterpretToInt(values[array[2]]).value;
    auto four = reinterpretToInt(values[array[3]]).value;
    
    if constexpr (utils::is_same_v<SIMD, simd_float>)
    {
      values[array[0]] = reinterpretToFloat(_mm_insert_epi32(one, valueArray[0], 0));
      values[array[1]] = reinterpretToFloat(_mm_insert_epi32(two, valueArray[1], 1));
      values[array[2]] = reinterpretToFloat(_mm_insert_epi32(three, valueArray[2], 2));
      values[array[3]] = reinterpretToFloat(_mm_insert_epi32(four, valueArray[3], 3));
    }
    else
    {
      values[array[0]] = _mm_insert_epi32(one, valueArray[0], 0);
      values[array[1]] = _mm_insert_epi32(two, valueArray[1], 1);
      values[array[2]] = _mm_insert_epi32(three, valueArray[2], 2);
      values[array[3]] = _mm_insert_epi32(four, valueArray[3], 3);
    }
  #elif COMPLEX_NEON
    static constexpr simd_mask one = utils::array{ kFullMask, 0U, 0U, 0U };
    static constexpr simd_mask two = utils::array{ 0U, kFullMask, 0U, 0U };
    static constexpr simd_mask three = utils::array{ 0U, 0U, kFullMask, 0U };
    static constexpr simd_mask four = utils::array{ 0U, 0U, 0U, kFullMask };

    values[array[0]] = merge(values[array[0]], value, one);
    values[array[1]] = merge(values[array[1]], value, two);
    values[array[2]] = merge(values[array[2]], value, three);
    values[array[3]] = merge(values[array[3]], value, four);
  #endif
  }

  template<SimdValue SIMD>
  strict_inline SIMD vector_call gatherComplex(const SIMD *values, simd_int indices) noexcept
  {
    auto array = indices.getArrayOfValues();
    SIMD result = values[array[0]];
    for (usize i = 1; i < kChannelsPerInOut; ++i)
      result = merge(result, values[array[2 * i]], kChannelMasks[i]);
    return result;
  }

  template<SimdValue SIMD>
  strict_inline void vector_call scatterComplex(SIMD *values, simd_int indices, SIMD value, simd_mask mask) noexcept
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i] & mask);
  }
  template<SimdValue SIMD>
  strict_inline void vector_call scatterComplex(SIMD *values, simd_int indices, SIMD value) noexcept
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i]);
  }



  // conditionally unsigns ints if they are negative and 
  // returns a mask which can be used to xor the value restore the sign
  // if flag is set, a full mask where values are negative
  template<bool ReturnFullMask = false>
  strict_inline simd_mask vector_call unsignSimd(simd_int &value) noexcept
  {
    static constexpr simd_mask signMask = kSignMask;
    simd_mask mask = simd_mask::equal(value & signMask, signMask);
    auto value_ = merge(value, -value, mask);
    if constexpr (ReturnFullMask)
    {
      value = value_;
      return mask;
    }
    else
    {
      // xor the (certainly) positive and input to get mask to restore sign
      mask = value ^ value_;
      value = value_;
      return mask;
    }
  }

  // conditionally unsigns floats if they are negative and returns full mask where values are negative
  template<bool ReturnFullMask = false>
  strict_inline simd_mask vector_call unsignSimd(simd_float &value) noexcept
  {
    static constexpr simd_mask signMask = kSignMask;
    simd_mask mask = reinterpretToInt(value) & signMask;
    value ^= mask;
    if constexpr (ReturnFullMask)
      return simd_mask::equal(mask, signMask);
    else
      return mask;
  }

  // if equalsWrap == true/false, then the value will wrap around when it reaches/when it is greater than the modulo
  strict_inline simd_int vector_call modOnce(simd_int value, simd_int mod, bool equalsWrap = true) noexcept
  {
    simd_mask lessMask = (equalsWrap) ? simd_int::lessThanSigned(value, mod) :
      simd_int::lessThanOrEqualSigned(value, mod);
    simd_int lower = value - mod;
    return merge(lower, value, lessMask);
  }

  // if equalsWrap == true/false, then the value will wrap around when it reaches/when it is greater than the modulo
  strict_inline simd_float vector_call modOnce(simd_float value, simd_float mod, bool equalsWrap = true) noexcept
  {
    simd_mask lessMask = (equalsWrap) ? simd_float::lessThan(value, mod) : 
      simd_float::lessThanOrEqual(value, mod);
    simd_float lower = value - mod;
    return merge(lower, value, lessMask);
  }

  strict_inline simd_float vector_call modOnceSymmetric(simd_float value, simd_float mod, bool equalsWrap = true) noexcept
  {
    simd_mask signMask = unsignSimd(value);
    simd_mask lessMask = (equalsWrap) ? simd_float::lessThan(value, mod) : 
      simd_float::lessThanOrEqual(value, mod);
    simd_float lower = value - mod * 2.0f;
    return merge(lower, value, lessMask) ^ signMask;
  }

  strict_inline simd_float vector_call modSymmetric(simd_float value, simd_float mod) noexcept
  {
    value /= mod;
    value -= simd_float::round(value * 0.5f) * 2.0f;
    return value * mod;
  }


  strict_inline simd_float vector_call horizontalAdd(simd_float one, simd_float two) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_hadd_ps(one.value, two.value);
  #elif COMPLEX_NEON
    return vpaddq_f32(one.value, two.value);
  #endif
  }

  strict_inline simd_float vector_call horizontalSub(simd_float one, simd_float two) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_hsub_ps(one.value, two.value);
  #elif COMPLEX_NEON
    static constexpr simd_mask kMinusPlus = { 0U, kSignMask };
    return vpaddq_f32((one ^ kMinusPlus).value, (two ^ kMinusPlus).value);
  #endif
  }

  strict_inline simd_int vector_call horizontalMin(simd_int value) noexcept
  {
  #if COMPLEX_SSE4_1
    auto reversed = _mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3));
    auto one = _mm_min_epi32(value.value, reversed);
    auto switched = _mm_shuffle_epi32(one, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_min_epi32(one, switched);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_s32(vminvq_s32(vreinterpretq_s32_u32(value.value)));
  #endif
  }

  strict_inline simd_float vector_call horizontalMin(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return reinterpretToFloat(horizontalMin(reinterpretToInt(value)));
  #elif COMPLEX_NEON
    return vminvq_f32(value);
  #endif
  }

  strict_inline simd_float vector_call reciprocal(simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_rcp_ps(value.value);
  #elif COMPLEX_NEON
    return vrecpeq_f32(values.value);
  #endif
  }

  template<u32 Shift>
  strict_inline simd_int vector_call shiftRight(simd_int values) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_srli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshrq_n_u32(values.value, shift);
  #endif
  }

  template<u32 Shift>
  strict_inline simd_int vector_call shiftLeft(simd_int values) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_slli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshlq_n_u32(values.value, shift);
  #endif
  }

  template<u32 Shift>
  strict_inline simd_float vector_call shiftRight(simd_float value) noexcept
  {
    static constexpr auto decrement = kFloatExponentUnit * Shift;
    return reinterpretToFloat(reinterpretToInt(value) - decrement);
  }

  template<u32 Shift>
  strict_inline simd_float vector_call shiftLeft(simd_float value) noexcept
  {
    static constexpr auto increment = kFloatExponentUnit * Shift;
    return reinterpretToFloat(reinterpretToInt(value) + increment);
  }



  strict_inline simd_float vector_call exp2(simd_float exponent) noexcept
  {
    // taylor expansion of 2^x at 0
    // coefficients are (ln(2)^n) / n!
    static constexpr simd_float kCoefficient0 = 1.0f;
    static constexpr simd_float kCoefficient1 = 16970.0f / 24483.0f;
    static constexpr simd_float kCoefficient2 = 1960.0f / 8161.0f;
    static constexpr simd_float kCoefficient3 = 1360.0f / 24483.0f;
    static constexpr simd_float kCoefficient4 = 80.0f / 8161.0f;
    static constexpr simd_float kCoefficient5 = 32.0f / 24483.0f;

    // the closer the exponent is to a whole number, the more accurate it's going to be
    // since it only requires to add it the overall floating point exponent
    simd_float rounded = simd_float::round(exponent);
    simd_float t = exponent - rounded;
    // clamp the lowest value otherwise get garbage results when shifting left
    simd_float power = reinterpretToFloat(shiftLeft<23>(simd_int::maxSigned((u32)-127, toInt(rounded)) + 127));

    // we exp2 whatever decimal number is left with the taylor series
    // the domain we're in is [0.0f, 0.5f], we don't expect negative numbers
    simd_float interpolate = simd_float::mulAdd(kCoefficient2, t, simd_float::mulAdd(kCoefficient3,
      t, simd_float::mulAdd(kCoefficient4, t, kCoefficient5)));
    interpolate = simd_float::mulAdd(kCoefficient0, t, simd_float::mulAdd(kCoefficient1, t, interpolate));
    return power * interpolate;
  }

  strict_inline simd_float vector_call log2(simd_float value) noexcept
  {
    // i have no idea how these coefficients were derived
    static constexpr simd_float kCoefficient0 = -1819.0f / 651.0f;
    static constexpr simd_float kCoefficient1 = 5.0f;
    static constexpr simd_float kCoefficient2 = -10.0f / 3.0f;
    static constexpr simd_float kCoefficient3 = 10.0f / 7.0f;
    static constexpr simd_float kCoefficient4 = -1.0f / 3.0f;
    static constexpr simd_float kCoefficient5 = 1.0f / 31.0f;

    static constexpr simd_mask mantissaMask = 0x7fffff;
    static constexpr simd_mask exponentOffset = 0x7f << 23;

    // effectively log2s only the exponent; gets it in terms an int
    simd_int flooredLog2 = shiftRight<23>(reinterpretToInt(value)) - 0x7f;
    // 0x7fffff masks the entire mantissa
    // then we bring the exponent to 2^0 to get the entire number between [1, 2]
    simd_float t = (value & mantissaMask) | exponentOffset;

    // we log2 the mantissa with the taylor series coefficients
    simd_float interpolate = simd_float::mulAdd(kCoefficient2, t, simd_float::mulAdd(kCoefficient3, t,
      simd_float::mulAdd(kCoefficient4, t, kCoefficient5)));
    interpolate = simd_float::mulAdd(kCoefficient0, t, simd_float::mulAdd(kCoefficient1, t, interpolate));

    // we add the int with the mantissa to get our final result
    return toFloat(flooredLog2) + interpolate;
  }

  strict_inline simd_float vector_call exp(simd_float exponent) noexcept
  { return exp2(exponent * kExpConversionMult); }

  strict_inline simd_float vector_call log(simd_float value) noexcept
  { return log2(value) * kLogConversionMult; }

  strict_inline simd_float vector_call pow(simd_float base, simd_float exponent) noexcept
  { return exp2(log2(base) * exponent); }

  strict_inline simd_float vector_call midiOffsetToRatio(simd_float note_offset) noexcept
  { return exp2(note_offset * (1.0f / kNotesPerOctave)); }

  strict_inline simd_float vector_call midiNoteToFrequency(simd_float note) noexcept
  { return midiOffsetToRatio(note) * kMidi0Frequency; }

  // fast approximation of the original equation
  strict_inline simd_float vector_call amplitudeToDb(simd_float magnitude) noexcept
  { return log2(magnitude) * kAmplitudeToDbConversionMult; }

  // fast approximation of the original equation
  strict_inline simd_float vector_call dbToAmplitude(simd_float decibels) noexcept
  { return exp2(decibels * kDbToAmplitudeConversionMult); }

  strict_inline simd_float vector_call normalisedToDb(simd_float normalised, float maxDb) noexcept
  { return pow(maxDb + 1.0f, normalised) - 1.0f; }

  strict_inline simd_float vector_call dbToNormalised(simd_float db, float maxDb) noexcept
  { return log2(db + 1.0f) / log2(simd_float{ maxDb + 1.0f }); }

  strict_inline simd_float vector_call normalisedToFrequency(simd_float normalised, float sampleRate) noexcept
  { return pow(sampleRate * 0.5f / (float)kMinFrequency, normalised) * kMinFrequency; }

  strict_inline simd_float vector_call frequencyToNormalised(simd_float frequency, float sampleRate) noexcept
  { return log2(frequency / kMinFrequency) / log2(simd_float{ sampleRate * 0.5f / (float)kMinFrequency }); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2
  strict_inline simd_float vector_call normalisedToBin(simd_float normalised, u32 FFTSize, float sampleRate) noexcept
  {
    simd_mask zeroMask = simd_float::notEqual(normalised, 0.0f);
    return simd_float::round(normalisedToFrequency(normalised, sampleRate) / sampleRate * (float)FFTSize) & zeroMask;
  }

  strict_inline simd_float vector_call binToNormalised(simd_float bin, u32 FFTSize, float sampleRate) noexcept
  {
    // for 0 logarithm doesn't produce valid values
    // so we mask that with dummy values to not get errors
    simd_mask zeroMask = simd_float::notEqual(bin, 0.0f);
    return frequencyToNormalised(bin * sampleRate / (float)FFTSize, sampleRate) & zeroMask;
  }

  strict_inline float exp2(float value) noexcept
  {
    simd_float input = value;
    simd_float result = exp2(input);
    return result[0];
  }

  strict_inline float log2(float value) noexcept
  {
    simd_float input = value;
    simd_float result = log2(input);
    return result[0];
  }

  strict_inline float exp(float exponent) noexcept
  { return exp2(exponent * kExpConversionMult); }

  strict_inline float log(float value) noexcept
  { return log2(value) * kLogConversionMult; }

  strict_inline float pow(float base, float exponent) noexcept
  { return exp2(log2(base) * exponent); }

  strict_inline simd_float powerScale(simd_float value, simd_float power)
  {
    static constexpr float kMinPowerMag = 0.005f;
    simd_mask zeroMask = simd_float::lessThan(power, kMinPowerMag) & simd_float::lessThan(-power, kMinPowerMag);
    simd_float numerator = exp(power * value) - 1.0f;
    simd_float denominator = exp(power) - 1.0f;
    simd_float result = numerator / denominator;
    return merge(result, value, zeroMask);
  }

  strict_inline float powerScale(float value, float power)
  {
    static constexpr float kMinPower = 0.01f;

    if (power < kMinPower && power > -kMinPower)
      return value;

    float numerator = exp(power * value) - 1.0f;
    float denominator = exp(power) - 1.0f;
    return numerator / denominator;
  }

  strict_inline simd_float vector_call getStereoDifference(simd_float value) noexcept
  {
    return (value - switchInner(value)) * 0.5f;
  }

  // assumes value is signed
  strict_inline simd_int vector_call getStereoDifference(simd_int value) noexcept
  {
    simd_int highestBit = value & kSignMask;
    return highestBit | shiftRight<1>(value - reinterpretToInt(switchInner(reinterpretToFloat(value))));
  }
}