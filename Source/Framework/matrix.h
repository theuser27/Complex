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

		perf_inline matrix() = default;
		perf_inline matrix(const simd_float row)
		{ 
			for (size_t i = 0; i < kSimdRatio; i++)
				rows_[i] = row;
		}
		perf_inline matrix(const std::array<simd_float, kSimdRatio> rows) : rows_(rows) { }
		perf_inline matrix(const std::array<simd_float, kComplexSimdRatio> rows) : isComplex(true)
		{
			for (size_t i = 0; i < kComplexSimdRatio; i++)
				rows_[i] = rows[i];
		}

		perf_inline void transpose()
		{ simd_float::transpose(rows_); }

		perf_inline void complexTranspose()
		{ simd_float::complexTranspose(rows_); }

		perf_inline simd_float sumRows()
		{ 
			simd_float sum{ 0.0f };
			for (size_t i = 0; i < rows_.size(); i++)
				sum += rows_[i];
			return sum;
		}

		perf_inline simd_float complexCartSumRows()
		{ return sumRows(); }

		perf_inline simd_float multiplyAndSumRows(const matrix &other)
		{
			simd_float summedVector = 0;
			for (u32 i = 0; i < simd_float::kSize; i += 2)
				summedVector += simd_float::mulAdd(rows_[i] * other.rows_[i], rows_[i + 1], other.rows_[i + 1]);
			return summedVector;
		}
	};
}