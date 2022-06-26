/*
	==============================================================================

		matrix.h
		Created: 6 Aug 2021 6:21:58pm
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "simd_values.h"

namespace Framework
{
	struct matrix
	{
		std::array<simd_float, kSimdRatio> rows_;
		bool isComplex = false;

		force_inline matrix() = default;
		force_inline matrix(const simd_float row)
		{ 
			for (size_t i = 0; i < kSimdRatio; i++)
				rows_[i] = row;
		}
		force_inline matrix(const std::array<simd_float, kSimdRatio> rows) : rows_(rows) { }
		force_inline matrix(const std::array<simd_float, kComplexSimdRatio> rows) : isComplex(true)
		{
			for (size_t i = 0; i < kComplexSimdRatio; i++)
				rows_[i] = rows[i];
		}

		force_inline void transpose()
		{ simd_float::transpose(rows_); }

		force_inline void complexTranspose()
		{ utils::complexTranspose(rows_); }

		force_inline simd_float sumRows()
		{ 
			simd_float sum{ 0.0f };
			for (size_t i = 0; i < rows_.size(); i++)
				sum += rows_[i];
			return sum;
		}

		force_inline simd_float complexCartSumRows()
		{ return sumRows(); }

		force_inline simd_float multiplyAndSumRows(const matrix &other)
		{
			simd_float summedVector = 0;
			for (u32 i = 0; i < simd_float::kSize; i += 2)
				summedVector += simd_float::mulAdd(rows_[i] * other.rows_[i], rows_[i + 1], other.rows_[i + 1]);
			return summedVector;
		}
	};
}