
// Created: 2024-02-13 20:05:06

#include "fourier_transform.hpp"
#include "memory.hpp"

#ifdef COMPLEX_INTEL_IPP
  #include "ipps.h"
#else
  #include "Third Party/pffft/pffft.h"
  #include "simd_values.hpp"
#endif


namespace Framework
{
#ifdef COMPLEX_INTEL_IPP

  void createFFTRoutines(FFT &instance, u32 minOrder, u32 maxOrder)
  {
    static constexpr int cachelLineAlignment = 64;

    COMPLEX_ASSERT(minOrder <= maxOrder);

    // full array is needed so that extending FFT orders works
    auto *ippSpecs = arranew(instance.arena, void *, maxOrder + 1);

    auto orderCount = maxOrder - minOrder + 1;
    // compute sizes for all specs and add padding so that the specBuffer is also 64-byte aligned just in case
    int *tempData = arranew(instance.arena, int, orderCount * 4);
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

    Ipp8u *buffer = arranew(instance.arena, Ipp8u, totalSize);
    Ipp8u *rest = buffer + maxBufferSize + (cachelLineAlignment - (maxBufferSize % cachelLineAlignment)) % cachelLineAlignment;

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

    utils::bumpArena::remove(tempData);

    instance.ippSpecs_.store(ippSpecs, satomi::memory_order_relaxed);
    instance.buffer_.store(buffer, satomi::memory_order_relaxed);
  }

  static void destroyFFTRoutines(FFT &instance)
  {
    if (auto buffer = instance.buffer_.load(satomi::memory_order_relaxed))
      utils::bumpArena::remove(buffer);
    if (auto ippSpecs = instance.ippSpecs_.load(satomi::memory_order_relaxed))
      utils::bumpArena::remove(ippSpecs);
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
  forceinline simd_float vectorcall toSimdFloat(const float *aligned) noexcept
  {
  #if COMPLEX_SSE4_1
    return _mm_load_ps(aligned);
  #elif COMPLEX_NEON
    return vld1q_f32(aligned);
  #endif
  }

  forceinline void vectorcall fromSimdFloat(float *aligned, simd_float value) noexcept
  {
  #if COMPLEX_SSE4_1
    _mm_store_ps(aligned, value.value);
  #elif COMPLEX_NEON
    vst1q_f32(aligned, value.value);
  #endif
  }

  static void createFFTRoutines(FFT &instance, u32 minOrder, u32 maxOrder)
  {
    // full array is needed so that extending FFT orders works
    auto *plans = arranew(instance.arena, void *, maxOrder + 1);

    for (usize i = minOrder; i < maxOrder + 1; ++i)
      plans[i] = pffft_new_setup(1 << i, PFFFT_REAL, 
        [](void *ud, usize size, usize alignment) -> void * { return utils::bumpArena::insert((utils::bumpArena *)ud, size, alignment); },
        [](void *, void *allocation) { utils::bumpArena::remove(allocation); },
        instance.arena);

    instance.plans_.store(plans, satomi::memory_order_relaxed);

    // buffer needs to be 16 byte aligned for sse/neon
    instance.scratchBuffers_.store((float *)instance.arena->insert(instance.arena, 
      ((usize(1) << maxOrder) * sizeof(float)), pffft_simd_size() * alignof(float), true),
      satomi::memory_order_relaxed);
  }

  static void destroyFFTRoutines(FFT &instance)
  {
    auto [minOrder, maxOrder] = instance.orders.load(satomi::memory_order_acquire);

    if (auto *plans = instance.plans_.load(satomi::memory_order_relaxed))
    {
      for (usize i = minOrder; i < maxOrder + 1; ++i)
        if (plans[i])
          pffft_destroy_setup((PFFFT_Setup *)plans[i]);

      utils::bumpArena::remove(plans);
    }
    if (auto *scratch = instance.scratchBuffers_.load(satomi::memory_order_relaxed))
      utils::bumpArena::remove(scratch);
  }

  void FFT::transformRealForward(u32 order, float *input, u32) const noexcept
  {
    COMPLEX_ASSERT(order >= orders.load(satomi::memory_order_relaxed).first);
    usize size = 1ULL << order;

    auto plan = (PFFFT_Setup *)plans_.load(satomi::memory_order_acquire)[order];
    auto scratch = scratchBuffers_.load(satomi::memory_order_relaxed);

    // zeroing out nyquist from previous transforms
    input[size] = 0.0f;
    input[size + 1] = 0.0f;
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

  FFT::~FFT() noexcept
  {
    destroyFFTRoutines(*this);
  }

  void FFT::extendFFTOrders(u32 newMinOrder, u32 newMaxOrder)
  {
    auto [minOrder, maxOrder] = orders.load(satomi::memory_order_acquire);

    // just performing an atomic swap after creating the new plans and buffer
    if (newMinOrder >= minOrder && newMaxOrder <= maxOrder)
      return;

    destroyFFTRoutines(*this);
    createFFTRoutines(*this, newMinOrder, newMaxOrder);

    orders.store({ newMinOrder, newMaxOrder }, satomi::memory_order_relaxed);
  }
}
