
// Created: 2021-09-14 00:55:12

#pragma once

#include "simd_buffer.hpp"
#include "simd_utils.hpp"

namespace utils
{
  // layout of complex cartesian and polar vectors is assumed to be
  // { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

  // cos and sin
  forceinline utils::pair<simd_float, simd_float> vectorcall
  cossin(simd_float radians)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr simd_float kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr simd_float kHalfPiPart2 = "0 01110011 11111011010101000100010"_fbl;

    // taylor coefficients of sin
    static constexpr simd_float kSin1 = -0.166666518f;
    static constexpr simd_float kSin2 = 0.00833202855f;
    static constexpr simd_float kSin3 = -0.0001950085f;

    // taylor coefficients of cos
    static constexpr simd_float kCos0 = 1.0f;
    static constexpr simd_float kCos1 = -0.5f;
    static constexpr simd_float kCos2 = 0.0416666459f;
    static constexpr simd_float kCos3 = -0.0013887321f;
    static constexpr simd_float kCos4 = 0.000024433157f;

    static constexpr simd_float k2InvPi = 2.0f / kPi;
    // adding this forces the value to get rounded to int (only the lower 23 bits are valid) and
    // subtracting it again yields the rounded f32 (assuming value is positive)
    static constexpr simd_float kRound = 12582912.0f;

    simd_float normalisedInput = radians * k2InvPi;
    // using the magic constant and the hidden GRS bits to round normalisedInput
    simd_int roundedInt = reinterpretToInt(normalisedInput + kRound);
    simd_float roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra masking is to guard against nefarious bits left by fast math
    simd_mask exactMask = simd_float::equal(roundedFloat, normalisedInput & simd_mask{ ~1U });

    simd_float position = radians - (roundedFloat * kHalfPiPart1) - (roundedFloat * kHalfPiPart2);
    simd_float position2 = position * position;

    // LSB/LSB+1 represents a 90/180 degree rotation
    simd_int lowestMantissaBit = roundedInt & 1;
    simd_mask sinSign = shiftLeft<30>(roundedInt & 2);
    simd_mask cosSign = shiftLeft<30>((roundedInt + lowestMantissaBit) & 2);
    simd_mask quadrantMask = simd_int::equal(lowestMantissaBit, 0);

    // simple taylor series
    simd_float cos = simd_float::mulAdd(kCos0, position2, simd_float::mulAdd(kCos1, position2,
      simd_float::mulAdd(kCos2, position2, simd_float::mulAdd(kCos3, position2, kCos4))));
    simd_float sin = simd_float::mulAdd(position, position, position2 *
      simd_float::mulAdd(kSin1, position2, simd_float::mulAdd(kSin2, position2, kSin3)));

    cos = merge(cos, 1.0f, exactMask);
    // equivalent: sin = merge(sin, 0.0f, exactMask);
    sin = sin & ~exactMask;

    return { merge(sin, cos, quadrantMask) ^ cosSign,
      merge(cos, sin, quadrantMask) ^ sinSign };
  }

  // [cos(angle[0]), sin(angle[1]), cos(angle[2]), sin(angle[3])]
  forceinline simd_float vectorcall
  cis(simd_float angle)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr simd_float kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr simd_float kHalfPiPart2 = "0 01110011 11111011010101000100010"_fbl;

    // modified taylor coefficients of { cos, sin }
    static constexpr simd_float k0 = { 1.0f, 0.0f };
    static constexpr simd_float k1 = { -0.5f, 1.0f };
    static constexpr simd_float k2 = { 0.0416666459f, -0.166666518f };
    static constexpr simd_float k3 = { -0.0013887321f, 0.00833202855f };
    static constexpr simd_float k4 = { 0.000024433157f, -0.0001950085f };

    static constexpr simd_float k2InvPi = 2.0f / kPi;
    // adding this forces the value to get rounded to int (only the lower 23 bits are valid) and
    // subtracting it again yields the rounded f32 (assuming value is positive)
    static constexpr simd_float kRound = 12582912.0f;
    static constexpr simd_float kExact = { 1.0f, 0.0f };

    simd_float normalisedInput = angle * k2InvPi;
    // using the magic constant and the hidden GRS bits to round normalisedInput
    simd_int roundedInt = reinterpretToInt(normalisedInput + kRound);
    simd_float roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra masking is to guard against nefarious bits left by fast math
    simd_mask exactMask = simd_float::equal(roundedFloat, normalisedInput & simd_mask{ ~1U });

    simd_float position = angle - (roundedFloat * kHalfPiPart1) - (roundedFloat * kHalfPiPart2);
    simd_float position2 = position * position;

    // LSB/LSB+1 represents a 90/180 degree rotation
    simd_int lowestMantissaBit = roundedInt & 1;
    simd_mask signs = shiftLeft<30>((roundedInt + (lowestMantissaBit & kRealMask)) & 2);
    simd_mask quadrantMask = simd_int::equal(lowestMantissaBit, 0);

    // simple taylor series
    simd_float values = simd_float::mulAdd(k0, merge(position, position2, kRealMask),
      simd_float::mulAdd(k1, position2, simd_float::mulAdd(k2, position2, simd_float::mulAdd(k3, position2, k4))));
    values = merge(values, kExact, exactMask);
    values = merge(switchInner(values), values, quadrantMask) ^ signs;
    return values;
  }

  forceinline simd_float vectorcall sin(simd_float radians) { return cossin(radians).second; }
  forceinline simd_float vectorcall cos(simd_float radians) { return cossin(radians).first; }
  forceinline simd_float vectorcall
  tan(simd_float radians)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr simd_float kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr simd_float kHalfPiPart2 = "0 01110011 11111011010000000000000"_fbl;
    static constexpr simd_float kHalfPiPart3 = "0 01100111 01000100010000000000000"_fbl;
    static constexpr simd_float kHalfPiPart4 = "0 01011000 01101000110000100011010"_fbl;

    // pade coefficients for tan
    static constexpr simd_float kNum0 = 0.9999997615814208984375f;
    static constexpr simd_float kNum1 = -0.0958017408847808837890625f;
    static constexpr simd_float kDen0 = kNum0;
    static constexpr simd_float kDen1 = -0.4291356503963470458984375f;
    static constexpr simd_float kDen2 = 0.009716848842799663543701171875f;

    static constexpr simd_float k2InvPi = 2.0f / kPi;
    static constexpr simd_float kRound = 12582912.0f;

    simd_float absInput = simd_float::abs(radians);
    simd_mask inputSign = simd_float::signMask(radians);
    simd_int roundedInt = reinterpretToInt(absInput * k2InvPi + kRound);
    simd_float roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    simd_mask quadrantMask = simd_int::equal(roundedInt & 1, 0);

    simd_float position = absInput - (kHalfPiPart1 * roundedFloat) - (kHalfPiPart2 * roundedFloat) -
      (kHalfPiPart3 * roundedFloat) - (kHalfPiPart4 * roundedFloat);
    simd_float position2 = position * position;

    // pade approximants of tan(x)
    simd_float numerator = position * simd_float::mulAdd(kNum0, position2, kNum1);
    simd_float denominator = simd_float::mulAdd(kDen0, position2, simd_float::mulAdd(kDen1, position2, kDen2));
    return (merge(denominator, numerator, quadrantMask) / merge(numerator, denominator, quadrantMask)) ^
      shiftLeft<31>(roundedInt) ^ inputSign;
  }

  forceinline simd_float vectorcall
  atan2(simd_float y, simd_float x)
  {
    // taylor series coefficients of atan(x)
    static constexpr simd_float k0 = 1.0f;
    static constexpr simd_float k1 = -0.3333307206630706787109375f;
    static constexpr simd_float k2 = 0.199926197528839111328125f;
    static constexpr simd_float k3 = -0.14203643798828125f;
    static constexpr simd_float k4 = 0.1064093410968780517578125f;
    static constexpr simd_float k5 = -0.07504294812679290771484375f;
    static constexpr simd_float k6 = 0.042691521346569061279296875f;
    static constexpr simd_float k7 = -0.016068629920482635498046875f;
    static constexpr simd_float k8 = 0.00284988968633115291595458984375f;

    simd_float xAbs = simd_float::abs(x);
    simd_float yAbs = simd_float::abs(y);
    simd_float sign = y & simd_int{ kSignMask };
    simd_mask mask = simd_float::lessThan(xAbs, yAbs);
    simd_float value = (utils::merge(yAbs, xAbs, mask) / utils::merge(xAbs, yAbs, mask)) | (mask & simd_int{ kSignMask });
    simd_float v2 = value * value;
    simd_float v4 = v2 * v2;
    simd_mask zeroOverZeroMask = simd_int::equal(0, reinterpretToInt(xAbs)) & simd_int::equal(0, reinterpretToInt(yAbs));

    // taylor series split into 2 radix-4 sums
    simd_float one = v2 * simd_float::mulAdd(k1, v4, simd_float::mulAdd(k3, v4, simd_float::mulAdd(k5, k7, v4)));
    simd_float two = simd_float::mulAdd(k0, v4, simd_float::mulAdd(k2, v4, simd_float::mulAdd(k4, v4, simd_float::mulAdd(k6, k8, v4))));
    simd_float three = (x & simd_mask{ kSignMask }) ^ simd_float::mulAdd((simd_float{ kPi * 0.5f } &mask), value, (one + two));
    simd_float result = (three + (simd_float{ kPi } & simd_float::lessThan(x, 0.0f))) ^ sign;

    return result & ~zeroOverZeroMask;
  }

  // magnitude and phase
  forceinline utils::pair<simd_float, simd_float> vectorcall
  phasor(simd_float real, simd_float imaginary)
  {
    auto magnitude = simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary));
    return { magnitude, atan2(imaginary, real) };
  }

  forceinline simd_float vectorcall
  complexCartMul(simd_float one, simd_float two)
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

  forceinline simd_float vectorcall
  complexPolarMul(simd_float one, simd_float two)
  { return merge(one * two, one + two, kPhaseMask); }

  forceinline simd_float vectorcall
  complexMagnitude(simd_float value, bool toSqrt)
  {
    value *= value;
    value += switchInner(value);
    return (toSqrt) ? simd_float::sqrt(value) : value;
  }

  forceinline simd_float vectorcall
  complexMagnitude(const utils::array<simd_float, simd_float::complexSize> &values, bool toSqrt)
  {
    simd_float one = values[0] * values[0];
    simd_float two = values[1] * values[1];
    one = horizontalAdd(one, two);
    return (toSqrt) ? simd_float::sqrt(one) : one;
  }

  forceinline simd_float vectorcall
  complexPhase(simd_float value)
  {
    simd_float real = copyFromEven(value);
    simd_float imaginary = copyFromOdd(value);

    return atan2(imaginary, real);
  }

  forceinline simd_float vectorcall
  complexPhase(const utils::array<simd_float, simd_float::complexSize> &values)
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

  forceinline simd_float vectorcall
  complexReal(simd_float value)
  {
    simd_float magnitude = copyFromEven(value);
    simd_float phase = copyFromOdd(value);

    return magnitude * cos(phase);
  }

  forceinline simd_float vectorcall
  complexImaginary(simd_float value)
  {
    simd_float magnitude = copyFromEven(value);
    simd_float phase = copyFromOdd(value);

    return magnitude * sin(phase);
  }

  forceinline void vectorcall complexValueMerge(simd_float &one, simd_float &two)
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

  forceinline void vectorcall complexCartToPolar(simd_float &one, simd_float &two)
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

  forceinline void vectorcall complexPolarToCart(simd_float &one, simd_float &two)
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
  forceinline void convertBuffer(const Framework::SimdBuffer *source,
    Framework::SimdBuffer *destination, usize size)
  {
    auto rawSource = source->get();
    auto rawDestination = destination->get();
    usize sourceSize = source->size;
    usize destinationSize = destination->size;

    for (usize i = 0; i < source->getSimdChannels(); i++)
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
  forceinline void convertBufferInPlace(Framework::SimdBuffer *buffer, usize size)
  {
    for (u32 i = 0; i < buffer->getSimdChannels(); ++i)
    {
      auto data = buffer->get(i);
      for (usize j = 0; j < size - 1; j += 2)
      {
        simd_float one = data[j];
        simd_float two = data[j + 1];
        ConversionFunction(one, two);
        data[j] = one;
        data[j + 1] = two;
      }
      simd_float nyquist = data[size - 1];
      simd_float dummy{};
      ConversionFunction(nyquist, dummy);
      data[size - 1] = nyquist;
    }
  }
}
