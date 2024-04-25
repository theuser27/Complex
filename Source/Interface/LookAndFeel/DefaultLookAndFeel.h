/*
  ==============================================================================

    DefaultLookAndFeel.h
    Created: 16 Nov 2022 6:49:15am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Interface
{
  class DefaultLookAndFeel final : public juce::LookAndFeel_V4 {
  public:
    static constexpr int kPopupMenuBorder = 4;

    ~DefaultLookAndFeel() override = default;

    int getPopupMenuBorderSize() override { return kPopupMenuBorder; }

    void drawTextEditorOutline([[maybe_unused]] juce::Graphics &g, [[maybe_unused]] int width, 
      [[maybe_unused]] int height, [[maybe_unused]] juce::TextEditor &textEditor) override { }
    void fillTextEditorBackground([[maybe_unused]] juce::Graphics &g, [[maybe_unused]] int width,
      [[maybe_unused]] int height, [[maybe_unused]] juce::TextEditor &textEditor) override { }
    void drawPopupMenuBackground(juce::Graphics &g, int width, int height) override;

    void drawScrollbar(juce::Graphics &g, juce::ScrollBar& scrollBar, int x, int y, int width, int height,
      bool vertical, int thumbPosition, int thumbSize,
      bool mouseOver, bool mouseDown) override;

    void drawComboBox(juce::Graphics &g, int width, int height, bool buttonDown,
      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;

    void drawTickBox(juce::Graphics &g, juce::Component &component,
      float x, float y, float w, float h, bool ticked,
      bool enabled, bool mouseOver, bool buttonDown) override;

    void drawCallOutBoxBackground(juce::CallOutBox &callOutBox, juce::Graphics &g, const juce::Path &path, juce::Image &) override;

    void drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &backgroundColor,
      bool hover, bool down) override;

    int getSliderPopupPlacement(juce::Slider &slider) override;

    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont(juce::Slider &slider) override;

    juce::Slider::SliderLayout getSliderLayout(juce::Slider &slider) override
    { return { slider.getLocalBounds(), { 0, 0, 0, 0 } }; }

    int getMenuWindowFlags() override { return 0; }

    static DefaultLookAndFeel* instance() {
      static DefaultLookAndFeel instance;
      return &instance;
    }

  protected:
    DefaultLookAndFeel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultLookAndFeel)
  };
}