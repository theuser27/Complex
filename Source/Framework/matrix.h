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
		Matrix(const std::array<simd_float, kSimdRatio> &rows) noexcept : rows_(rows) { }
		Matrix(const std::array<simd_float, kComplexSimdRatio> &rows) noexcept : isComplex(true)
		{
			for (size_t i = 0; i < kComplexSimdRatio; i++)
				rows_[i] = rows[i];
		}

		void transpose() noexcept
		{ simd_float::transpose(rows_); }

		simd_float sumRows() const noexcept
		{ 
			simd_float sum = 0.0f;
			for (auto row : rows_)
				sum += row;
			return sum;
		}

		simd_float complexCartSumRows() const noexcept
		{ return sumRows(); }

		simd_float multiplyAndSumRows(const Matrix &other) const noexcept
		{
			simd_float summedVector = 0;
			for (size_t i = 0; i < kSimdRatio; i += 2)
				summedVector += simd_float::mulAdd(rows_[i] * other.rows_[i], rows_[i + 1], other.rows_[i + 1]);
			return summedVector;
		}
	};
}