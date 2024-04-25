/*
	==============================================================================

		fourier_transform.cpp
		Created: 13 Feb 2024 8:05:06pm
		Author:  theuser27

	==============================================================================
*/

#include "fourier_transform.h"

#ifdef INTEL_IPP
	#include "ipps.h"
#else
	#include "Third Party/pffft/pffft.h"
	#include "simd_values.h"
#endif

namespace Framework
{
#if INTEL_IPP

	FFT::FFT()
	{
		static constexpr int cachelLineAlignment = 64;

		// compute sizes for all specs and add padding so that the specBuffer is also 64-byte aligned just in case
		int specSizes[kMaxFFTOrder - kMinFFTOrder + 1];
		int specSizesPadding[kMaxFFTOrder - kMinFFTOrder + 1];
		int specBufferSizes[kMaxFFTOrder - kMinFFTOrder + 1];
		int specBufferSizesPadding[kMaxFFTOrder - kMinFFTOrder + 1];
		int totalSize = 0;
		int maxBufferSize = 0;

		for (u32 i = 0; i < kMaxFFTOrder - kMinFFTOrder + 1; ++i)
		{
			int bufferSize;
			ippsFFTGetSize_R_32f((int)(kMinFFTOrder + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSizes[i], &specBufferSizes[i], &bufferSize);
			maxBufferSize = (maxBufferSize > bufferSize) ? maxBufferSize : bufferSize;

			specSizesPadding[i] = (cachelLineAlignment - (specSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
			totalSize += specSizes[i] + specSizesPadding[i];

			specBufferSizesPadding[i] = (cachelLineAlignment - (specBufferSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
			totalSize += specBufferSizes[i] + specBufferSizesPadding[i];
		}

		totalSize += maxBufferSize;

		buffer_ = ippsMalloc_8u(totalSize);
		Ipp8u *rest = (Ipp8u *)buffer_ + maxBufferSize + (cachelLineAlignment - (maxBufferSize % cachelLineAlignment)) % cachelLineAlignment;

		for (u32 i = 0; i < kMaxFFTOrder - kMinFFTOrder + 1; ++i)
		{
			Ipp8u *spec = rest;
			rest += specSizes[i] + specSizesPadding[i];
			Ipp8u *specBuffer = rest;
			rest += specBufferSizes[i] + specBufferSizesPadding[i];
			ippsFFTInit_R_32f(reinterpret_cast<IppsFFTSpec_R_32f **>(&ippSpecs_[i]), (int)(kMinFFTOrder + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec, specBuffer);
		}
	}

	FFT::~FFT() noexcept
	{
		ippsFree(buffer_);
	}

	float *FFT::transformRealForward(size_t order, float *input, size_t) const noexcept
	{
		COMPLEX_ASSERT(order >= kMinFFTOrder);
		size_t size = 1ULL << order;

		input[size] = 0.0f;
		ippsFFTFwd_RToCCS_32f_I(input, (IppsFFTSpec_R_32f *)ippSpecs_[order - kMinFFTOrder], (Ipp8u *)buffer_);
		// putting the nyquist bin together with dc bin
		input[1] = input[size];
		input[size] = 0.0f;

		return input;
	}

	void FFT::transformRealInverse(size_t order, float *output, size_t) const noexcept
	{
		COMPLEX_ASSERT(order >= kMinFFTOrder);
		size_t size = 1ULL << order;

		// separating dc and nyquist bins
		output[size] = output[1];
		output[1] = 0.0f;
		output[size + 1] = 0.0f;
		ippsFFTInv_CCSToR_32f_I(output, (IppsFFTSpec_R_32f *)ippSpecs_[order - kMinFFTOrder], (Ipp8u *)buffer_);
	}

#else

	// pffft requires all inputs and outputs be aligned to the simd type at use
	// so we can safely use aligned loads and stores
	strict_inline simd_float vector_call toSimdFloat(const float *aligned) noexcept
	{
	#if COMPLEX_SSE4_1
		return _mm_load_ps(aligned);
	#elif COMPLEX_NEON
		static_assert(false, "not yet implemented");
	#endif
	}

	strict_inline void vector_call fromSimdFloat(float *aligned, simd_float value) noexcept
	{
	#if COMPLEX_SSE4_1
		_mm_store_ps(aligned, value.value);
	#elif COMPLEX_NEON
		static_assert(false, "not yet implemented");
	#endif
	}

	FFT::FFT()
	{
		for (size_t i = 0; i < kMaxFFTOrder - kMinFFTOrder + 1; ++i)
		{
			plans_[i] = pffft_new_setup(1 << (kMinFFTOrder + i), PFFFT_REAL);
		}

		scratchBuffers_ = (float *)pffft_aligned_malloc((size_t)kMaxFFTBufferLength * sizeof(float));
	}

	FFT::~FFT() noexcept
	{
		for (size_t i = 0; i < kMaxFFTOrder - kMinFFTOrder + 1; ++i)
		{
			pffft_destroy_setup((PFFFT_Setup *)plans_[i]);
		}

		pffft_aligned_free(scratchBuffers_);
	}

	float *FFT::transformRealForward(size_t order, float *input, size_t channel) const noexcept
	{
		COMPLEX_ASSERT(order >= kMinFFTOrder);
		COMPLEX_ASSERT(channel < kNumTotalChannels);
		size_t size = 1ULL << order;

		input[channel * kMaxFFTBufferLength + size] = 0.0f;
		pffft_transform_ordered((PFFFT_Setup *)plans_[order - kMinFFTOrder], input, input, scratchBuffers_, PFFFT_FORWARD);
		// putting the nyquist bin together with dc bin
		scratchBuffers_[channel * kMaxFFTBufferLength + 1] = scratchBuffers_[channel * kMaxFFTBufferLength + size];
		scratchBuffers_[channel * kMaxFFTBufferLength + size] = 0.0f;

		return input;
	}

	void FFT::transformRealInverse(size_t order, float *output, size_t channel) const noexcept
	{
		COMPLEX_ASSERT(order >= kMinFFTOrder);
		COMPLEX_ASSERT(channel < kNumTotalChannels);
		size_t size = 1ULL << order;

		COMPLEX_ASSERT((uintptr_t)output % sizeof(simd_float) == 0);
		simd_float scaling = 1.0f / (float)size;
		for (size_t i = 0; i < size; i += kSimdRatio)
			fromSimdFloat(output + i, toSimdFloat(output + i) * scaling);

		// separating dc and nyquist bins and cleaning accidental writes to nyquist imaginary part
		scratchBuffers_[channel * kMaxFFTBufferLength + size] = scratchBuffers_[channel * kMaxFFTBufferLength + 1];
		scratchBuffers_[channel * kMaxFFTBufferLength + 1] = 0.0f;
		scratchBuffers_[channel * kMaxFFTBufferLength + size + 1] = 0.0f;
		pffft_transform_ordered((PFFFT_Setup *)plans_[order - kMinFFTOrder], output, output, scratchBuffers_, PFFFT_BACKWARD);
	}

#endif
}
