/*
  ==============================================================================

    fourier_transform.hpp
    Created: 14 Jul 2021 3:26:35pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "platform_definitions.hpp"

namespace Framework
{
  class FFT
  {
  public:
    // in and out buffers need to be 2 * bits
    explicit FFT(u32 minOrder, u32 maxOrder);
    ~FFT() noexcept;

    void transformRealForward(u32 order, float *input, u32 channel) const noexcept;
    void transformRealInverse(u32 order, float *output, u32 channel) const noexcept;

  private:
  #ifdef INTEL_IPP
    // Intel IPP
    void **ippSpecs_;
    void *buffer_;

  #else
    // pffft
    void **plans_;
    float *scratchBuffers_;

  #endif
    // TODO: add vDSP FFT option

    u32 minOrder_;
    u32 maxOrder_;
  };
}
