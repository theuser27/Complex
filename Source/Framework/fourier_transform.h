/*
	==============================================================================

		fourier_transform.h
		Created: 14 Jul 2021 3:26:35pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Framework
{
	#if INTEL_IPP
		#include "ipps.h"

	class FFT
	{
	public:
		// in and out buffers need to be 2 * bits
		explicit FFT(int bits) noexcept : size_(1 << bits)
		{
			int specSize = 0;
			int specBufferSize = 0;
			int bufferSize = 0;
			ippsFFTGetSize_R_32f(bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSize, &specBufferSize, &bufferSize);

			memory_ = std::make_unique<Ipp8u[]>(specSize + specBufferSize + bufferSize);
			spec_ = memory_.get();
			specBuffer_ = &spec_[specSize];
			buffer_ = &specBuffer_[specBufferSize];

			ippsFFTInit_R_32f(&ippSpecs_, bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec_, specBuffer_);
		}

		// src buffer needs to have exactly as many samples as FFT size
		void transformRealForward(float *inOut) const noexcept
		{
			inOut[size_] = 0.0f;
			ippsFFTFwd_RToCCS_32f_I((Ipp32f *)inOut, ippSpecs_, buffer_);
			// putting the nyquist bin together with dc bin
			inOut[1] = inOut[size_];
			inOut[size_] = 0.0f;
		}

		// src needs to be in the CCS format
		void transformRealInverse(float *inOut) const noexcept
		{
			// separating dc and nyquist bins
			inOut[size_] = inOut[1];
			inOut[1] = 0.0f;
			inOut[size_ + 1] = 0.0f;
			ippsFFTInv_CCSToR_32f_I((Ipp32f *)inOut, ippSpecs_, buffer_);
		}

	private:
		int size_;
		IppsFFTSpec_R_32f *ippSpecs_ = nullptr;
		std::unique_ptr<Ipp8u[]> memory_;
		Ipp8u* spec_;
		Ipp8u *specBuffer_;
		Ipp8u *buffer_;

		JUCE_LEAK_DETECTOR(FFT)
	};

#elif JUCE_MODULE_AVAILABLE_juce_dsp

	class FFT
	{
	public:
		// in and out buffers need to be 2 * bits
		FFT(int bits) : fft{ bits } { }

		// src buffer needs to have exactly as many samples as FFT size and dst buffer 2 * src
		void transformRealForward(float *src, float *dst) { fft.perform((juce::dsp::Complex<float>*)src, (juce::dsp::Complex<float>*)dst, false); }

		// src needs to be in the std::complex<float> format
		void transformRealInverse(float *src, float *dst) { fft.perform((juce::dsp::Complex<float>*)src, (juce::dsp::Complex<float>*)dst, true); }

	private:
		dsp::FFT fft;

		JUCE_LEAK_DETECTOR(FFT)
	};


	// TODO: add more FFT options

#endif
}
