/*
  ==============================================================================

    TextLookAndFeel.cpp
    Created: 16 Nov 2022 6:47:50am
    Author:  theuser27

  ==============================================================================
*/

#include "TextLookAndFeel.h"
#include "Skin.h"
#include "Fonts.h"
#include "../Components/BaseButton.h"
#include "../Sections/BaseSection.h"
#include "../Components/BaseSlider.h"

namespace Interface
{
  void TextLookAndFeel::drawRotarySlider(Graphics &g, int x, int y, int width, int height,
    float sliderPositionNormalised, float start_angle, float end_angle,
    Slider &slider)
  {
    static constexpr float kTextPercentage = 0.5f;

    float text_percentage = kTextPercentage;
    bool active = true;
    String text = slider.getTextFromValue(slider.getValue());
    float offset = 0.0f;
    float font_size = slider.getHeight() * text_percentage;

    if (auto *baseSlider = dynamic_cast<BaseSlider *>(&slider))
    {
      text_percentage = baseSlider->getTextHeightPercentage();
      font_size = slider.getHeight() * text_percentage;
      active = baseSlider->isActive();
      text = baseSlider->getSliderTextFromValue(slider.getValue());

      offset = baseSlider->findValue(Skin::kTextComponentOffset);
      if (text_percentage == 0.0f)
        font_size = baseSlider->findValue(Skin::kTextComponentFontSize);
    }

    Colour text_color = slider.findColour(Skin::kTextComponentText, true);
    if (!active)
      text_color = text_color.withMultipliedAlpha(0.5f);

    g.setColour(text_color);
    g.setFont(Fonts::instance()->getInterMediumFont().withPointHeight(font_size));
    g.drawText(text, x, y + std::round(offset), width, height, Justification::centred, false);
  }

  void TextLookAndFeel::drawToggleButton(Graphics &g, ToggleButton &button, bool hover, bool is_down)
  {
    static constexpr float kTextPercentage = 0.7f;
    static constexpr float kTextShrinkage = 0.98f;

    std::span<const std::string_view> string_lookup{};
    if (auto *baseButton = dynamic_cast<BaseButton *>(&button))
      string_lookup = baseButton->getStringLookup();

    bool toggled = button.getToggleState();
    if (toggled && string_lookup.empty())
    {
      if (is_down)
        g.setColour(button.findColour(Skin::kIconButtonOnPressed, true));
      else if (hover)
        g.setColour(button.findColour(Skin::kIconButtonOnHover, true));
      else
        g.setColour(button.findColour(Skin::kIconButtonOn, true));
    }
    else
    {
      if (is_down)
        g.setColour(button.findColour(Skin::kIconButtonOffPressed, true));
      else if (hover)
        g.setColour(button.findColour(Skin::kIconButtonOffHover, true));
      else
        g.setColour(button.findColour(Skin::kIconButtonOff, true));
    }

    String text = button.getButtonText();
    if (!string_lookup.empty())
    {
      int index = toggled ? 1 : 0;
      text = string_lookup[index].data();
    }

    int rounding = 0;
    float text_percentage = kTextPercentage;
    if (is_down)
      text_percentage *= kTextShrinkage;

    float font_size = button.getHeight() * text_percentage;
    if (auto *section = button.findParentComponentOfClass<BaseSection>())
    {
      font_size = section->findValue(Skin::kButtonFontSize);
      rounding = section->findValue(Skin::kWidgetRoundedCorner);
    }

    if (text.isEmpty())
      g.fillRoundedRectangle(button.getLocalBounds().toFloat(), rounding);
    else
    {
      g.setFont(Fonts::instance()->getInterMediumFont().withPointHeight(font_size));
      g.drawText(text, 0, 0, button.getWidth(), button.getHeight(), Justification::centred);
    }
  }

  void TextLookAndFeel::drawTickBox(Graphics &g, Component &component,
    float x, float y, float w, float h, bool ticked,
    bool enabled, bool mouse_over, bool button_down)
  {
    float border_width = 1.5f;
    if (ticked)
    {
      g.setColour(Colours::red);
      g.fillRect(x + 3 * border_width, y + 3 * border_width,
        w - 6 * border_width, h - 6 * border_width);
    }
  }

  void TextLookAndFeel::drawLabel(Graphics &g, Label &label)
  {
    Colour text = label.findColour(Skin::kBodyText, true);
    label.setColour(Label::textColourId, text);

    DefaultLookAndFeel::drawLabel(g, label);
  }

  void TextLookAndFeel::drawComboBox(Graphics &g, int width, int height, bool is_down,
    int button_x, int button_y, int button_w, int button_h, ComboBox &box)
  {
    Colour background = box.findColour(Skin::kTextComponentBackground, true);
    Colour text = box.findColour(Skin::kBodyText, true);
    Colour caret = box.findColour(Skin::kTextEditorCaret, true);

    box.setColour(ComboBox::backgroundColourId, background);
    box.setColour(ComboBox::arrowColourId, caret);
    box.setColour(ComboBox::outlineColourId, Colours::transparentBlack);
    box.setColour(ComboBox::textColourId, text);

    DefaultLookAndFeel::drawComboBox(g, width, height, is_down, button_x, button_y, button_w, button_h, box);
  }
}