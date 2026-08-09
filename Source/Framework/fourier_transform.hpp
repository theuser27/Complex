
// Created: 2021-07-14 15:26:35

#pragma once

#include "platform.hpp"
#include "satomi.hpp"
#include "stl_utils.hpp"

namespace utils
{
  struct bumpArena;
}

namespace Framework
{
  struct FFT
  {
    FFT() = default;
    ~FFT() noexcept;

    void extendFFTOrders(u32 newMinOrder, u32 newMaxOrder);

    void transformRealForward(u32 order, float *input, u32 channel) const noexcept;
    void transformRealInverse(u32 order, float *output, u32 channel) const noexcept;

    // TODO: why are these even atomics 
    // if a single instance of this struct can't be used by multiple states??
    satomi::atomic<utils::pair<u32, u32>> orders{};
    utils::bumpArena *arena{};

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
