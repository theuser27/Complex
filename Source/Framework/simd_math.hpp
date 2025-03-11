/*
  ==============================================================================

    simd_math.hpp
    Created: 14 Sep 2021 12:55:12am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "simd_buffer.hpp"
#include "simd_utils.hpp"

namespace utils
{
  // layout of complex cartesian and polar vectors is assumed to be
  // { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

  // [cos, sin]
  strict_inline utils::pair<simd_float, simd_float> vector_call cossin(simd_float radians)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr simd_float kHalfPiPart1 = 1.5703125f;
    static constexpr simd_float kHalfPiPart2 = 0.0004838267953f;

    // taylor coefficients of sin
    static constexpr simd_float kSin1 = -0.16666655f;
    static constexpr simd_float kSin2 = 0.00833216f;
    static constexpr simd_float kSin3 = -0.00019515282f;

    // taylor coefficients of cos
    static constexpr simd_float kCos0 = 1.0f;
    static constexpr simd_float kCos1 = -0.5f;
    static constexpr simd_float kCos2 = 0.041666646f;
    static constexpr simd_float kCos3 = -0.0013887316f;
    static constexpr simd_float kCos4 = 0.000024433157f;

    static constexpr simd_float k2InvPi = 2.0f / kPi;
    static constexpr simd_float kTruncate = 12582912.0f;

    simd_float normalisedInput = radians * k2InvPi;
    simd_int truncatedInt = reinterpretToInt(normalisedInput + kTruncate);
    simd_float truncatedFloat = reinterpretToFloat(truncatedInt) - kTruncate;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra xor is to guard against nefarious bits left by fast math
    simd_mask exactMask = simd_float::equal(truncatedFloat, normalisedInput & simd_mask{ ~1U });

    simd_float position = radians - (truncatedFloat * kHalfPiPart1) - (truncatedFloat * kHalfPiPart2);
    simd_float position2 = position * position;

    simd_int lowestMantissaBit = truncatedInt & 1;
    simd_mask sinSign = shiftLeft<30>(truncatedInt & 2);
    simd_mask cosSign = shiftLeft<30>((truncatedInt + lowestMantissaBit) & 2);
    simd_mask hasLowestBitMask = simd_int::notEqual(lowestMantissaBit, 0);

    simd_float cos = simd_float::mulAdd(kCos0, position2, simd_float::mulAdd(kCos1, position2,
      simd_float::mulAdd(kCos2, position2, simd_float::mulAdd(kCos3, position2, kCos4))));
    simd_float sin = simd_float::mulAdd(position, position, position2 *
      simd_float::mulAdd(kSin1, position2, simd_float::mulAdd(kSin2, position2, kSin3)));

    cos = merge(cos, 1.0f, exactMask);
    sin = sin & ~exactMask;

    return { merge(cos, sin, hasLowestBitMask) ^ cosSign,
      merge(sin, cos, hasLowestBitMask) ^ sinSign };
  }

  strict_inline simd_float vector_call cis(simd_float angle)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr simd_float kHalfPiPart1 = 1.5703125f;
    static constexpr simd_float kHalfPiPart2 = 0.0004838267953f;

    // modified taylor coefficients of { cos, sin }
    static constexpr simd_float k0 = { 1.0f, 0.0f };
    static constexpr simd_float k1 = { -0.5f, 1.0f };
    static constexpr simd_float k2 = { 0.041666646f, -0.16666655f };
    static constexpr simd_float k3 = { -0.0013887316f, 0.00833216f };
    static constexpr simd_float k4 = { 0.000024433157f, -0.00019515282f };

    static constexpr simd_float k2InvPi = 2.0f / kPi;
    static constexpr simd_float kTruncate = 12582912.0f;
    static constexpr simd_float kExact = { 1.0f, 0.0f };

    simd_float normalisedInput = angle * k2InvPi;
    simd_int truncatedInt = reinterpretToInt(normalisedInput + kTruncate);
    simd_float truncatedFloat = reinterpretToFloat(truncatedInt) - kTruncate;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra and is to guard against nefarious bits left by fast math
    simd_mask exactMask = simd_float::equal(truncatedFloat, normalisedInput & simd_mask{ ~1U });

    simd_float position = angle - (truncatedFloat * kHalfPiPart1) - (truncatedFloat * kHalfPiPart2);
    simd_float position2 = position * position;

    simd_int lowestMantissaBit = truncatedInt & 1;
    simd_mask signs = shiftLeft<30>((truncatedInt + (lowestMantissaBit & kRealMask)) & 2);
    simd_mask hasLowestBitMask = simd_int::notEqual(lowestMantissaBit, 0);

    simd_float values = simd_float::mulAdd(k0, merge(position, position2, kRealMask),
      simd_float::mulAdd(k1, position2, simd_float::mulAdd(k2, position2, simd_float::mulAdd(k3, position2, k4))));
    values = merge(values, kExact, exactMask);
    values = merge(values, switchInner(values), hasLowestBitMask) ^ signs;
    return values;
  }

  strict_inline simd_float vector_call sin(simd_float radians) { return cossin(radians).second; }
  strict_inline simd_float vector_call cos(simd_float radians) { return cossin(radians).first; }
  strict_inline simd_float vector_call tan(simd_float radians)
  {
  #ifdef COMPLEX_INTEL_SVML
    return _mm_tan_ps(radians.value);
  #else
    auto [cos, sin] = cossin(radians);
    return sin / cos;
  #endif
  }

  strict_inline simd_float vector_call atan2(simd_float y, simd_float x)
  {
  #ifndef COMPLEX_INTEL_SVML
    // based on "Efficient approximations for the arctangent function"
    // max error ~= 0.008 degrees
    // https://www.desmos.com/calculator/nmhr3wmgzj
    static constexpr simd_float a = 0.35496f;
    static constexpr simd_float b = -0.0815f;

    simd_float yxDiv = y / x;
    simd_float yxDivSqr = yxDiv * yxDiv;
    simd_float xyDiv = reciprocal(yxDiv);
    simd_float xyDivSqr = xyDiv * xyDiv;

    simd_float firstHalf = yxDiv / (yxDivSqr * simd_float::abs(yxDiv) * b + yxDivSqr * a + 1.0f);
    simd_float secondHalf = (simd_float(kPi * 0.5f) ^ getSign(xyDiv)) - xyDiv / (xyDivSqr * simd_float::abs(xyDiv) * b + xyDivSqr * a + 1.0f);
    simd_float angle = merge(firstHalf, secondHalf, simd_float::greaterThan(simd_float::abs(yxDiv), 1.0f));

    simd_mask realEqualZeroMask = simd_float::equal(x, 0.0f);
    simd_mask imaginaryEqualZeroMask = simd_float::equal(y, 0.0f);
    simd_float extraShift = (simd_float(kPi) & ~realEqualZeroMask) & simd_float::lessThanOrEqual(x, 0.0f);
    extraShift ^= getSign(y);

    angle += extraShift;
    angle &= ~(realEqualZeroMask & imaginaryEqualZeroMask);

    return angle;
  #else
    return _mm_atan2_ps(y.value, x.value);
  #endif
  }

  // [cos, sin]
  strict_inline utils::pair<simd_float, simd_float> vector_call cisFast(simd_float radians)
  {
    // TODO: implement bhaskara's algo again

    // pade approximants of sine
    // max error ~= 3.00438 * 10^(-6)
    // https://www.desmos.com/calculator/oit7uxh1wm
    static constexpr simd_float kNum1 = 166320.0f * kPi;
    static constexpr simd_float kNum2 = -22260.0f * kPi * kPi * kPi;
    static constexpr simd_float kNum3 = 551.0f * kPi * kPi * kPi * kPi * kPi;
    static constexpr simd_float kDen1 = 166320.0f;
    static constexpr simd_float kDen2 = 5460.0f * kPi * kPi;
    static constexpr simd_float kDen3 = 75.0f * kPi * kPi * kPi * kPi;

    // correction for angles after +/-pi, normalises to +/-1
    radians *= kInvPi;
    radians -= simd_float::round(radians * 0.5f) * 2.0f;

    simd_mask cosSign = simd_float::greaterThanOrEqual(radians, 0.0f);
    simd_mask sinSign = simd_float::greaterThan(simd_float::abs(radians), 0.5f);

    simd_float cosPosition = radians + 0.5f - (simd_float{ 1.0f } & cosSign);
    simd_float sinPosition = radians - ((simd_float{ 1.0f } & sinSign) ^ getSign(radians));

    simd_float cosPosition2 = cosPosition * cosPosition;
    simd_float sinPosition2 = sinPosition * sinPosition;

    simd_float cos = (cosPosition * simd_float::mulAdd(kNum1, cosPosition2, simd_float::mulAdd(kNum2, cosPosition2, kNum3))) /
                                    simd_float::mulAdd(kDen1, cosPosition2, simd_float::mulAdd(kDen2, cosPosition2, kDen3));
    simd_float sin = (sinPosition * simd_float::mulAdd(kNum1, sinPosition2, simd_float::mulAdd(kNum2, sinPosition2, kNum3))) /
                                    simd_float::mulAdd(kDen1, sinPosition2, simd_float::mulAdd(kDen2, sinPosition2, kDen3));

    return { cos ^ (cosSign & kSignMask), sin ^ (sinSign & kSignMask) };
  }

  // [magnitude, phase]
  strict_inline utils::pair<simd_float, simd_float> vector_call phasor(simd_float real, simd_float imaginary)
  {
    auto magnitude = simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary));
  #ifdef COMPLEX_INTEL_SVML
    return { magnitude, simd_float{ _mm_atan2_ps(imaginary.value, real.value) } };
  #else
    return { magnitude, atan2(imaginary, real) };
  #endif
  }

  strict_inline simd_float vector_call complexCartMul(simd_float one, simd_float two)
  {
    // [a1c1, a1d1, a2c2, a2d2]
    simd_float sums1 = copyFromEven(one) * two;
    // [b1d1, b1c1, b2d2, b2c2]
    simd_float sums2 = copyFromOdd(one) * switchInner(two);

    // [a1c1 - b1d1, a1d1 + b1c1, a2c2 - b2d2, a2d2 + b2c2]
  #if COMPLEX_SSE4_1
    // yes it's addsub but the first op is sub
    return _mm_addsub_ps(sums1.value, sums2.value);
  #elif COMPLEX_NEON
    static constexpr simd_mask kMinusPlus = { kSignMask, 0U };
    return sums1 + (sums2 ^ kMinusPlus);
  #endif
  }

  strict_inline simd_float vector_call complexPolarMul(simd_float one, simd_float two)
  { return merge(one * two, one + two, kPhaseMask); }

  strict_inline simd_float vector_call complexMagnitude(simd_float value, bool toSqrt)
  {
    value *= value;
    value += switchInner(value);
    return (toSqrt) ? simd_float::sqrt(value) : value;
  }

  strict_inline simd_float vector_call complexMagnitude(
    const utils::array<simd_float, simd_float::complexSize> &values, bool toSqrt)
  {
    simd_float one = values[0] * values[0];
    simd_float two = values[1] * values[1];
    one = horizontalAdd(one, two);
    return (toSqrt) ? simd_float::sqrt(one) : one;
  }

  strict_inline simd_float vector_call complexPhase(simd_float value)
  {
    simd_float real = copyFromEven(value);
    simd_float imaginary = copyFromOdd(value);

    return atan2(imaginary, real);
  }

  strict_inline simd_float vector_call complexPhase(
    const utils::array<simd_float, simd_float::complexSize> &values)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    simd_float real = vuzp1q_f32(values[0].value, values[1].value);
    simd_float imaginary = vuzp2q_f32(values[0].value, values[1].value);
  #endif

    return atan2(imaginary, real);
  }

  strict_inline simd_float vector_call complexReal(simd_float value)
  {
    simd_float magnitude = copyFromEven(value);
    simd_float phase = copyFromOdd(value);

    return magnitude * cos(phase);
  }

  strict_inline simd_float vector_call complexImaginary(simd_float value)
  {
    simd_float magnitude = copyFromEven(value);
    simd_float phase = copyFromOdd(value);

    return magnitude * sin(phase);
  }

  strict_inline void vector_call complexValueMerge(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    auto one_ = _mm_unpacklo_ps(one.value, two.value);
    two.value = _mm_unpackhi_ps(one.value, two.value);
    one.value = one_;
  #elif COMPLEX_NEON
    auto one_ = vzip1q_f32(one.value, two.value);
    two.value = vzip2q_f32(one.value, two.value);
    one.value = one_;
  #endif
  }

  strict_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    simd_float real = vuzp1q_f32(one.value, two.value);
    simd_float imaginary = vuzp2q_f32(one.value, two.value);
  #endif
    auto [magnitude, phase] = phasor(real, imaginary);
    complexValueMerge(magnitude, phase);
    one = magnitude;
    two = phase;
  }

  strict_inline void vector_call complexPolarToCart(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float phases = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    simd_float phases = vuzp2q_f32(one.value, two.value);
  #endif
    auto [real, imaginary] = cossin(phases);
    complexValueMerge(real, imaginary);
    simd_float magnitudesOne = copyFromEven(one);
    simd_float magnitudesTwo = copyFromEven(two);
    one = real * magnitudesOne;
    two = imaginary * magnitudesTwo;
  }

  template<auto ConversionFunction>
  strict_inline void convertBuffer(const Framework::SimdBufferView<Framework::complex<float>, simd_float> &source,
    Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, usize size)
  {
    auto rawSource = source.get();
    auto rawDestination = destination.get();
    usize sourceSize = source.getSize();
    usize destinationSize = destination.getSize();
    
    for (usize i = 0; i < source.getSimdChannels(); i++)
    {
      // size - 1 to skip nyquist since it doesn't need to get processed
      for (usize j = 0; j < size - 1; j += 2)
      {
        simd_float one = rawSource[sourceSize * i + j];
        simd_float two = rawSource[sourceSize * i + j + 1];
        ConversionFunction(one, two);
        rawDestination[destinationSize * i + j] = one;
        rawDestination[destinationSize * i + j + 1] = two;
      }
      // dc
      rawDestination[destinationSize * i] = rawSource[sourceSize * i];
      // nyquist
      rawDestination[destinationSize * i + size - 1] = rawSource[sourceSize * i + size - 1];
    }
  }

  template<auto ConversionFunction>
  strict_inline void convertBufferInPlace(Framework::SimdBuffer<Framework::complex<float>, simd_float> &buffer, usize size)
  {
    auto data = buffer.get();
    usize dataSize = buffer.getSize();

    for (usize i = 0; i < buffer.getSimdChannels(); i++)
    {
      auto dc = data[dataSize * i];
      // size - 1 to skip nyquist since it doesn't need to change
      for (usize j = 0; j < size - 1; j += 2)
      {
        simd_float one = data[dataSize * i + j];
        simd_float two = data[dataSize * i + j + 1];
        ConversionFunction(one, two);
        data[dataSize * i + j] = one;
        data[dataSize * i + j + 1] = two;
      }
      data[dataSize * i] = dc;
    }
  }
}
