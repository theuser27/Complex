/*
  ==============================================================================

    DefaultLookAndFeel.cpp
    Created: 16 Nov 2022 6:49:15am
    Author:  theuser27

  ==============================================================================
*/

#include "DefaultLookAndFeel.h"
#include "Skin.h"
#include "Paths.h"
#include "Fonts.h"
#include "Interface/Components/BaseSlider.h"

namespace Interface
{
  DefaultLookAndFeel::DefaultLookAndFeel()
  {
  	setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff111111));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffcccccc));
    setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xffffffff));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff8458b7));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));

    setColour(juce::BubbleComponent::backgroundColourId, juce::Colour(0xff111111));
    setColour(juce::BubbleComponent::outlineColourId, juce::Colour(0xff333333));

    setColour(juce::TooltipWindow::textColourId, juce::Colour(0xffdddddd));
  }

  void DefaultLookAndFeel::drawPopupMenuBackground(juce::Graphics &g, int width, int height)
  {
    g.setColour(findColour(juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, kPopupMenuBorder);
    g.setColour(findColour(juce::BubbleComponent::outlineColourId));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, kPopupMenuBorder, 1.0f);
  }

  void DefaultLookAndFeel::drawScrollbar(juce::Graphics &g, juce::ScrollBar &scrollBar, int, int, int width,
    int height, bool, int thumbPosition, int thumbSize, bool mouseOver, bool mouseDown)
  {
    if (thumbSize >= height)
      return;

    int drawWidth = width / 2 - 2;
    if (mouseDown || mouseOver)
      drawWidth = width - 2;

    int drawTimes = 2;
    if (mouseDown)
      drawTimes = 4;

    int drawX = width - 1 - drawWidth;

    g.setColour(scrollBar.findColour(Skin::kLightenScreen, true));
    for (int i = 0; i < drawTimes; ++i)
      g.fillRoundedRectangle((float)drawX, (float)thumbPosition, (float)drawWidth,
        (float)thumbSize, (float)drawWidth / 2.0f);
  }

  void DefaultLookAndFeel::drawComboBox(juce::Graphics &g, int, int height, const bool,
    int, int, int, int, juce::ComboBox &box)
  {
    static constexpr float kRoundness = 4.0f;
    g.setColour(findColour(juce::BubbleComponent::backgroundColourId));
    g.fillRoundedRectangle(box.getLocalBounds().toFloat(), kRoundness);
    juce::Path path = Paths::downTriangle();

    g.setColour(box.findColour(Skin::kTextComponentText, true));
    juce::Rectangle<int> arrow_bounds = box.getLocalBounds().removeFromRight(height);
    g.fillPath(path, path.getTransformToScaleToFit(arrow_bounds.toFloat(), true));
  }

  void DefaultLookAndFeel::drawTickBox(juce::Graphics &g, juce::Component &component, float x, float y,
    float w, float h, bool ticked, bool, bool, bool)
  {
    static constexpr float kBorderPercent = 0.15f;
    if (ticked)
      g.setColour(component.findColour(Skin::kIconButtonOn, true));
    else
      g.setColour(component.findColour(Skin::kLightenScreen, true));

    float border_width = h * kBorderPercent;
    g.fillRect(x + border_width, y + border_width, w - 2 * border_width, h - 2 * border_width);
  }

  void DefaultLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox &callOutBox, 
    juce::Graphics &g, const juce::Path &path, juce::Image &)
  {
    g.setColour(callOutBox.findColour(Skin::kBody, true));
    g.fillPath(path);

    g.setColour(callOutBox.findColour(Skin::kPopupBorder, true));
    g.strokePath(path, juce::PathStrokeType(1.0f));
  }

  void DefaultLookAndFeel::drawButtonBackground(juce::Graphics &g, juce::Button &button,
    const juce::Colour &, bool, bool)
  {
    g.setColour(button.findColour(Skin::kPopupSelectorBackground, true));
    g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 5.0f);
  }

  int DefaultLookAndFeel::getSliderPopupPlacement(juce::Slider &slider)
  {
    if (auto *pSlider = dynamic_cast<BaseSlider *>(&slider); pSlider)
      return (std::underlying_type_t<BubblePlacement>)pSlider->getPopupPlacement();

    return LookAndFeel_V3::getSliderPopupPlacement(slider);
  }

  juce::Font DefaultLookAndFeel::getPopupMenuFont()
  { return Fonts::instance()->getInterVFont().withPointHeight(14.0f); }

  juce::Font DefaultLookAndFeel::getSliderPopupFont(juce::Slider &)
  { return Fonts::instance()->getInterVFont().withPointHeight(14.0f); }

}
