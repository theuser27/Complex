/*
	==============================================================================

		simd_values.h
		Created: 22 May 2021 6:22:20pm
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include <cstdint>
#include <climits>
#include <cstdlib>
#include <concepts>
#include <bit>
#include <array>

// there isn't a macro to check SSE3, so we define it here
// the project cannot run without vectorisation (either x86 SSE2 or ARM NEON)

#if defined (_MSC_VER)
	#define __SSE3__ 1
#endif

// TODO: implement AVX2 later
#if 0
	#define COMPLEX_AVX2 1
	#define COMPLEX_SIMD_ALIGNMENT 32
#elif __SSE3__
	#define COMPLEX_SSE3 1
	#define COMPLEX_SIMD_ALIGNMENT 16
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
	#define COMPLEX_NEON 1
	#define COMPLEX_SIMD_ALIGNMENT 16
#endif

#if COMPLEX_SSE3
#include <immintrin.h>
#include <tmmintrin.h>
#elif COMPLEX_NEON
#include <arm_neon.h>
#endif

#if !defined (force_inline)
#if defined (_MSC_VER)
#define force_inline __forceinline
#define vector_call __vectorcall
#else
#define force_inline inline __attribute__((always_inline))
#define vector_call
#endif
#endif

// using rust naming because yes
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

namespace simd_values
{
	static constexpr u32 kNoChangeMask = UINT32_MAX;
	static constexpr u32 kSignMask = INT32_MAX + 1;
	static constexpr u32 kNotSignMask = INT32_MAX;

	struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_int
	{
		/////////////////////////////
		// Variables and Constants //
		/////////////////////////////

	#if COMPLEX_AVX2
		static constexpr size_t kSize = 8;
		typedef __m256i simd_type;
	#elif COMPLEX_SSE3
		static constexpr size_t kSize = 4;
		typedef __m128i simd_type;
	#elif COMPLEX_NEON
		static constexpr size_t kSize = 4;
		typedef uint32x4_t simd_type;
	#endif 

		simd_type value;

		static constexpr u32 kFullMask = UINT32_MAX;
		static constexpr u32 kSignMask = 0x80000000;
		static constexpr u32 kNotSignedMask = kFullMask ^ kSignMask;


		///////////////////////////////////
		// Static Method Implementations //
		///////////////////////////////////

		static force_inline simd_type vector_call init(u32 scalar)
		{
		#if COMPLEX_AVX2
			return _mm256_set1_epi32((int32_t)scalar);
		#elif COMPLEX_SSE3
			return _mm_set1_epi32((int32_t)scalar);
		#elif COMPLEX_NEON
			return vdupq_n_u32(scalar);
		#endif 
		}

		static force_inline simd_type vector_call load(const u32 *memory)
		{
		#if COMPLEX_AVX2
			return _mm256_loadu_si256((const __m256i *)memory)
			#elif COMPLEX_SSE3
			return _mm_loadu_si128((const __m128i *)memory);
		#elif COMPLEX_NEON
			return vld1q_u32(memory);
		#endif 
		}

		static force_inline simd_type vector_call add(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_add_epi32(one, two);
		#elif COMPLEX_SSE3
			return _mm_add_epi32(one, two);
		#elif COMPLEX_NEON
			return vaddq_u32(one, two);
		#endif
		}

		static force_inline simd_type vector_call sub(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_sub_epi32(one, two);
		#elif COMPLEX_SSE3
			return _mm_sub_epi32(one, two);
		#elif COMPLEX_NEON
			return vsubq_u32(one, two);
		#endif
		}

		static force_inline simd_type vector_call neg(simd_type value)
		{
		#if COMPLEX_AVX2
			return _mm256_sub_epi32(_mm256_set1_epi32(0), value);
		#elif COMPLEX_SSE3
			return _mm_sub_epi32(_mm_set1_epi32(0), value);
		#elif COMPLEX_NEON
			return vmulq_n_u32(value, -1);
		#endif
		}

		static force_inline simd_type vector_call mul(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return __mm256_mul_epi32(one, two);
		#elif COMPLEX_SSE3
			simd_type mul0_2 = _mm_mul_epu32(one, two);
			simd_type mul1_3 = _mm_mul_epu32(_mm_shuffle_epi32(one, _MM_SHUFFLE(2, 3, 0, 1)),
				_mm_shuffle_epi32(two, _MM_SHUFFLE(2, 3, 0, 1)));
			return _mm_unpacklo_epi32(_mm_shuffle_epi32(mul0_2, _MM_SHUFFLE(0, 0, 2, 0)),
				_mm_shuffle_epi32(mul1_3, _MM_SHUFFLE(0, 0, 2, 0)));
		#elif COMPLEX_NEON
			return vmulq_n_u32(value, -1);
		#endif
		}

		static force_inline simd_type vector_call bitAnd(simd_type value, simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_and_si256(value, mask);
		#elif COMPLEX_SSE3
			return _mm_and_si128(value, mask);
		#elif COMPLEX_NEON
			return vandq_u32(value, mask);
		#endif
		}

		static force_inline simd_type vector_call bitOr(simd_type value, simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_or_si256(value, mask);
		#elif COMPLEX_SSE3
			return _mm_or_si128(value, mask);
		#elif COMPLEX_NEON
			return vorrq_u32(value, mask);
		#endif
		}

		static force_inline simd_type vector_call bitXor(simd_type value, simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_xor_si256(value, mask);
		#elif COMPLEX_SSE3
			return _mm_xor_si128(value, mask);
		#elif COMPLEX_NEON
			return veorq_u32(value, mask);
		#endif
		}

		static force_inline simd_type vector_call bitNot(simd_type value) { return bitXor(value, init(-1)); }

		static force_inline simd_type vector_call equal(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_cmpeq_epi32(one, two);
		#elif COMPLEX_SSE3
			return _mm_cmpeq_epi32(one, two);
		#elif COMPLEX_NEON
			return vceqq_u32(one, two);
		#endif
		}

		static force_inline simd_type vector_call greaterThanSigned(simd_type one, simd_type two) {
		#if COMPLEX_AVX2
			return _mm256_cmpgt_epi32(one, two);
		#elif COMPLEX_SSE3
			return _mm_cmpgt_epi32(one, two);
		#elif COMPLEX_NEON
			return vcgtq_s32(one, two);
		#endif
		}

		static force_inline simd_type vector_call greaterThanUnsigned(simd_type one, simd_type two) {
		#if COMPLEX_AVX2
			return _mm256_cmpgt_epi32(_mm256_xor_si256(one, init(kSignMask)), _mm256_xor_si256(two, init(kSignMask)));
		#elif COMPLEX_SSE3
			return _mm_cmpgt_epi32(_mm_xor_si128(one, init(kSignMask)), _mm_xor_si128(two, init(kSignMask)));
		#elif COMPLEX_NEON
			return vcgtq_u32(one, two);
		#endif
		}

		static force_inline simd_type vector_call max(simd_type one, simd_type two) {
		#if COMPLEX_AVX2
			return _mm256_max_epi32(one, two);
		#elif COMPLEX_SSE3
			simd_type greater_than_mask = greaterThanUnsigned(one, two);
			return _mm_or_si128(_mm_and_si128(greater_than_mask, one), _mm_andnot_si128(greater_than_mask, two));
		#elif COMPLEX_NEON
			return vmaxq_u32(one, two);
		#endif
		}

		static force_inline simd_type vector_call min(simd_type one, simd_type two) {
		#if COMPLEX_AVX2
			return _mm256_min_epi32(one, two);
		#elif COMPLEX_SSE3
			simd_type less_than_mask = _mm_cmpgt_epi32(two, one);
			return _mm_or_si128(_mm_and_si128(less_than_mask, one), _mm_andnot_si128(less_than_mask, two));
		#elif COMPLEX_NEON
			return vminq_u32(one, two);
		#endif
		}

		static force_inline u32 vector_call sum(simd_type value) {
		#if COMPLEX_AVX2
			simd_type flip = _mm256_permute4x64_epi64(value, _MM_SHUFFLE(1, 0, 3, 2))
				simd_type sum = _mm256_hadd_epi32(value, flip);
			sum = _mm256_hadd_epi32(sum, sum);
			return _mm256_cvtsi256_si32(sum);
		#elif COMPLEX_SSE3
			auto scalars = std::bit_cast<std::array<u32, kSize>>(value);
			u32 total = 0;
			for (int i = 0; i < kSize; ++i)
				total += scalars[i];
			return total;
		#elif COMPLEX_NEON
			uint32x2_t sum = vpadd_u32(vget_low_u32(value), vget_high_u32(value));
			sum = vpadd_u32(sum, sum);
			return vget_lane_u32(sum, 0);
		#endif
		}

		static force_inline u32 vector_call anyMask(simd_type value)
		{
		#if COMPLEX_AVX2
			return _mm256_movemask_epi8(value);
		#elif COMPLEX_SSE3
			return _mm_movemask_epi8(value);
		#elif COMPLEX_NEON
			uint32x2_t max = vpmax_u32(vget_low_u32(value), vget_high_u32(value));
			max = vpmax_u32(max, max);
			return vget_lane_u32(max, 0);
		#endif
		}


		/////////////////////////
		// Static Method Calls //
		/////////////////////////

		static force_inline simd_int vector_call max(simd_int one, simd_int two)
		{	return max(one.value, two.value); }

		static force_inline simd_int vector_call min(simd_int one, simd_int two)
		{ return min(one.value, two.value); }

		static force_inline simd_int vector_call equal(simd_int one, simd_int two)
		{	return equal(one.value, two.value); }

		static force_inline simd_int vector_call greaterThanUnsigned(simd_int one, simd_int two)
		{	return greaterThanUnsigned(one.value, two.value); }

		static force_inline simd_int vector_call lessThanUnsigned(simd_int one, simd_int two)
		{	return greaterThanUnsigned(two.value, one.value); }		
		
		static force_inline simd_int vector_call greaterThanSigned(simd_int one, simd_int two)
		{	return greaterThanSigned(one.value, two.value); }

		static force_inline simd_int vector_call lessThanSigned(simd_int one, simd_int two)
		{	return greaterThanSigned(two.value, one.value); }

		static force_inline simd_int vector_call greaterThanOrEqualSigned(simd_int one, simd_int two)
		{	return greaterThanSigned(one, two) | equal(one, two); }


		//////////////////////////////////
		// Constructors and Destructors //
		//////////////////////////////////

		force_inline simd_int() noexcept : value(init(0)) { }
		force_inline simd_int(simd_type initial_value) noexcept : value(initial_value) { }
		force_inline simd_int(u32 initial_value) noexcept : value(init(initial_value)) { }
		force_inline simd_int(const std::array<u32, kSize> &scalars) noexcept
			: value(std::bit_cast<simd_type, std::array<u32, kSize>>(scalars)) {	}

		template<typename T> requires requires (T x) { (sizeof(u32) * kSize) % sizeof(x) == 0; }
		force_inline simd_int(const std::array<T, (sizeof(u32) *kSize) / sizeof(T)> &scalars) noexcept
			: value(std::bit_cast<simd_type>(scalars)) {	}

		force_inline ~simd_int() noexcept = default;


		///////////////
		// Operators //
		///////////////

		force_inline u32 vector_call access(size_t index) const noexcept
		{
			auto scalars = std::bit_cast<std::array<u32, kSize>, simd_type>(value);
			return scalars[index];
		}

		force_inline void vector_call set(size_t index, u32 newValue) noexcept
		{
			auto scalars = std::bit_cast<std::array<u32, kSize>, simd_type>(value);
			scalars[index] = newValue;
			value = std::bit_cast<simd_type, std::array<u32, kSize>>(scalars);
		}

		force_inline std::array<u32, kSize> getArrayOfValues() const
		{	return std::bit_cast<std::array<u32, kSize>, simd_type>(value); }

		template<typename T> requires requires (T x) { (sizeof(u32) * kSize) % sizeof(x) == 0; }
		force_inline auto getArrayOfValues() const
		{	return std::bit_cast<std::array<T, ((sizeof(u32) *kSize) / sizeof(T))>>(value); }

		force_inline void swap(simd_int &other)
		{
			simd_type temp = other.value;
			other.value = value;
			value = temp;
		}

		///////////////
		// Operators //
		///////////////

		force_inline u32 vector_call operator[](size_t index) const noexcept
		{
			return access(index);
		}

		force_inline simd_int vector_call operator+(simd_int other) const noexcept
		{
			return add(value, other.value);
		}

		force_inline simd_int vector_call operator-(simd_int other) const noexcept
		{
			return sub(value, other.value);
		}

		force_inline simd_int vector_call operator*(simd_int other) const noexcept
		{
			return mul(value, other.value);
		}

		force_inline simd_int vector_call operator&(simd_int other) const noexcept
		{
			return bitAnd(value, other.value);
		}

		force_inline simd_int vector_call operator|(simd_int other) const noexcept
		{
			return bitOr(value, other.value);
		}

		force_inline simd_int vector_call operator^(simd_int other) const noexcept
		{
			return bitXor(value, other.value);
		}

		force_inline simd_int vector_call operator-() const noexcept
		{
			return neg(value);
		}

		force_inline simd_int vector_call operator~() const noexcept
		{
			return bitNot(value);
		}

		force_inline u32 vector_call sum() const noexcept
		{
			return sum(value);
		}

		force_inline u32 vector_call anyMask() const noexcept
		{
			return anyMask(value);
		}

		force_inline simd_int &vector_call operator+=(simd_int other) noexcept
		{
			value = add(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator-=(simd_int other) noexcept
		{
			value = sub(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator*=(simd_int other) noexcept
		{
			value = mul(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator&=(simd_int other) noexcept
		{
			value = bitAnd(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator|=(simd_int other) noexcept
		{
			value = bitOr(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator^=(simd_int other) noexcept
		{
			value = bitXor(value, other.value);
			return *this;
		}

		force_inline simd_int &vector_call operator+=(simd_type other) noexcept
		{
			value = add(value, other);
			return *this;
		}

		force_inline simd_int &vector_call operator-=(simd_type other) noexcept
		{
			value = sub(value, other);
			return *this;
		}

		force_inline simd_int &vector_call operator*=(simd_type other) noexcept
		{
			value = mul(value, other);
			return *this;
		}

		force_inline simd_int &vector_call operator&=(simd_type other) noexcept
		{
			value = bitAnd(value, other);
			return *this;
		}

		force_inline simd_int &vector_call operator|=(simd_type other) noexcept
		{
			value = bitOr(value, other);
			return *this;
		}

		force_inline simd_int &vector_call operator^=(simd_type other) noexcept
		{
			value = bitXor(value, other);
			return *this;
		}
	};

	typedef simd_int simd_mask;


	struct alignas(COMPLEX_SIMD_ALIGNMENT) simd_float
	{
		/////////////////////////////
		// Variables and Constants //
		/////////////////////////////

	#if COMPLEX_AVX2
		static constexpr size_t kSize = 8;
		static constexpr size_t kComplexSize = kSize / 2;
		typedef __m256 simd_type;
		typedef __m256i mask_simd_type;
	#elif COMPLEX_SSE3
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


		///////////////////////////////////
		// Static Methods Implementation //
		///////////////////////////////////

		static force_inline mask_simd_type vector_call toMask(simd_type value)
		{
		#if COMPLEX_AVX2
			return _mm256_castps_si256(value);
		#elif COMPLEX_SSE3
			return _mm_castps_si128(value);
		#elif COMPLEX_NEON
			return vreinterpretq_u32_f32(value);
		#endif
		}

		static force_inline simd_type vector_call toSimd(mask_simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_castsi256_ps(mask);
		#elif COMPLEX_SSE3
			return _mm_castsi128_ps(mask);
		#elif COMPLEX_NEON
			return vreinterpretq_f32_u32(mask);
		#endif
		}

		static force_inline simd_type vector_call init(float scalar)
		{
		#if COMPLEX_AVX2
			return _mm256_broadcast_ss(&scalar);
		#elif COMPLEX_SSE3
			return _mm_set1_ps(scalar);
		#elif COMPLEX_NEON
			return vdupq_n_f32(scalar);
		#endif
		}

		static force_inline simd_type vector_call load(const float *memory)
		{
		#if COMPLEX_AVX2
			return _mm256_loadu_ps(memory);
		#elif COMPLEX_SSE3
			return _mm_loadu_ps(memory);
		#elif COMPLEX_NEON
			return vld1q_f32(memory);
		#endif
		}

		static force_inline simd_type vector_call add(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_add_ps(one, two);
		#elif COMPLEX_SSE3
			return _mm_add_ps(one, two);
		#elif COMPLEX_NEON
			return vaddq_f32(one, two);
		#endif
		}

		static force_inline simd_type vector_call sub(simd_type one, simd_type two) {
		#if COMPLEX_AVX2
			return _mm256_sub_ps(one, two);
		#elif COMPLEX_SSE3
			return _mm_sub_ps(one, two);
		#elif COMPLEX_NEON
			return vsubq_f32(one, two);
		#endif
		}

		static force_inline simd_type vector_call neg(simd_type value) {
		#if COMPLEX_AVX2
			return _mm256_xor_ps(value, _mm256_set1_ps(-0.0f));
		#elif COMPLEX_SSE3
			return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
		#elif COMPLEX_NEON
			return vmulq_n_f32(value, -1.0f);
		#endif
		}

		static force_inline simd_type vector_call mul(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_mul_ps(one, two);
		#elif COMPLEX_SSE3
			return _mm_mul_ps(one, two);
		#elif COMPLEX_NEON
			return vmulq_f32(one, two);
		#endif
		}

		static force_inline simd_type vector_call mulScalar(simd_type value, float scalar)
		{
		#if COMPLEX_AVX2
			return _mm256_mul_ps(value, _mm256_set1_ps(scalar));
		#elif COMPLEX_SSE3
			return _mm_mul_ps(value, _mm_set1_ps(scalar));
		#elif COMPLEX_NEON
			return vmulq_n_f32(value, scalar);
		#endif
		}

		static force_inline simd_type vector_call mulAdd(simd_type add, simd_type mul_one, simd_type mul_two)
		{
		#if COMPLEX_AVX2
			return _mm256_fmadd_ps(mul_one, mul_two, add);
		#elif COMPLEX_SSE3
			return _mm_add_ps(add, _mm_mul_ps(mul_one, mul_two));
		#elif COMPLEX_NEON
		#if defined(NEON_VFP_V3)
			return vaddq_f32(add, vmulq_f32(mul_one, mul_two));
		#else
			return vmlaq_f32(add, mul_one, mul_two);
		#endif
		#endif
		}

		static force_inline simd_type vector_call mulSub(simd_type sub, simd_type mul_one, simd_type mul_two)
		{
		#if COMPLEX_AVX2
			return _mm256_fsub_ps(mul_one, mul_two, sub);
		#elif COMPLEX_SSE3
			return _mm_sub_ps(sub, _mm_mul_ps(mul_one, mul_two));
		#elif COMPLEX_NEON
		#if defined(NEON_VFP_V3)
			return vsubq_f32(sub, vmulq_f32(mul_one, mul_two));
		#else
			return vmlsq_f32(sub, mul_one, mul_two);
		#endif
		#endif
		}

		static force_inline simd_type vector_call div(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_div_ps(one, two);
		#elif COMPLEX_SSE3
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

		static force_inline simd_type vector_call sqrt(simd_type value)
		{
		#if COMPLEX_AVX2
			return _mm256_sqrt_ps(value);
		#elif COMPLEX_SSE3
			return _mm_sqrt_ps(value);
		#elif COMPLEX_NEON
			
		#endif
		}

		static force_inline simd_type vector_call bitAnd(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_and_ps(value, toSimd(mask));
		#elif COMPLEX_SSE3
			return _mm_and_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(vandq_u32(toMask(value), mask));
		#endif
		}

		static force_inline simd_type vector_call bitOr(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_or_ps(value, toSimd(mask));
		#elif COMPLEX_SSE3
			return _mm_or_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(vorrq_u32(toMask(value), mask));
		#endif
		}

		static force_inline simd_type vector_call bitXor(simd_type value, mask_simd_type mask)
		{
		#if COMPLEX_AVX2
			return _mm256_xor_ps(value, toSimd(mask));
		#elif COMPLEX_SSE3
			return _mm_xor_ps(value, toSimd(mask));
		#elif COMPLEX_NEON
			return toSimd(veorq_u32(toMask(value), mask));
		#endif
		}

		static force_inline simd_type vector_call bitNot(simd_type value)
		{
			return bitXor(value, simd_mask::init(-1));
		}

		static force_inline simd_type vector_call max(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_max_ps(one, two);
		#elif COMPLEX_SSE3
			return _mm_max_ps(one, two);
		#elif COMPLEX_NEON
			return vmaxq_f32(one, two);
		#endif
		}

		static force_inline simd_type vector_call min(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return _mm256_min_ps(one, two);
		#elif COMPLEX_SSE3
			return _mm_min_ps(one, two);
		#elif COMPLEX_NEON
			return vminq_f32(one, two);
		#endif
		}

		static force_inline simd_type vector_call abs(simd_type value)
		{ return bitAnd(value, simd_mask::init(simd_mask::kNotSignedMask)); }

		static force_inline mask_simd_type vector_call signMask(simd_type value)
		{
			return toMask(bitAnd(value, simd_mask::init(simd_mask::kSignMask)));
		}


		static force_inline mask_simd_type vector_call equal(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return toMask(_mm256_cmp_ps(one, two, _CMP_EQ_OQ));
		#elif COMPLEX_SSE3
			return toMask(_mm_cmpeq_ps(one, two));
		#elif COMPLEX_NEON
			return vceqq_f32(one, two);
		#endif
		}

		static force_inline mask_simd_type vector_call greaterThan(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return toMask(_mm256_cmp_ps(one, two, _CMP_GT_OQ));
		#elif COMPLEX_SSE3
			return toMask(_mm_cmpgt_ps(one, two));
		#elif COMPLEX_NEON
			return vcgtq_f32(one, two);
		#endif
		}

		static force_inline mask_simd_type vector_call greaterThanOrEqual(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return toMask(_mm256_cmp_ps(one, two, _CMP_GE_OQ));
		#elif COMPLEX_SSE3
			return toMask(_mm_cmpge_ps(one, two));
		#elif COMPLEX_NEON
			return vcgeq_f32(one, two);
		#endif
		}

		static force_inline mask_simd_type vector_call notEqual(simd_type one, simd_type two)
		{
		#if COMPLEX_AVX2
			return toMask(_mm256_cmp_ps(one, two, _CMP_NEQ_OQ));
		#elif COMPLEX_SSE3
			return toMask(_mm_cmpneq_ps(one, two));
		#elif COMPLEX_NEON
			simd_mask greater = greaterThan(one, two);
			simd_mask less = lessThan(one, two);
			return simd_mask::bitOr(greater.value, less.value);
		#endif
		}

		static force_inline float vector_call sum(simd_type value)
		{
		#if COMPLEX_AVX2
			simd_type flip = _mm256_permute2f128_ps(value, value, 1);
			simd_type sum = _mm256_hadd_ps(value, flip);
			sum = _mm256_hadd_ps(sum, sum);
			return _mm256_cvtss_f32(sum);
		#elif COMPLEX_SSE3
			simd_type flip = _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2));
			simd_type sum = _mm_add_ps(value, flip);
			simd_type swap = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(2, 3, 0, 1));
			return _mm_cvtss_f32(_mm_add_ps(sum, swap));
		#elif COMPLEX_NEON
			float32x2_t sum = vpadd_f32(vget_low_f32(value), vget_high_f32(value));
			sum = vpadd_f32(sum, sum);
			return vget_lane_f32(sum, 0);
		#endif
		}

		static force_inline void vector_call transpose(std::array<simd_float, simd_float::kSize> &rows)
		{
		#if COMPLEX_AVX2
			static_assert(false, "AVX2 transpose not supported yet");
		#elif COMPLEX_SSE3
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

		static force_inline simd_type vector_call shift(simd_type value, simd_mask shiftMask)
		{
			// if all the masks are kNoChangeMask, we simply return
			if (shiftMask.sum() == (u32)(-kSize))
				return value;

			// doing shifting
			static constexpr u32 arraySize = sizeof(u32) / sizeof(u8);
			auto shifts = shiftMask.getArrayOfValues();
			u8 byteShifts[kSize * arraySize];
			for (u32 i = 0; i < shifts.size(); i++)
			{
				if (shifts[i] == kNoChangeMask)
				{
					for (u32 j = 0; j < arraySize; j++)
					{
						byteShifts[i * arraySize + j] = i * arraySize + j;
					}
				}
				else
				{
					for (u32 j = 0; j < arraySize; j++)
					{
						byteShifts[i * arraySize + j] = shifts[i] * arraySize + j;
					}
				}
			}

			// TODO: reverse intrinsic for avx2 and neon
		#if COMPLEX_AVX2
			
		#elif COMPLEX_SSE3 // SSSE3
			return toSimd(_mm_shuffle_epi8(toMask(value), std::bit_cast<simd_mask::simd_type>(byteShifts)));
		#elif COMPLEX_NEON

		#endif
		}

		static force_inline simd_type vector_call reverse(simd_type value)
		{
			// TODO: reverse intrinsic for avx2 and neon
		#if COMPLEX_AVX2
			
		#elif COMPLEX_SSE3
			// right to left
			// *4th* value - in first place, *3rd* value - in second place, 
			// *2nd* value - in third place, *1st* value - in forth place
			return _mm_shuffle_ps(value, value, _MM_SHUFFLE(0, 1, 2, 3));
		#elif COMPLEX_NEON

		#endif

		}

		template<float(*func)(float)>
		static force_inline simd_float map(simd_float value) {
			simd_float result;
			for (int i = 0; i < kSize; ++i)
				result.set(i, func(value[i]));
			return result;
		}

		template<float(*func)(float, float)>
		static force_inline simd_float map(simd_float one, simd_float two) {
			simd_float result;
			for (int i = 0; i < kSize; ++i)
				result.set(i, func(one[i], two[i]));
			return result;
		}
		// TODO: optimise sin, cos and atan2 calls
		static force_inline simd_type vector_call sin(simd_type value) { return map<sinf>(value).value; }
		static force_inline simd_type vector_call cos(simd_type value) { return map<cosf>(value).value; }
		static force_inline simd_type vector_call atan2(simd_type one, simd_type two) { return map<atan2f>(one, two).value; }

		static force_inline void vector_call complexValueMerge(simd_type &one, simd_type &two)
		{
			// TODO: implement complexMerge for AVX and NEON
		#if COMPLEX_AVX2

		#elif COMPLEX_SSE3
			simd_type one_ = _mm_unpacklo_ps(one, two);
			two = _mm_unpackhi_ps(one, two);
			one = one_;
		#elif COMPLEX_NEON

		#endif
		}

		static force_inline simd_type vector_call complexCartAdd(simd_type one, simd_type two)
		{ return add(one, two); }

		static force_inline simd_type vector_call complexCartSub(simd_type one, simd_type two)
		{ return sub(one, two); }

		static force_inline simd_type vector_call complexCartMul(simd_type one, simd_type two)
		{

		#if COMPLEX_AVX2
			// TODO: implement permutation
			static_assert(false, "AVX2 complexCartMul not supported yet");
		#elif COMPLEX_SSE3
			simd_type realSums = mul(one, two);
			simd_type imaginarySums = mul(one, _mm_shuffle_ps(two, two, _MM_SHUFFLE(2, 3, 0, 1)));
			realSums = _mm_hsub_ps(realSums, realSums);
			imaginarySums = _mm_hadd_ps(imaginarySums, imaginarySums);
			return _mm_unpacklo_ps(realSums, imaginarySums);
		#elif COMPLEX_NEON
			static_assert(false, "ARM NEON complexCartMul not supported yet");
		#endif
		}

		static force_inline simd_type vector_call complexPolarMul(simd_type one, simd_type two)
		{
			simd_type magnitudes = mul(one, two);
			simd_type phases = add(one, two);
		#if COMPLEX_AVX2
			// TODO: implement permutation
			static_assert(false, "AVX2 complexPolarMul not supported yet");
		#elif COMPLEX_SSE3
			magnitudes = _mm_shuffle_ps(magnitudes, magnitudes, _MM_SHUFFLE(2, 0, 2, 0));
			phases = _mm_shuffle_ps(phases, phases, _MM_SHUFFLE(3, 1, 3, 1));
			return _mm_unpacklo_ps(magnitudes, phases);
		#elif COMPLEX_NEON
			static_assert(false, "ARM NEON complexPolarMul not supported yet");
		#endif
		}

		static force_inline void vector_call complexTranspose(std::array<simd_float, simd_float::kSize> &rows)
		{
		#if COMPLEX_AVX2
			static_assert(false, "AVX2 complexTranspose not supported yet");
		#elif COMPLEX_SSE3
			simd_type low = _mm_movelh_ps(rows[0].value, rows[1].value);
			simd_type high = _mm_movehl_ps(rows[1].value, rows[0].value);
			rows[0].value = low;
			rows[1].value = high;
		#elif COMPLEX_NEON
			// TODO: implement complexTranspose for NEON
			static_assert(false, "ARM NEON complexTranspose not supported yet");
		#endif
		}

		static force_inline simd_type vector_call complexMagnitude(simd_type one, simd_type two)
		{
			// TODO: hypot intrinsics for avx2 and neon
		#if COMPLEX_AVX2

		#elif COMPLEX_SSE3
			simd_type real = _mm_shuffle_ps(one, two, _MM_SHUFFLE(2, 0, 2, 0));
			simd_type imaginary = _mm_shuffle_ps(one, two, _MM_SHUFFLE(3, 1, 3, 1));
		#elif COMPLEX_NEON

		#endif

			return sqrt(mulAdd(mul(real, real), imaginary, imaginary));
		}

		static force_inline simd_type vector_call complexPhase(simd_type one, simd_type two)
		{
			// TODO: atan2 intrinsics for avx2 and neon
		#if COMPLEX_AVX2

		#elif COMPLEX_SSE3
			simd_type real = _mm_shuffle_ps(one, two, _MM_SHUFFLE(2, 0, 2, 0));
			simd_type imaginary = _mm_shuffle_ps(one, two, _MM_SHUFFLE(3, 1, 3, 1));

		#elif COMPLEX_NEON

		#endif

			return simd_float::atan2(imaginary, real);
		}

		static force_inline simd_type vector_call complexReal(simd_type one, simd_type two)
		{
			// TODO: atan2 intrinsics for avx2 and neon
		#if COMPLEX_AVX2

		#elif COMPLEX_SSE3
			simd_type magnitude = _mm_shuffle_ps(one, two, _MM_SHUFFLE(2, 0, 2, 0));
			simd_type phase = _mm_shuffle_ps(one, two, _MM_SHUFFLE(3, 1, 3, 1));

		#elif COMPLEX_NEON

		#endif

			return mul(magnitude, simd_float::cos(phase));
		}

		static force_inline simd_type vector_call complexImaginary(simd_type one, simd_type two)
		{
			// TODO: atan2 intrinsics for avx2 and neon
		#if COMPLEX_AVX2

		#elif COMPLEX_SSE3
			simd_type magnitude = _mm_shuffle_ps(one, two, _MM_SHUFFLE(2, 0, 2, 0));
			simd_type phase = _mm_shuffle_ps(one, two, _MM_SHUFFLE(3, 1, 3, 1));

		#elif COMPLEX_NEON

		#endif

			return mul(magnitude, simd_float::sin(phase));
		}


		/////////////////////////
		// Static Method Calls //
		/////////////////////////

		static force_inline simd_float vector_call mulAdd(simd_float add, simd_float mul_one, simd_float mul_two)
		{	return mulAdd(add.value, mul_one.value, mul_two.value);	}

		static force_inline simd_float vector_call mulSub(simd_float sub, simd_float mul_one, simd_float mul_two)
		{	return mulSub(sub.value, mul_one.value, mul_two.value); }

		static force_inline simd_float vector_call max(simd_float one, simd_float two)
		{ return max(one.value, two.value); }

		static force_inline simd_float vector_call min(simd_float one, simd_float two)
		{	return min(one.value, two.value);	}

		static force_inline simd_float vector_call clamp(simd_float low, simd_float high, simd_float value)
		{ return max(min(value.value, high.value), low.value); }

		static force_inline simd_float vector_call abs(simd_float value)
		{	return abs(value.value); }

		static force_inline simd_mask vector_call sign_mask(simd_float value)
		{	return sign_mask(value.value);	}

		static force_inline simd_mask vector_call equal(simd_float one, simd_float two)
		{	return equal(one.value, two.value);	}

		static force_inline simd_mask vector_call notEqual(simd_float one, simd_float two)
		{	return notEqual(one.value, two.value); }

		static force_inline simd_mask vector_call greaterThan(simd_float one, simd_float two)
		{	return greaterThan(one.value, two.value);	}

		static force_inline simd_mask vector_call greaterThanOrEqual(simd_float one, simd_float two)
		{	return greaterThanOrEqual(one.value, two.value); }

		static force_inline simd_mask vector_call lessThan(simd_float one, simd_float two)
		{	return greaterThan(two.value, one.value); }

		static force_inline simd_mask vector_call lessThanOrEqual(simd_float one, simd_float two)
		{	return greaterThanOrEqual(two.value, one.value); }

		static force_inline simd_float vector_call reverse(simd_float value)
		{	return reverse(value.value); }

		static force_inline simd_float vector_call complexCartAdd(simd_float one, simd_float two)
		{ return complexCartAdd(one.value, two.value); }

		static force_inline simd_float vector_call complexCartSub(simd_float one, simd_float two)
		{ return complexCartSub(one.value, two.value); }

		static force_inline simd_float vector_call complexCartMul(simd_float one, simd_float two)
		{	return complexCartMul(one.value, two.value); }

		static force_inline simd_float vector_call complexPolarMul(simd_float one, simd_float two)
		{	return complexPolarMul(one.value, two.value); }

		static force_inline simd_float vector_call complexMagnitude(simd_float one, simd_float two)
		{	return complexMagnitude(one.value, two.value); }

		static force_inline simd_float vector_call complexPhase(simd_float one, simd_float two)
		{	return complexPhase(one.value, two.value); }

		static force_inline void vector_call complexCartToPolar(simd_float &one, simd_float &two)
		{
			simd_type magnitudes = complexMagnitude(one.value, two.value);
			simd_type phases = complexPhase(one.value, two.value);
			complexValueMerge(magnitudes, phases);
			one.value = magnitudes; 
			two.value = phases;
		}

		static force_inline void vector_call complexPolarToCart(simd_float &one, simd_float &two)
		{
			// TODO: generalise
			simd_type realValues = complexMagnitude(one.value, two.value);
			simd_type imaginaryValues = complexPhase(one.value, two.value);
			complexValueMerge(realValues, imaginaryValues);
			one.value = realValues;
			two.value = imaginaryValues;
		}


		//////////////////////////////////
		// Constructors and Destructors //
		//////////////////////////////////

		force_inline simd_float() noexcept : value(init(0.0f)) { }
		force_inline simd_float(simd_type initial_value) noexcept : value(initial_value) { }
		force_inline simd_float(simd_int initial_value) noexcept : value(std::bit_cast<simd_type>(initial_value.value)) { }
		force_inline simd_float(float initial_value) noexcept : value(init(initial_value)) { }
		force_inline simd_float(const std::array<float, kSize> &scalars) noexcept
			: value(std::bit_cast<simd_type>(scalars)) { }

		template<typename T> requires requires (T x) { (sizeof(float) * kSize) % sizeof(x) == 0; }
		force_inline simd_float(const std::array<T, (sizeof(float) *kSize) / sizeof(T)> &scalars) noexcept
			: value(std::bit_cast<simd_type>(scalars)) { }

		force_inline ~simd_float() noexcept = default;

		force_inline float vector_call access(size_t index) const noexcept {
			auto scalars = std::bit_cast<std::array<float, kSize>>(value);
			return scalars[index];
		}

		force_inline void vector_call set(size_t index, float newValue) noexcept
		{
			auto scalars = std::bit_cast<std::array<float, kSize>>(value);
			scalars[index] = newValue;
			value = std::bit_cast<simd_type>(scalars);
		}

		force_inline auto vector_call getArrayOfValues()
		{
			return std::bit_cast<std::array<float, kSize>, simd_type>(value);
		}

		template<typename T> requires requires (T x) { (sizeof(float) * kSize) % sizeof(x) == 0; }
		force_inline auto vector_call getArrayOfValues()
		{
			return std::bit_cast<std::array<T, ((sizeof(float) *kSize) / sizeof(T))>>(value);
		}

		///////////////
		// Operators //
		///////////////

		force_inline float vector_call operator[](size_t index) const noexcept
		{
			return access(index);
		}

		force_inline simd_float& vector_call operator+=(simd_float other) noexcept
		{
			value = add(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator-=(simd_float other) noexcept
		{
			value = sub(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator*=(simd_float other) noexcept
		{
			value = mul(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator/=(simd_float other) noexcept
		{
			value = div(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator&=(simd_mask other) noexcept
		{
			value = bitAnd(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator|=(simd_mask other) noexcept
		{
			value = bitOr(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator^=(simd_mask other) noexcept
		{
			value = bitXor(value, other.value);
			return *this;
		}

		force_inline simd_float& vector_call operator+=(simd_type other) noexcept
		{
			value = add(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator-=(simd_type other) noexcept
		{
			value = sub(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator*=(simd_type other) noexcept
		{
			value = mul(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator/=(simd_type other) noexcept
		{
			value = div(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator&=(mask_simd_type other) noexcept
		{
			value = bitAnd(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator|=(mask_simd_type other) noexcept
		{
			value = bitOr(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator^=(mask_simd_type other) noexcept
		{
			value = bitXor(value, other);
			return *this;
		}

		force_inline simd_float& vector_call operator+=(float scalar) noexcept
		{
			value = add(value, init(scalar));
			return *this;
		}

		force_inline simd_float& vector_call operator-=(float scalar) noexcept
		{
			value = sub(value, init(scalar));
			return *this;
		}

		force_inline simd_float& vector_call operator*=(float scalar) noexcept
		{
			value = mulScalar(value, scalar);
			return *this;
		}

		force_inline simd_float& vector_call operator/=(float scalar) noexcept
		{
			value = div(value, init(scalar));
			return *this;
		}

		// overloaded operator() does value shifting inside the simd package
		force_inline simd_float& vector_call operator()(simd_mask shiftMask) noexcept
		{
			value = shift(value, shiftMask);
			return *this;
		}


		force_inline simd_float vector_call operator+(simd_float other) const noexcept
		{
			return add(value, other.value);
		}

		force_inline simd_float vector_call operator-(simd_float other) const noexcept
		{
			return sub(value, other.value);
		}

		force_inline simd_float vector_call operator*(simd_float other) const noexcept
		{
			return mul(value, other.value);
		}

		force_inline simd_float vector_call operator/(simd_float other) const noexcept
		{
			return div(value, other.value);
		}

		force_inline simd_float vector_call operator*(float scalar) const noexcept
		{
			return mulScalar(value, scalar);
		}

		force_inline simd_float vector_call operator&(simd_mask other) const noexcept
		{
			return bitAnd(value, other.value);
		}

		force_inline simd_float vector_call operator|(simd_mask other) const noexcept
		{
			return bitOr(value, other.value);
		}

		force_inline simd_float vector_call operator^(simd_mask other) const noexcept
		{
			return bitXor(value, other.value);
		}

		force_inline simd_float vector_call operator-() const noexcept
		{
			return neg(value);
		}

		force_inline simd_float vector_call operator~() const noexcept
		{
			return bitNot(value);
		}

		force_inline float vector_call sum() const noexcept
		{
			return sum(value);
		}
	};

	template<typename T>
	concept SimdValue = std::same_as<T, simd_int> || std::same_as<T, simd_float>;

}

using namespace simd_values;