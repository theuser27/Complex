/*
  ==============================================================================

    fourier_transform.cpp
    Created: 13 Feb 2024 8:05:06pm
    Author:  theuser27

  ==============================================================================
*/

#include "fourier_transform.hpp"

#ifdef INTEL_IPP
  #include "ipps.h"
#else
  #include "Third Party/pffft/pffft.h"
  #include "simd_values.hpp"
#endif

namespace Framework
{
#if INTEL_IPP

  FFT::FFT(u32 minOrder, u32 maxOrder) : minOrder_(minOrder), maxOrder_(maxOrder)
  {
    static constexpr int cachelLineAlignment = 64;

    auto orderCount = maxOrder_ - minOrder_ + 1;
    ippSpecs_ = new void *[orderCount];
    // compute sizes for all specs and add padding so that the specBuffer is also 64-byte aligned just in case
    int *tempData = new int[orderCount * 4];
    int *specSizes = tempData;
    int *specSizesPadding = specSizes + orderCount;
    int *specBufferSizes = specSizesPadding + orderCount;
    int *specBufferSizesPadding = specBufferSizes + orderCount;
    int totalSize = 0;
    int maxBufferSize = 0;

    for (u32 i = 0; i < orderCount; ++i)
    {
      int bufferSize;
      ippsFFTGetSize_R_32f((int)(minOrder_ + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSizes[i], &specBufferSizes[i], &bufferSize);
      maxBufferSize = (maxBufferSize > bufferSize) ? maxBufferSize : bufferSize;

      specSizesPadding[i] = (cachelLineAlignment - (specSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
      totalSize += specSizes[i] + specSizesPadding[i];

      specBufferSizesPadding[i] = (cachelLineAlignment - (specBufferSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
      totalSize += specBufferSizes[i] + specBufferSizesPadding[i];
    }

    totalSize += maxBufferSize;

    buffer_ = ippsMalloc_8u(totalSize);
    Ipp8u *rest = static_cast<Ipp8u *>(buffer_) + maxBufferSize + (cachelLineAlignment - (maxBufferSize % cachelLineAlignment)) % cachelLineAlignment;

    for (u32 i = 0; i < orderCount; ++i)
    {
      Ipp8u *spec = rest;
      rest += specSizes[i] + specSizesPadding[i];
      Ipp8u *specBuffer = rest;
      rest += specBufferSizes[i] + specBufferSizesPadding[i];

      IppsFFTSpec_R_32f *plan = nullptr;
      ippsFFTInit_R_32f(&plan, (int)(minOrder_ + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec, specBuffer);
      ippSpecs_[i] = plan;
    }

    delete[] tempData;
  }

  FFT::~FFT() noexcept
  {
    ippsFree(buffer_);
    delete[] ippSpecs_;
  }

  void FFT::transformRealForward(u32 order, float *input, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= minOrder_);
    size_t size = 1ULL << order;

    // zeroing out nyquist from previous transforms
    input[size] = 0.0f;
    ippsFFTFwd_RToCCS_32f_I(input, static_cast<IppsFFTSpec_R_32f *>(ippSpecs_[order - minOrder_]), static_cast<Ipp8u *>(buffer_));
  }

  void FFT::transformRealInverse(u32 order, float *output, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= minOrder_);
    size_t size = 1ULL << order;

    // clearing out dc and nyquist imaginary parts since they shouldn't exist
    // but you don't know what might have happened during processing
    output[1] = 0.0f;
    output[size + 1] = 0.0f;
    ippsFFTInv_CCSToR_32f_I(output, static_cast<IppsFFTSpec_R_32f *>(ippSpecs_[order - minOrder_]), static_cast<Ipp8u *>(buffer_));
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

  FFT::FFT(u32 minOrder, u32 maxOrder) : minOrder_(minOrder), maxOrder_(maxOrder)
  {
    auto orderCount = maxOrder_ - minOrder_ + 1;
    plans_ = new void *[orderCount];

    for (size_t i = 0; i < orderCount; ++i)
      plans_[i] = pffft_new_setup(1 << (minOrder_ + i), PFFFT_REAL);

    scratchBuffers_ = static_cast<float *>(pffft_aligned_malloc((size_t(1) << maxOrder_) * sizeof(float)));
  }

  FFT::~FFT() noexcept
  {
    for (size_t i = 0; i < maxOrder_ - minOrder_ + 1; ++i)
      pffft_destroy_setup(static_cast<PFFFT_Setup *>(plans_[i]));

    pffft_aligned_free(scratchBuffers_);
    delete[] plans_;
  }

  void FFT::transformRealForward(u32 order, float *input, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= minOrder_);
    size_t size = 1ULL << order;

    // zeroing out nyquist from previous transforms
    input[size] = 0.0f;
    pffft_transform_ordered((PFFFT_Setup *)plans_[order - minOrder_], input, input, scratchBuffers_, PFFFT_FORWARD);
  }

  void FFT::transformRealInverse(u32 order, float *output, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= minOrder_);
    size_t size = 1ULL << order;

    COMPLEX_ASSERT((uintptr_t)output % sizeof(simd_float) == 0 && "Output buffer is not aligned");
    simd_float scaling = 1.0f / (float)size;
    for (size_t i = 0; i < size; i += kSimdRatio)
      fromSimdFloat(output + i, toSimdFloat(output + i) * scaling);

    // separating dc and nyquist bins and cleaning accidental writes to nyquist imaginary part
    scratchBuffers_[1] = 0.0f;
    scratchBuffers_[size + 1] = 0.0f;
    pffft_transform_ordered((PFFFT_Setup *)plans_[order - minOrder_], output, output, scratchBuffers_, PFFFT_BACKWARD);
  }

#endif
}
