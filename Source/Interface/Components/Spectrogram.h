/*
	==============================================================================

		Spectrogram.h
		Created: 3 Feb 2023 6:36:09pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/simd_buffer.h"
#include "OpenGlLineRenderer.h"

namespace Interface
{
	class Spectrogram : public OpenGlLineRenderer
	{
	public:
		static constexpr int kResolution = 400;
		static constexpr float kDecayMult = 0.008f;
		static constexpr int kBits = 14;
		static constexpr int kAudioSize = 1 << kBits;
		static constexpr float kDefaultMaxDb = 0.0f;
		static constexpr float kDefaultMinDb = -80.0f;
		static constexpr float kDefaultMaxFrequency = 21000.0f;
		static constexpr float kDbSlopePerOctave = 3.0f;

		Spectrogram();
		~Spectrogram() override = default;

		void render(OpenGlWrapper &open_gl, bool animate) override;
		void paintBackground(Graphics &g) override;


		void paintBackgroundLines(bool paint) noexcept { paint_background_lines_ = paint; }
		void setShouldDisplayPhases(bool shouldDisplayPhases) noexcept { shouldDisplayPhases_ = shouldDisplayPhases; }

		void setMinFrequency(float frequency) noexcept { min_frequency_ = frequency; }
		void setMaxFrequency(float frequency) noexcept { max_frequency_ = frequency; }
		void setMinDb(float db) noexcept { min_db_ = db; }
		void setMaxDb(float db) noexcept { max_db_ = db; }
		void setSpectrumMemory(Framework::SimdBufferView<std::complex<float>, 
			simd_float> memory) noexcept { bufferView_ = std::move(memory); }
		void setScratchBuffer(Framework::SimdBuffer<std::complex<float>, 
			simd_float> *scratchBuffer) noexcept { scratchBuffer_ = scratchBuffer; }
		void setIsDataPolar(bool isDataPolar) noexcept { isDataPolar_ = isDataPolar; }


	private:
		void updateAmplitudes();
		void setLineValues(OpenGlWrapper &open_gl);

		Framework::SimdBufferView<std::complex<float>, simd_float> bufferView_{};
		Framework::SimdBuffer<std::complex<float>, simd_float> *scratchBuffer_ = nullptr;
		bool isDataPolar_ = false;

		float nyquistFreq_ = kDefaultSampleRate / 2.0;
		u32 effectiveFFTSize = 1 << (kDefaultFFTOrder - 1);
		float min_frequency_ = kMinFrequency;
		float max_frequency_ = kDefaultMaxFrequency;
		float min_db_ = kDefaultMinDb;
		float max_db_ = kDefaultMaxDb;
		bool paint_background_lines_ = true;
		bool shouldDisplayPhases_ = false;
		/*float transform_buffer_[2 * kAudioSize]{};
		float left_amps_[kAudioSize]{};
		float right_amps_[kAudioSize]{};*/

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrogram)
	};
}
