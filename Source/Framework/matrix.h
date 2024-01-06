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
		{
		#if COMPLEX_SSE4_1
			auto low0 = _mm_unpacklo_ps(rows_[0].value, rows_[1].value);
			auto low1 = _mm_unpacklo_ps(rows_[2].value, rows_[3].value);
			auto high0 = _mm_unpackhi_ps(rows_[0].value, rows_[1].value);
			auto high1 = _mm_unpackhi_ps(rows_[2].value, rows_[3].value);
			rows_[0].value = _mm_movelh_ps(low0, low1);
			rows_[1].value = _mm_movehl_ps(low1, low0);
			rows_[2].value = _mm_movelh_ps(high0, high1);
			rows_[3].value = _mm_movehl_ps(high1, high0);
		#elif COMPLEX_NEON
			auto swapLow = vtrnq_f32(rows_[0].value, rows_[1].value);
			auto swapHigh = vtrnq_f32(rows_[2].value, rows_[3].value);
			rows_[0].value = vextq_f32(vextq_f32(swapLow.val[0], swapLow.val[0], 2), swapHigh.val[0], 2);
			rows_[1].value = vextq_f32(vextq_f32(swapLow.val[1], swapLow.val[1], 2), swapHigh.val[1], 2);
			rows_[2].value = vextq_f32(swapLow.val[0], vextq_f32(swapHigh.val[0], swapHigh.val[0], 2), 2);
			rows_[3].value = vextq_f32(swapLow.val[1], vextq_f32(swapHigh.val[1], swapHigh.val[1], 2), 2);
		#endif
		}

		void complexTranspose() noexcept
		{
			// TODO: implement complexTranspose for NEON
		#if COMPLEX_SSE4_1
			auto low = _mm_movelh_ps(rows_[0].value, rows_[1].value);
			auto high = _mm_movehl_ps(rows_[1].value, rows_[0].value);
			rows_[0].value = low;
			rows_[1].value = high;
		#elif COMPLEX_NEON
			static_assert(false, "ARM NEON complexTranspose not supported yet");
		#endif
		}

		simd_float sumRows() const noexcept
		{ 
			simd_float sum{ 0.0f };
			for (auto row : rows_)
				sum += row;
			return sum;
		}

		simd_float complexCartSumRows() const noexcept
		{ return sumRows(); }

		simd_float multiplyAndSumRows(const Matrix &other) const noexcept
		{
			simd_float summedVector = 0;
			for (size_t i = 0; i < simd_float::kSize; i += 2)
				summedVector += simd_float::mulAdd(rows_[i] * other.rows_[i], rows_[i + 1], other.rows_[i + 1]);
			return summedVector;
		}
	};
}