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
  template<size_t Resolution>
  class Lookup
  {
    // extra data points needed to perform cspline lookup at edges
    // when the requested values are exactly at 0.0f or 1.0f
    static constexpr size_t kExtraValues = 3;

    template<typename T>
    static T clamp(T value, T min, T max) noexcept 
    { return (value < min) ? min : (value > max) ? max : value; }

  public:
    constexpr Lookup(float(&function)(float), float scale = 1.0f) noexcept : scale_(Resolution / scale)
    {
      for (size_t i = 0; i < Resolution + kExtraValues; i++)
      {
        float t = ((float)i - 1.0f) / (Resolution - 1.0f);
        lookup_[i] = function(t * scale);
      }
    }

    // gets catmull-rom spline interpolated y-values at their corresponding x-values
    simd_float cubicLookup(simd_float xValues) const noexcept
    {
      COMPLEX_ASSERT(simd_float::lessThan(xValues, 0.0f).anyMask() == 0 &&
        simd_float::greaterThan(xValues, 1.0f).anyMask() == 0);

      simd_float boost = (xValues * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      utils::Matrix interpolationMatrix = utils::getCatmullInterpolationMatrix(t);
      utils::Matrix valueMatrix = utils::getValueMatrix<kSimdRatio>(lookup_, indices - simd_int(1));
      valueMatrix.transpose();

      return interpolationMatrix.multiplyAndSumRows(valueMatrix);
    }

    // gets linearly interpolated y-values at their corresponding x-values
    simd_float linearLookup(simd_float xValues) const noexcept
    {
      COMPLEX_ASSERT(simd_float::lessThan(xValues, 0.0f).anyMask() == 0 &&
        simd_float::greaterThan(xValues, 1.0f).anyMask() == 0);

      simd_float boost = (xValues * scale_) + 1.0f;
      simd_int indices = simd_int::clampUnsigned(utils::toInt(boost), simd_int(1), simd_int(Resolution));
      simd_float t = boost - utils::toFloat(indices);

      utils::Matrix interpolationMatrix = utils::getLinearInterpolationMatrix(t);
      utils::Matrix valueMatrix = utils::getValueMatrix<kSimdRatio>(lookup_, indices - simd_int(1));
      valueMatrix.transpose();

      return interpolationMatrix.multiplyAndSumRows(valueMatrix);
    }

    // gets catmull-rom spline interpolated y-value at the corresponding x-value
    float cubicLookup(float xValue) const noexcept
    {
      static constexpr auto exponents = utils::array{ 3.0f, 2.0f, 1.0f, 0.0f };
      COMPLEX_ASSERT(xValue >= 0.0f && xValue <= 1.0f);
      float boost = (xValue * scale_) + 1.0f;
      size_t index = clamp((size_t)boost, (size_t)1, Resolution);
      float t = boost - (float)index;
      utils::pow(t, simd_float(exponents));
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
    float linearLookup(float xValue) const noexcept
    {
      COMPLEX_ASSERT(xValue >= 0.0f && xValue <= 1.0f);
      float boost = (xValue * scale_) + 1.0f;
      size_t index = clamp((size_t)boost, (size_t)1, Resolution);
      float t = boost - (float)index;
      return (1.0f - t) * lookup_[index] + t * lookup_[index + 1];
    }

  private:
    float lookup_[Resolution + kExtraValues];
    float scale_;
  };
}