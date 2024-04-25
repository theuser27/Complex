/*
	==============================================================================

		fourier_transform.h
		Created: 14 Jul 2021 3:26:35pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "constants.h"

namespace Framework
{
	class FFT
	{
	public:
		// in and out buffers need to be 2 * bits
		explicit FFT();
		~FFT() noexcept;

		// src buffer needs to have exactly as many samples as FFT size
		float *transformRealForward(size_t order, float *input, size_t channel) const noexcept;

		// src needs to be in the CCS format
		void transformRealInverse(size_t order, float *output, size_t channel) const noexcept;

	private:
	#ifdef INTEL_IPP
		// Intel IPP
		void *ippSpecs_[kMaxFFTOrder - kMinFFTOrder + 1];
		void *buffer_;

	#else
		// pffft
		void *plans_[kMaxFFTOrder - kMinFFTOrder + 1];

		float *scratchBuffers_;

	#endif

		// TODO: add more FFT options
	};
}
