/*
  ==============================================================================

    Spectrogram.cpp
    Created: 3 Feb 2023 6:36:09pm
    Author:  theuser27

  ==============================================================================
*/

#include "Spectrogram.hpp"

#include "Framework/utils.hpp"
#include "Framework/simd_math.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Skin.hpp"
#include "OpenGlQuad.hpp"
#include "../Sections/BaseSection.hpp"

namespace
{
  strict_inline void vector_call complexMagnitude(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    simd_float real = vuzp1q_f32(one.value, two.value);
    simd_float imaginary = vuzp2q_f32(one.value, two.value);
  #endif
    auto magnitude = simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary));
    static constexpr simd_float zeroes = 0.0f;
  #if COMPLEX_SSE4_1
    one.value = _mm_unpacklo_ps(magnitude.value, zeroes.value);
    two.value = _mm_unpackhi_ps(magnitude.value, zeroes.value);
  #elif COMPLEX_NEON
    one.value = vzip1q_f32(magnitude.value, zeroes.value);
    two.value = vzip2q_f32(magnitude.value, zeroes.value);
  #endif
  }

  template<auto function = [](simd_float &, simd_float &) { }>
  strict_inline void vector_call midSide(simd_float &one, simd_float &two)
  {
  #if COMPLEX_SSE4_1
    auto lowerOne = _mm_unpacklo_ps(one.value, one.value);
    auto upperOne = _mm_unpackhi_ps(one.value, one.value);
    auto addSubOne = _mm_addsub_ps(lowerOne, upperOne);

    auto lowerTwo = _mm_unpacklo_ps(two.value, two.value);
    auto upperTwo = _mm_unpackhi_ps(two.value, two.value);
    auto addSubTwo = _mm_addsub_ps(lowerTwo, upperTwo);
  #elif COMPLEX_NEON
    static constexpr simd_mask kMinusPlus = { kSignMask, 0U };
    auto lowerOne = vzip1q_f32(one.value, one.value);
    auto upperOne = vzip2q_f32(one.value, one.value);
    auto addSubOne = lowerOne + (upperOne ^ kMinusPlus);

    auto lowerTwo = vzip1q_f32(two.value, two.value);
    auto upperTwo = vzip2q_f32(two.value, two.value);
    auto addSubTwo = lowerTwo + (upperTwo ^ kMinusPlus);
  #endif
    one = utils::groupOdd(addSubOne);
    two = utils::groupOdd(addSubTwo);

    function(one, two);
  }
}

namespace Interface
{
  Spectrogram::Spectrogram(String name) : OpenGlComponent{ COMPLEX_MOVE(name) }
  {
    //Logger::setCurrentLogger(&log_);
    //Logger::writeToLog("position,1,2,3,4");
    setInterceptsMouseClicks(true, false);

    auto maxBinCount = uiRelated.renderer->getPlugin().getMaxBinCount();
    scratchBuffer_.reserve(kChannelsPerInOut, maxBinCount);

    amplitudeRenderers_.reserve(kChannelsPerInOut);
    for (size_t i = 0; i < kChannelsPerInOut; ++i)
    {
      amplitudeRenderers_.emplace_back(utils::up<OpenGlLineRenderer>::create(kResolution));
      amplitudeRenderers_[i]->setFill(true);
    }

    phaseRenderers_.reserve(kChannelsPerInOut);
    for (size_t i = 0; i < kChannelsPerInOut / 2; ++i)
    {
      phaseRenderers_.emplace_back(utils::up<OpenGlLineRenderer>::create(kResolution));
      phaseRenderers_[i]->setFill(false);
    }

    corners_.setInterceptsMouseClicks(false, false);
    corners_.setTargetComponent(this);

    background_.setInterceptsMouseClicks(false, false);
    background_.setTargetComponent(this);
    background_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>) { paintBackground(g); });
  }

  Spectrogram::~Spectrogram() { Logger::setCurrentLogger(nullptr); destroy(); }

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

  void Spectrogram::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isLeftButtonDown())
      shouldInterpolateLines_ = !shouldInterpolateLines_.get();
  }

  void Spectrogram::mouseDrag(const juce::MouseEvent &)
  {
    //if (e.mods.isRightButtonDown())

  }

  bool Spectrogram::updateAmplitudes(bool shouldDisplayPhases,
    float startDecade, float decadeCount, float decadeSlope)
  {
    using namespace utils;

    auto &bufferView = bufferView_.lock();
    if (bufferView.isEmpty())
    {
      bufferView_.unlock();
      return false;
    }

    COMPLEX_ASSERT(scratchBuffer_.getSimdChannels() == bufferView.getSimdChannels()
      && "Scratch buffer doesn't match the number of channels in memory");

    u32 bufferPosition;
    {
      utils::ScopedLock g{ bufferView.getLock(), false, WaitMechanism::Sleep };
      scratchBuffer_.copy(bufferView, 0, 0, binCount_);
      bufferPosition = bufferView.getBufferPosition();
    }

    bufferView_.unlock();

    // convert data to polar form
    if (shouldDisplayPhases)
    {
      // only for TESTING, don't use otherwise
      /*simd_float blockPhase = (float)((double)bufferPosition / (double)(binCount_ * 2));
      auto data = scratchBuffer_.get();
      usize dataSize = scratchBuffer_.getSize();

      for (usize i = 0; i < scratchBuffer_.getSimdChannels(); i++)
      {
        auto dc = data[dataSize * i];
        // size - 1 to skip nyquist since it doesn't need to change
        for (usize j = 0; j < binCount_ - 1; j += 2)
        {
          simd_float one = data[dataSize * i + j];
          simd_float two = data[dataSize * i + j + 1];
          ::midSide<complexCartToPolar>(one, two);
          //data[dataSize * i + j] = one;
          //data[dataSize * i + j + 1] = two;
          simd_float offsetOne = (float)j * blockPhase * k2Pi;
          simd_float offsetTwo = (float)(j + 1) * blockPhase * k2Pi;
          data[dataSize * i + j] = merge(one, modSymmetric(one - offsetOne, kPi), kPhaseMask);
          data[dataSize * i + j + 1] = merge(two, modSymmetric(two - offsetTwo, kPi), kPhaseMask);
        }
        data[dataSize * i] = dc;
      }

      if (previosPosition != bufferPosition)
      {
        String string{};
        string.preallocateBytes(128);
        string << (int)bufferPosition << ',';
        for (usize i = 0; i < 4; ++i)
        {
          string << (data[i + 1][1] - previousData[i]) << ',';
          //string << data[i + 1][1] << ',';
          previousData[i] = data[i + 1][1];
        }
        string = string.dropLastCharacters(1);
        Logger::writeToLog(string);
        previosPosition = bufferPosition;
      }*/
    }
    else
      utils::convertBufferInPlace<::midSide<::complexMagnitude>>(scratchBuffer_, binCount_);
    
    auto scratchBufferRaw = scratchBuffer_.get();

    // handle dc and nyquist separately because the conversion functions wrote garbage there
    {
      simd_float dc = scratchBufferRaw[0];
      simd_float nyquist = scratchBufferRaw[binCount_ - 1];
      ::midSide(dc, nyquist);
      scratchBufferRaw[0] = simd_float::abs(dc);
      scratchBufferRaw[binCount_ - 1] = simd_float::abs(nyquist);
    }

    static constexpr simd_float defaultValue = { 0.001f, 0.0f };
    const float maxBin = (float)binCount_ - 1.0f;
    const bool isInterpolating = shouldInterpolateLines_;
    const simd_float scalingFactor = { 0.5f / (float)binCount_, 1.0f };
    const float minDb = minDb_;
    const float maxDb = maxDb_;
    const float height = (float)getHeightSafe();
    const float width = (float)getWidthSafe();
    const float rangeMult = 1.0f / (maxDb - minDb);

    // yes these are magic numbers, change at your own risk
    const float decay = 0.25f + std::max(0.0f, 0.05f * std::log2(2048.0f / (float)binCount_ - 1.0f));

    constexpr float resolution = 1.0f / (kResolution - 1.0f);
    const float rangeMultiplier = std::pow(10.0f, decadeCount * resolution);
    float rangeBegin = std::pow(10.0f, startDecade);
    float rangeEnd = rangeBegin;
    // we're adding the starting decade because the slope needs to be agnostic towards fft size
    const float slopeMultiplier = (float)dbToAmplitude(((decadeCount + startDecade) * decadeSlope) * resolution);
    float slope = 1.0f;

    auto resultBufferRaw = resultBuffer_.get();
    
    // current/previous - current/previous bin
    simd_float current, previous;
    u32 j, beginIndex, endIndex;

    auto findLargestInRange = [&]() // finds largest entries in the range
    {
      simd_int indices = beginIndex;
      // the point covers a range of bins, we need to find the loudest one
      for (u32 k = beginIndex + 1; k <= endIndex; ++k)
      {
        simd_float next = scratchBufferRaw[k];
        simd_mask mask = copyFromEven(simd_float::greaterThan(next, current));
        indices = merge(indices, simd_int{ k }, mask);
        current = merge(current, next, mask);
      }
    };

    auto calculateAmplitude = [&]() // scales amplitudes and clamps
    {
      current *= scalingFactor * simd_float{ slope, 1.0f };
      simd_float amplitude = lerp(resultBufferRaw[j], current, decay);
      current = merge(amplitude, current, kPhaseMask);
      resultBufferRaw[j] = current;
      current = merge(defaultValue, current, copyFromEven(simd_float::greaterThan(current, defaultValue)));
    };

    for (j = 0; j < kResolution; ++j)
    {
      if (isInterpolating)
      {
        beginIndex = (u32)std::floor(rangeBegin);
        endIndex = (u32)std::floor(rangeEnd);
        current = scratchBufferRaw[beginIndex];

        if (endIndex - beginIndex <= 1)
        {
          simd_float lower = current;
          u32 nextIndex = (u32)std::ceil(rangeBegin);
          simd_float upper = scratchBufferRaw[nextIndex];

          current = dbToAmplitude(lerp(amplitudeToDb(lower), amplitudeToDb(upper), rangeBegin - (float)beginIndex));
          current = merge(current, circularLerpSymmetric(lower, upper, rangeBegin - (float)beginIndex, kPi), kPhaseMask);
        }
        else
          findLargestInRange();

        calculateAmplitude();
      }
      else
      {
        // the reason for rounding instead of flooring is because the dc bin is 
        // halfway between the positive and negative frequencies and this gives us a half bin offset
        beginIndex = (u32)std::round(rangeBegin);
        endIndex = (u32)std::round(rangeEnd);

        if (endIndex - beginIndex <= 1)
        {
          // the point is still on the same bin as in the previous iteration
          // or is entering the next one

          if (endIndex - beginIndex != 1 && j > 0)
          {
            // this is not the first iteration, we can use the value from the previous one
            resultBufferRaw[j] = current;
          }
          else
          {
            // the point is entering the next bin, calculate its value
            current = scratchBufferRaw[endIndex];

            calculateAmplitude();
          }
        }
        else
        {
          current = scratchBufferRaw[beginIndex];
          findLargestInRange();
          calculateAmplitude();
        }
      }

      // updating variables for next iteration
      previous = current;
      rangeBegin = rangeEnd;
      rangeEnd = std::min(rangeEnd * rangeMultiplier, maxBin);
      slope *= slopeMultiplier;

      float x = (float)j * resolution * width;
      simd_float amplitudeY = (amplitudeToDb(current) - minDb) * rangeMult;
      for (size_t k = 0; k < amplitudeRenderers_.size(); ++k)
      {
        amplitudeRenderers_[k]->setXAt((int)j, x);
        amplitudeRenderers_[k]->setYAt((int)j, height - amplitudeY[k * 2] * height);
      }

      simd_float phaseY = current / k2Pi + 0.5f;
      for (size_t k = 0; k < phaseRenderers_.size(); ++k)
      {
        phaseRenderers_[k]->setXAt((int)j, x);
        phaseRenderers_[k]->setYAt((int)j, height - phaseY[k * 2 + 1] * height);
      }
    }

    return true;
  }

  void Spectrogram::init(OpenGlWrapper &openGl)
  {
    COMPLEX_ASSERT(!isInitialised_.load(std::memory_order_acquire), "Init method more than once");

    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->init(openGl);

    for (auto &phaseRenderer : phaseRenderers_)
      phaseRenderer->init(openGl);

    corners_.init(openGl);
    background_.init(openGl);

    isInitialised_.store(true, std::memory_order_release);
  }

  void Spectrogram::render(OpenGlWrapper &openGl)
  {
    static constexpr float octaveToDecade = 1.0f / gcem::log10(2.0f);

    {
      ScopedBoundsEmplace b{ openGl.parentStack, this };
      background_.render(openGl);
    }

    auto bounds = getLocalBoundsSafe();
    auto &plugin = uiRelated.renderer->getPlugin();
    nyquistFreq_ = plugin.getSampleRate() * 0.5f;
    binCount_ = plugin.getFFTSize() / 2;

    bool shouldDisplayPhases = false;
    float minFrequency = minFrequency_;
    float maxFrequency = maxFrequency_;

    float decadeSlope = dbSlope_ * octaveToDecade;
    float sampleHz = nyquistFreq_ / (float)binCount_;
    float startDecade = std::log10(minFrequency / sampleHz);
    float decadeCount = std::log10(maxFrequency / minFrequency);

    if (!updateAmplitudes(shouldDisplayPhases, startDecade, decadeCount, decadeSlope))
      return;

    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->render(openGl, *this, bounds);

    if (shouldDisplayPhases)
      for (auto &phaseRenderer : phaseRenderers_)
        phaseRenderer->render(openGl, *this, bounds);

    {
      ScopedBoundsEmplace b{ openGl.parentStack, this };
      corners_.render(openGl);
    }
  }

  void Spectrogram::destroy()
  {
    if (!isInitialised_.load(std::memory_order_acquire))
      return;

    for (auto &amplitudeRenderer : amplitudeRenderers_)
      amplitudeRenderer->destroy(*uiRelated.renderer);

    for (auto &phaseRenderer : phaseRenderers_)
      phaseRenderer->destroy(*uiRelated.renderer);

    corners_.destroy();
    background_.destroy();

    isInitialised_.store(false, std::memory_order_release);
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
