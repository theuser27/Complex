/*
  ==============================================================================

    Spectrogram.hpp
    Created: 3 Feb 2023 6:36:09pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/simd_buffer.hpp"
#include "OpenGlQuad.hpp"
#include "OpenGlImage.hpp"
#include "OpenGlLineRenderer.hpp"

namespace Interface
{
  class Spectrogram final : public OpenGlComponent
  {
  public:
    static constexpr int kResolution = 400;
    static constexpr float kDecayMult = 0.07f;
    static constexpr float kDefaultMaxDb = 0.0f;
    static constexpr float kDefaultMinDb = -50.0f;
    static constexpr float kDefaultMinFrequency = 10.7f;
    static constexpr float kDefaultMaxFrequency = 21000.0f;
    static constexpr float kDbSlopePerOctave = 3.0f;

    Spectrogram(String name = "Spectrogram");
    ~Spectrogram() override;

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl) override;
    void destroy() override;

    void resized() override;
    void mouseDown(const MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;

    void paintBackgroundLines(bool paint) noexcept { shouldPaintBackgroundLines_ = paint; }
    void setShouldDisplayPhases(bool shouldDisplayPhases) noexcept { shouldDisplayPhases_ = shouldDisplayPhases; }

    void setMinFrequency(float frequency) noexcept { minFrequency_ = frequency; }
    void setMaxFrequency(float frequency) noexcept { maxFrequency_ = frequency; }
    void setMinDb(float db) noexcept { minDb_ = db; }
    void setMaxDb(float db) noexcept { maxDb_ = db; }
    void setDecayMultiplier(float decay) noexcept { decayMultiplier_ = decay; }
    void setSlope(float slope) noexcept { dbSlope_ = slope; }
    void setSpectrumData(Framework::SimdBufferView<Framework::complex<float>,
      simd_float> data, bool isDataPolar) noexcept 
    {
      lastBufferVersion = data.getLock().versionFlag;
      bufferView_ = COMPLEX_MOV(data);
      isDataPolar_ = isDataPolar;
    }

    void setCornerColour(Colour colour) noexcept { corners_.setColor(colour); }
  private:
    bool updateAmplitudes(bool shouldDisplayPhases, float startDecade, 
      float decadeCount, float decadeSlope, float overlap);
    void paintBackground(Graphics &g) const;

    std::vector<std::unique_ptr<OpenGlLineRenderer>> amplitudeRenderers_{};
    std::vector<std::unique_ptr<OpenGlLineRenderer>> phaseRenderers_{};
    OpenGlCorners corners_;
    OpenGlImage background_;

    Framework::SimdBuffer<Framework::complex<float>, simd_float> scratchBuffer_{};
    Framework::SimdBuffer<Framework::complex<float>, simd_float> oldBuffer_{};
    Framework::SimdBuffer<Framework::complex<float>, simd_float> oldBuffer2_{};
    Framework::SimdBuffer<Framework::complex<float>, simd_float> resultBuffer_{ kChannelsPerInOut, kResolution };

    utils::shared_value_block<Framework::SimdBufferView<Framework::complex<float>, simd_float>> bufferView_{};
    utils::shared_value<bool> isDataPolar_ = false;
    utils::shared_value<u64> lastBufferVersion = 0;

    utils::shared_value<float> minFrequency_ = kDefaultMinFrequency;
    utils::shared_value<float> maxFrequency_ = kDefaultMaxFrequency;
    utils::shared_value<float> minDb_ = kDefaultMinDb;
    utils::shared_value<float> maxDb_ = kDefaultMaxDb;
    utils::shared_value<bool> shouldDisplayPhases_ = false;
    utils::shared_value<float> referencePhase_ = 0.0f;
    utils::shared_value<bool> shouldInterpolateLines_ = true;
    utils::shared_value<bool> shouldPaintBackgroundLines_ = true;
    utils::shared_value<float> decayMultiplier_ = kDecayMult;
    utils::shared_value<float> dbSlope_ = kDbSlopePerOctave;

    float nyquistFreq_ = kDefaultSampleRate * 0.5f;
    u32 binCount_ = 0;
  };
}
