/*
  ==============================================================================

    simd_math.hpp
    Created: 14 Sep 2021 12:55:12am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Third Party/gcem/gcem.hpp"

#include "sync_primitives.hpp"
#include "simd_buffer.hpp"
#include "simd_utils.hpp"

namespace utils
{
  // layout of complex cartesian and polar vectors is assumed to be
  // { real, imaginary, real, imaginary } and { magnitude, phase, magnitude, phase } respectively

  // this number of iterations produces results with max error of <= 0.01 degrees
  inline constexpr usize kDefaultCordicIterations = 12;
  inline constexpr simd_mask kFloatMantissaMask = 0x007fffffU;
  inline constexpr simd_mask kFloatExponentMask = 0x7f800000U;
  inline constexpr simd_mask kNotFloatExponentMask = ~kFloatExponentMask[0];
  inline constexpr simd_float kInvPi = 1.0f / kPi;
  inline constexpr simd_float kInv2Pi = 1.0f / k2Pi;

  // TODO: 
  /*strict_inline void vector_call cordic2Rotation(simd_float radians)
  {
    // TODO: implement later using ints
    static constexpr simd_float kStage2Factors[] = { 0.0f, 16.0f / 63.0f, 33.0f / 56.0f, 39.0f / 52.0f };
    static constexpr simd_float kStage3Factors[] = { 0.0f, 252.0f / 3965.0f, 445.0f / 3948.0f };
    static constexpr simd_float kStage4Factors[] = { 0.0f, 89.0f / 3960.0f };
    static constexpr simd_float kStage5Scale = 1.0f / 2848.118582f;
    static constexpr u32 kStage5RotationCount = 1 << 5;
    static constexpr simd_float kStage5RoundingTerm = kStage5RotationCount * 360.0f / 0.643746f;


  }*/

  /**
   * \brief 
   * \tparam Iterations number of iterations the algorithm should do
   * \param radians [-inf; inf] phases the algorithm should use to create the cis pairs
   * \return { unscaled cos part, unscaled sin part, scaling factor }
   */
  template<usize Iterations = kDefaultCordicIterations>
  strict_inline utils::array<simd_float, 3> vector_call cordicRotation(simd_float radians)
  {
    static constexpr simd_float kScale = []()
    {
      float result = 1.0f;
      for (usize i = 0; i < Iterations; i++)
        result *= 1.0f / gcem::sqrt(1.0f + gcem::pow(2.0f, -2.0f * (float)i));
      return result;
    }();

    // kThetaDeltas[i] = atan(2^(-i)) / kPi
    // division by pi because we get rid of it inside radians
    static constexpr auto kThetaDeltas = []()
    {
      utils::array<simd_float, Iterations + 1> angles{};
      for (i32 i = 0; i <= (i32)Iterations; i++)
        angles[i] = gcem::atan(gcem::pow(2.0f, (float)(-i))) / kPi;
      return angles;
    }();

    static constexpr simd_int kIncrement = 1 << 23;

    // correction for angles after +/-pi, normalises to +/-1
    radians *= kInvPi;
    radians -= simd_float::round(radians * 0.5f) * 2.0f;

    // correction so that the algorithm works
    simd_mask sinMask = unsignSimd(radians);
    radians -= 0.5f;

    utils::pair<simd_float, simd_float> result = { 0.0f, 1.0f };
    simd_float multiplier = 1.0f;
    for (usize i = 0; i <= Iterations; i++)
    {
      simd_mask signMask = getSign(radians);
      radians -= kThetaDeltas[i] ^ signMask;

      simd_float prevX = result.first;
      simd_float prevY = result.second;

      // x[i + 1] = x[i] - y[i] * 2^(-i) * "sign"
      result.first  = -simd_float::mulSub(prevX, prevY, multiplier ^ signMask);
      // y[i + 1] = y[i] + x[i] * 2^(-i) * "sign"
      result.second =  simd_float::mulAdd(prevY, prevX, multiplier ^ signMask);
      multiplier = reinterpretToFloat(reinterpretToInt(multiplier) - kIncrement);
    }

    return { result.first, result.second | sinMask, kScale };
  }

  /**
   * \brief 
   * \tparam Iterations number of iterations the algorithm should do
   * \param x only the real parts
   * \param y only the imaginary parts
   * \return { unscaled magnitude, phase, scaling factor }
   */
  template<usize Iterations = kDefaultCordicIterations>
  strict_inline utils::array<simd_float, 3> vector_call cordicVectoring(simd_float x, simd_float y)
  {
    static constexpr simd_float kScale = []()
    {
      float result = 1.0f;
      for (usize i = 0; i < Iterations; i++)
        result *= 1.0f / gcem::sqrt(1.0f + gcem::pow(2.0f, -2.0f * (float)i));
      return result;
    }();

    // kThetaDeltas[i] = atan(2^(-i))
    static constexpr auto kThetaDeltas = []()
    {
      utils::array<simd_float, Iterations + 1> angles{};
      for (i32 i = 0; i <= (i32)Iterations; i++)
        angles[i] = gcem::atan(gcem::pow(2.0f, (float)(-i)));
      return angles;
    }();

    static constexpr simd_int kIncrement = 1 << 23;

    simd_mask xNegativeMask = unsignSimd(x);
    simd_mask signMask = getSign(y);
    simd_float angle = (simd_float{ kPi } ^ signMask) & simd_mask::equal(xNegativeMask, kSignMask);
    simd_float multiplier = 1.0f;
    for (usize i = 0; i <= Iterations; ++i)
    {
      angle += kThetaDeltas[i] ^ (signMask ^ xNegativeMask);

      simd_float prevX = x;
      simd_float prevY = y;

      // x[i + 1] = x[i] + y[i] * 2^(-i) * "sign"
      x =  simd_float::mulAdd(prevX, prevY, multiplier ^ signMask);
      // y[i + 1] = y[i] - x[i] * 2^(-i) * "sign"
      y = -simd_float::mulSub(prevY, prevX, multiplier ^ signMask);

      multiplier = reinterpretToFloat(reinterpretToInt(multiplier) - kIncrement);
      signMask = getSign(y);
    }

    return { x, angle & simd_mask::notEqual(0, reinterpretToInt(x & kFloatExponentMask)), kScale };
  }

  template<usize Iterations = kDefaultCordicIterations>
  strict_inline simd_float vector_call sin(simd_float radians)
  {
  #ifdef COMPLEX_INTEL_SVML
    return _mm_sin_ps(radians.value);
  #else
    auto result = cordicRotation<Iterations>(radians);
    return result[1] * result[2];
  #endif
  }

  template<usize Iterations = kDefaultCordicIterations>
  strict_inline simd_float vector_call cos(simd_float radians)
  {
  #ifdef COMPLEX_INTEL_SVML
    return _mm_cos_ps(radians.value);
  #else
    auto result = cordicRotation<Iterations>(radians);
    return result[0] * result[2];
  #endif
  }

  template<usize Iterations = kDefaultCordicIterations>
  strict_inline simd_float vector_call tan(simd_float radians)
  {
  #ifdef COMPLEX_INTEL_SVML
    return _mm_tan_ps(radians.value);
  #else
    auto result = cordicRotation<Iterations>(radians);
    return result[1] / result[0];
  #endif
  }

  template<usize Iterations = kDefaultCordicIterations>
  strict_inline simd_float vector_call atan2(simd_float y, simd_float x)
  {
  #ifdef COMPLEX_INTEL_SVML
    return _mm_atan2_ps(y.value, x.value);
  #else
    return cordicVectoring<Iterations>(x, y)[1];
  #endif
  }

  strict_inline simd_float vector_call atan2Fast(simd_float y, simd_float x)
  {
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
  }

  // [cos, sin]
  template<usize Iterations = kDefaultCordicIterations>
  strict_inline utils::pair<simd_float, simd_float> vector_call cis(simd_float radians)
  {
  #ifdef COMPLEX_INTEL_SVML
    simd_float cos = utils::uninitialised;
    simd_float sin = _mm_sincos_ps(&cos.value, radians.value);
    return { cos, sin };
  #else
    auto result = cordicRotation<Iterations>(radians);
    return { result[0] * result[2], result[1] * result[2] };
  #endif
  }

  // [cos, sin]
  strict_inline utils::pair<simd_float, simd_float> vector_call cisFastt(simd_float radians)
  {
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
  template<usize Iterations = kDefaultCordicIterations>
  strict_inline utils::pair<simd_float, simd_float> vector_call phasor(simd_float real, simd_float imaginary)
  {
  #ifdef COMPLEX_INTEL_SVML
    return { simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), 
             simd_float(_mm_atan2_ps(imaginary.value, real.value)) };
  #else
    auto result = cordicVectoring<Iterations>(real, imaginary);
    return { result[0] * result[2], result[1] };
  #endif
  }

  // [magnitude, phase]
  strict_inline utils::pair<simd_float, simd_float> vector_call phasorFast(simd_float real, simd_float imaginary)
  { return { simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary)), atan2Fast(imaginary, real) }; }

  strict_inline simd_float vector_call complexCartAdd(simd_float one, simd_float two) {	return one + two; }

  strict_inline simd_float vector_call complexCartSub(simd_float one, simd_float two) {	return one - two; }

  strict_inline simd_float vector_call complexCartMul(simd_float one, simd_float two)
  {
  #if COMPLEX_SSE4_1
    // [a1c1, a1d1, a2c2, a2d2]
    auto sums1 = simd_float::mul(_mm_shuffle_ps(one.value, one.value, _MM_SHUFFLE(2, 2, 0, 0)), two.value);
    // [b1d1, b1c1, b2d2, b2c2]
    auto sums2 = simd_float::mul(
      _mm_shuffle_ps(one.value, one.value, _MM_SHUFFLE(3, 3, 1, 1)), 
      _mm_shuffle_ps(two.value, two.value, _MM_SHUFFLE(2, 3, 0, 1)));
    // [a1c1 - b1d1, a1d1 + b1c1, a2c2 - b2d2, a2d2 + b2c2]
    // yes it's addsub but the first op is sub
    return _mm_addsub_ps(sums1, sums2);
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
  }

  strict_inline simd_float vector_call complexPolarMul(simd_float one, simd_float two)
  {
    auto magnitudes = one * two;
    auto phases = one + two;
  #if COMPLEX_SSE4_1
    auto value = _mm_shuffle_ps(magnitudes.value, phases.value, _MM_SHUFFLE(3, 1, 2, 0));
    return _mm_shuffle_ps(value, value, _MM_SHUFFLE(3, 1, 2, 0));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
  }

  strict_inline simd_float vector_call complexMagnitude(simd_float value, bool toSqrt)
  {
    value *= value;
  #if COMPLEX_SSE4_1
    value += simd_float{ _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 3, 0, 1)) };
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    return (toSqrt) ? simd_float::sqrt(value) : value;
  }

  strict_inline simd_float vector_call complexMagnitude(const utils::array<simd_float, simd_float::complexSize> &values, bool toSqrt)
  {
    simd_float one = values[0] * values[0];
    simd_float two = values[1] * values[1];
  #if COMPLEX_SSE4_1
    one.value = _mm_hadd_ps(one.value, two.value);
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    return (toSqrt) ? simd_float::sqrt(one) : one;
  }

  strict_inline simd_float vector_call complexPhase(simd_float value)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
    simd_float imaginary = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif

    return atan2Fast(imaginary, real);
  }

  strict_inline simd_float vector_call complexPhase(const utils::array<simd_float, simd_float::complexSize> &values)
  {
  #if COMPLEX_SSE4_1
    auto real = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(2, 0, 2, 0));
    auto imaginary = _mm_shuffle_ps(values[0].value, values[1].value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif

    return atan2Fast(imaginary, real);
  }

  strict_inline simd_float vector_call complexReal(simd_float value)
  {
  #if COMPLEX_SSE4_1
    simd_float magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
    simd_float phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif

    return magnitude * cos(phase);
  }

  strict_inline simd_float vector_call complexImaginary(simd_float value)
  {
  #if COMPLEX_SSE4_1
    auto magnitude = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(2, 2, 0, 0));
    auto phase = _mm_shuffle_ps(value.value, value.value, _MM_SHUFFLE(3, 3, 1, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif

    return simd_float::mul(magnitude, utils::sin(phase).value);
  }

  strict_inline void vector_call complexValueMerge(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    auto one_ = _mm_unpacklo_ps(one.value, two.value);
    two.value = _mm_unpackhi_ps(one.value, two.value);
    one.value = one_;
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
  }

  strict_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    auto [magnitude, phase] = phasorFast(real, imaginary);
    complexValueMerge(magnitude, phase);
    one = magnitude;
    two = phase;
  }

  strict_inline void vector_call complexPolarToCart(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float magnitudesOne = _mm_shuffle_ps(one.value, one.value, _MM_SHUFFLE(2, 2, 0, 0));
    simd_float magnitudesTwo = _mm_shuffle_ps(two.value, two.value, _MM_SHUFFLE(2, 2, 0, 0));
    simd_float phases = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    auto [real, imaginary] = cisFastt(phases);
    complexValueMerge(real, imaginary);
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
      // starting at 1 to skip dc and size - 1 to skip nyquist since they don't need to change
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
      // starting at 1 to skip dc and size - 1 to skip nyquist since they don't need to change
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
