/*
  ==============================================================================

    Spectrogram.hpp
    Created: 3 Feb 2023 6:36:09pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/simd_buffer.hpp"
#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  class OpenGlLineRenderer;

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

    void reinitialise();

    bool render(OpenGlWrapper &openGl) override;
    void init(OpenGlWrapper &openGl);
    void destroy();

    bool mouseDown(const MouseEvent &e) override;

    utils::vector<OpenGlLineRenderer *> amplitudeRenderers_{};
    utils::vector<OpenGlLineRenderer *> phaseRenderers_{};

    Framework::SimdBuffer *scratchBuffer_{};
    Framework::SimdBuffer *resultBuffer_{};

    const Framework::SimdBuffer *bufferView_{};

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
    void setLineRendererData();
    bool updateAmplitudes(bool shouldDisplayPhases, float startDecade,
      float decadeCount, float decadeSlope);
  };
}
