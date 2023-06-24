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
#include "Spectrogram.h"
#include "../Sections/BaseSection.h"
#include "../Sections/InterfaceEngineLink.h"

namespace Interface
{
  Spectrogram::Spectrogram() : OpenGlLineRenderer(kNumChannels * kResolution)
  {
    static constexpr float kDefaultAmp = 0.000001f;

    setFill(true);

    /*for (int i = 0; i < kAudioSize; ++i)
    {
      left_amps_[i] = kDefaultAmp;
      right_amps_[i] = kDefaultAmp;
    }*/

    addRoundedCorners();
  }

  void Spectrogram::updateAmplitudes()
  {
    /*
    static constexpr float kMinAmp = 0.000001f;
    static constexpr float kStartScaleAmp = 0.001f;
    static constexpr float kMinDecay = 0.06f;

    if (bufferView_.isEmpty())
      return;

    COMPLEX_ASSERT(scratchBuffer_ != nullptr && "Scratch buffer was not set for this Spectrogram");
    COMPLEX_ASSERT(scratchBuffer_->getSimdChannels() == bufferView_.getSimdChannels() 
      && "Scratch buffer doesn't match the # channels in memory");

    float sample_hz = nyquistFreq_ / effectiveFFTSize;
    float start_octave = log2f(min_frequency_ / sample_hz);
    float end_octave = log2f(max_frequency_ / sample_hz);
    float num_octaves = end_octave - start_octave;
    float last_bin = 0.0f;

    for (u32 i = 0; i < bufferView_.getChannels(); i += bufferView_.getRelativeSize())
    {
      if (isDataPolar_)
      {
        COMPLEX_ASSERT(false);
      }
      else
      {
        for (u32 j = 0; j < effectiveFFTSize; j += kComplexSimdRatio)
        {
          simd_float amplitudes = utils::complexMagnitude(bufferView_.readSimdValueAt(i, j), bufferView_.readSimdValueAt(i, j + 1), true);
          u32 writeIndex = j / kComplexSimdRatio;
          scratchBuffer_->writeSimdValueAt(amplitudes, i, writeIndex);

          if (displayPhases)
          {
            simd_float phases = utils::complexPhase(bufferView_.readSimdValueAt(i, j), bufferView_.readSimdValueAt(i, j + 1));
            scratchBuffer_->writeSimdValueAt(phases, j, writeIndex + effectiveFFTSize / 2);
          }
        }

        for (int i = 0; i < kResolution; ++i)
        {
          float t = i / (kResolution - 1.0f);
          float octave = num_octaves * t + start_octave;
          float bin = powf(2.0f, octave);

          int bin_index = last_bin;
          float bin_t = last_bin - bin_index;
          //auto values = 

          float prev_amplitude = std::abs(frequency_data[bin_index]);
          float next_amplitude = std::abs(frequency_data[bin_index + 1]);
          float amplitude = std::lerp(prev_amplitude, next_amplitude, bin_t);
          if (bin - last_bin > 1.0f)
          {
            for (int j = last_bin + 1; j < bin; ++j)
              amplitude = std::max(amplitude, std::abs(frequency_data[j]));
          }
          last_bin = bin;

          amplitude = std::max(kMinAmp, 2.0f * amplitude / kAudioSize);
          float db = utils::amplitudeToDb(std::max(amplitude, amps[i]) / kStartScaleAmp);
          db += octave * kDbSlopePerOctave;
          float decay = std::max(kMinDecay, std::min(1.0f, kDecayMult * db));
          amps[i] = std::max(kMinAmp, std::lerp(amps[i], amplitude, decay));
        }
      }


    }    

    bufferView_->readSamples(transform_buffer_, kAudioSize, offset, index);
    std::complex<float> *frequency_data = (std::complex<float>*)transform_buffer_;

    
    */
  }

  void Spectrogram::setLineValues(OpenGlWrapper &open_gl)
  {
    /*float *amps = index == 0 ? left_amps_ : right_amps_;
    float height = getHeight();
    float width = getWidth();
    float range_mult = 1.0f / (max_db_ - min_db_);
    float num_octaves = log2f(max_frequency_ / min_frequency_);

    for (int i = 0; i < kResolution; ++i)
    {
      float t = i / (kResolution - 1.0f);
      float db = utils::amplitudeToDb(amps[i]);
      db += t * num_octaves * kDbSlopePerOctave;
      float y = (db - min_db_) * range_mult;
      setXAt(i, t * width);
      setYAt(i, height - y * height);
    }
    OpenGlLineRenderer::render(open_gl, true);*/
  }

  void Spectrogram::render(OpenGlWrapper &open_gl, bool animate)
  {
    InterfaceEngineLink *synth_interface = findParentComponentOfClass<InterfaceEngineLink>();
    if (synth_interface == nullptr)
      return;

    nyquistFreq_ = synth_interface->getPlugin().getSampleRate() / 2.0f;

    setLineWidth(2.0f);
    setFillCenter(-1.0f);

    updateAmplitudes();

    Colour color = findColour(Skin::kWidgetPrimary1, true);
    setColor(color);
    Colour fill_color = findColour(Skin::kWidgetPrimary2, true);
    float fill_fade = 0.0f;
    if (parent_)
      fill_fade = parent_->findValue(Skin::kWidgetFillFade);
    setFillColors(fill_color.withMultipliedAlpha(1.0f - fill_fade), fill_color);
    setLineValues(open_gl);
    renderCorners(open_gl, animate);
  }

  void Spectrogram::paint(Graphics &g)
  {
    static constexpr int kLineSpacing = 10;

    OpenGlLineRenderer::paint(g);
    if (!paint_background_lines_)
      return;

    int height = getHeight();
    float max_octave = log2f(max_frequency_ / min_frequency_);
    g.setColour(findColour(Skin::kLightenScreen, true).withMultipliedAlpha(0.5f));
    float frequency = 0.0f;
    float increment = 1.0f;

    int x = 0;
    while (frequency < max_frequency_)
    {
      for (int i = 0; i < kLineSpacing; ++i)
      {
        frequency += increment;
        float t = log2f(frequency / min_frequency_) / max_octave;
        x = std::round(t * getWidth());
        g.fillRect(x, 0, 1, height);
      }
      g.fillRect(x, 0, 1, height);
      increment *= kLineSpacing;
    }
  }

}
