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
  class Spectrogram final : public Component
  {
  public:
    static constexpr int kResolution = 400;
    static constexpr float kDecayMult = 0.07f;
    static constexpr float kDefaultMaxDb = 0.0f;
    static constexpr float kDefaultMinDb = -50.0f;
    static constexpr float kDefaultMinFrequency = 10.7f;
    static constexpr float kDefaultMaxFrequency = 21000.0f;
    static constexpr float kDbSlopePerOctave = 3.0f;

    Spectrogram();
    ~Spectrogram() noexcept override;

    void init(OpenGlWrapper &openGl) override;
    bool render(OpenGlWrapper &openGl) override;
    void destroy() override;

    void resized() override;
    void mouseDown(const MouseEvent &e) override;
    void mouseDrag(const MouseEvent &e) override;

    void setCornerColour(Colour colour) noexcept 
    { corners_.fragment.uniforms.colour = colour.getNormalisedRGBA(); }

    std::vector<utils::up<OpenGlLineRenderer>> amplitudeRenderers_{};
    std::vector<utils::up<OpenGlLineRenderer>> phaseRenderers_{};
    OpenGlCorners corners_;
    OpenGlImage background_;

    Framework::SimdBuffer<Framework::complex<float>, simd_float> scratchBuffer_{};
    Framework::SimdBuffer<Framework::complex<float>, simd_float> resultBuffer_{ kChannelsPerInOut, kResolution };

    Framework::SimdBufferView<Framework::complex<float>, simd_float> bufferView_{};

    float minFrequency_ = kDefaultMinFrequency;
    float maxFrequency_ = kDefaultMaxFrequency;
    float minDb_ = kDefaultMinDb;
    float maxDb_ = kDefaultMaxDb;
    bool shouldDisplayPhases_ = false;
    float referencePhase_ = 0.0f;
    bool shouldInterpolateLines_ = true;
    bool shouldPaintBackgroundLines_ = true;
    float decayMultiplier_ = kDecayMult;
    float dbSlope_ = kDbSlopePerOctave;

    float nyquistFreq_ = kDefaultSampleRate * 0.5f;
    u32 binCount_ = 0;

  private:
    bool updateAmplitudes(bool shouldDisplayPhases, float startDecade,
      float decadeCount, float decadeSlope);
    void paintBackground(Graphics &g) const;
  };
}
