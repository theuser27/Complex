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

    utils::vector<OpenGlLineRenderer *> amplitudeRenderers{};
    utils::vector<OpenGlLineRenderer *> phaseRenderers{};

    Framework::SimdBuffer *scratchBuffer{};
    Framework::SimdBuffer *resultBuffer{};

    const Framework::SimdBuffer *bufferView{};

    float minFrequency = kDefaultMinFrequency;
    float maxFrequency = kDefaultMaxFrequency;
    float minDb = kDefaultMinDb;
    float maxDb = kDefaultMaxDb;
    bool shouldDisplayPhases = false;
    float referencePhase = 0.0f;
    bool shouldInterpolateLines = true;
    bool shouldPaintBackgroundLines = true;
    float decayMultiplier = kDecayMult;
    float dbSlope = kDbSlopePerOctave;

    float nyquistFreq = kDefaultSampleRate * 0.5f;
    u32 binCount = 0;

  private:
    void setLineRendererData();
    bool updateAmplitudes(float startDecade, float decadeCount, float decadeSlope);
  };
}
