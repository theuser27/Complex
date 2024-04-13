/*
	==============================================================================

		simd_values.h
		Created: 22 May 2021 6:22:20pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <cstdint>
#include <type_traits>
#include <bit>
#include <utility>
#include <array>

#include "platform_definitions.h"

// the project cannot run without vectorisation (either x86 SSE4.1 or ARM NEON, however it can run without FMA)
#if COMPLEX_SSE4_1
	#include <immintrin.h>
#elif COMPLEX_NEON
	#include <arm_neon.h>
#else
	#error Either SSE4.1 or ARM NEON is needed for this program to work
#endif

namespace simd_values
{
	inline constexpr u32 kFullMask = UINT32_MAX;
	inline constexpr u32 kNoChangeMask = UINT32_MAX;
	inline constexpr u32 kNotSignMask = (u32)INT32_MAX;
	inline constexpr u32 kSignMask = kNotSignMask + 1U;

	struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_int
	{
		/////////////////////////////
		// Variables and Constants //
		/////////////////////////////

	#if COMPLEX_SSE4_1
		static constexpr size_t kSize = 4;
		typedef __m128i simd_type;
	#elif COMPLEX_NEON
		static constexpr size_t kSize = 4;
		typedef uint32x4_t simd_type;
	#endif 

		simd_type value;


		////////////////////////////
		// Method Implementations //
		////////////////////////////

		static strict_inline simd_type vector_call init(u32 scalar)
		{
		#if COMPLEX_SSE4_1
			return _mm_set1_epi32((int32_t)scalar);
		#elif COMPLEX_NEON
			return vdupq_n_u32(scalar);
		#endif 
		}

		static strict_inline simd_type vector_call load(const u32 *memory)
		{
		#if COMPLEX_SSE4_1
			return _mm_loadu_si128((const __m128i *)memory);
		#elif COMPLEX_NEON
			return vld1q_u32(memory);
		#endif 
		}

		static strict_inline simd_type vector_call add(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_add_epi32(one, two);
		#elif COMPLEX_NEON
			return vaddq_u32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call sub(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_sub_epi32(one, two);
		#elif COMPLEX_NEON
			return vsubq_u32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call neg(simd_type value)
		{
		#if COMPLEX_SSE4_1
			return _mm_sub_epi32(_mm_set1_epi32(0), value);
		#elif COMPLEX_NEON
			return vmulq_n_u32(value, -1);
		#endif
		}

		static strict_inline simd_type vector_call mul(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			simd_type mul0_2 = _mm_mul_epu32(one, two);
			simd_type mul1_3 = _mm_mul_epu32(_mm_shuffle_epi32(one, _MM_SHUFFLE(2, 3, 0, 1)),
				_mm_shuffle_epi32(two, _MM_SHUFFLE(2, 3, 0, 1)));
			return _mm_unpacklo_epi32(_mm_shuffle_epi32(mul0_2, _MM_SHUFFLE(0, 0, 2, 0)),
				_mm_shuffle_epi32(mul1_3, _MM_SHUFFLE(0, 0, 2, 0)));
		#elif COMPLEX_NEON
			return vmulq_n_u32(value, -1);
		#endif
		}

		static strict_inline simd_type vector_call bitAnd(simd_type value, simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_and_si128(value, mask);
		#elif COMPLEX_NEON
			return vandq_u32(value, mask);
		#endif
		}

		static strict_inline simd_type vector_call bitOr(simd_type value, simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_or_si128(value, mask);
		#elif COMPLEX_NEON
			return vorrq_u32(value, mask);
		#endif
		}

		static strict_inline simd_type vector_call bitXor(simd_type value, simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_xor_si128(value, mask);
		#elif COMPLEX_NEON
			return veorq_u32(value, mask);
		#endif
		}

		static strict_inline simd_type vector_call bitNot(simd_type value) { return bitXor(value, init(kFullMask)); }

		static strict_inline simd_type vector_call equal(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_cmpeq_epi32(one, two);
		#elif COMPLEX_NEON
			return vceqq_u32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call greaterThanSigned(simd_type one, simd_type two) {
		#if COMPLEX_SSE4_1
			return _mm_cmpgt_epi32(one, two);
		#elif COMPLEX_NEON
			return vcgtq_s32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call greaterThanUnsigned(simd_type one, simd_type two) {
		#if COMPLEX_SSE4_1
			return _mm_cmpgt_epi32(_mm_xor_si128(one, init(kSignMask)), _mm_xor_si128(two, init(kSignMask)));
		#elif COMPLEX_NEON
			return vcgtq_u32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call max(simd_type one, simd_type two) {
		#if COMPLEX_SSE4_1
			simd_type greater_than_mask = greaterThanUnsigned(one, two);
			return _mm_or_si128(_mm_and_si128(greater_than_mask, one), _mm_andnot_si128(greater_than_mask, two));
		#elif COMPLEX_NEON
			return vmaxq_u32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call min(simd_type one, simd_type two) {
		#if COMPLEX_SSE4_1
			simd_type less_than_mask = _mm_cmpgt_epi32(two, one);
			return _mm_or_si128(_mm_and_si128(less_than_mask, one), _mm_andnot_si128(less_than_mask, two));
		#elif COMPLEX_NEON
			return vminq_u32(one, two);
		#endif
		}

		static strict_inline u32 vector_call sum(simd_type value) {
		#if COMPLEX_SSE4_1
			simd_type flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(1, 0, 3, 2));
			value = _mm_add_epi32(value, flip);
			flip = _mm_shuffle_epi32(value, _MM_SHUFFLE(2, 3, 0, 1));
			return _mm_cvtsi128_si32(_mm_add_epi32(value, flip));
		#elif COMPLEX_NEON
			uint32x2_t sum = vpadd_u32(vget_low_u32(value), vget_high_u32(value));
			sum = vpadd_u32(sum, sum);
			return vget_lane_u32(sum, 0);
		#endif
		}

		static strict_inline u32 vector_call anyMask(simd_type value)
		{
		#if COMPLEX_SSE4_1
			return _mm_movemask_epi8(value);
		#elif COMPLEX_NEON
			uint32x2_t max = vpmax_u32(vget_low_u32(value), vget_high_u32(value));
			max = vpmax_u32(max, max);
			return vget_lane_u32(max, 0);
		#endif
		}


		//////////////////
		// Method Calls //
		//////////////////

		static strict_inline simd_int vector_call max(simd_int one, simd_int two)
		{	return max(one.value, two.value); }

		static strict_inline simd_int vector_call min(simd_int one, simd_int two)
		{ return min(one.value, two.value); }

		static strict_inline simd_int vector_call clamp(simd_int low, simd_int high, simd_int value)
		{ return max(min(value.value, high.value), low.value); }

		static strict_inline simd_int vector_call equal(simd_int one, simd_int two)
		{	return equal(one.value, two.value); }

		static strict_inline simd_int vector_call notEqual(simd_int one, simd_int two)
		{ return ~equal(one, two); }

		static strict_inline simd_int vector_call greaterThanSigned(simd_int one, simd_int two)
		{	return greaterThanSigned(one.value, two.value); }

		static strict_inline simd_int vector_call lessThanSigned(simd_int one, simd_int two)
		{	return greaterThanSigned(two.value, one.value); }

		static strict_inline simd_int vector_call greaterThanOrEqualSigned(simd_int one, simd_int two)
		{	return ~lessThanSigned(one, two); }

		static strict_inline simd_int vector_call lessThanOrEqualSigned(simd_int one, simd_int two)
		{	return ~greaterThanSigned(one, two); }

		strict_inline u32 vector_call sum() const noexcept
		{ return sum(value); }

		strict_inline u32 vector_call anyMask() const noexcept
		{ return anyMask(value); }


		//////////////////
		// Constructors //
		//////////////////

	#ifdef COMPLEX_MSVC
	private:
		template<size_t ... N>
		static constexpr auto getInternalValue(const auto &scalars, std::index_sequence<N...>) noexcept { return simd_type{ .m128i_u32 = { scalars[N]... } }; }
	public:
	#endif

		constexpr strict_inline simd_int() noexcept : simd_int(0) { }
		constexpr strict_inline simd_int(u32 initialValue) noexcept
		{
			if (std::is_constant_evaluated())
			{
				std::array<u32, kSize> values{};
				values.fill(initialValue);
			#ifdef COMPLEX_MSVC
				value = getInternalValue(values, std::make_index_sequence<kSize>());
			#elif
				value = std::bit_cast<simd_type>(values);
			#endif
			}
			else
				value = init(initialValue);
		}
		constexpr strict_inline simd_int(simd_type initialValue) noexcept : value(initialValue) { }
		constexpr strict_inline simd_int(const std::array<u32, kSize> &scalars) noexcept :
	#ifdef COMPLEX_MSVC
			value(getInternalValue(scalars, std::make_index_sequence<kSize>())) { }
	#elif
			value(std::bit_cast<simd_type>(scalars)) { }
	#endif


		///////////////
		// Operators //
		///////////////

		strict_inline u32 vector_call access(size_t index) const noexcept
		{ return std::bit_cast<std::array<u32, kSize>, simd_type>(value)[index]; }

		strict_inline void vector_call set(size_t index, u32 newValue) noexcept
		{
			auto scalars = std::bit_cast<std::array<u32, kSize>, simd_type>(value);
			scalars[index] = newValue;
			value = std::bit_cast<simd_type, std::array<u32, kSize>>(scalars);
		}

		strict_inline std::array<u32, kSize> getArrayOfValues() const
		{	return std::bit_cast<std::array<u32, kSize>, simd_type>(value); }

		template<typename T> requires requires (T x) { (sizeof(u32) * kSize) % sizeof(x) == 0; }
		strict_inline auto getArrayOfValues() const
		{	return std::bit_cast<std::array<T, ((sizeof(u32) *kSize) / sizeof(T))>>(value); }

		strict_inline void swap(simd_int &other) noexcept
		{
			simd_type temp = other.value;
			other.value = value;
			value = temp;
		}

		///////////////
		// Operators //
		///////////////

		strict_inline u32 vector_call operator[](size_t index) const noexcept
		{ return access(index); }

		strict_inline simd_int vector_call operator+(simd_int other) const noexcept
		{ return add(value, other.value); }

		strict_inline simd_int vector_call operator-(simd_int other) const noexcept
		{ return sub(value, other.value); }

		strict_inline simd_int vector_call operator*(simd_int other) const noexcept
		{ return mul(value, other.value); }

		strict_inline simd_int vector_call operator&(simd_int other) const noexcept
		{ return bitAnd(value, other.value); }

		strict_inline simd_int vector_call operator|(simd_int other) const noexcept
		{ return bitOr(value, other.value); }

		strict_inline simd_int vector_call operator^(simd_int other) const noexcept
		{ return bitXor(value, other.value); }

		strict_inline simd_int vector_call operator-() const noexcept
		{ return neg(value); }

		strict_inline simd_int vector_call operator~() const noexcept
		{ return bitNot(value); }

		strict_inline simd_int &vector_call operator+=(simd_int other) noexcept
		{ value = add(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator-=(simd_int other) noexcept
		{ value = sub(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator*=(simd_int other) noexcept
		{ value = mul(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator&=(simd_int other) noexcept
		{ value = bitAnd(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator|=(simd_int other) noexcept
		{ value = bitOr(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator^=(simd_int other) noexcept
		{ value = bitXor(value, other.value); return *this; }

		strict_inline simd_int &vector_call operator+=(simd_type other) noexcept
		{ value = add(value, other); return *this; }

		strict_inline simd_int &vector_call operator-=(simd_type other) noexcept
		{ value = sub(value, other); return *this; }

		strict_inline simd_int &vector_call operator*=(simd_type other) noexcept
		{ value = mul(value, other); return *this; }

		strict_inline simd_int &vector_call operator&=(simd_type other) noexcept
		{ value = bitAnd(value, other); return *this; }

		strict_inline simd_int &vector_call operator|=(simd_type other) noexcept
		{ value = bitOr(value, other); return *this; }

		strict_inline simd_int &vector_call operator^=(simd_type other) noexcept
		{ value = bitXor(value, other); return *this; }
	};

	using simd_mask = simd_int;


	struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_float
	{
		/////////////////////////////
		// Variables and Constants //
		/////////////////////////////

	#if COMPLEX_SSE4_1
		static constexpr size_t kSize = 4;
		static constexpr size_t kComplexSize = kSize / 2;
		typedef __m128 simd_type;
		typedef __m128i mask_simd_type;
	#elif COMPLEX_NEON
		static constexpr size_t kSize = 4;
		static constexpr size_t kComplexSize = kSize / 2;
		typedef float32x4_t simd_type;
		typedef uint32x4_t mask_simd_type;
	#endif

		simd_type value;


		////////////////////////////
		// Methods Implementation //
		////////////////////////////

		static strict_inline mask_simd_type vector_call toMask(simd_type value)
		{
		#if COMPLEX_SSE4_1
			return _mm_castps_si128(value);
		#elif COMPLEX_NEON
			return vreinterpretq_u32_f32(value);
		#endif
		}

		static strict_inline simd_type vector_call toSimd(mask_simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_castsi128_ps(mask);
		#elif COMPLEX_NEON
			return vreinterpretq_f32_u32(mask);
		#endif
		}

		static strict_inline simd_type vector_call init(float scalar)
		{
		#if COMPLEX_SSE4_1
			return _mm_set1_ps(scalar);
		#elif COMPLEX_NEON
			return vdupq_n_f32(scalar);
		#endif
		}

		static strict_inline simd_type vector_call load(const float *memory)
		{
		#if COMPLEX_SSE4_1
			return _mm_loadu_ps(memory);
		#elif COMPLEX_NEON
			return vld1q_f32(memory);
		#endif
		}

		static strict_inline simd_type vector_call add(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_add_ps(one, two);
		#elif COMPLEX_NEON
			return vaddq_f32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call sub(simd_type one, simd_type two) 
		{
		#if COMPLEX_SSE4_1
			return _mm_sub_ps(one, two);
		#elif COMPLEX_NEON
			return vsubq_f32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call neg(simd_type value) {
		#if COMPLEX_SSE4_1
			return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
		#elif COMPLEX_NEON
			return vmulq_n_f32(value, -1.0f);
		#endif
		}

		static strict_inline simd_type vector_call mul(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_mul_ps(one, two);
		#elif COMPLEX_NEON
			return vmulq_f32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call mulScalar(simd_type value, float scalar)
		{
		#if COMPLEX_SSE4_1
			return _mm_mul_ps(value, _mm_set1_ps(scalar));
		#elif COMPLEX_NEON
			return vmulq_n_f32(value, scalar);
		#endif
		}

		static strict_inline simd_type vector_call mulAdd(simd_type add, simd_type mul_one, simd_type mul_two)
		{
		#if COMPLEX_SSE4_1
		#if COMPLEX_FMA
			return _mm_fmadd_ps(mul_one, mul_two, add);
		#else
			return _mm_add_ps(add, _mm_mul_ps(mul_one, mul_two));
		#endif
		#elif COMPLEX_NEON
		#if defined(NEON_VFP_V3)
			return vaddq_f32(add, vmulq_f32(mul_one, mul_two));
		#else
			return vmlaq_f32(add, mul_one, mul_two);
		#endif
		#endif
		}

		static strict_inline simd_type vector_call mulSub(simd_type sub, simd_type mul_one, simd_type mul_two)
		{
		#if COMPLEX_SSE4_1
		#if COMPLEX_FMA
			return _mm_fmsub_ps(mul_one, mul_two, sub);
		#else
			return _mm_sub_ps(sub, _mm_mul_ps(mul_one, mul_two));
		#endif
		#elif COMPLEX_NEON
		#if defined(NEON_VFP_V3)
			return vsubq_f32(sub, vmulq_f32(mul_one, mul_two));
		#else
			return vmlsq_f32(sub, mul_one, mul_two);
		#endif
		#endif
		}

		static strict_inline simd_type vector_call div(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_div_ps(one, two);
		#elif COMPLEX_NEON
		#if defined(NEON_ARM32)
			simd_type reciprocal = vrecpeq_f32(two);
			reciprocal = vmulq_f32(vrecpsq_f32(two, reciprocal), reciprocal);
			reciprocal = vmulq_f32(vrecpsq_f32(two, reciprocal), reciprocal);
			return vmulq_f32(one, reciprocal);
		#else
			return vdivq_f32(one, two);
		#endif
		#endif
		}

		static strict_inline simd_type vector_call sqrt(simd_type value) noexcept
		{
		#if COMPLEX_SSE4_1
			return _mm_sqrt_ps(value);
		#elif COMPLEX_NEON
			return vsqrtq_f32(value);
		#endif
		}

		static strict_inline simd_type vector_call invSqrt(simd_type value) noexcept
		{
		#if COMPLEX_SSE4_1
			return _mm_rsqrt_ps(value);
		#elif COMPLEX_NEON
			return vrsqrteq_f32(value);
		#endif
		}

		static strict_inline simd_type vector_call bitAnd(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_and_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(vandq_u32(toMask(value), mask));
		#endif
		}

		static strict_inline simd_type vector_call bitOr(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_or_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(vorrq_u32(toMask(value), mask));
		#endif
		}

		static strict_inline simd_type vector_call bitXor(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_SSE4_1
			return _mm_xor_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(veorq_u32(toMask(value), mask));
		#endif
		}

		static strict_inline simd_type vector_call bitNot(simd_type value)
		{	return bitXor(value, simd_mask::init((u32)(-1))); }

		static strict_inline simd_type vector_call max(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_max_ps(one, two);
		#elif COMPLEX_NEON
			return vmaxq_f32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call min(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return _mm_min_ps(one, two);
		#elif COMPLEX_NEON
			return vminq_f32(one, two);
		#endif
		}

		static strict_inline simd_type vector_call truncate(simd_type values)
		{
		#if COMPLEX_SSE4_1
			return _mm_round_ps(values, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}

		static strict_inline simd_type vector_call floor(simd_type values)
		{
		#if COMPLEX_SSE4_1
			return _mm_round_ps(values, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}

		static strict_inline simd_type vector_call ceil(simd_type values)
		{
		#if COMPLEX_SSE4_1
			return _mm_round_ps(values, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}

		static strict_inline simd_type vector_call round(simd_type values)
		{
		#if COMPLEX_SSE4_1
			return _mm_round_ps(values, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}

		static strict_inline simd_type vector_call abs(simd_type value)
		{ return bitAnd(value, simd_mask::init(kNotSignMask)); }

		static strict_inline mask_simd_type vector_call signMask(simd_type value)
		{ return toMask(bitAnd(value, simd_mask::init(kSignMask))); }

		static strict_inline mask_simd_type vector_call equal(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return toMask(_mm_cmpeq_ps(one, two));
		#elif COMPLEX_NEON
			return vceqq_f32(one, two);
		#endif
		}

		static strict_inline mask_simd_type vector_call greaterThan(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return toMask(_mm_cmpgt_ps(one, two));
		#elif COMPLEX_NEON
			return vcgtq_f32(one, two);
		#endif
		}

		static strict_inline mask_simd_type vector_call greaterThanOrEqual(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return toMask(_mm_cmpge_ps(one, two));
		#elif COMPLEX_NEON
			return vcgeq_f32(one, two);
		#endif
		}

		static strict_inline mask_simd_type vector_call notEqual(simd_type one, simd_type two)
		{
		#if COMPLEX_SSE4_1
			return toMask(_mm_cmpneq_ps(one, two));
		#elif COMPLEX_NEON
			simd_mask greater = greaterThan(one, two);
			simd_mask less = lessThan(one, two);
			return simd_mask::bitOr(greater.value, less.value);
		#endif
		}

		static strict_inline float vector_call sum(simd_type value)
		{
		#if COMPLEX_SSE4_1
			simd_type flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2));
			value = _mm_add_ps(value, flip);
			flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(2, 3, 0, 1));
			return _mm_cvtss_f32(_mm_add_ps(value, flip));
		#elif COMPLEX_NEON
			float32x2_t sum = vpadd_f32(vget_low_f32(value), vget_high_f32(value));
			sum = vpadd_f32(sum, sum);
			return vget_lane_f32(sum, 0);
		#endif
		}

		static strict_inline void vector_call transpose(std::array<simd_float, kSize> &rows)
		{
		#if COMPLEX_SSE4_1
			simd_type low0 = _mm_unpacklo_ps(rows[0].value, rows[1].value);
			simd_type low1 = _mm_unpacklo_ps(rows[2].value, rows[3].value);
			simd_type high0 = _mm_unpackhi_ps(rows[0].value, rows[1].value);
			simd_type high1 = _mm_unpackhi_ps(rows[2].value, rows[3].value);
			rows[0].value = _mm_movelh_ps(low0, low1);
			rows[1].value = _mm_movehl_ps(low1, low0);
			rows[2].value = _mm_movelh_ps(high0, high1);
			rows[3].value = _mm_movehl_ps(high1, high0);
		#elif COMPLEX_NEON
			simd_type swap_low = vtrnq_f32(rows[0].value, rows[1].value);
			simd_type swap_high = vtrnq_f32(rows[2].value, rows[3].value);
			rows[0].value = vextq_f32(vextq_f32(swap_low.val[0], swap_low.val[0], 2), swap_high.val[0], 2);
			rows[1].value = vextq_f32(vextq_f32(swap_low.val[1], swap_low.val[1], 2), swap_high.val[1], 2);
			rows[2].value = vextq_f32(swap_low.val[0], vextq_f32(swap_high.val[0], swap_high.val[0], 2), 2);
			rows[3].value = vextq_f32(swap_low.val[1], vextq_f32(swap_high.val[1], swap_high.val[1], 2), 2);
		#endif
		}

		static strict_inline void vector_call complexTranspose(std::array<simd_float, kSize> &rows)
		{
		#if COMPLEX_SSE4_1
			auto low = _mm_movelh_ps(rows[0].value, rows[1].value);
			auto high = _mm_movehl_ps(rows[1].value, rows[0].value);
			rows[0].value = low;
			rows[1].value = high;
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}

		static strict_inline simd_type vector_call reverse(simd_type value)
		{
		#if COMPLEX_SSE4_1
			// right to left
			// *1st* value - in forth place,
			// *2nd* value - in third place, 
			// *3rd* value - in second place, 
			// *4th* value - in first place 
			return _mm_shuffle_ps(value, value, _MM_SHUFFLE(0, 1, 2, 3));
		#elif COMPLEX_NEON
			static_assert(false, "not implemented yet");
		#endif
		}


		//////////////////
		// Method Calls //
		//////////////////

		static strict_inline simd_float vector_call mulAdd(simd_float add, simd_float mul_one, simd_float mul_two)
		{ return mulAdd(add.value, mul_one.value, mul_two.value);	}

		static strict_inline simd_float vector_call mulSub(simd_float sub, simd_float mul_one, simd_float mul_two)
		{ return mulSub(sub.value, mul_one.value, mul_two.value); }

		static strict_inline simd_float vector_call sqrt(simd_float value)
		{ return sqrt(value.value); }

		static strict_inline simd_float vector_call invSqrt(simd_float value)
		{ return invSqrt(value.value); }

		static strict_inline simd_float vector_call max(simd_float one, simd_float two)
		{ return max(one.value, two.value); }

		static strict_inline simd_float vector_call min(simd_float one, simd_float two)
		{ return min(one.value, two.value);	}

		static strict_inline simd_float vector_call clamp(simd_float value, simd_float low, simd_float high)
		{ return max(min(value.value, high.value), low.value); }

		static strict_inline simd_float vector_call truncate(simd_float value)
		{ return truncate(value.value); }

		static strict_inline simd_float vector_call floor(simd_float value)
		{ return floor(value.value); }

		static strict_inline simd_float vector_call ceil(simd_float value)
		{ return ceil(value.value); }

		static strict_inline simd_float vector_call round(simd_float value)
		{ return round(value.value); }

		static strict_inline simd_float vector_call abs(simd_float value)
		{ return abs(value.value); }

		static strict_inline simd_mask vector_call signMask(simd_float value)
		{ return signMask(value.value); }

		static strict_inline simd_mask vector_call equal(simd_float one, simd_float two)
		{ return equal(one.value, two.value);	}

		static strict_inline simd_mask vector_call notEqual(simd_float one, simd_float two)
		{ return notEqual(one.value, two.value); }

		static strict_inline simd_mask vector_call greaterThan(simd_float one, simd_float two)
		{ return greaterThan(one.value, two.value);	}

		static strict_inline simd_mask vector_call greaterThanOrEqual(simd_float one, simd_float two)
		{ return greaterThanOrEqual(one.value, two.value); }

		static strict_inline simd_mask vector_call lessThan(simd_float one, simd_float two)
		{ return greaterThan(two.value, one.value); }

		static strict_inline simd_mask vector_call lessThanOrEqual(simd_float one, simd_float two)
		{ return greaterThanOrEqual(two.value, one.value); }

		static strict_inline simd_float vector_call reverse(simd_float value)
		{ return reverse(value.value); }
		
		strict_inline float vector_call sum() const noexcept
		{ return sum(value); }


		//////////////////////////////////
		// Constructors and Destructors //
		//////////////////////////////////

	#ifdef COMPLEX_MSVC
	private:
		template<size_t ... N>
		static constexpr auto getInternalValue(const auto &scalars, std::index_sequence<N...>) noexcept { return simd_type{ .m128_f32 = { scalars[N]... } }; }
	public:
	#endif
		
		constexpr strict_inline simd_float() noexcept : simd_float(0.0f) { }
		constexpr strict_inline simd_float(float initialValue) noexcept
		{
			if (std::is_constant_evaluated())
			{
				std::array<float, kSize> values{};
				values.fill(initialValue);
			#ifdef COMPLEX_MSVC
				value = getInternalValue(values, std::make_index_sequence<kSize>());
			#elif
				value = std::bit_cast<simd_type>(values);
			#endif
			}
			else
				value = init(initialValue);
		}
		constexpr strict_inline simd_float(simd_type initialValue) noexcept : value(initialValue) { }
		constexpr strict_inline simd_float(const std::array<float, kSize> &scalars) noexcept :
		#ifdef COMPLEX_MSVC
			value(getInternalValue(scalars, std::make_index_sequence<kSize>())) { }
		#elif
			value(std::bit_cast<simd_type>(scalars)) { }
		#endif
		
		template<typename T, size_t N> requires requires { sizeof(simd_type) == sizeof(T) * N; }
		strict_inline simd_float(const std::array<T, N> &scalars) noexcept :
			value(std::bit_cast<simd_type>(scalars)) { }

		constexpr strict_inline float vector_call access(size_t index) const noexcept 
		{ return std::bit_cast<std::array<float, kSize>>(value)[index]; }

		strict_inline void vector_call set(size_t index, float newValue) noexcept
		{
			auto scalars = std::bit_cast<std::array<float, kSize>>(value);
			scalars[index] = newValue;
			value = std::bit_cast<simd_type>(scalars);
		}

		constexpr strict_inline auto vector_call getArrayOfValues() const noexcept
		{ return std::bit_cast<std::array<float, kSize>, simd_type>(value); }

		template<typename T> requires requires (T x) { (sizeof(float) * kSize) % sizeof(x) == 0; }
		constexpr strict_inline auto vector_call getArrayOfValues() const noexcept
		{ return std::bit_cast<std::array<T, ((sizeof(float) * kSize) / sizeof(T))>>(value); }

		///////////////
		// Operators //
		///////////////

		constexpr strict_inline float vector_call operator[](size_t index) const noexcept
		{ return access(index); }

		strict_inline simd_float& vector_call operator+=(simd_float other) noexcept
		{ value = add(value, other.value); return *this; }

		strict_inline simd_float& vector_call operator-=(simd_float other) noexcept
		{ value = sub(value, other.value); return *this; }

		strict_inline simd_float& vector_call operator*=(simd_float other) noexcept
		{ value = mul(value, other.value); return *this; }

		strict_inline simd_float& vector_call operator*=(float scalar) noexcept
		{ value = mulScalar(value, scalar); return *this; }

		strict_inline simd_float& vector_call operator/=(simd_float other) noexcept
		{ value = div(value, other.value); return *this; }


		strict_inline simd_float& vector_call operator&=(simd_mask other) noexcept
		{ value = bitAnd(value, other.value); return *this; }

		strict_inline simd_float& vector_call operator|=(simd_mask other) noexcept
		{ value = bitOr(value, other.value); return *this; }

		strict_inline simd_float& vector_call operator^=(simd_mask other) noexcept
		{ value = bitXor(value, other.value); return *this; }
		
		strict_inline simd_float& vector_call operator&=(simd_float other) noexcept
		{ value = bitAnd(value, toMask(other.value)); return *this; }

		strict_inline simd_float& vector_call operator|=(simd_float other) noexcept
		{ value = bitOr(value, toMask(other.value)); return *this; }
		
		strict_inline simd_float& vector_call operator^=(simd_float other) noexcept
		{ value = bitXor(value, toMask(other.value)); return *this; }


		strict_inline simd_float vector_call operator+(simd_float other) const noexcept
		{ return add(value, other.value); }

		strict_inline simd_float vector_call operator-(simd_float other) const noexcept
		{ return sub(value, other.value); }

		strict_inline simd_float vector_call operator*(simd_float other) const noexcept
		{ return mul(value, other.value); }

		strict_inline simd_float vector_call operator*(float scalar) const noexcept
		{ return mulScalar(value, scalar); }

		strict_inline simd_float vector_call operator/(simd_float other) const noexcept
		{ return div(value, other.value); }


		strict_inline simd_float vector_call operator&(simd_mask other) const noexcept
		{ return bitAnd(value, other.value); }

		strict_inline simd_float vector_call operator|(simd_mask other) const noexcept
		{ return bitOr(value, other.value); }

		strict_inline simd_float vector_call operator^(simd_mask other) const noexcept
		{ return bitXor(value, other.value); }
		
		strict_inline simd_float vector_call operator&(simd_float other) const noexcept
		{ return bitAnd(value, toMask(other.value)); }

		strict_inline simd_float vector_call operator|(simd_float other) const noexcept
		{ return bitOr(value, toMask(other.value)); }

		strict_inline simd_float vector_call operator^(simd_float other) const noexcept
		{ return bitXor(value, toMask(other.value)); }

		strict_inline simd_float vector_call operator-() const noexcept
		{ return neg(value); }

		strict_inline simd_float vector_call operator~() const noexcept
		{ return bitNot(value); }
	};

	template<typename T>
	concept SimdInt = std::same_as<T, simd_int>;

	template<typename T>
	concept SimdFloat = std::same_as<T, simd_float>;

	template<typename T>
	concept SimdValue = SimdInt<T> || SimdFloat<T>;
}

using namespace simd_values;

