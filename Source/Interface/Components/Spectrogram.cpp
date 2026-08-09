
// Created: 2023-02-03 18:36:09

#include "Spectrogram.hpp"

#include "Framework/utils.hpp"
#include "Framework/simd_math.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Skin.hpp"

namespace
{
  #define CHECK_NAN(x) //if (simd_mask::anyMask(simd_float::isNan(x)) != 0) COMPLEX_DEBUG_TRAP();

  forceinline simd_float vectorcall clearNan(simd_float value) { return value & simd_mask{ ~kFloatMantissaMask }; }
  forceinline void vectorcall complexMagnitude(simd_float &one, simd_float &two)
  {
    static constexpr simd_mask exponentMask = kFloatExponentMask;

  #if COMPLEX_SSE4_1
    simd_float real = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(2, 0, 2, 0));
    simd_float imaginary = _mm_shuffle_ps(one.value, two.value, _MM_SHUFFLE(3, 1, 3, 1));
  #elif COMPLEX_NEON
    simd_float real = vuzp1q_f32(one.value, two.value);
    simd_float imaginary = vuzp2q_f32(one.value, two.value);
  #endif
    auto magnitude = simd_float::sqrt(simd_float::mulAdd(real * real, imaginary, imaginary));
    //CHECK_NAN(magnitude);
    magnitude = merge(magnitude, clearNan(magnitude), simd_float::isNan(magnitude));
    static constexpr simd_float zeroes = 0.0f;
  #if COMPLEX_SSE4_1
    one.value = _mm_unpacklo_ps(magnitude.value, zeroes.value);
    two.value = _mm_unpackhi_ps(magnitude.value, zeroes.value);
  #elif COMPLEX_NEON
    one.value = vzip1q_f32(magnitude.value, zeroes.value);
    two.value = vzip2q_f32(magnitude.value, zeroes.value);
  #endif
  }

  forceinline void vectorcall midSide(simd_float &one, simd_float &two)
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
    simd_float lowerOne = vzip1q_f32(one.value, one.value);
    simd_float upperOne = vzip2q_f32(one.value, one.value);
    simd_float addSubOne = lowerOne + (upperOne ^ kMinusPlus);

    simd_float lowerTwo = vzip1q_f32(two.value, two.value);
    simd_float upperTwo = vzip2q_f32(two.value, two.value);
    simd_float addSubTwo = lowerTwo + (upperTwo ^ kMinusPlus);
  #endif
    one = utils::groupOdd(addSubOne);
    two = utils::groupOdd(addSubTwo);
  }

  forceinline void vectorcall convert(simd_float &one, simd_float &two)
  {
    midSide(one, two);
    complexMagnitude(one, two);
  }
}

namespace Interface
{
  void Spectrogram::reinitialise()
  {
    componentFlags.clickable = true;
    componentFlags.clickableChildren = false;

    auto maxBinCount = getUiRelated()->plugin.state_->getMaxBinCount();

    if (!arena)
    {
      auto scratchBufferSize = utils::kChannelsPerInOut * maxBinCount * sizeof(simd_float);
      auto resultBufferSize = utils::kChannelsPerInOut * kResolution * sizeof(simd_float);
      arena = utils::bumpArena::createNested(parent->arena, (resultBufferSize + scratchBufferSize) + COMPLEX_MB(2));
    }
    utils::bumpArena::clear(arena);

    scratchBuffer = Framework::SimdBuffer::create(arena, utils::kChannelsPerInOut, maxBinCount);
    resultBuffer = Framework::SimdBuffer::create(arena, utils::kChannelsPerInOut, kResolution);
  }

  bool
  Spectrogram::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::leftButtonModifier))
      shouldInterpolateLines = !shouldInterpolateLines;

    return true;
  }

  bool
  Spectrogram::updateAmplitudes(float startDecade, float decadeCount, float decadeSlope)
  {
    using namespace utils;

    if (!bufferView->channels || !bufferView->size)
      return false;

    COMPLEX_ASSERT(scratchBuffer->getSimdChannels() == bufferView->getSimdChannels()
      && "Scratch buffer doesn't match the number of channels in memory");

    //u32 bufferPosition;
    {
      utils::ScopedLock g{ bufferView->dataLock, false, WaitMechanism::Sleep };
      Framework::applyToThisNoMask<utils::MathOperations::Assign>(scratchBuffer, bufferView,
        utils::min(scratchBuffer->channels, bufferView->channels), binCount);
      //bufferPosition = bufferView.getBufferPosition();
    }

    //CHECK_NAN(scratchBuffer->data[0]);
    // convert data to polar form
    if (shouldDisplayPhases)
    {
      // TODO: look into computing unwrapped phase
      // The cepstrum: A guide to processing, page 5
      // Echo removal by discrete generalized linear filtering, page 54
      //
      // linear phase component caused by normal oscillation can be subtracted by:
      // phase -= k2Pi / FFTSize * binIndex * sampleShift (samples since beginning)
      // might be able to do sampleShift = sampleShift % FFTSize?
      //
      //
      //
      // only for TESTING, don't use otherwise
      /*simd_float blockPhase = (float)((double)bufferPosition / (double)(binCount * 2));
      auto data = scratchBuffer.get();
      usize dataSize = scratchBuffer.getSize();

      for (usize i = 0; i < scratchBuffer.getSimdChannels(); i++)
      {
        auto dc = data[dataSize * i];
        // size - 1 to skip nyquist since it doesn't need to change
        for (usize j = 0; j < binCount - 1; j += 2)
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
      utils::convertBufferInPlace<::convert>(scratchBuffer, binCount);

    auto scratchBufferRaw = scratchBuffer->get();

    static constexpr simd_float defaultValue = { 0.001f, 0.0f };
    const float maxBin = (float)binCount - 1.0f;
    const bool isInterpolating = shouldInterpolateLines;
    const simd_float scalingFactor = { 0.5f / (float)binCount, 1.0f };
    const float height = (float)bounds.h;
    const float width = (float)bounds.w;
    const float rangeMult = 1.0f / (maxDb - minDb);

    // yes these are magic numbers, change at your own risk
    const simd_float decay = 0.25f - 0.02f * ::log2f(utils::max(1.0f, 2048.0f / (float)binCount - 1.0f));

    constexpr float resolution = 1.0f / (kResolution - 1.0f);
    const float rangeMultiplier = ::powf(10.0f, decadeCount * resolution);
    float rangeBegin = ::powf(10.0f, startDecade);
    float rangeEnd = rangeBegin;
    // we're adding the starting decade because the slope needs to be agnostic towards fft size
    const float slopeMultiplier = (float)dbToAmplitude(((decadeCount + startDecade) * decadeSlope) * resolution);
    float slope = 1.0f;

    auto resultBufferRaw = resultBuffer->get();

    // current/previous - current/previous bin
    simd_float currentBin, previousBin;
    u32 j, beginIndex, endIndex;

    auto findLargestInRange = [&]() // finds largest entries in the range
    {
      simd_int indices = beginIndex;
      // the point covers a range of bins, we need to find the loudest one
      for (u32 k = beginIndex + 1; k <= endIndex; ++k)
      {
        simd_float next = scratchBufferRaw[k];
        simd_mask mask = copyFromEven(simd_float::greaterThan(next, currentBin));
        indices = merge(indices, simd_int{ k }, mask);
        currentBin = merge(currentBin, next, mask);
      }
    };

    auto calculateAmplitude = [&]() // scales amplitudes and clamps
    {
      currentBin *= scalingFactor * simd_float{ slope, 1.0f };
      CHECK_NAN(resultBufferRaw[j]);
      simd_float amplitude = lerp(resultBufferRaw[j], currentBin, decay);
      amplitude = merge(amplitude, kFloatInf, simd_float::isNan(amplitude));
      currentBin = merge(amplitude, currentBin, kPhaseMask);
      resultBufferRaw[j] = currentBin;
      currentBin = merge(defaultValue, currentBin, copyFromEven(simd_float::greaterThan(currentBin, defaultValue)));
    };

    for (j = 0; j < kResolution; ++j)
    {
      if (isInterpolating)
      {
        beginIndex = (u32)::floorf(rangeBegin);
        endIndex = (u32)::floorf(rangeEnd);
        currentBin = scratchBufferRaw[beginIndex];
        CHECK_NAN(currentBin);

        if (endIndex - beginIndex <= 1)
        {
          simd_float lower = currentBin;
          u32 nextIndex = (u32)::ceilf(rangeBegin);
          simd_float upper = scratchBufferRaw[nextIndex];
          CHECK_NAN(upper);

          currentBin = dbToAmplitude(lerp(amplitudeToDb(lower), amplitudeToDb(upper), rangeBegin - (float)beginIndex));
          currentBin = merge(currentBin, circularLerpSymmetric(lower, upper, rangeBegin - (float)beginIndex, kPi), kPhaseMask);
        }
        else
          findLargestInRange();

        calculateAmplitude();
      }
      else
      {
        // the reason for rounding instead of flooring is because the dc bin is
        // halfway between the positive and negative frequencies and this gives us a half bin offset
        beginIndex = (u32)::roundf(rangeBegin);
        endIndex = (u32)::roundf(rangeEnd);

        if (endIndex - beginIndex <= 1)
        {
          // the point is still on the same bin as in the previous iteration
          // or is entering the next one

          if (endIndex - beginIndex != 1 && j > 0)
          {
            CHECK_NAN(currentBin);
            // this is not the first iteration, we can use the value from the previous one
            resultBufferRaw[j] = currentBin;
          }
          else
          {
            // the point is entering the next bin, calculate its value
            currentBin = scratchBufferRaw[endIndex];
            CHECK_NAN(currentBin);

            calculateAmplitude();
          }
        }
        else
        {
          currentBin = scratchBufferRaw[beginIndex];
          CHECK_NAN(currentBin);
          findLargestInRange();
          calculateAmplitude();
        }
      }

      // updating variables for next iteration
      previousBin = currentBin;
      rangeBegin = rangeEnd;
      rangeEnd = utils::min(rangeEnd * rangeMultiplier, maxBin);
      slope *= slopeMultiplier;

      float x = (float)j * resolution * width;
      simd_float amplitudeY = (amplitudeToDb(currentBin) - minDb) * rangeMult;
      for (usize k = 0; k < countof(lineData); ++k)
      {
        lineData[k][j][0] = x;
        lineData[k][j][1] = (1.0f - amplitudeY[k * 2]) * height;
        COMPLEX_ASSERT(lineData[k][j][1] == lineData[k][j][1]);
      }

      //simd_float phaseY = currentBin / k2Pi + 0.5f;
      //for (usize k = 0; k < phaseRenderers.size(); ++k)
      //{
      //  phaseRenderers[k]->setXAt((int)j, x);
      //  phaseRenderers[k]->setYAt((int)j, (1.0f - phaseY[k * 2 + 1]) * height);
      //}
    }

    return true;
  }

  static void paintBackground(Graphics &g, Rectangle<float> bounds,
    float minFrequency, float maxFrequency)
  {
    static constexpr int kLineSpacing = 10;

    float maxOctave = ::log10f(maxFrequency / minFrequency);
    float frequency = 0.0f;
    float increment = 1.0f;

    auto fillColour = getColour(Skin::kLightenScreen, Skin::kNone).withMultipliedAlpha(0.5f);
    nvgFillColor(g.context, fillColour);

    while (frequency < maxFrequency)
    {
      frequency = 0.0f;
      for (int i = 0; i < kLineSpacing; ++i)
      {
        frequency += increment;
        float t = ::log10f(frequency / minFrequency) / maxOctave;
        float x = ::roundf(t * bounds.w);
        if (x > 0.0f && x < bounds.w)
        {
          nvgBeginPath(g.context);
          nvgRect(g.context, x, 0.0f, 1.0f, bounds.h);
          nvgFill(g.context);
        }
      }

      increment *= kLineSpacing;
    }

    strokeRect(g.context, bounds, 1.0f, fillColour, getValue(Skin::kWidgetRoundedCorner, true, Skin::kNone));
    //nvgBeginPath(g.context);
    //nvgStrokeWidth(g.context, 1.0f);
    //nvgRoundedRect(g.context, 0.5f, 0.5f, bounds.w - 1.0f, bounds.h - 1.0f, getValue(Skin::kWidgetRoundedCorner, true));
    //nvgStroke(g.context);
  }

  hotreloadable void drawSpectrum(Graphics &g, Spectrogram *s)
  {
    COMPLEX_HOTRELOAD_CHECK(getUiRelated()->plugin.state_.get(), drawSpectrum, g, s);

    Colour colour = getColour(Skin::kWidgetPrimary1, s);
    Colour fillColour = getColour(Skin::kWidgetPrimary2, s);

    for (usize i = 0; i < countof(s->lineData); ++i)
    {
      nvgBeginPath(g);
      nvgMoveTo(g, s->lineData[i][0][0], s->lineData[i][0][1]);

      for (usize j = 1; j < countof(s->lineData[0]); ++j)
        nvgLineTo(g, s->lineData[i][j][0], s->lineData[i][j][1]);

      nvgStrokeWidth(g, 1.8f);
      nvgStrokePaint(g, nvgLinearGradient(g,
        0.0f, 0.0f, 0.0f, (float)s->bounds.h, colour, colour.withMultipliedAlpha(0.2f)));
      nvgStroke(g);
      colour = colour.withMultipliedAlpha(0.65f);
    }
  }

  bool
  Spectrogram::render(Graphics &g)
  {
    //fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kBody, this));

    if (shouldPaintBackgroundLines)
    {
      paintBackground(g, getLocalBounds().toFloat(), minFrequency, maxFrequency);
    }

    auto localBounds = getLocalBounds();
    auto &plugin = getUiRelated()->plugin;
    nyquistFreq = plugin.getSampleRate() * 0.5f;
    binCount = plugin.state_->getFFTSize() / 2;

    float decadeSlope = dbSlope * kOctaveToDecadeConversionMult;
    float sampleHz = nyquistFreq / (float)binCount;
    float startDecade = ::log10f(minFrequency / sampleHz);
    float decadeCount = ::log10f(maxFrequency / minFrequency);

    if (!updateAmplitudes(startDecade, decadeCount, decadeSlope))
      return true;

    drawSpectrum(g, this);

    return true;
  }

#undef CHECK_NAN
}
