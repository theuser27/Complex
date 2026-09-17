
// Created: 2021-09-14 00:55:12

#pragma once

#include "simd_buffer.hpp"
#include "simd_utils.hpp"

namespace utils
{
  // layout of complex cartesian and polar vectors is assumed to be
  // { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

  // cos and sin
  forceinline utils::pair<f4, f4> vectorcall
  cossin(f4 radians)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr f4 kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr f4 kHalfPiPart2 = "0 01110011 11111011010101000100010"_fbl;

    // taylor coefficients of sin
    static constexpr f4 kSin1 = -0.166666518f;
    static constexpr f4 kSin2 = 0.00833202855f;
    static constexpr f4 kSin3 = -0.0001950085f;

    // taylor coefficients of cos
    static constexpr f4 kCos0 = 1.0f;
    static constexpr f4 kCos1 = -0.5f;
    static constexpr f4 kCos2 = 0.0416666459f;
    static constexpr f4 kCos3 = -0.0013887321f;
    static constexpr f4 kCos4 = 0.000024433157f;

    static constexpr f4 k2InvPi = 2.0f / kPi;
    // adding this forces the value to get rounded to int (only the lower 23 bits are valid) and
    // subtracting it again yields the rounded f32 (assuming value is positive)
    static constexpr f4 kRound = 12582912.0f;

    f4 normalisedInput = radians * k2InvPi;
    // using the magic constant and the hidden GRS bits to round normalisedInput
    i4 roundedInt = reinterpretToInt(normalisedInput + kRound);
    f4 roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra masking is to guard against nefarious bits left by fast math
    i4mask exactMask = f4::equal(roundedFloat, normalisedInput & i4mask{ ~1U });

    f4 position = radians - (roundedFloat * kHalfPiPart1) - (roundedFloat * kHalfPiPart2);
    f4 position2 = position * position;

    // LSB/LSB+1 represents a 90/180 degree rotation
    i4 lowestMantissaBit = roundedInt & 1;
    i4mask sinSign = shiftLeft<30>(roundedInt & 2);
    i4mask cosSign = shiftLeft<30>((roundedInt + lowestMantissaBit) & 2);
    i4mask quadrantMask = i4::equal(lowestMantissaBit, 0);

    // simple taylor series
    f4 cos = f4::mulAdd(kCos0, position2, f4::mulAdd(kCos1, position2,
      f4::mulAdd(kCos2, position2, f4::mulAdd(kCos3, position2, kCos4))));
    f4 sin = f4::mulAdd(position, position, position2 *
      f4::mulAdd(kSin1, position2, f4::mulAdd(kSin2, position2, kSin3)));

    cos = merge(cos, 1.0f, exactMask);
    // equivalent: sin = merge(sin, 0.0f, exactMask);
    sin = sin & ~exactMask;

    return { merge(sin, cos, quadrantMask) ^ cosSign,
      merge(cos, sin, quadrantMask) ^ sinSign };
  }

  // [cos(angle[0]), sin(angle[1]), cos(angle[2]), sin(angle[3])]
  forceinline f4 vectorcall
  cis(f4 angle)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr f4 kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr f4 kHalfPiPart2 = "0 01110011 11111011010101000100010"_fbl;

    // modified taylor coefficients of { cos, sin }
    static constexpr f4 k0 = { 1.0f, 0.0f };
    static constexpr f4 k1 = { -0.5f, 1.0f };
    static constexpr f4 k2 = { 0.0416666459f, -0.166666518f };
    static constexpr f4 k3 = { -0.0013887321f, 0.00833202855f };
    static constexpr f4 k4 = { 0.000024433157f, -0.0001950085f };

    static constexpr f4 k2InvPi = 2.0f / kPi;
    // adding this forces the value to get rounded to int (only the lower 23 bits are valid) and
    // subtracting it again yields the rounded f32 (assuming value is positive)
    static constexpr f4 kRound = 12582912.0f;
    static constexpr f4 kExact = { 1.0f, 0.0f };

    f4 normalisedInput = angle * k2InvPi;
    // using the magic constant and the hidden GRS bits to round normalisedInput
    i4 roundedInt = reinterpretToInt(normalisedInput + kRound);
    f4 roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    // checks if angle is exactly 0, +/-90 or +/-180 degrees
    // extra masking is to guard against nefarious bits left by fast math
    i4mask exactMask = f4::equal(roundedFloat, normalisedInput & i4mask{ ~1U });

    f4 position = angle - (roundedFloat * kHalfPiPart1) - (roundedFloat * kHalfPiPart2);
    f4 position2 = position * position;

    // LSB/LSB+1 represents a 90/180 degree rotation
    i4 lowestMantissaBit = roundedInt & 1;
    i4mask signs = shiftLeft<30>((roundedInt + (lowestMantissaBit & kRealMask)) & 2);
    i4mask quadrantMask = i4::equal(lowestMantissaBit, 0);

    // simple taylor series
    f4 values = f4::mulAdd(k0, merge(position, position2, kRealMask),
      f4::mulAdd(k1, position2, f4::mulAdd(k2, position2, f4::mulAdd(k3, position2, k4))));
    values = merge(values, kExact, exactMask);
    values = merge(switchInner(values), values, quadrantMask) ^ signs;
    return values;
  }

  forceinline f4 vectorcall sin(f4 radians) { return cossin(radians).second; }
  forceinline f4 vectorcall cos(f4 radians) { return cossin(radians).first; }
  forceinline f4 vectorcall
  tan(f4 radians)
  {
    // split pi / 2 into multiple parts to take advantage of the
    // hidden GRS bits during subtraction for more accurate radian wrapping
    static constexpr f4 kHalfPiPart1 = "0 01111111 10010010000000000000000"_fbl;
    static constexpr f4 kHalfPiPart2 = "0 01110011 11111011010000000000000"_fbl;
    static constexpr f4 kHalfPiPart3 = "0 01100111 01000100010000000000000"_fbl;
    static constexpr f4 kHalfPiPart4 = "0 01011000 01101000110000100011010"_fbl;

    // pade coefficients for tan
    static constexpr f4 kNum0 = 0.9999997615814208984375f;
    static constexpr f4 kNum1 = -0.0958017408847808837890625f;
    static constexpr f4 kDen0 = kNum0;
    static constexpr f4 kDen1 = -0.4291356503963470458984375f;
    static constexpr f4 kDen2 = 0.009716848842799663543701171875f;

    static constexpr f4 k2InvPi = 2.0f / kPi;
    static constexpr f4 kRound = 12582912.0f;

    f4 absInput = f4::abs(radians);
    i4mask inputSign = f4::signMask(radians);
    i4 roundedInt = reinterpretToInt(absInput * k2InvPi + kRound);
    f4 roundedFloat = reinterpretToFloat(roundedInt) - kRound;
    i4mask quadrantMask = i4::equal(roundedInt & 1, 0);

    f4 position = absInput - (kHalfPiPart1 * roundedFloat) - (kHalfPiPart2 * roundedFloat) -
      (kHalfPiPart3 * roundedFloat) - (kHalfPiPart4 * roundedFloat);
    f4 position2 = position * position;

    // pade approximants of tan(x)
    f4 numerator = position * f4::mulAdd(kNum0, position2, kNum1);
    f4 denominator = f4::mulAdd(kDen0, position2, f4::mulAdd(kDen1, position2, kDen2));
    return (merge(denominator, numerator, quadrantMask) / merge(numerator, denominator, quadrantMask)) ^
      shiftLeft<31>(roundedInt) ^ inputSign;
  }

  forceinline f4 vectorcall
  atan2(f4 y, f4 x)
  {
    // taylor series coefficients of atan(x)
    static constexpr f4 k0 = 1.0f;
    static constexpr f4 k1 = -0.3333307206630706787109375f;
    static constexpr f4 k2 = 0.199926197528839111328125f;
    static constexpr f4 k3 = -0.14203643798828125f;
    static constexpr f4 k4 = 0.1064093410968780517578125f;
    static constexpr f4 k5 = -0.07504294812679290771484375f;
    static constexpr f4 k6 = 0.042691521346569061279296875f;
    static constexpr f4 k7 = -0.016068629920482635498046875f;
    static constexpr f4 k8 = 0.00284988968633115291595458984375f;

    f4 xAbs = f4::abs(x);
    f4 yAbs = f4::abs(y);
    f4 sign = y & i4{ kSignMask };
    i4mask mask = f4::lessThan(xAbs, yAbs);
    f4 value = (utils::merge(yAbs, xAbs, mask) / utils::merge(xAbs, yAbs, mask)) | (mask & i4{ kSignMask });
    f4 v2 = value * value;
    f4 v4 = v2 * v2;
    i4mask zeroOverZeroMask = i4::equal(0, reinterpretToInt(xAbs)) & i4::equal(0, reinterpretToInt(yAbs));

    // taylor series split into 2 radix-4 sums
    f4 one = v2 * f4::mulAdd(k1, v4, f4::mulAdd(k3, v4, f4::mulAdd(k5, k7, v4)));
    f4 two = f4::mulAdd(k0, v4, f4::mulAdd(k2, v4, f4::mulAdd(k4, v4, f4::mulAdd(k6, k8, v4))));
    f4 three = (x & i4mask{ kSignMask }) ^ f4::mulAdd((f4{ kPi * 0.5f } &mask), value, (one + two));
    f4 result = (three + (f4{ kPi } & f4::lessThan(x, 0.0f))) ^ sign;

    return result & ~zeroOverZeroMask;
  }

  // magnitude and phase
  forceinline utils::pair<f4, f4> vectorcall
  phasor(f4 real, f4 imaginary)
  {
    auto magnitude = f4::sqrt(f4::mulAdd(real * real, imaginary, imaginary));
    return { magnitude, atan2(imaginary, real) };
  }

  forceinline f4 vectorcall
  complexCartMul(f4 one, f4 two)
  {
    // [a1c1, a1d1, a2c2, a2d2]
    f4 sums1 = copyFromEven(one) * two;
    // [b1d1, b1c1, b2d2, b2c2]
    f4 sums2 = copyFromOdd(one) * switchInner(two);

    // [a1c1 - b1d1, a1d1 + b1c1, a2c2 - b2d2, a2d2 + b2c2]
  #if COMPLEX_SSE4_1
    // yes it's addsub but the first op is sub
    return _mm_addsub_ps(sums1.value, sums2.value);
  #elif COMPLEX_NEON
    static constexpr i4mask kMinusPlus = { kSignMask, 0U };
    return sums1 + (sums2 ^ kMinusPlus);
  #endif
  }

  forceinline f4 vectorcall
  complexPolarMul(f4 one, f4 two)
  { return merge(one * two, one + two, kPhaseMask); }

  forceinline f4 vectorcall
  complexMagnitude(f4 value, bool toSqrt)
  {
    value *= value;
    value += switchInner(value);
    return (toSqrt) ? f4::sqrt(value) : value;
  }

  forceinline f4 vectorcall
  complexMagnitude(const utils::array<f4, f4::complexSize> &values, bool toSqrt)
  {
    f4 one = values[0] * values[0];
    f4 two = values[1] * values[1];
    one = horizontalAdd(one, two);
    return (toSqrt) ? f4::sqrt(one) : one;
  }

  forceinline f4 vectorcall
  complexPhase(f4 value)
  {
    f4 real = copyFromEven(value);
    f4 imaginary = copyFromOdd(value);

    return atan2(imaginary, real);
  }

  forceinline f4 vectorcall
  complexPhase(const utils::array<f4, f4::complexSize> &values)
  {
  #if COMPLEX_SSE4_1
    f4 real = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(2, 0, 2, 0));
    f4 imaginary = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    f4 real = vuzp1q_f32(values[0].value, values[1].value);
    f4 imaginary = vuzp2q_f32(values[0].value, values[1].value);
  #endif

    return atan2(imaginary, real);
  }

  forceinline f4 vectorcall
  complexReal(f4 value)
  {
    f4 magnitude = copyFromEven(value);
    f4 phase = copyFromOdd(value);

    return magnitude * cos(phase);
  }

  forceinline f4 vectorcall
  complexImaginary(f4 value)
  {
    f4 magnitude = copyFromEven(value);
    f4 phase = copyFromOdd(value);

    return magnitude * sin(phase);
  }

  forceinline void vectorcall complexValueMerge(f4 &one, f4 &two)
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

  forceinline void vectorcall complexCartToPolar(f4 &one, f4 &two)
  {
  #if COMPLEX_SSE4_1
    f4 real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    f4 imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    f4 real = vuzp1q_f32(one.value, two.value);
    f4 imaginary = vuzp2q_f32(one.value, two.value);
  #endif
    auto [magnitude, phase] = phasor(real, imaginary);
    complexValueMerge(magnitude, phase);
    one = magnitude;
    two = phase;
  }

  forceinline void vectorcall complexPolarToCart(f4 &one, f4 &two)
  {
  #if COMPLEX_SSE4_1
    f4 phases = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    f4 phases = vuzp2q_f32(one.value, two.value);
  #endif
    auto [real, imaginary] = cossin(phases);
    complexValueMerge(real, imaginary);
    f4 magnitudesOne = copyFromEven(one);
    f4 magnitudesTwo = copyFromEven(two);
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
        f4 one = rawSource[sourceSize * i + j];
        f4 two = rawSource[sourceSize * i + j + 1];
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
        f4 one = data[j];
        f4 two = data[j + 1];
        ConversionFunction(one, two);
        data[j] = one;
        data[j + 1] = two;
      }
      f4 nyquist = data[size - 1];
      f4 dummy{};
      ConversionFunction(nyquist, dummy);
      data[size - 1] = nyquist;
    }
  }
}
