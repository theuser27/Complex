/*
  ==============================================================================

    fourier_transform.cpp
    Created: 13 Feb 2024 20:05:06
    Author:  theuser27

  ==============================================================================
*/

#include "fourier_transform.hpp"

#ifdef COMPLEX_INTEL_IPP
  #include "ipps.h"
#else
  #include "Third Party/pffft/pffft.h"
  #include "simd_values.hpp"
#endif


namespace Framework
{
#if COMPLEX_INTEL_IPP

  FFT::FFT(u32 minOrder, u32 maxOrder) : orders{ { minOrder, maxOrder } }
  {
    static constexpr int cachelLineAlignment = 64;

    COMPLEX_ASSERT(minOrder <= maxOrder);

    // full array is needed so that extending FFT orders works
    auto ippSpecs = (void **)ippsMalloc_8u((maxOrder + 1) * sizeof(void *));

    auto orderCount = maxOrder - minOrder + 1;
    // compute sizes for all specs and add padding so that the specBuffer is also 64-byte aligned just in case
    int *tempData = (int *)ippsMalloc_8u((orderCount * 4) * sizeof(int));
    int *specSizes = tempData;
    int *specSizesPadding = specSizes + orderCount;
    int *specBufferSizes = specSizesPadding + orderCount;
    int *specBufferSizesPadding = specBufferSizes + orderCount;
    int totalSize = 0;
    int maxBufferSize = 0;

    for (u32 i = 0; i < orderCount; ++i)
    {
      int bufferSize;
      ippsFFTGetSize_R_32f((int)(minOrder + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSizes[i], &specBufferSizes[i], &bufferSize);
      maxBufferSize = (maxBufferSize > bufferSize) ? maxBufferSize : bufferSize;

      specSizesPadding[i] = (cachelLineAlignment - (specSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
      totalSize += specSizes[i] + specSizesPadding[i];

      specBufferSizesPadding[i] = (cachelLineAlignment - (specBufferSizes[i] % cachelLineAlignment)) % cachelLineAlignment;
      totalSize += specBufferSizes[i] + specBufferSizesPadding[i];
    }

    totalSize += maxBufferSize;

    auto buffer = ippsMalloc_8u(totalSize);
    Ipp8u *rest = (Ipp8u *)buffer + maxBufferSize + (cachelLineAlignment - (maxBufferSize % cachelLineAlignment)) % cachelLineAlignment;

    for (u32 i = 0; i < orderCount; ++i)
    {
      Ipp8u *spec = rest;
      rest += specSizes[i] + specSizesPadding[i];
      Ipp8u *specBuffer = rest;
      rest += specBufferSizes[i] + specBufferSizesPadding[i];

      IppsFFTSpec_R_32f *plan = nullptr;
      ippsFFTInit_R_32f(&plan, (int)(minOrder + i), IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec, specBuffer);
      ippSpecs[minOrder + i] = plan;
    }

    ippsFree(tempData);

    ippSpecs_.store(ippSpecs, satomi::memory_order_relaxed);
    buffer_.store(buffer, satomi::memory_order_relaxed);
  }

  FFT::~FFT() noexcept
  {
    if (auto buffer = buffer_.load(satomi::memory_order_relaxed))
      ippsFree(buffer);
    if (auto ippSpecs = ippSpecs_.load(satomi::memory_order_relaxed))
      ippsFree(ippSpecs);
  }

  void FFT::transformRealForward(u32 order, float *input, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= orders.load(satomi::memory_order_relaxed).first);
    usize size = 1ULL << order;

    // zeroing out nyquist from previous transforms
    input[size] = 0.0f;
    ippsFFTFwd_RToCCS_32f_I(input, 
      (IppsFFTSpec_R_32f *)ippSpecs_.load(satomi::memory_order_acquire)[order], 
      (Ipp8u *)buffer_.load(satomi::memory_order_relaxed));
  }

  void FFT::transformRealInverse(u32 order, float *output, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= orders.load(satomi::memory_order_relaxed).first);
    usize size = 1ULL << order;

    // clearing out dc and nyquist imaginary parts since they shouldn't exist
    // but you don't know what might have happened during processing
    output[1] = 0.0f;
    output[size + 1] = 0.0f;
    ippsFFTInv_CCSToR_32f_I(output, 
      (IppsFFTSpec_R_32f *)ippSpecs_.load(satomi::memory_order_acquire)[order], 
      (Ipp8u *)buffer_.load(satomi::memory_order_relaxed));
  }

#else

  // pffft requires all inputs and outputs be aligned to the simd type at use
  // so we can safely use aligned loads and stores
  strict_inline simd_float vector_call toSimdFloat(const float *aligned) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_load_ps(aligned);
  #elif COMPLEX_NEON
    return vld1q_f32(aligned);
  #endif
  }

  strict_inline void vector_call fromSimdFloat(float *aligned, simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    _mm_store_ps(aligned, value.value);
  #elif COMPLEX_NEON
    vst1q_f32(aligned, value.value);
  #endif
  }

  FFT::FFT(u32 minOrder, u32 maxOrder) : orders{ { minOrder, maxOrder } }
  {
    // full array is needed so that extending FFT orders works
    auto plans = (void **)utils::allocate((maxOrder + 1) * sizeof(void *));

    for (usize i = minOrder; i < maxOrder + 1; ++i)
      plans[i] = pffft_new_setup(1 << i, PFFFT_REAL);

    plans_.store(plans, satomi::memory_order_relaxed);
    scratchBuffers_.store((float *)pffft_aligned_malloc((usize(1) << maxOrder) * sizeof(float)), 
      satomi::memory_order_relaxed);
  }

  FFT::~FFT() noexcept
  {
    auto [minOrder, maxOrder] = orders.load(satomi::memory_order_acquire);

    if (auto *plans = plans_.load(satomi::memory_order_relaxed))
    {
      for (usize i = minOrder; i < maxOrder + 1; ++i)
        if (plans[i])
          pffft_destroy_setup((PFFFT_Setup *)plans[i]);

      utils::deallocate(plans);
    }
    if (auto *scratch = scratchBuffers_.load(satomi::memory_order_relaxed))
      pffft_aligned_free(scratch);
  }

  void FFT::transformRealForward(u32 order, float *input, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= orders.load(satomi::memory_order_relaxed).first);
    usize size = 1ULL << order;

    auto plan = (PFFFT_Setup *)plans_.load(satomi::memory_order_acquire)[order];
    auto scratch = scratchBuffers_.load(satomi::memory_order_relaxed);

    // zeroing out nyquist from previous transforms
    input[size] = 0.0f;
    pffft_transform_ordered(plan, input, input, scratch, PFFFT_FORWARD);
  }

  void FFT::transformRealInverse(u32 order, float *output, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= orders.load(satomi::memory_order_relaxed).first);
    usize size = 1ULL << order;

    COMPLEX_ASSERT((uintptr_t)output % sizeof(simd_float) == 0 && "Output buffer is not aligned");
    simd_float scaling = 1.0f / (float)size;
    for (usize i = 0; i < size; i += simd_float::size)
      fromSimdFloat(output + i, toSimdFloat(output + i) * scaling);

    auto plan = (PFFFT_Setup *)plans_.load(satomi::memory_order_acquire)[order];
    auto scratch = scratchBuffers_.load(satomi::memory_order_relaxed);

    // separating dc and nyquist bins and cleaning accidental writes to nyquist imaginary part
    scratch[1] = 0.0f;
    scratch[size + 1] = 0.0f;
    pffft_transform_ordered(plan, output, output, scratch, PFFFT_BACKWARD);
  }

#endif

  void FFT::extendFFTOrders(u32 newMinOrder, u32 newMaxOrder)
  {
    auto [minOrder, maxOrder] = orders.load(satomi::memory_order_acquire);

    // just performing an atomic swap after creating the new plans and buffer
    if (newMinOrder >= minOrder && newMaxOrder <= maxOrder)
      return;

    FFT newFFT{ newMinOrder, newMaxOrder };

  #if COMPLEX_INTEL_IPP
    newFFT.buffer_.store(
      buffer_.exchange(
        newFFT.buffer_.load(satomi::memory_order_relaxed),
        satomi::memory_order_release),
      satomi::memory_order_relaxed
    );

    newFFT.ippSpecs_.store(
      ippSpecs_.exchange(
        newFFT.ippSpecs_.load(satomi::memory_order_relaxed),
        satomi::memory_order_release),
      satomi::memory_order_relaxed
    );
  #else
    newFFT.scratchBuffers_.store(
      scratchBuffers_.exchange(
        newFFT.scratchBuffers_.load(satomi::memory_order_relaxed),
        satomi::memory_order_release),
      satomi::memory_order_relaxed
    );

    newFFT.plans_.store(
      plans_.exchange(
        newFFT.plans_.load(satomi::memory_order_relaxed),
        satomi::memory_order_release),
      satomi::memory_order_relaxed
    );
  #endif

    newFFT.orders.store(
      orders.exchange(
        { newMinOrder, newMaxOrder },
        satomi::memory_order_release),
      satomi::memory_order_relaxed
    );
  }
}
