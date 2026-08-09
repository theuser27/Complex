
// Created: 2023-02-03 18:36:09

#pragma once

#include "Framework/simd_buffer.hpp"
#include "../LookAndFeel/Component.hpp"

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

    void reinitialise();

    bool render(Graphics &g) override;

    bool mouseDown(const MouseEvent &e) override;

    Framework::SimdBuffer *scratchBuffer{};
    Framework::SimdBuffer *resultBuffer{};

    const Framework::SimdBuffer *bufferView{};

    float minFrequency = kDefaultMinFrequency;
    float maxFrequency = kDefaultMaxFrequency;
    float minDb = kDefaultMinDb;
    float maxDb = kDefaultMaxDb;
    float referencePhase = 0.0f;
    bool shouldDisplayPhases = false;
    bool shouldInterpolateLines = true;
    bool shouldPaintBackgroundLines = true;
    float decayMultiplier = kDecayMult;
    float dbSlope = kDbSlopePerOctave;

    float nyquistFreq = kDefaultSampleRate * 0.5f;
    u32 binCount = 0;

    float lineData[2][kResolution][2]{};

  private:
    bool updateAmplitudes(float startDecade, float decadeCount, float decadeSlope);
  };
}
