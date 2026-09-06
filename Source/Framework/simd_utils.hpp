
// Created: 2021-08-26 03:53:04

#pragma once

#include "constants.hpp"
#include "simd_values.hpp"

namespace utils
{
  forceinline simd_float vectorcall 
  toFloat(simd_int value)
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtepi32_ps(value.value);
  #elif COMPLEX_NEON
    return vcvtq_f32_s32(vreinterpretq_s32_u32(value.value));
  #endif
  }
  constexpr forceinline simd_float vectorcall toFloat(simd_float value) { return value; }

  forceinline simd_int vectorcall 
  toInt(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtps_epi32(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_s32(vcvtq_s32_f32(value.value));
  #endif
  }
  constexpr forceinline simd_int vectorcall toInt(simd_int value) { return value; }

  forceinline simd_float vectorcall 
  reinterpretToFloat(simd_int value)
  {
  #if COMPLEX_SSE4_1
    return _mm_castsi128_ps(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_f32_u32(value.value);
  #endif
  }
  constexpr forceinline simd_float vectorcall reinterpretToFloat(simd_float value) { return value; }

  forceinline simd_int vectorcall 
  reinterpretToInt(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_castps_si128(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_f32(value.value);
  #endif
  }
  constexpr forceinline simd_int vectorcall reinterpretToInt(simd_int value) { return value; }

  forceinline simd_float vectorcall 
  toSimdFloatFromUnaligned(const float *unaligned)
  {
  #if COMPLEX_SSE4_1
    return _mm_loadu_ps(unaligned);
  #elif COMPLEX_NEON
    return vld1q_f32(unaligned);
  #endif
  }

  forceinline void vectorcall transpose(utils::array<simd_float, simd_float::size> &rows)
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

    rows[0].value = vcombine_f32(vget_low_f32(swapLow.val[0]), vget_low_f32(swapHigh.val[0]));
    rows[1].value = vcombine_f32(vget_low_f32(swapLow.val[1]), vget_low_f32(swapHigh.val[1]));
    rows[2].value = vcombine_f32(vget_high_f32(swapLow.val[0]), vget_high_f32(swapHigh.val[0]));
    rows[3].value = vcombine_f32(vget_high_f32(swapLow.val[1]), vget_high_f32(swapHigh.val[1]));
  #endif
  }

  forceinline void vectorcall complexTranspose(utils::array<simd_float, simd_float::complexSize> &rows)
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

  forceinline simd_float vectorcall 
  merge(simd_float falseValue, simd_float trueValue, simd_mask mask)
  {
  #ifdef COMPLEX_SSE4_1
    return _mm_blendv_ps(falseValue.value, trueValue.value, reinterpretToFloat(mask).value);
  #elif COMPLEX_NEON
    return vbslq_f32(mask.value, trueValue.value, falseValue.value);
  #endif
  }

  forceinline simd_int vectorcall 
  merge(simd_int falseValue, simd_int trueValue, simd_mask mask)
  {
  #ifdef COMPLEX_SSE4_1
    return reinterpretToInt(_mm_blendv_ps(reinterpretToFloat(falseValue).value,
      reinterpretToFloat(trueValue).value, reinterpretToFloat(mask).value));
  #elif COMPLEX_NEON
    return vbslq_u32(mask.value, trueValue.value, falseValue.value);
  #endif
  }

  forceinline simd_float vectorcall lerp(simd_float from, simd_float to, simd_float t)
  { return simd_float::mulAdd(from, to - from, t); }

  forceinline simd_float vectorcall getDecimalPlaces(simd_float value)
  { return value - simd_float::floor(value); }

  forceinline simd_mask vectorcall getSign(simd_int value)
  { return value & kSignMask; }

  forceinline simd_mask vectorcall getSign(simd_float value)
  { return reinterpretToInt(value) & kSignMask; }

  // lerps between the closest range of from and to inside [0; range]
  forceinline simd_float vectorcall circularLerp(simd_float from, simd_float to, simd_float t, simd_float range)
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
  forceinline simd_float vectorcall circularLerpSymmetric(simd_float from,
    simd_float to, simd_float t, simd_float range)
  { return circularLerp(from + range, to + range, t, range * 2.0f) - range; }

  forceinline auto vectorcall getLinearInterpolationMatrix(simd_float t)
  { return utils::array{ simd_float{ 0.0f }, simd_float{ 1.0f } - t, t, simd_float{ 0.0f } }; }

  forceinline auto vectorcall getCatmullInterpolationMatrix(simd_float t)
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

  forceinline auto vectorcall getValueMatrix(const float *buffer, simd_int indices)
  {
    utils::array<simd_float, simd_float::size> values;
    for (u32 i = 0; i < values.size(); i++)
      values[i] = toSimdFloatFromUnaligned(buffer + indices[i]);
    return values;
  }

  template<auto N>
  forceinline simd_float multiplyAndSumRows(const utils::array<simd_float, N> &one,
    const utils::array<simd_float, N> &two)
  {
    simd_float summedVector = 0;
    for (usize i = 0; i < N; ++i)
      summedVector = simd_float::mulAdd(summedVector, one[i], two[i]);
    return summedVector;
  }

  forceinline simd_float vectorcall copyFromEven(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    float32x2_t a00 = vdup_laneq_f32(value.value, 0);
    float32x2_t b22 = vdup_laneq_f32(value.value, 2);
    return vcombine_f32(a00, b22);
  #endif
  }

  forceinline simd_int vectorcall copyFromEven(simd_int value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    uint32x2_t a00 = vdup_laneq_u32(value.value, 0);
    uint32x2_t b22 = vdup_laneq_u32(value.value, 2);
    return vcombine_u32(a00, b22);
  #endif
  }

  forceinline simd_float vectorcall copyFromOdd(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    float32x2_t a11 = vdup_laneq_f32(value.value, 1);
    float32x2_t b33 = vdup_laneq_f32(value.value, 3);
    return vcombine_f32(a11, b33);
  #endif
  }

  forceinline simd_int vectorcall copyFromOdd(simd_int value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    uint32x2_t a11 = vdup_laneq_u32(value.value, 1);
    uint32x2_t b33 = vdup_laneq_u32(value.value, 3);
    return vcombine_u32(a11, b33);
  #endif
  }

  forceinline simd_float vectorcall groupEven(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 1, 2, 0));
  #elif COMPLEX_NEON
    return vuzp1q_f32(value.value, vrev64q_f32(value.value));
  #endif
  }

  forceinline simd_float vectorcall groupEvenReverse(simd_float value)
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

  forceinline simd_float vectorcall groupOdd(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 0, 3, 1));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEvenReverse(value).value);
  #endif
  }

  forceinline simd_float vectorcall groupOddReverse(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(0, 2, 1, 3));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEven(value).value);
  #endif
  }

  forceinline simd_float vectorcall switchInner(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 3, 0, 1));
  #elif COMPLEX_NEON
    value.value = vrev64q_f32(value.value);
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }

  forceinline simd_float vectorcall switchOuter(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(1, 0, 3, 2));
  #elif COMPLEX_NEON
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }


  template<SimdValue SIMD>
  forceinline SIMD vectorcall gather(const SIMD *values, simd_int indices)
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
  forceinline void vectorcall scatter(SIMD *values, SIMD value, simd_int indices)
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
      values[array[0]] = reinterpretToFloat(simd_int{ _mm_insert_epi32(one, valueArray[0], 0) });
      values[array[1]] = reinterpretToFloat(simd_int{ _mm_insert_epi32(two, valueArray[1], 1) });
      values[array[2]] = reinterpretToFloat(simd_int{ _mm_insert_epi32(three, valueArray[2], 2) });
      values[array[3]] = reinterpretToFloat(simd_int{ _mm_insert_epi32(four, valueArray[3], 3) });
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
  forceinline SIMD vectorcall gatherComplex(const SIMD *values, simd_int indices)
  {
    auto array = indices.getArrayOfValues();
    SIMD result = values[array[0]];
    for (usize i = 1; i < kChannelsPerInOut; ++i)
      result = merge(result, values[array[2 * i]], kChannelMasks[i]);
    return result;
  }

  template<SimdValue SIMD>
  forceinline void vectorcall scatterComplex(SIMD *values, simd_int indices, SIMD value, simd_mask mask)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i] & mask);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterComplex(SIMD *values, simd_int indices, SIMD value)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i]);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterAddComplex(SIMD *values, simd_int indices, SIMD value, simd_mask mask)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], values[array[2 * i]] + value, kChannelMasks[i] & mask);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterAddComplex(SIMD *values, simd_int indices, SIMD value)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], values[array[2 * i]] + value, kChannelMasks[i]);
  }



  // conditionally unsigns ints if they are negative and
  // returns a mask which can be used to xor the value restore the sign
  // if flag is set, a full mask where values are negative
  template<bool ReturnFullMask = false>
  forceinline simd_mask vectorcall unsignSimd(simd_int &value)
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
  forceinline simd_mask vectorcall unsignSimd(simd_float &value)
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
  forceinline simd_int vectorcall modOnce(simd_int value, simd_int mod, bool equalsWrap = true)
  {
    simd_mask lessMask = (equalsWrap) ? simd_int::lessThanSigned(value, mod) :
      simd_int::lessThanOrEqualSigned(value, mod);
    simd_int lower = value - mod;
    return merge(lower, value, lessMask);
  }

  // if equalsWrap == true/false, then the value will wrap around when it reaches/when it is greater than the modulo
  forceinline simd_float vectorcall modOnce(simd_float value, simd_float mod, bool equalsWrap = true)
  {
    simd_mask lessMask = (equalsWrap) ? simd_float::lessThan(value, mod) :
      simd_float::lessThanOrEqual(value, mod);
    simd_float lower = value - mod;
    return merge(lower, value, lessMask);
  }

  forceinline simd_float vectorcall modOnceSymmetric(simd_float value, simd_float mod, bool equalsWrap = true)
  {
    simd_mask signMask = unsignSimd(value);
    simd_mask lessMask = (equalsWrap) ? simd_float::lessThan(value, mod) :
      simd_float::lessThanOrEqual(value, mod);
    simd_float lower = value - mod * 2.0f;
    return merge(lower, value, lessMask) ^ signMask;
  }

  forceinline simd_float vectorcall modSymmetric(simd_float value, simd_float mod)
  {
    value /= mod;
    value -= simd_float::round(value * 0.5f) * 2.0f;
    return value * mod;
  }


  forceinline simd_float vectorcall horizontalAdd(simd_float one, simd_float two)
  {
  #if COMPLEX_SSE4_1
    return _mm_hadd_ps(one.value, two.value);
  #elif COMPLEX_NEON
    return vpaddq_f32(one.value, two.value);
  #endif
  }

  forceinline simd_float vectorcall horizontalSub(simd_float one, simd_float two)
  {
  #if COMPLEX_SSE4_1
    return _mm_hsub_ps(one.value, two.value);
  #elif COMPLEX_NEON
    static constexpr simd_mask kMinusPlus = { 0U, kSignMask };
    return vpaddq_f32((one ^ kMinusPlus).value, (two ^ kMinusPlus).value);
  #endif
  }

  forceinline simd_int vectorcall horizontalMin(simd_int value)
  {
  #if COMPLEX_SSE4_1
    auto reversed = _mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3));
    auto one = _mm_min_epi32(value.value, reversed);
    auto switched = _mm_shuffle_epi32(one, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_min_epi32(one, switched);
  #elif COMPLEX_NEON
    return simd_int{ (u32)vminvq_s32(vreinterpretq_s32_u32(value.value)) };
  #endif
  }

  forceinline simd_float vectorcall horizontalMin(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return reinterpretToFloat(horizontalMin(reinterpretToInt(value)));
  #elif COMPLEX_NEON
    return vminvq_f32(value.value);
  #endif
  }

  forceinline simd_float vectorcall reciprocal(simd_float value)
  {
  #if COMPLEX_SSE4_1
    return _mm_rcp_ps(value.value);
  #elif COMPLEX_NEON
    return vrecpeq_f32(value.value);
  #endif
  }

  template<u32 Shift>
  forceinline simd_int vectorcall shiftRight(simd_int values)
  {
  #if COMPLEX_SSE4_1
    return _mm_srli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshrq_n_u32(values.value, Shift);
  #endif
  }

  template<u32 shift>
  forceinline simd_int vectorcall shiftRightArithmetic(simd_int values) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_srai_epi32(values.value, shift);
  #elif COMPLEX_NEON
    return vshrq_n_s32(values.value, shift);
  #endif
  }

  template<u32 Shift>
  forceinline simd_int vectorcall shiftLeft(simd_int values)
  {
  #if COMPLEX_SSE4_1
    return _mm_slli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshlq_n_u32(values.value, Shift);
  #endif
  }



  forceinline simd_float vectorcall exp2(simd_float exponent)
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
    simd_float interpolate = simd_float::mulAdd(kCoefficient0, t,
      simd_float::mulAdd(kCoefficient1, t, simd_float::mulAdd(kCoefficient2, t,
        simd_float::mulAdd(kCoefficient3, t, simd_float::mulAdd(kCoefficient4, t, kCoefficient5)))));

    return power * interpolate;
  }
  
  // (intentionally) does not handle nans and negative numbers correctly
  // at 0/inf it returns the min/max exponent value
  forceinline simd_float vectorcall log2(simd_float value)
  {
    // modified/minimax-ed taylor coefficients for atanh 
    // in order to approximatea a contracted version of log2
    // k0 = 2/log(2), k1 ~= 2/(3*log(2)), k2 ~= 2/(5*log(2)), k3 ~= 2/(7*log(2))
    static constexpr simd_float k0 = 1.44269504088896340736f;
    static constexpr simd_float k1 = 0.96180442874507310336f;
    static constexpr simd_float k2 = 0.57647797555770874389f;
    static constexpr simd_float k3 = 0.43323255205601618467f;
    static constexpr simd_int kSqrt2Addition = 0x004afb10;

    COMPLEX_ASSERT(simd_int::anyMask(simd_float::lessThan(value, 0.0f)) == 0);

    simd_int reinterpreted = reinterpretToInt(value);
    simd_int mantissaOnly = reinterpreted & kFloatMantissaMask;
    // if the raw mantissa represents a number larger than sqrt(2) (assuming the exponent is 2^0), 
    // mod it once in order to bring it back to the range [sqrt(2)/2, sqrt(2)) (more info below)
    // this happens by adding a magic constant and extract the lowest bit of the exponent
    simd_int exponentOffset = (mantissaOnly + kSqrt2Addition) & kFloatExponentUnit;
    simd_int roundLog2 = shiftRightArithmetic<23>(reinterpreted.value) - 127 + shiftRightArithmetic<23>(exponentOffset);

    // one optimisation that can be made to lower the number of coefficients needed
    // if we look at the full taylor series, log2(x) = 2/log(2) * (x - x^2/2 + x^3/3 - x^4/4 + x^5/5 - ...)
    // but we substitute x with z = (x - 1)/(x + 1), then the identity can be defined as 
    // log2(x) = 2/log(2) * atanh(z) = 2/log(2) * (z + z^3/3 + z^5/5 + z^7/7 + ...)
    // 
    // another optimisation is to limit our input range
    // if we define the input as x = 2^e * m then log2(x) = e + log2(m), so if m ∈ [1, 2) => log2(m) = [0, 1)
    // unfortunately when we substitute z = (m - 1)/(m + 1), z ∈ [0, 1/3) which isn't symmetric around 0
    // meaning we have a rather big error maximum at 1/3 (https://www.desmos.com/calculator/wilg3htefn)
    // on the other hand if we change m ∈ [sqrt(2)/2, sqrt(2)) => log(2) = [-1/2, 1/2)
    // and z ∈ [-0.1715728753, 0.1715728753) which is symmetric around 0 and has a much lower max error
    // only extra work we need to do is mod the input to lie in [sqrt(2)/2, sqrt(2)), which is what we did above
    // 
    // the complete calculation looks like log2(x) = e + 2/log(2) * (z + z^3/3 + z^5/5 + z^7/7 + ...)
    // 
    // the last thing is that the coefficients are used in the taylor series approximation are minimax-ed
    // for better accuracy so they don't exactly follow the 1, 1/3, 1/5, 1/7 values

    // modding the input
    simd_float m = reinterpretToFloat((reinterpretToInt(simd_float{ 1.0f }) ^ exponentOffset) | mantissaOnly);
    simd_float numerator = m - 1.0f;
    simd_float z = numerator / (m + 1.0f);

    simd_float z2 = z * z;
    simd_float z4 = z2 * z2;

    // the following line is what is effectively being executed below but it's commented out 
    // because the other version produces results with lower fp inaccuracies
    //simd_float result = toFloat(ceilLog2) + z * (2.0f * k0 + ((k2 * z4) + z2 * (k1 + z4 * k3)));
    simd_float numInvLog2 = numerator * k0;
    simd_float result = toFloat(roundLog2) + (numInvLog2 + (z * (-numInvLog2 + ((k2 * z4) + z2 * (k1 + z4 * k3)))));

    return result;
  }

  forceinline simd_float vectorcall exp(simd_float exponent)
  { return exp2(exponent * kExpConversionMult); }

  forceinline simd_float vectorcall log(simd_float value)
  { return log2(value) * kLogConversionMult; }

  forceinline simd_float vectorcall pow(simd_float base, simd_float exponent)
  { return exp2(log2(base) * exponent); }

  forceinline simd_float vectorcall midiOffsetToRatio(simd_float note_offset)
  { return exp2(note_offset * (1.0f / kNotesPerOctave)); }

  forceinline simd_float vectorcall midiNoteToFrequency(simd_float note)
  { return midiOffsetToRatio(note) * kMidi0Frequency; }

  // fast approximation of the original equation
  forceinline simd_float vectorcall amplitudeToDb(simd_float magnitude)
  { return log2(magnitude) * kAmplitudeToDbConversionMult; }

  // fast approximation of the original equation
  forceinline simd_float vectorcall dbToAmplitude(simd_float decibels)
  { return exp2(decibels * kDbToAmplitudeConversionMult); }

  forceinline simd_float vectorcall normalisedToDb(simd_float normalised, float maxDb)
  { return pow(maxDb + 1.0f, normalised) - 1.0f; }

  forceinline simd_float vectorcall dbToNormalised(simd_float db, float maxDb)
  { return log2(db + 1.0f) / log2(simd_float{ maxDb + 1.0f }); }

  forceinline simd_float vectorcall normalisedToFrequency(simd_float normalised, float sampleRate, float minFrequency = kMinFrequency)
  { return pow(sampleRate * 0.5f / minFrequency, normalised) * minFrequency; }

  forceinline simd_float vectorcall frequencyToNormalised(simd_float frequency, float sampleRate, float minFrequency = kMinFrequency)
  { return log2(frequency / minFrequency) / log2(simd_float{ sampleRate * 0.5f / minFrequency }); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2
  forceinline simd_float vectorcall normalisedToBin(simd_float normalised, u32 FFTSize, float sampleRate)
  {
    simd_mask zeroMask = simd_float::notEqual(normalised, 0.0f);
    return simd_float::round(normalisedToFrequency(normalised, sampleRate) / sampleRate * (float)FFTSize) & zeroMask;
  }

  forceinline simd_float vectorcall binToNormalised(simd_float bin, u32 FFTSize, float sampleRate)
  {
    // for 0 logarithm doesn't produce valid values
    // so we mask that with dummy values to not get errors
    simd_mask zeroMask = simd_float::notEqual(bin, 0.0f);
    return frequencyToNormalised(bin * sampleRate / (float)FFTSize, sampleRate) & zeroMask;
  }

  forceinline float exp2(float value)
  {
    simd_float input = value;
    simd_float result = exp2(input);
    return result[0];
  }

  forceinline float log2(float value)
  {
    simd_float input = value;
    simd_float result = log2(input);
    return result[0];
  }

  forceinline float pow(float base, float exponent)
  { return exp2(log2(base) * exponent); }

  forceinline float exp(float exponent)
  { return exp2(exponent * kExpConversionMult); }

  forceinline float log(float value)
  { return log2(value) * kLogConversionMult; }

  forceinline float exp10(float exponent)
  { return exp2(exponent * kExp10ConversionMult); }

  forceinline float log10(float exponent)
  { return log2(exponent) * kLog10ConversionMult; }

  forceinline simd_float powerScale(simd_float value, simd_float power)
  {
    static constexpr float kMinPowerMag = 0.005f;
    simd_mask zeroMask = simd_float::lessThan(power, kMinPowerMag) & simd_float::lessThan(-power, kMinPowerMag);
    simd_float numerator = exp(power * value) - 1.0f;
    simd_float denominator = exp(power) - 1.0f;
    simd_float result = numerator / denominator;
    return merge(result, value, zeroMask);
  }

  forceinline float powerScale(float value, float power)
  {
    static constexpr float kMinPower = 0.01f;

    if (power < kMinPower && power > -kMinPower)
      return value;

    float numerator = exp(power * value) - 1.0f;
    float denominator = exp(power) - 1.0f;
    return numerator / denominator;
  }

  forceinline simd_float vectorcall getStereoDifference(simd_float value)
  {
    return (value - switchInner(value)) * 0.5f;
  }

  // assumes value is signed
  forceinline simd_int vectorcall getStereoDifference(simd_int value)
  {
    simd_int highestBit = value & kSignMask;
    return highestBit | shiftRight<1>(value - reinterpretToInt(switchInner(reinterpretToFloat(value))));
  }

  template<usize Resolution>
  struct Lookup
  {
    // extra data points needed to perform cspline lookup at edges
    // when the requested values are exactly at 0.0f or 1.0f
    static constexpr usize kExtraValues = 3;

    float lookup_[Resolution + kExtraValues];
    float scale_;

    constexpr Lookup(float(*function)(float), float scale = 1.0f) : scale_(Resolution / scale)
    {
      for (usize i = 0; i < Resolution + kExtraValues; i++)
      {
        float t = ((float)i - 1.0f) / (Resolution - 1.0f);
        lookup_[i] = function(t * scale);
      }
    }

    // gets catmull-rom spline interpolated y-values at their corresponding x-values
    simd_float cubicLookup(simd_float x) const
    {
      COMPLEX_ASSERT(simd_mask::anyMask(simd_float::lessThan(x, 0.0f)) == 0 &&
        simd_mask::anyMask(simd_float::greaterThan(x, 1.0f)) == 0);

      simd_float boost = (x * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      auto interpolationMatrix = utils::getCatmullInterpolationMatrix(t);
      auto valueMatrix = utils::getValueMatrix(lookup_, indices - simd_int(1));
      transpose(valueMatrix);

      return multiplyAndSumRows(interpolationMatrix, valueMatrix);
    }

    // gets linearly interpolated y-values at their corresponding x-values
    simd_float linearLookup(simd_float x) const
    {
      COMPLEX_ASSERT(simd_mask::anyMask(simd_float::lessThan(x, 0.0f)) == 0 &&
        simd_mask::anyMask(simd_float::greaterThan(x, 1.0f)) == 0);

      simd_float boost = (x * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      auto valueMatrix = utils::getValueMatrix(lookup_, indices - simd_int(1));
      transpose(valueMatrix);

      return simd_float::mulAdd((1.0f - t) * valueMatrix[1], t, valueMatrix[2]);
    }

    // gets catmull-rom spline interpolated y-value at the corresponding x-value
    constexpr float cubicLookup(float x) const
    {
      COMPLEX_ASSERT(x >= 0.0f && x <= 1.0f);
      float boost = (x * scale_) + 1.0f;
      usize index = utils::clamp((usize)boost, (usize)1, Resolution);
      float t = boost - (float)index;
      float halfT = t * 0.5f;
      float halfT2 = t * halfT;
      float halfT3 = t * halfT2;
      float threeHalfT3 = halfT3 * 3.0f;

      return (halfT2 * 2.0f - halfT3 - halfT) * lookup_[index - 1] +
        (threeHalfT3 - 5.0f * halfT2 + 1.0f) * lookup_[index] +
        (4.0f * halfT2 + halfT - threeHalfT3) * lookup_[index + 1] +
        (halfT3 - halfT2) * lookup_[index + 2];
    }

    // gets linearly interpolated y-value at the corresponding x-value
    constexpr float linearLookup(float x) const
    {
      COMPLEX_ASSERT(x >= 0.0f && x <= 1.0f);
      float boost = (x * scale_) + 1.0f;
      usize index = utils::clamp((usize)boost, (usize)1, Resolution);
      float t = boost - (float)index;
      return (1.0f - t) * lookup_[index] + t * lookup_[index + 1];
    }
  };
}
