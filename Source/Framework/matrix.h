/*
	==============================================================================

		matrix.h
		Created: 6 Aug 2021 6:21:58pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "simd_values.h"

namespace Framework
{
	struct Matrix
	{
		std::array<simd_float, kSimdRatio> rows_;
		bool isComplex = false;

		Matrix() = default;
		Matrix(const simd_float row) noexcept
		{ 
			for (size_t i = 0; i < kSimdRatio; i++)
				rows_[i] = row;
		}
		Matrix(const std::array<simd_float, kSimdRatio> rows) noexcept : rows_(rows) { }
		Matrix(const std::array<simd_float, kComplexSimdRatio> rows) noexcept : isComplex(true)
		{
			for (size_t i = 0; i < kComplexSimdRatio; i++)
				rows_[i] = rows[i];
		}

		void transpose() noexcept
		{ simd_float::transpose(rows_); }

		void complexTranspose() noexcept
		{ simd_float::complexTranspose(rows_); }

		simd_float sumRows() noexcept
		{ 
			simd_float sum{ 0.0f };
			for (size_t i = 0; i < rows_.size(); i++)
				sum += rows_[i];
			return sum;
		}

		simd_float complexCartSumRows() noexcept
		{ return sumRows(); }

		simd_float multiplyAndSumRows(const Matrix &other) noexcept
		{
			simd_float summedVector = 0;
			for (size_t i = 0; i < simd_float::kSize; i += 2)
				summedVector += simd_float::mulAdd(rows_[i] * other.rows_[i], rows_[i + 1], other.rows_[i + 1]);
			return summedVector;
		}
	};
}