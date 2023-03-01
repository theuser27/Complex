/*
  ==============================================================================

    DefaultLookAndFeel.h
    Created: 16 Nov 2022 6:49:15am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Interface
{
  class DefaultLookAndFeel : public juce::LookAndFeel_V4 {
  public:
    static constexpr int kPopupMenuBorder = 4;

    ~DefaultLookAndFeel() override = default;

    int getPopupMenuBorderSize() override { return kPopupMenuBorder; }

    void drawTextEditorOutline(Graphics &g, int width, int height, TextEditor &textEditor) override { }
    void fillTextEditorBackground(Graphics &g, int width, int height, TextEditor &textEditor) override;
    void drawPopupMenuBackground(Graphics &g, int width, int height) override;

    void drawScrollbar(Graphics &g, ScrollBar& scrollBar, int x, int y, int width, int height,
      bool vertical, int thumbPosition, int thumbSize,
      bool mouseOver, bool mouseDown) override;

    void drawComboBox(Graphics &g, int width, int height, bool buttonDown,
      int buttonX, int buttonY, int buttonW, int buttonH, ComboBox& box) override;

    void drawTickBox(Graphics &g, Component &component,
      float x, float y, float w, float h, bool ticked,
      bool enabled, bool mouseOver, bool buttonDown) override;

    void drawCallOutBoxBackground(CallOutBox &callOutBox, Graphics &g, const Path &path, Image &) override;

    void drawButtonBackground(Graphics &g, Button &button, const Colour &backgroundColor,
      bool hover, bool down) override;

    int getSliderPopupPlacement(Slider &slider) override;

    Font getPopupMenuFont() override;
    Font getSliderPopupFont(Slider &slider) override;

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