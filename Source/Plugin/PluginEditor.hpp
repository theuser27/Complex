/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Interface
{
  class MainInterface;
  class Renderer;

  class BorderBoundsConstrainer final : public juce::ComponentBoundsConstrainer
  {
  public:
    void checkBounds(juce::Rectangle<int> &bounds, const juce::Rectangle<int> &previous, const juce::Rectangle<int> &limits,
      bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) override;

    void resizeStart() override;
    void resizeEnd() override;

    void setGui(MainInterface *gui) noexcept { gui_ = gui; }
    void setRenderer(Renderer *renderer) noexcept { renderer_ = renderer; }

  protected:
    static std::pair<int, int> unscaleDimensions(int width, int height, double currentScaling) noexcept
    {
      return std::pair{ (int)std::round((double)width / currentScaling),
        (int)std::round((double)height / currentScaling) };
    }

    MainInterface *gui_ = nullptr;
    Renderer *renderer_ = nullptr;
  };
}

class ComplexAudioProcessor;

class ComplexAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
  ComplexAudioProcessorEditor(ComplexAudioProcessor &);
  ~ComplexAudioProcessorEditor() override;

  void paint (juce::Graphics &) override { }
  void resized() override;
  void setScaleFactor(float newScale) override;

private:
  Interface::Renderer &renderer_;
  Interface::BorderBoundsConstrainer constrainer_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplexAudioProcessorEditor)
};
