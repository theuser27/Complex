
// Created: 2021-07-14 15:26:35

#pragma once

#include "platform_definitions.hpp"
#include "satomi.hpp"
#include "stl_utils.hpp"

namespace Framework
{
  class FFT
  {
  public:
    FFT() = default;
    // in and out buffers need to be 2 * bits
    FFT(u32 minOrder, u32 maxOrder);
    ~FFT() noexcept;

    void extendFFTOrders(u32 newMinOrder, u32 newMaxOrder);

    void transformRealForward(u32 order, float *input, u32 channel) const noexcept;
    void transformRealInverse(u32 order, float *output, u32 channel) const noexcept;

    satomi::atomic<utils::pair<u32, u32>> orders{};

  private:
  #ifdef COMPLEX_INTEL_IPP
    // Intel IPP
    satomi::atomic<void **> ippSpecs_{};
    satomi::atomic<void *> buffer_{};

  #else
    // pffft
    satomi::atomic<void **> plans_{};
    satomi::atomic<float *> scratchBuffers_{};

  #endif
    // TODO: add vDSP FFT option

  };
}
