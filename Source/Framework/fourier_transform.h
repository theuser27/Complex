/*
	==============================================================================

		fourier_transform.h
		Created: 14 Jul 2021 3:26:35pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#ifdef INTEL_IPP
	#include "ipps.h"
#endif

namespace Framework
{
	class FFT
	{
	public:
		// in and out buffers need to be 2 * bits
		explicit FFT(int bits);
		~FFT() noexcept;

		// src buffer needs to have exactly as many samples as FFT size
		void transformRealForward(float *inOut) const noexcept;

		// src needs to be in the CCS format
		void transformRealInverse(float *inOut) const noexcept;

	private:
	#if INTEL_IPP
		int size_;
		IppsFFTSpec_R_32f *ippSpecs_;
		Ipp8u *memory_;
		Ipp8u *spec_;
		Ipp8u *specBuffer_;
		Ipp8u *buffer_;

	// TODO: add more FFT options
#else
	#error No FFT algorithm could be chosen
#endif
	};
}
