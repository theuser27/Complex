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
		FFT(int bits) : size_(1 << bits)
		{
			int spec_size = 0;
			int spec_buffer_size = 0;
			int buffer_size = 0;
			ippsFFTGetSize_R_32f(bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &spec_size, &spec_buffer_size, &buffer_size);

			spec_ = std::make_unique<Ipp8u[]>(spec_size);
			spec_buffer_ = std::make_unique<Ipp8u[]>(spec_buffer_size);
			buffer_ = std::make_unique<Ipp8u[]>(buffer_size);

			ippsFFTInit_R_32f(&ipp_specs_, bits, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, spec_.get(), spec_buffer_.get());
		}

		// src buffer needs to have exactly as many samples as FFT size
		perf_inline void transformRealForward(float *inOut)
		{
			inOut[size_] = 0.0f;
			ippsFFTFwd_RToCCS_32f_I((Ipp32f *)inOut, ipp_specs_, buffer_.get());
		}

		// src needs to be in the CCS format
		perf_inline void transformRealInverse(float *inOut)
		{
			ippsFFTInv_CCSToR_32f_I((Ipp32f *)inOut, ipp_specs_, buffer_.get());
		}

	private:
		int size_;
		IppsFFTSpec_R_32f *ipp_specs_;
		// TODO: allocate a single block of memory for all of these pointers
		std::unique_ptr<Ipp8u[]> spec_;
		std::unique_ptr<Ipp8u[]> spec_buffer_;
		std::unique_ptr<Ipp8u[]> buffer_;

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
