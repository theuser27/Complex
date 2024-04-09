/*
  ==============================================================================

    Spectrogram.cpp
    Created: 3 Feb 2023 6:36:09pm
    Author:  theuser27

  ==============================================================================
*/

#include "Framework/utils.h"
#include "Framework/spectral_support_functions.h"
#include "../LookAndFeel/Skin.h"
#include "../LookAndFeel/Shaders.h"
#include "OpenGlMultiQuad.h"
#include "Spectrogram.h"
#include "../Sections/BaseSection.h"
#include "Plugin/Complex.h"
#include "Plugin/Renderer.h"

namespace
{
  strict_inline void vector_call complexMagnitude(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    auto magnitude = simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary));
    simd_float zeroes = 0.0f;
  #if COMPLEX_SSE4_1
    one.value = _mm_unpacklo_ps(magnitude.value, zeroes.value);
    two.value = _mm_unpackhi_ps(magnitude.value, zeroes.value);
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
  }

  strict_inline simd_mask vector_call copyFromEven(simd_mask value)
  {
  #if COMPLEX_SSE4_1
    return _mm_shuffle_epi32(value.value, _MM_SHUFFLE(2, 2, 0, 0));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
  }

  strict_inline simd_float vector_call midSide(simd_float value)
  {
    simd_float half = value * 0.5f;
  #if COMPLEX_SSE4_1
    auto one = _mm_unpacklo_ps(value.value, half.value);
    auto two = _mm_unpackhi_ps(value.value, half.value);
    auto addSub = _mm_addsub_ps(one, two);
    auto result = _mm_shuffle_ps(addSub, addSub, _MM_SHUFFLE(2, 0, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif
    return simd_float::abs(result);
  }
}

namespace Interface
{
  Spectrogram::Spectrogram(String name) : OpenGlComponent(std::move(name))
  {
    amplitudeRenderers_.reserve(kNumChannels);
    for (size_t i = 0; i < kNumChannels; ++i)
    {
      amplitudeRenderers_.emplace_back(std::make_unique<OpenGlLineRenderer>(kResolution));
      amplitudeRenderers_[i]->setFill(true);
    }
  }

  Spectrogram::~Spectrogram() = default;

  void Spectrogram::resized()
  {
    Colour colour = getColour(Skin::kWidgetPrimary1);
    Colour fillColour = getColour(Skin::kWidgetPrimary2);

    float fillFade = 0.0f;
    if (auto container = container_.get())
      fillFade = container->getValue(Skin::kWidgetFillFade);

    for (auto &amplitudeRenderer : amplitudeRenderers_)
    {
      amplitudeRenderer->setLineWidth(1.8f);
      amplitudeRenderer->setFillCenter(-1.0f);
      amplitudeRenderer->setColour(colour);
      colour = colour.withMultipliedAlpha(0.5f);
      amplitudeRenderer->setFillColours(fillColour.withMultipliedAlpha(1.0f - fillFade), fillColour);
    }

    corners_.setBounds(getLocalBounds());
    corners_.setCorners(getLocalBounds(), getValue(Skin::kWidgetRoundedCorner));
  }

  bool Spectrogram::updateAmplitudes(bool shouldDisplayPhases, float minFrequency, float maxFrequency, float dbSlope)
  {
    static constexpr float kMinAmp = 0.000001f;
    static constexpr float kMinDecay = 0.12f;

    static const simd_mask oddMask = std::array{ 0U, kFullMask, 0U, kFullMask };

    auto bufferView = bufferView_.lock();
    if (bufferView.isEmpty())
    {
      bufferView_.unlock();
      return false;
    }

    bool isDataPolar = isDataPolar_;

    COMPLEX_ASSERT(scratchBuffer_.getSimdChannels() == bufferView.getSimdChannels()
      && "Scratch buffer doesn't match the number of channels in memory");

    float sampleHz = nyquistFreq_ / (float)binCount_;
    float startOctave = std::log2(minFrequency / sampleHz);
    float endOctave = std::log2(maxFrequency / sampleHz);
    float numOctaves = endOctave - startOctave;
    float maxBin = (float)binCount_ - 1.0f;
    float decayMultiplier = decayMultiplier_ * std::min((float)binCount_ / 2048.0f, 1.0f);
    bool isInterpolating = binCount_ > 1024;
    
    if (isDataPolar)
    {
      utils::ScopedLock g{ bufferView.getLock(), false, utils::WaitMechanism::Sleep };
      scratchBuffer_.copy(bufferView, 0, 0, binCount_);
    }
    else if (shouldDisplayPhases)
      utils::convertBufferLock<utils::complexCartToPolar>(bufferView, scratchBuffer_, binCount_, utils::WaitMechanism::Sleep);
    else
      utils::convertBufferLock<complexMagnitude>(bufferView, scratchBuffer_, binCount_, utils::WaitMechanism::Sleep);

    bufferView_.unlock();

    for (u32 i = 0; i < scratchBuffer_.getChannels(); i += scratchBuffer_.getRelativeSize())
    {
      float rangeBegin = std::exp2(startOctave);
      for (u32 j = 0; j < kResolution; ++j)
      {
        float t = (float)j / (kResolution - 1.0f);
        float octave = numOctaves * t + startOctave;
        float rangeEnd = std::min(std::exp2(octave), maxBin);

        int beginIndex = (int)rangeBegin;
        int endIndex = (int)rangeEnd;
        simd_float current = scratchBuffer_.readSimdValueAt(i, beginIndex);

        if (endIndex - beginIndex > 1)
        {
          // the point covers a range of bins, we need to find the loudest one
          for (int k = beginIndex + 1; k <= endIndex; ++k)
          {
            simd_float next = scratchBuffer_.readSimdValueAt(i, k);
            simd_mask mask = copyFromEven(simd_float::greaterThan(next, current));
            current = utils::maskLoad(current, next, mask);
          }
        }
        else
        {
          // the point is still on the same bin as in the previous iteration
          // or is entering the next one

          if (isInterpolating)
          {
            simd_float next = scratchBuffer_.readSimdValueAt(i, beginIndex + 1);
            current = utils::lerp(current, next, rangeBegin - (float)beginIndex);
          }
          else
          {
            if (endIndex - beginIndex != 1 && j > 0)
            {
              // if this is not the first iteration we can just copy the value from the previous one
              resultBuffer_.writeSimdValueAt(resultBuffer_.readSimdValueAt(i, j - 1), i, j);
              rangeBegin = rangeEnd;
              continue;
            }

            // the point is entering the next bin, calculate its value
            current = scratchBuffer_.readSimdValueAt(i, endIndex);
          }

        }
        rangeBegin = rangeEnd;

        simd_float magnitude = simd_float::max(kMinAmp, current / ((float)binCount_ * 2.0f));
        magnitude *= (float)utils::dbToAmplitude(t * numOctaves * dbSlope);
        simd_float oldValue = resultBuffer_.readSimdValueAt(i, j);
        simd_float db = utils::amplitudeToDb(simd_float::max(magnitude, oldValue));
        simd_float decay = simd_float::clamp(db * decayMultiplier, kMinDecay, 1.0f);
        magnitude = simd_float::max(kMinAmp, utils::lerp(oldValue, magnitude, decay));

        simd_float result = utils::maskLoad(magnitude, current, oddMask);
        resultBuffer_.writeSimdValueAt(result, i, j);
      }
    }

    return true;
  }

  void Spectrogram::init(OpenGlWrapper &openGl)
  {
    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->init(openGl);

    corners_.init(openGl);
  }

  void Spectrogram::render(OpenGlWrapper &openGl, bool animate)
  {
    auto &plugin = container_.get()->getRenderer()->getPlugin();
    nyquistFreq_ = plugin.getSampleRate() * 0.5f;
    binCount_ = plugin.getFFTSize() / 2;

    bool shouldDisplayPhases = shouldDisplayPhases_;
    float minFrequency = minFrequency_;
    float maxFrequency = maxFrequency_;
    float minDb = minDb_;
    float maxDb = maxDb_;
    float dbSlope = dbSlope_;
    
    if (!updateAmplitudes(shouldDisplayPhases, minFrequency, maxFrequency, dbSlope))
      return;

    float height = (float)getHeightSafe();
    float width = (float)getWidthSafe();
    float rangeMult = 1.0f / (maxDb - minDb);

    for (int i = 0; i < kResolution; ++i)
    {
      float t = (float)i / (kResolution - 1.0f);
      simd_float db = utils::amplitudeToDb(midSide(resultBuffer_.readSimdValueAt(0, i)));
      simd_float y = (db - minDb) * rangeMult;

      for (size_t j = 0; j < amplitudeRenderers_.size(); ++j)
      {
        amplitudeRenderers_[j]->setXAt(i, t * width);
        amplitudeRenderers_[j]->setYAt(i, height - y[j * 2] * height);
      }
    }

    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->render(openGl, this, getLocalBounds());

    corners_.render(openGl, animate);
  }

  void Spectrogram::destroy()
  {
    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->destroy();

    corners_.destroy();
  }

  void Spectrogram::paint(Graphics &g)
  {
    static constexpr int kLineSpacing = 10;

    if (!shouldPaintBackgroundLines_)
      return;

    float minFrequency = minFrequency_;
    float maxFrequency = maxFrequency_;

    int height = getHeight();
    float max_octave = log2f(maxFrequency / minFrequency);
    g.setColour(getColour(Skin::kLightenScreen).withMultipliedAlpha(0.5f));
    float frequency = 0.0f;
    float increment = 1.0f;

    int x = 0;
    while (frequency < maxFrequency)
    {
      for (int i = 0; i < kLineSpacing; ++i)
      {
        frequency += increment;
        float t = log2f(frequency / minFrequency) / max_octave;
        x = (int)std::round(t * (float)getWidth());
        g.fillRect(x, 0, 1, height);
      }
      g.fillRect(x, 0, 1, height);
      increment *= kLineSpacing;
    }
  }

}
