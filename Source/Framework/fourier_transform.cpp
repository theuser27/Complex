/*
	==============================================================================

		fourier_transform.cpp
		Created: 13 Feb 2024 8:05:06pm
		Author:  theuser27

	==============================================================================
*/

#include "fourier_transform.h"

namespace Framework
{
#if INTEL_IPP
	FFT::FFT(int bits) : size_(1 << bits)
	{
		int specSize = 0;
		int specBufferSize = 0;
		int bufferSize = 0;
		ippsFFTGetSize_R_32f(bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSize, &specBufferSize, &bufferSize);

		memory_ = new Ipp8u[specSize + specBufferSize + bufferSize];
		spec_ = memory_;
		specBuffer_ = &spec_[specSize];
		buffer_ = &specBuffer_[specBufferSize];

		ippsFFTInit_R_32f(&ippSpecs_, bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec_, specBuffer_);
	}

	FFT::~FFT() noexcept
	{
		delete[] memory_;
	}

	void FFT::transformRealForward(float *inOut) const noexcept
	{
		inOut[size_] = 0.0f;
		ippsFFTFwd_RToCCS_32f_I((Ipp32f *)inOut, ippSpecs_, buffer_);
		// putting the nyquist bin together with dc bin
		inOut[1] = inOut[size_];
		inOut[size_] = 0.0f;
	}

	void FFT::transformRealInverse(float *inOut) const noexcept
	{
		// separating dc and nyquist bins
		inOut[size_] = inOut[1];
		inOut[1] = 0.0f;
		inOut[size_ + 1] = 0.0f;
		ippsFFTInv_CCSToR_32f_I((Ipp32f *)inOut, ippSpecs_, buffer_);
	}
#endif
}
