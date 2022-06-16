/*
	==============================================================================

		lookup.h
		Created: 19 Aug 2021 1:52:21am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "common.h"
#include "simd_utils.h"

namespace Framework
{
	template<size_t resolution>
	class Lookup
	{
	private:
		// extra data points needed to perform cspline lookup at edges
		// when the requested values are exactly at 0.0f or 1.0f
		static constexpr int kExtraValues = 3;

	public:
		constexpr Lookup(float(&function)(float), float scale = 1.0f) : scale_(resolution / scale)
		{
			for (size_t i = 0; i < resolution + kExtraValues; i++)
			{
				float t = (i - 1.0f) / (resolution - 1.0f);
				lookup_[i] = function(t * scale);
			}
		}
		~Lookup() = default;


		// gets catmull-rom spline interpolated y-values at their corresponding x-values
		force_inline simd_float cubicLookup(simd_float xValues) const
		{
			//COMPLEX_ASSERT(simd_float::greaterThanOrEqual(xValues, simd_float(0.0f)) && xValues <= simd_float(1.0f));
			simd_float boost = (xValues * scale_) + 1.0f;
			simd_int indices = utils::clamp(utils::toInt(boost), simd_int(1), simd_int(resolution));
			simd_float t = boost - utils::toFloat(indices);

			matrix interpolationMatrix = utils::getCatmullInterpolationMatrix(t);
			matrix valueMatrix = utils::getValueMatrix<kSimdRatio>(lookup_.data(), indices - simd_int(1));
			valueMatrix.transpose();

			return interpolationMatrix.multiplyAndSumRows(valueMatrix);
		}

		// gets linearly interpolated y-values at their corresponding x-values
		force_inline simd_float linearLookup(simd_float xValues) const
		{
			//COMPLEX_ASSERT(xValues >= simd_float(0.0f) && xValues <= simd_float(1.0f));
			simd_float boost = (xValues * scale_) + 1.0f;
			simd_int indices = utils::clamp(utils::toInt(boost), simd_int(1), simd_int(resolution));
			simd_float t = boost - utils::toFloat(indices);

			matrix interpolationMatrix = utils::getLinearInterpolationMatrix(t);
			matrix valueMatrix = utils::getValueMatrix<kSimdRatio>(lookup_.data(), indices - simd_int(1));
			valueMatrix.transpose();

			return interpolationMatrix.multiplyAndSumRows(valueMatrix);
		}

		// gets catmull-rom spline interpolated y-value at the corresponding x-value
		force_inline float cubicLookup(float xValue) const
		{
			COMPLEX_ASSERT(xValue >= 0.0f && xValue <= 1.0f);
			float boost = (xValue * scale_) + 1.0f;
			size_t index = utils::iclamp((size_t)boost, 1, resolution);
			float t = boost - (float)index;
			float half_t = t * 0.5f;
			float half_t2 = t * half_t;
			float half_t3 = t * half_t2;
			float half_three_t3 = half_t3 * 3.0f;

			return (half_t2 * 2.0f - half_t3 - half_t) * lookup_[index - 1] +
				(half_three_t3 - 5.0f * half_t2 + 1.0f) * lookup_[index] +
				(4.0f * half_t2 + half_t - half_three_t3) * lookup_[index + 1] +
				(half_t3 - half_t2) * lookup_[index + 2];
		}

		// gets linearly interpolated y-value at the corresponding x-value
		force_inline float linearLookup(float xValue) const
		{
			COMPLEX_ASSERT(xValue >= 0.0f && xValue <= 1.0f);
			float boost = (xValue * scale_) + 1.0f;
			size_t index = utils::iclamp((size_t)boost, 1, resolution);
			float t = boost - (float)index;
			return (1.0f - t) * lookup_[index] + t * lookup_[index + 1];
		}

	private:
		std::array<float, resolution + kExtraValues> lookup_{};
		float scale_;
	};
}