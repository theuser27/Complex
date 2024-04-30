/*
  ==============================================================================

    Spectrogram.cpp
    Created: 3 Feb 2023 6:36:09pm
    Author:  theuser27

  ==============================================================================
*/

#include "Spectrogram.h"

#include "Framework/utils.h"
#include "Framework/spectral_support_functions.h"
#include "../LookAndFeel/Skin.h"
#include "OpenGlMultiQuad.h"
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

  template<auto function = [](simd_float &, simd_float &){}>
  strict_inline void vector_call midSide(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    auto lowerOne = _mm_unpacklo_ps(one.value, one.value);
    auto upperOne = _mm_unpackhi_ps(one.value, one.value);
    auto addSubOne = _mm_addsub_ps(lowerOne, upperOne);
    one.value = _mm_shuffle_ps(addSubOne, addSubOne, _MM_SHUFFLE(2, 0, 3, 1));

    auto lowerTwo = _mm_unpacklo_ps(two.value, two.value);
    auto upperTwo = _mm_unpackhi_ps(two.value, two.value);
    auto addSubTwo = _mm_addsub_ps(lowerTwo, upperTwo);
    two.value = _mm_shuffle_ps(addSubTwo, addSubTwo, _MM_SHUFFLE(2, 0, 3, 1));
  #elif COMPLEX_NEON
    static_assert(false, "not implemented yet");
  #endif

    function(one, two);
  }
}

namespace Interface
{
  Spectrogram::Spectrogram(String name) : OpenGlComponent(std::move(name))
  {
    setInterceptsMouseClicks(true, false);

    amplitudeRenderers_.reserve(kNumChannels);
    for (size_t i = 0; i < kNumChannels; ++i)
    {
      amplitudeRenderers_.emplace_back(std::make_unique<OpenGlLineRenderer>(kResolution));
      amplitudeRenderers_[i]->setFill(true);
    }

    phaseRenderers_.reserve(kNumChannels);
    for (size_t i = 0; i < kNumChannels / 2; ++i)
    {
      phaseRenderers_.emplace_back(std::make_unique<OpenGlLineRenderer>(kResolution));
      phaseRenderers_[i]->setFill(false);
    }

    corners_.setInterceptsMouseClicks(false, false);
    corners_.setTargetComponent(this);

    background_.setInterceptsMouseClicks(false, false);
    background_.setTargetComponent(this);
    background_.setPaintFunction([this](Graphics &g) { paintBackground(g); });
  }

  Spectrogram::~Spectrogram() = default;

  void Spectrogram::resized()
  {
    Colour colour = getColour(Skin::kWidgetPrimary1);
    Colour fillColour = getColour(Skin::kWidgetPrimary2);

    float fillFade = getValue(Skin::kWidgetFillFade);

    for (auto &amplitudeRenderer : amplitudeRenderers_)
    {
      amplitudeRenderer->setLineWidth(1.8f);
      amplitudeRenderer->setFillCenter(-1.0f);
      amplitudeRenderer->setColour(colour);
      colour = colour.withMultipliedAlpha(0.5f);
      amplitudeRenderer->setFillColours(fillColour.withMultipliedAlpha(1.0f - fillFade), fillColour);
    }

    colour = colour.withRotatedHue(-0.33f);

    for (auto &phaseRenderer : phaseRenderers_)
    {
      phaseRenderer->setLineWidth(1.5f);
      phaseRenderer->setFillCenter(-1.0f);
      phaseRenderer->setColour(colour);
      colour = colour.withMultipliedAlpha(0.5f);
    }

    corners_.setCorners(getLocalBounds(), getValue(Skin::kWidgetRoundedCorner));
    background_.redrawImage();
  }

  bool Spectrogram::updateAmplitudes(bool shouldDisplayPhases, float minFrequency, float maxFrequency, float dbSlope)
  {
    static constexpr float log2ToLog10 = 1.0f / gcem::log10(2.0f);
    static constexpr float kMinAmp = 0.00001f;
    static constexpr simd_float defaultValue = { kMinAmp, 0.0f };

    auto bufferView = bufferView_.lock();
    if (bufferView.isEmpty())
    {
      bufferView_.unlock();
      return false;
    }

    bool isDataPolar = isDataPolar_;

    COMPLEX_ASSERT(scratchBuffer_.getSimdChannels() == bufferView.getSimdChannels()
      && "Scratch buffer doesn't match the number of channels in memory");

    dbSlope *= log2ToLog10;
    float sampleHz = nyquistFreq_ / (float)binCount_;
    float startOctave = std::log10(minFrequency / sampleHz);
    float endOctave = std::log10(maxFrequency / sampleHz);
    float numOctaves = endOctave - startOctave;
    float maxBin = (float)binCount_ - 1.0f;
    bool isInterpolating = shouldInterpolateLines_;
    // yes these are magic numbers, change at own risk
    float decay = 0.25f + std::max(0.0f, 0.05f * std::log2(2048.0f / (float)binCount_ - 1.0f));
    simd_float scalingFactor = { 0.25f / (float)binCount_, 1.0f };
    
    {
      utils::ScopedLock g{ bufferView.getLock(), false, utils::WaitMechanism::Sleep };
      scratchBuffer_.copy(bufferView, 0, 0, binCount_);
    }

    bufferView_.unlock();

    if (!isDataPolar)
    {
      if (shouldDisplayPhases)
        utils::convertBufferInPlace<midSide<utils::complexCartToPolar>>(scratchBuffer_, binCount_);
      else
        utils::convertBufferInPlace<midSide<complexMagnitude>>(scratchBuffer_, binCount_);

      for (u32 i = 0; i < scratchBuffer_.getChannels(); i += scratchBuffer_.getRelativeSize())
      {
        simd_float dcAndNyquist = scratchBuffer_.readSimdValueAt(i, 0);
        simd_float zeroes = 0.0f;
        midSide(dcAndNyquist, zeroes);
        scratchBuffer_.writeSimdValueAt(simd_float::abs(dcAndNyquist), i, 0);
      }
    }

    for (u32 i = 0; i < scratchBuffer_.getChannels(); i += scratchBuffer_.getRelativeSize())
    { 
      float rangeBegin = std::pow(10.0f, startOctave);
      for (u32 j = 0; j < kResolution; ++j)
      {
        float t = (float)j / (kResolution - 1.0f);
        float octave = numOctaves * t + startOctave;
        float rangeEnd = std::min(std::pow(10.0f, octave), maxBin);

        simd_float current;

        if (isInterpolating)
        {
          int beginIndex = (int)std::floor(rangeBegin);
          int endIndex = (int)std::floor(rangeEnd);
          current = scratchBuffer_.readSimdValueAt(i, beginIndex);

          if (endIndex - beginIndex > 1)
          {
            // the point covers a range of bins, we need to find the loudest one
            for (int k = beginIndex + 1; k <= endIndex; ++k)
            {
              simd_float next = scratchBuffer_.readSimdValueAt(i, k);
              simd_mask mask = utils::copyFromEven(simd_float::greaterThan(next, current));
              current = utils::maskLoad(current, next, mask);
            }
          }
          else
          {
            simd_float temp = current;
            simd_float next = scratchBuffer_.readSimdValueAt(i, (int)std::ceil(rangeBegin));
            current = utils::dbToAmplitude(utils::lerp(utils::amplitudeToDb(temp), utils::amplitudeToDb(next), rangeBegin - (float)beginIndex));
            current = utils::maskLoad(current, utils::lerp(temp, next, rangeBegin - (float)beginIndex), utils::kPhaseMask);
          }

          rangeBegin = rangeEnd;
        }
        else
        {
          // the reason for rounding instead of flooring is because the dc bin is 
          // halfway between the positive and negative frequencies
          int beginIndex = (int)std::round(rangeBegin);
          int endIndex = (int)std::round(rangeEnd);
          rangeBegin = rangeEnd;
          current = scratchBuffer_.readSimdValueAt(i, beginIndex);

          if (endIndex - beginIndex > 1)
          {
            // the point covers a range of bins, we need to find the loudest one
            for (int k = beginIndex + 1; k <= endIndex; ++k)
            {
              simd_float next = scratchBuffer_.readSimdValueAt(i, k);
              simd_mask mask = utils::copyFromEven(simd_float::greaterThan(next, current));
              current = utils::maskLoad(current, next, mask);
            }
          }
          else
          {
            // the point is still on the same bin as in the previous iteration
            // or is entering the next one

            if (endIndex - beginIndex != 1 && j > 0)
            {
              // if this is not the first iteration we can just copy the value from the previous one
              resultBuffer_.writeSimdValueAt(resultBuffer_.readSimdValueAt(i, j - 1), i, j);
              continue;
            }

            // the point is entering the next bin, calculate its value
            current = scratchBuffer_.readSimdValueAt(i, endIndex);
          }
        }

        float slope = (float)utils::dbToAmplitude(t * numOctaves * dbSlope);
        current *= scalingFactor * simd_float{ slope, 1.0f };
        current = utils::lerp(resultBuffer_.readSimdValueAt(i, j), current, decay);

        current = utils::maskLoad(defaultValue, current, utils::copyFromEven(simd_float::greaterThan(current, kMinAmp)));
        resultBuffer_.writeSimdValueAt(current, i, j);
      }
    }

    return true;
  }

  void Spectrogram::init(OpenGlWrapper &openGl)
  {
    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->init(openGl);

    for (auto &phaseRenderer : phaseRenderers_)
      phaseRenderer->init(openGl);

    corners_.init(openGl);
    background_.init(openGl);
  }

  void Spectrogram::render(OpenGlWrapper &openGl, bool animate)
  {
    auto &plugin = container_.get()->getRenderer()->getPlugin();
    nyquistFreq_ = plugin.getSampleRate() * 0.5f;
    binCount_ = plugin.getFFTSize() / 2;

    background_.render(openGl, animate);

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

    auto setRendererValues = [&]<bool RenderPhase>()
    {
      for (int i = 0; i < kResolution; ++i)
      {
        float t = (float)i / (kResolution - 1.0f);
        float x = t * width;
        simd_float result = resultBuffer_.readSimdValueAt(0, i);
        simd_float db = utils::amplitudeToDb(result);
        simd_float phaseY, amplitudeY = (db - minDb) * rangeMult;

        if constexpr (RenderPhase)
        {
          phaseY = utils::modOnceSigned(result, kPi) / k2Pi + 0.5f;
        }

        for (size_t j = 0; j < amplitudeRenderers_.size(); ++j)
        {
          amplitudeRenderers_[j]->setXAt(i, x);
          amplitudeRenderers_[j]->setYAt(i, height - amplitudeY[j * 2] * height);
        }

        if constexpr (RenderPhase)
        {
          phaseRenderers_[0]->setXAt(i, x);
          phaseRenderers_[0]->setYAt(i, height - phaseY[3] * height);
        }
      }
    };

    if (shouldDisplayPhases)
      setRendererValues.template operator()<true>();
    else
      setRendererValues.template operator()<false>();

    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->render(openGl, this, getLocalBoundsSafe());

    if (shouldDisplayPhases)
      for (auto &phaseRenderer : phaseRenderers_)
        phaseRenderer->render(openGl, this, getLocalBoundsSafe());

    corners_.render(openGl, animate);
  }

  void Spectrogram::destroy()
  {
    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->destroy();

    for (auto &phaseRenderer : phaseRenderers_)
      phaseRenderer->destroy();

    corners_.destroy();
    background_.destroy();
  }

  void Spectrogram::paintBackground(Graphics &g) const
  {
    static constexpr int kLineSpacing = 10;

    if (!shouldPaintBackgroundLines_)
      return;

    float minFrequency = minFrequency_;
    float maxFrequency = maxFrequency_;

    float width = (float)getWidth();
    float height = (float)getHeight();
    float maxOctave = std::log10(maxFrequency / minFrequency);
    g.setColour(getColour(Skin::kLightenScreen).withMultipliedAlpha(0.5f));
    float frequency = 0.0f;
    float increment = 1.0f;

    while (frequency < maxFrequency)
    {
      frequency = 0.0f;
      for (int i = 0; i < kLineSpacing; ++i)
      {
        frequency += increment;
        float t = std::log10(frequency / minFrequency) / maxOctave;
        float x = std::round(t * width);
        if (x > 0.0f && x < width)
          g.fillRect(x, 0.0f, 1.0f, height);
      }

      increment *= kLineSpacing;
    }

    g.drawRoundedRectangle(0, 0, width, height, getValue(Skin::kWidgetRoundedCorner), 1.8f);
  }

}
