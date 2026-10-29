
// Created: 2021-08-26 03:53:04

#pragma once

#include "constants.hpp"
#include "simd.hpp"

namespace utils
{
  forceinline f4 vectorcall
  toFloat(i4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtepi32_ps(value.value);
  #elif COMPLEX_NEON
    return vcvtq_f32_s32(vreinterpretq_s32_u32(value.value));
  #endif
  }
  constexpr forceinline f4 vectorcall toFloat(f4 value) { return value; }

  forceinline i4 vectorcall
  toInt(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_cvtps_epi32(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_s32(vcvtq_s32_f32(value.value));
  #endif
  }
  constexpr forceinline i4 vectorcall toInt(i4 value) { return value; }

  forceinline f4 vectorcall
  reinterpretToFloat(i4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_castsi128_ps(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_f32_u32(value.value);
  #endif
  }
  constexpr forceinline f4 vectorcall reinterpretToFloat(f4 value) { return value; }

  forceinline i4 vectorcall
  reinterpretToInt(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_castps_si128(value.value);
  #elif COMPLEX_NEON
    return vreinterpretq_u32_f32(value.value);
  #endif
  }
  constexpr forceinline i4 vectorcall reinterpretToInt(i4 value) { return value; }


  forceinline void vectorcall transpose(utils::array<f4, f4::size> &rows)
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

  forceinline void vectorcall complexTranspose(utils::array<f4, f4::complexSize> &rows)
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

  forceinline f4 vectorcall
  merge(f4 falseValue, f4 trueValue, i4mask mask)
  {
  #ifdef COMPLEX_SSE4_1
    return _mm_blendv_ps(falseValue.value, trueValue.value, reinterpretToFloat(mask).value);
  #elif COMPLEX_NEON
    return vbslq_f32(mask.value, trueValue.value, falseValue.value);
  #endif
  }

  forceinline i4 vectorcall
  merge(i4 falseValue, i4 trueValue, i4mask mask)
  {
  #ifdef COMPLEX_SSE4_1
    return reinterpretToInt(_mm_blendv_ps(reinterpretToFloat(falseValue).value,
      reinterpretToFloat(trueValue).value, reinterpretToFloat(mask).value));
  #elif COMPLEX_NEON
    return vbslq_u32(mask.value, trueValue.value, falseValue.value);
  #endif
  }

  forceinline f4 vectorcall lerp(f4 from, f4 to, f4 t)
  { return f4::mulAdd(from, to - from, t); }

  forceinline f4 vectorcall getDecimalPlaces(f4 value)
  { return value - f4::floor(value); }

  forceinline i4mask vectorcall getSign(i4 value)
  { return value & kSignMask; }

  forceinline i4mask vectorcall getSign(f4 value)
  { return reinterpretToInt(value) & kSignMask; }

  // lerps between the closest range of from and to inside [0; range]
  forceinline f4 vectorcall circularLerp(f4 from, f4 to, f4 t, f4 range)
  {
    f4 fromTo = to - from;
    f4 toFrom = (range ^ getSign(fromTo)) - fromTo;

    f4 result = merge(
      from - f4::mulAdd(range, t, toFrom),
      f4::mulAdd(from, t, fromTo),
      f4::lessThan(f4::abs(fromTo), f4::abs(toFrom)));

    return result - f4::floor(result / range) * range;
  }

  // lerps between the closest range of from and to inside +/- range
  forceinline f4 vectorcall circularLerpSymmetric(f4 from,
    f4 to, f4 t, f4 range)
  { return circularLerp(from + range, to + range, t, range * 2.0f) - range; }

  forceinline auto vectorcall getLinearInterpolationMatrix(f4 t)
  { return utils::array{ f4{ 0.0f }, f4{ 1.0f } - t, t, f4{ 0.0f } }; }

  forceinline auto vectorcall getCatmullInterpolationMatrix(f4 t)
  {
    f4 halfT = t * 0.5f;
    f4 halfT2 = t * halfT;
    f4 halfT3 = t * halfT2;
    f4 halfThreeT3 = halfT3 * 3.0f;

    return utils::array{
      f4::mulAdd(-halfT3, halfT2, 2.0f) - halfT,
      f4::mulSub(halfThreeT3, halfT2, 5.0f) + 1.0f,
      f4::mulAdd(halfT, halfT3, 4.0f) - halfThreeT3,
      halfT3 - halfT2
    };
  }

  forceinline auto vectorcall getValueMatrix(const float *buffer, i4 indices)
  {
    utils::array<f4, f4::size> values;
    for (u32 i = 0; i < values.size(); i++)
      values[i] = f4::load(buffer + indices[i]);
    return values;
  }

  template<auto N>
  forceinline f4 multiplyAndSumRows(const utils::array<f4, N> &one,
    const utils::array<f4, N> &two)
  {
    f4 summedVector = 0;
    for (usize i = 0; i < N; ++i)
      summedVector = f4::mulAdd(summedVector, one[i], two[i]);
    return summedVector;
  }

  forceinline f4 vectorcall copyFromEven(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    float32x2_t a00 = vdup_laneq_f32(value.value, 0);
    float32x2_t b22 = vdup_laneq_f32(value.value, 2);
    return vcombine_f32(a00, b22);
  #endif
  }

  forceinline i4 vectorcall copyFromEven(i4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    uint32x2_t a00 = vdup_laneq_u32(value.value, 0);
    uint32x2_t b22 = vdup_laneq_u32(value.value, 2);
    return vcombine_u32(a00, b22);
  #endif
  }

  forceinline f4 vectorcall copyFromOdd(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    float32x2_t a11 = vdup_laneq_f32(value.value, 1);
    float32x2_t b33 = vdup_laneq_f32(value.value, 3);
    return vcombine_f32(a11, b33);
  #endif
  }

  forceinline i4 vectorcall copyFromOdd(i4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    uint32x2_t a11 = vdup_laneq_u32(value.value, 1);
    uint32x2_t b33 = vdup_laneq_u32(value.value, 3);
    return vcombine_u32(a11, b33);
  #endif
  }

  forceinline f4 vectorcall groupEven(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 1, 2, 0));
  #elif COMPLEX_NEON
    return vuzp1q_f32(value.value, vrev64q_f32(value.value));
  #endif
  }

  forceinline f4 vectorcall groupEvenReverse(f4 value)
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

  forceinline f4 vectorcall groupOdd(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 0, 3, 1));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEvenReverse(value).value);
  #endif
  }

  forceinline f4 vectorcall groupOddReverse(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(0, 2, 1, 3));
  #elif COMPLEX_NEON
    return vrev64q_f32(groupEven(value).value);
  #endif
  }

  forceinline f4 vectorcall switchInner(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 3, 0, 1));
  #elif COMPLEX_NEON
    value.value = vrev64q_f32(value.value);
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }

  forceinline f4 vectorcall switchOuter(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(1, 0, 3, 2));
  #elif COMPLEX_NEON
    return vcombine_f32(vget_high_f32(value.value), vget_low_f32(value.value));
  #endif
  }


  template<SimdValue SIMD>
  forceinline SIMD vectorcall gather(const SIMD *values, i4 indices)
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

    if constexpr (utils::is_same_v<SIMD, i4>)
      return reinterpretToInt(one);
    else
      return one;
  #elif COMPLEX_NEON
    static constexpr i4mask one = utils::array{ kFullMask, 0U, 0U, 0U };
    static constexpr i4mask two = utils::array{ 0U, kFullMask, 0U, 0U };
    static constexpr i4mask three = utils::array{ 0U, 0U, kFullMask, 0U };
    static constexpr i4mask four = utils::array{ 0U, 0U, 0U, kFullMask };

    return (values[array[0]] & one  ) | (values[array[1]] & two) |
           (values[array[2]] & three) | (values[array[3]] & four);
  #endif
  }

  template<SimdValue SIMD>
  forceinline void vectorcall scatter(SIMD *values, SIMD value, i4 indices)
  {
    auto array = indices.getArrayOfValues();
  #if COMPLEX_SSE4_1
    auto valueArray = value.getArrayOfValues();

    auto one = reinterpretToInt(values[array[0]]).value;
    auto two = reinterpretToInt(values[array[1]]).value;
    auto three = reinterpretToInt(values[array[2]]).value;
    auto four = reinterpretToInt(values[array[3]]).value;

    if constexpr (utils::is_same_v<SIMD, f4>)
    {
      values[array[0]] = reinterpretToFloat(i4{ _mm_insert_epi32(one, valueArray[0], 0) });
      values[array[1]] = reinterpretToFloat(i4{ _mm_insert_epi32(two, valueArray[1], 1) });
      values[array[2]] = reinterpretToFloat(i4{ _mm_insert_epi32(three, valueArray[2], 2) });
      values[array[3]] = reinterpretToFloat(i4{ _mm_insert_epi32(four, valueArray[3], 3) });
    }
    else
    {
      values[array[0]] = _mm_insert_epi32(one, valueArray[0], 0);
      values[array[1]] = _mm_insert_epi32(two, valueArray[1], 1);
      values[array[2]] = _mm_insert_epi32(three, valueArray[2], 2);
      values[array[3]] = _mm_insert_epi32(four, valueArray[3], 3);
    }
  #elif COMPLEX_NEON
    static constexpr i4mask one = utils::array{ kFullMask, 0U, 0U, 0U };
    static constexpr i4mask two = utils::array{ 0U, kFullMask, 0U, 0U };
    static constexpr i4mask three = utils::array{ 0U, 0U, kFullMask, 0U };
    static constexpr i4mask four = utils::array{ 0U, 0U, 0U, kFullMask };

    values[array[0]] = merge(values[array[0]], value, one);
    values[array[1]] = merge(values[array[1]], value, two);
    values[array[2]] = merge(values[array[2]], value, three);
    values[array[3]] = merge(values[array[3]], value, four);
  #endif
  }

  template<SimdValue SIMD>
  forceinline SIMD vectorcall gatherComplex(const SIMD *values, i4 indices)
  {
    auto array = indices.getArrayOfValues();
    SIMD result = values[array[0]];
    for (usize i = 1; i < kChannelsPerInOut; ++i)
      result = merge(result, values[array[2 * i]], kChannelMasks[i]);
    return result;
  }

  template<SimdValue SIMD>
  forceinline void vectorcall scatterComplex(SIMD *values, i4 indices, SIMD value, i4mask mask)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i] & mask);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterComplex(SIMD *values, i4 indices, SIMD value)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], value, kChannelMasks[i]);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterAddComplex(SIMD *values, i4 indices, SIMD value, i4mask mask)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], values[array[2 * i]] + value, kChannelMasks[i] & mask);
  }
  template<SimdValue SIMD>
  forceinline void vectorcall scatterAddComplex(SIMD *values, i4 indices, SIMD value)
  {
    auto array = indices.getArrayOfValues();
    for (usize i = 0; i < kChannelsPerInOut; ++i)
      values[array[2 * i]] = merge(values[array[2 * i]], values[array[2 * i]] + value, kChannelMasks[i]);
  }



  // conditionally unsigns ints if they are negative and
  // returns a mask which can be used to xor the value restore the sign
  // if flag is set, a full mask where values are negative
  template<bool ReturnFullMask = false>
  forceinline i4mask vectorcall unsignSimd(i4 &value)
  {
    static constexpr i4mask signMask = kSignMask;
    i4mask mask = i4mask::equal(value & signMask, signMask);
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
  forceinline i4mask vectorcall unsignSimd(f4 &value)
  {
    static constexpr i4mask signMask = kSignMask;
    i4mask mask = reinterpretToInt(value) & signMask;
    value ^= mask;
    if constexpr (ReturnFullMask)
      return i4mask::equal(mask, signMask);
    else
      return mask;
  }

  // if equalsWrap == true/false, then the value will wrap around when it reaches/when it is greater than the modulo
  forceinline i4 vectorcall modOnce(i4 value, i4 mod, bool equalsWrap = true)
  {
    i4mask lessMask = (equalsWrap) ? i4::lessThanSigned(value, mod) :
      i4::lessThanOrEqualSigned(value, mod);
    i4 lower = value - mod;
    return merge(lower, value, lessMask);
  }

  // if equalsWrap == true/false, then the value will wrap around when it reaches/when it is greater than the modulo
  forceinline f4 vectorcall modOnce(f4 value, f4 mod, bool equalsWrap = true)
  {
    i4mask lessMask = (equalsWrap) ? f4::lessThan(value, mod) :
      f4::lessThanOrEqual(value, mod);
    f4 lower = value - mod;
    return merge(lower, value, lessMask);
  }

  forceinline f4 vectorcall modOnceSymmetric(f4 value, f4 mod, bool equalsWrap = true)
  {
    i4mask signMask = unsignSimd(value);
    i4mask lessMask = (equalsWrap) ? f4::lessThan(value, mod) :
      f4::lessThanOrEqual(value, mod);
    f4 lower = value - mod * 2.0f;
    return merge(lower, value, lessMask) ^ signMask;
  }

  forceinline f4 vectorcall modSymmetric(f4 value, f4 mod)
  {
    value /= mod;
    value -= f4::round(value * 0.5f) * 2.0f;
    return value * mod;
  }


  forceinline f4 vectorcall horizontalAdd(f4 one, f4 two)
  {
  #if COMPLEX_SSE4_1
    return _mm_hadd_ps(one.value, two.value);
  #elif COMPLEX_NEON
    return vpaddq_f32(one.value, two.value);
  #endif
  }

  forceinline f4 vectorcall horizontalSub(f4 one, f4 two)
  {
  #if COMPLEX_SSE4_1
    return _mm_hsub_ps(one.value, two.value);
  #elif COMPLEX_NEON
    static constexpr i4mask kMinusPlus = { 0U, kSignMask };
    return vpaddq_f32((one ^ kMinusPlus).value, (two ^ kMinusPlus).value);
  #endif
  }

  forceinline i4 vectorcall horizontalMin(i4 value)
  {
  #if COMPLEX_SSE4_1
    auto reversed = _mm_shuffle_epi32(value.value, _MM_SHUFFLE(0, 1, 2, 3));
    auto one = _mm_min_epi32(value.value, reversed);
    auto switched = _mm_shuffle_epi32(one, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_min_epi32(one, switched);
  #elif COMPLEX_NEON
    return i4{ (u32)vminvq_s32(vreinterpretq_s32_u32(value.value)) };
  #endif
  }

  forceinline f4 vectorcall horizontalMin(f4 value)
  {
  #if COMPLEX_SSE4_1
    return reinterpretToFloat(horizontalMin(reinterpretToInt(value)));
  #elif COMPLEX_NEON
    return vminvq_f32(value.value);
  #endif
  }

  forceinline f4 vectorcall reciprocal(f4 value)
  {
  #if COMPLEX_SSE4_1
    return _mm_rcp_ps(value.value);
  #elif COMPLEX_NEON
    return vrecpeq_f32(value.value);
  #endif
  }

  template<u32 Shift>
  forceinline i4 vectorcall shiftRight(i4 values)
  {
  #if COMPLEX_SSE4_1
    return _mm_srli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshrq_n_u32(values.value, Shift);
  #endif
  }

  template<u32 shift>
  forceinline i4 vectorcall shiftRightArithmetic(i4 values) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_srai_epi32(values.value, shift);
  #elif COMPLEX_NEON
    return vshrq_n_s32(values.value, shift);
  #endif
  }

  template<u32 Shift>
  forceinline i4 vectorcall shiftLeft(i4 values)
  {
  #if COMPLEX_SSE4_1
    return _mm_slli_epi32(values.value, Shift);
  #elif COMPLEX_NEON
    return vshlq_n_u32(values.value, Shift);
  #endif
  }



  forceinline f4 vectorcall exp2(f4 exponent)
  {
    // taylor expansion of 2^x at 0
    // coefficients are (ln(2)^n) / n!
    static constexpr f4 kCoefficient0 = 1.0f;
    static constexpr f4 kCoefficient1 = 16970.0f / 24483.0f;
    static constexpr f4 kCoefficient2 = 1960.0f / 8161.0f;
    static constexpr f4 kCoefficient3 = 1360.0f / 24483.0f;
    static constexpr f4 kCoefficient4 = 80.0f / 8161.0f;
    static constexpr f4 kCoefficient5 = 32.0f / 24483.0f;

    // the closer the exponent is to a whole number, the more accurate it's going to be
    // since it only requires to add it the overall floating point exponent
    f4 rounded = f4::round(exponent);
    f4 t = exponent - rounded;
    // clamp the lowest value otherwise get garbage results when shifting left
    f4 power = reinterpretToFloat(shiftLeft<23>(i4::maxSigned((u32)-127, toInt(rounded)) + 127));

    // we exp2 whatever decimal number is left with the taylor series
    // the domain we're in is [0.0f, 0.5f], we don't expect negative numbers
    f4 interpolate = f4::mulAdd(kCoefficient0, t,
      f4::mulAdd(kCoefficient1, t, f4::mulAdd(kCoefficient2, t,
        f4::mulAdd(kCoefficient3, t, f4::mulAdd(kCoefficient4, t, kCoefficient5)))));

    return power * interpolate;
  }

  // (intentionally) does not handle nans and negative numbers correctly
  // at 0/inf it returns the min/max exponent value
  forceinline f4 vectorcall log2(f4 value)
  {
    // modified/minimax-ed taylor coefficients for atanh
    // in order to approximatea a contracted version of log2
    // k0 = 2/log(2), k1 ~= 2/(3*log(2)), k2 ~= 2/(5*log(2)), k3 ~= 2/(7*log(2))
    static constexpr f4 k0 = 1.44269504088896340736f;
    static constexpr f4 k1 = 0.96180442874507310336f;
    static constexpr f4 k2 = 0.57647797555770874389f;
    static constexpr f4 k3 = 0.43323255205601618467f;
    static constexpr i4 kSqrt2Addition = 0x004afb10;

    COMPLEX_ASSERT(i4::anyMask(f4::lessThan(value, 0.0f)) == 0);

    i4 reinterpreted = reinterpretToInt(value);
    i4 mantissaOnly = reinterpreted & kFloatMantissaMask;
    // if the raw mantissa represents a number larger than sqrt(2) (assuming the exponent is 2^0),
    // mod it once in order to bring it back to the range [sqrt(2)/2, sqrt(2)) (more info below)
    // this happens by adding a magic constant and extract the lowest bit of the exponent
    i4 exponentOffset = (mantissaOnly + kSqrt2Addition) & kFloatExponentUnit;
    i4 roundLog2 = shiftRightArithmetic<23>(reinterpreted.value) - 127 + shiftRightArithmetic<23>(exponentOffset);

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
    f4 m = reinterpretToFloat((reinterpretToInt(f4{ 1.0f }) ^ exponentOffset) | mantissaOnly);
    f4 numerator = m - 1.0f;
    f4 z = numerator / (m + 1.0f);

    f4 z2 = z * z;
    f4 z4 = z2 * z2;

    // the following line is what is effectively being executed below but it's commented out
    // because the other version produces results with lower fp inaccuracies
    //f4 result = toFloat(ceilLog2) + z * (2.0f * k0 + ((k2 * z4) + z2 * (k1 + z4 * k3)));
    f4 numInvLog2 = numerator * k0;
    f4 result = toFloat(roundLog2) + (numInvLog2 + (z * (-numInvLog2 + ((k2 * z4) + z2 * (k1 + z4 * k3)))));

    return result;
  }

  forceinline f4 vectorcall exp(f4 exponent)
  { return exp2(exponent * kExpConversionMult); }

  forceinline f4 vectorcall log(f4 value)
  { return log2(value) * kLogConversionMult; }

  forceinline f4 vectorcall pow(f4 base, f4 exponent)
  { return exp2(log2(base) * exponent); }

  forceinline f4 vectorcall midiOffsetToRatio(f4 note_offset)
  { return exp2(note_offset * (1.0f / kNotesPerOctave)); }

  forceinline f4 vectorcall midiNoteToFrequency(f4 note)
  { return midiOffsetToRatio(note) * kMidi0Frequency; }

  // fast approximation of the original equation
  forceinline f4 vectorcall amplitudeToDb(f4 magnitude)
  { return log2(magnitude) * kAmplitudeToDbConversionMult; }

  // fast approximation of the original equation
  forceinline f4 vectorcall dbToAmplitude(f4 decibels)
  { return exp2(decibels * kDbToAmplitudeConversionMult); }

  forceinline f4 vectorcall normalisedToDb(f4 normalised, float maxDb)
  { return pow(maxDb + 1.0f, normalised) - 1.0f; }

  forceinline f4 vectorcall dbToNormalised(f4 db, float maxDb)
  { return log2(db + 1.0f) / log2(f4{ maxDb + 1.0f }); }

  forceinline f4 vectorcall normalisedToFrequency(f4 normalised, float sampleRate, float minFrequency = kMinFrequency)
  { return pow(sampleRate * 0.5f / minFrequency, normalised) * minFrequency; }

  forceinline f4 vectorcall frequencyToNormalised(f4 frequency, float sampleRate, float minFrequency = kMinFrequency)
  { return log2(frequency / minFrequency) / log2(f4{ sampleRate * 0.5f / minFrequency }); }

  // returns the proper bin which may also be nyquist, which is outside a power-of-2
  forceinline f4 vectorcall normalisedToBin(f4 normalised, u32 FFTSize, float sampleRate)
  {
    i4mask zeroMask = f4::notEqual(normalised, 0.0f);
    return f4::round(normalisedToFrequency(normalised, sampleRate) / sampleRate * (float)FFTSize) & zeroMask;
  }

  forceinline f4 vectorcall binToNormalised(f4 bin, u32 FFTSize, float sampleRate)
  {
    // for 0 logarithm doesn't produce valid values
    // so we mask that with dummy values to not get errors
    i4mask zeroMask = f4::notEqual(bin, 0.0f);
    return frequencyToNormalised(bin * sampleRate / (float)FFTSize, sampleRate) & zeroMask;
  }

  forceinline float exp2(float value)
  {
    f4 input = value;
    f4 result = exp2(input);
    return result[0];
  }

  forceinline float log2(float value)
  {
    f4 input = value;
    f4 result = log2(input);
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

  forceinline f4 powerScale(f4 value, f4 power)
  {
    static constexpr float kMinPowerMag = 0.005f;
    i4mask zeroMask = f4::lessThan(power, kMinPowerMag) & f4::lessThan(-power, kMinPowerMag);
    f4 numerator = exp(power * value) - 1.0f;
    f4 denominator = exp(power) - 1.0f;
    f4 result = numerator / denominator;
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

  forceinline f4 vectorcall getStereoDifference(f4 value)
  {
    return (value - switchInner(value)) * 0.5f;
  }

  // assumes value is signed
  forceinline i4 vectorcall getStereoDifference(i4 value)
  {
    i4 highestBit = value & kSignMask;
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
    f4 cubicLookup(f4 x) const
    {
      COMPLEX_ASSERT(i4mask::anyMask(f4::lessThan(x, 0.0f)) == 0 &&
        i4mask::anyMask(f4::greaterThan(x, 1.0f)) == 0);

      f4 boost = (x * scale_) + 1.0f;
      i4 indices = i4::clampUnsigned(utils::toInt(boost), i4(1), i4(Resolution));
      f4 t = boost - utils::toFloat(indices);

      auto interpolationMatrix = utils::getCatmullInterpolationMatrix(t);
      auto valueMatrix = utils::getValueMatrix(lookup_, indices - i4(1));
      transpose(valueMatrix);

      return multiplyAndSumRows(interpolationMatrix, valueMatrix);
    }

    // gets linearly interpolated y-values at their corresponding x-values
    f4 linearLookup(f4 x) const
    {
      COMPLEX_ASSERT(i4mask::anyMask(f4::lessThan(x, 0.0f)) == 0 &&
        i4mask::anyMask(f4::greaterThan(x, 1.0f)) == 0);

      f4 boost = (x * scale_) + 1.0f;
      i4 indices = i4::clampUnsigned(utils::toInt(boost), i4(1), i4(Resolution));
      f4 t = boost - utils::toFloat(indices);

      auto valueMatrix = utils::getValueMatrix(lookup_, indices - i4(1));
      transpose(valueMatrix);

      return f4::mulAdd((1.0f - t) * valueMatrix[1], t, valueMatrix[2]);
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
