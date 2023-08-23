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
  	setColour(PopupMenu::backgroundColourId, Colour(0xff111111));
    setColour(PopupMenu::textColourId, Colour(0xffcccccc));
    setColour(PopupMenu::headerTextColourId, Colour(0xffffffff));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff8458b7));
    setColour(PopupMenu::highlightedTextColourId, Colour(0xffffffff));

    setColour(BubbleComponent::backgroundColourId, Colour(0xff111111));
    setColour(BubbleComponent::outlineColourId, Colour(0xff333333));

    setColour(TooltipWindow::textColourId, Colour(0xffdddddd));
  }

  void DefaultLookAndFeel::drawPopupMenuBackground(Graphics &g, int width, int height)
  {
    g.setColour(findColour(PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, kPopupMenuBorder);
    g.setColour(findColour(BubbleComponent::outlineColourId));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, kPopupMenuBorder, 1.0f);
  }

  void DefaultLookAndFeel::drawScrollbar(Graphics &g, ScrollBar &scrollBar, int, int, int width, 
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

  void DefaultLookAndFeel::drawComboBox(Graphics &g, int, int height, const bool,
    int, int, int, int, ComboBox &box)
  {
    static constexpr float kRoundness = 4.0f;
    g.setColour(findColour(BubbleComponent::backgroundColourId));
    g.fillRoundedRectangle(box.getLocalBounds().toFloat(), kRoundness);
    Path path = Paths::downTriangle();

    g.setColour(box.findColour(Skin::kTextComponentText, true));
    Rectangle<int> arrow_bounds = box.getLocalBounds().removeFromRight(height);
    g.fillPath(path, path.getTransformToScaleToFit(arrow_bounds.toFloat(), true));
  }

  void DefaultLookAndFeel::drawTickBox(Graphics &g, Component &component, float x, float y, 
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

  void DefaultLookAndFeel::drawCallOutBoxBackground(CallOutBox &callOutBox, Graphics &g, const Path &path, Image &)
  {
    g.setColour(callOutBox.findColour(Skin::kBody, true));
    g.fillPath(path);

    g.setColour(callOutBox.findColour(Skin::kPopupBorder, true));
    g.strokePath(path, PathStrokeType(1.0f));
  }

  void DefaultLookAndFeel::drawButtonBackground(Graphics &g, Button &button, 
    const Colour &, bool, bool)
  {
    g.setColour(button.findColour(Skin::kPopupSelectorBackground, true));
    g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 5.0f);
  }

  int DefaultLookAndFeel::getSliderPopupPlacement(Slider &slider)
  {
    if (auto *pSlider = dynamic_cast<BaseSlider *>(&slider); pSlider)
      return pSlider->getPopupPlacement();

    return LookAndFeel_V3::getSliderPopupPlacement(slider);
  }

  Font DefaultLookAndFeel::getPopupMenuFont()
  { return Fonts::instance()->getInterVFont().withPointHeight(14.0f); }

  Font DefaultLookAndFeel::getSliderPopupFont(Slider &)
  { return Fonts::instance()->getInterVFont().withPointHeight(14.0f); }

}
