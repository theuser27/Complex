/*
  ==============================================================================

    lookup.hpp
    Created: 19 Aug 2021 1:52:21am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "simd_utils.hpp"

namespace Framework
{
  template<usize Resolution>
  class Lookup
  {
    // extra data points needed to perform cspline lookup at edges
    // when the requested values are exactly at 0.0f or 1.0f
    static constexpr usize kExtraValues = 3;

    template<typename T>
    static constexpr T clamp(T value, T min, T max) noexcept 
    { return (value < min) ? min : (value > max) ? max : value; }

  public:
    constexpr Lookup(float(*function)(float), float scale = 1.0f) noexcept : scale_(Resolution / scale)
    {
      for (usize i = 0; i < Resolution + kExtraValues; i++)
      {
        float t = ((float)i - 1.0f) / (Resolution - 1.0f);
        lookup_[i] = function(t * scale);
      }
    }

    // gets catmull-rom spline interpolated y-values at their corresponding x-values
    simd_float cubicLookup(simd_float x) const noexcept
    {
      COMPLEX_ASSERT(simd_float::lessThan(x, 0.0f).anyMask() == 0 &&
        simd_float::greaterThan(x, 1.0f).anyMask() == 0);

      simd_float boost = (x * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      auto interpolationMatrix = utils::getCatmullInterpolationMatrix(t);
      auto valueMatrix = utils::getValueMatrix(lookup_, indices - simd_int(1));
      transpose(valueMatrix);

      return multiplyAndSumRows(interpolationMatrix, valueMatrix);
    }

    // gets linearly interpolated y-values at their corresponding x-values
    simd_float linearLookup(simd_float x) const noexcept
    {
      COMPLEX_ASSERT(simd_float::lessThan(x, 0.0f).anyMask() == 0 &&
        simd_float::greaterThan(x, 1.0f).anyMask() == 0);

      simd_float boost = (x * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      auto valueMatrix = utils::getValueMatrix(lookup_, indices - simd_int(1));
      transpose(valueMatrix);

      return simd_float::mulAdd((1.0f - t) * valueMatrix[1], t, valueMatrix[2]);
    }

    // gets catmull-rom spline interpolated y-value at the corresponding x-value
    constexpr float cubicLookup(float x) const noexcept
    {
      COMPLEX_ASSERT(x >= 0.0f && x <= 1.0f);
      float boost = (x * scale_) + 1.0f;
      usize index = clamp((usize)boost, (usize)1, Resolution);
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
    constexpr float linearLookup(float x) const noexcept
    {
      COMPLEX_ASSERT(x >= 0.0f && x <= 1.0f);
      float boost = (x * scale_) + 1.0f;
      usize index = clamp((usize)boost, (usize)1, Resolution);
      float t = boost - (float)index;
      return (1.0f - t) * lookup_[index] + t * lookup_[index + 1];
    }

  private:
    float lookup_[Resolution + kExtraValues];
    float scale_;
  };
}