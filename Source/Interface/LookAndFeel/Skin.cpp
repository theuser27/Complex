/*
  ==============================================================================

    Skin.cpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#include "Skin.h"
#include "DefaultLookAndFeel.h"
#include "../Sections/BaseSection.h"
#include "Framework/load_save.h"

namespace
{
  inline constexpr std::array<std::string_view, Interface::Skin::kNumSectionOverrides> kOverrideNames = {
    "All",
    "Header",
    "Overlays",
    "Effects Lane",
    "Effect Module"
  };

  inline constexpr std::array<std::string_view, Interface::Skin::kNumSkinValueIds> kValueNames = {
    "Body Rounding",
    "Label Height",
    "Label Background Height",
    "Label Rounding",
    "Label Offset",
    "Text Component Label Offset",
    "Rotary Option X Offset",
    "Rotary Option Y Offset",
    "Rotary Option Width",
    "Title Width",
    "Padding",
    "Large Padding",
    "Slider Width",
    "Text Component Height",
    "Text Component Offset",
    "Text Component Font Size",
    "Text Button Height",
    "Button Font Size",
    "Knob Arc Size",
    "Knob Arc Thickness",
    "Knob Body Size",
    "Knob Handle Length",
    "Knob Mod Amount Arc Size",
    "Knob Mod Amount Arc Thickness",
    "Knob Mod Meter Arc Size",
    "Knob Mod Meter Arc Thickness",
    "Knob Offset",
    "Knob Section Height",
    "Knob Shadow Width",
    "Knob Shadow Offset",
    "Modulation Button Width",
    "Modulation Font Size",
    "Widget Margin",
    "Widget Rounded Corner",
    "Widget Line Width",
    "Widget Line Boost",
    "Widget Fill Center",
    "Widget Fill Fade",
    "Widget Fill Boost"
  };

  inline constexpr std::array<std::string_view, Interface::Skin::kNumColors> kColorNames = {
    "Background",
    "Body",
    "Body Heading Background",
    "Heading Text",
    "Preset Text",
    "Body Text",
    "Border",
    "Label Background",
    "Label Connection",
    "Power Button On",
    "Power Button Off",

    "Overlay Screen",
    "Lighten Screen",
    "Shadow",
    "Popup Selector Background",
    "Popup Background",
    "Popup Border",

    "Text Component Background",
    "Text Component Text",

    "Rotary Arc",
    "Rotary Arc Disabled",
    "Rotary Arc Unselected",
    "Rotary Arc Unselected Disabled",
    "Rotary Hand",
    "Rotary Body",
    "Rotary Body Border",

    "Linear Slider",
    "Linear Slider Disabled",
    "Linear Slider Unselected",
    "Linear Slider Thumb",
    "Linear Slider Thumb Disabled",

    "Pin Slider",
    "Pin Slider Disabled",
    "Pin Slider Thumb",
    "Pin Slider Thumb Disabled",

    "Text Selector Text",
    "Text Selector Text Disabled",

    "Number Box Slider",

    "Widget Center Line",
    "Widget Primary 1",
    "Widget Primary 2",
    "Widget Primary Disabled",
    "Widget Secondary 1",
    "Widget Secondary 2",
    "Widget Secondary Disabled",
    "Widget Accent 1",
    "Widget Accent 2",
    "Widget Background",

    "Modulation Meter",
    "Modulation Meter Left",
    "Modulation Meter Right",
    "Modulation Meter Control",
    "Modulation Button Selected",
    "Modulation Button Dragging",
    "Modulation Button Unselected",

    "Icon Selector Icon",

    "Icon Button Off",
    "Icon Button Off Hover",
    "Icon Button Off Pressed",
    "Icon Button On",
    "Icon Button On Hover",
    "Icon Button On Pressed",

    "UI Button",
    "UI Button Text",
    "UI Button Hover",
    "UI Button Press",
    "UI Action Button",
    "UI Action Button Hover",
    "UI Action Button Press",

    "Text Editor Background",
    "Text Editor Border",
    "Text Editor Caret",
    "Text Editor Selection"
  };
}

namespace Interface
{
  bool Skin::shouldScaleValue(ValueId valueId) noexcept
  {
    return valueId != kWidgetFillFade && valueId != kWidgetFillBoost &&
      valueId != kWidgetLineBoost && valueId != kKnobHandleLength &&
      valueId != kWidgetFillCenter && valueId != kFrequencyDisplay;
  }

  Skin::Skin()
  {
    File defaultSkin = Framework::LoadSave::getDefaultSkin();

    // temporary solution to ensure there's a skin file
    // if this throws put Complex.skin at Users\(user)\AppData\Roaming\Complex
    try
    {
      if (!defaultSkin.exists())
        throw std::exception("skin file doesn't exist");
      
      loadFromFile(defaultSkin);

    }
    catch (const std::exception &e)
    {
      std::cerr << e.what() << '\n';
    }

    copyValuesToLookAndFeel(DefaultLookAndFeel::instance());
  }

  void Skin::clearSkin()
  {
    for (int i = 0; i < kNumSectionOverrides; ++i)
      colorOverrides_[i].clear();
    for (int i = 0; i < kNumSectionOverrides; ++i)
      valueOverrides_[i].clear();
  }

  void Skin::applyComponentColors(Component *component) const
  {
    for (int i = 0; i < Skin::kNumColors; ++i)
    {
      int colorId = i + Skin::kInitialColor;
      Colour color = getColor(static_cast<Skin::ColorId>(colorId));
      component->setColour(colorId, color);
    }
  }

  void Skin::applyComponentColors(Component *component, SectionOverride sectionOverride, bool topLevel) const
  {
    if (topLevel)
    {
      applyComponentColors(component);
      return;
    }

    for (int i = kInitialColor; i < kFinalColor; ++i)
      component->removeColour(i);

    for (const auto &color : colorOverrides_[sectionOverride])
      component->setColour(color.first, color.second);
  }

  void Skin::applyComponentValues(BaseSection *component) const
  {
    std::map<ValueId, float> values;
    for (int i = 0; i < kNumSkinValueIds; ++i)
      values[(ValueId)i] = values_[i];

    component->setSkinValues(values);
  }

  void Skin::applyComponentValues(BaseSection *component, SectionOverride section_override, bool topLevel) const
  {
    if (topLevel)
      applyComponentValues(component);
    else
      component->setSkinValues(valueOverrides_[section_override]);
  }

  bool Skin::overridesColor(int section, ColorId colorId) const
  {
    if (section == kNone)
      return true;

    return colorOverrides_[section].contains(colorId);
  }

  bool Skin::overridesValue(int section, ValueId valueId) const
  {
    if (section == kNone)
      return true;

    return valueOverrides_[section].contains(valueId);
  }

  void Skin::copyValuesToLookAndFeel(LookAndFeel *lookAndFeel) const
  {
    lookAndFeel->setColour(PopupMenu::backgroundColourId, getColor(Skin::kPopupBackground));
    lookAndFeel->setColour(PopupMenu::textColourId, getColor(Skin::kBodyText));
    lookAndFeel->setColour(TooltipWindow::textColourId, getColor(Skin::kBodyText));

    lookAndFeel->setColour(BubbleComponent::backgroundColourId, getColor(Skin::kPopupBackground));
    lookAndFeel->setColour(BubbleComponent::outlineColourId, getColor(Skin::kPopupBorder));

    for (int i = kInitialColor; i < kFinalColor; ++i)
      lookAndFeel->setColour(i, getColor(static_cast<Skin::ColorId>(i)));
  }

  Colour Skin::getColor(int section, ColorId colorId) const
  {
    if (section == kNone)
      return getColor(colorId);

    if (colorOverrides_[section].count(colorId))
      return colorOverrides_[section].at(colorId);

    return Colours::black;
  }

  float Skin::getValue(int section, ValueId valueId) const
  {
    if (valueOverrides_[section].count(valueId))
      return valueOverrides_[section].at(valueId);

    return getValue(valueId);
  }

  void Skin::addOverrideColor(int section, ColorId colorId, Colour color)
  {
    if (section == kNone)
      setColor(colorId, color);
    else
      colorOverrides_[section][colorId] = color;
  }

  void Skin::removeOverrideColor(int section, ColorId colorId)
  {
    if (section != kNone)
      colorOverrides_[section].erase(colorId);
  }

  void Skin::addOverrideValue(int section, ValueId valueId, float value)
  {
    if (section == kNone)
      setValue(valueId, value);
    else
      valueOverrides_[section][valueId] = value;
  }

  void Skin::removeOverrideValue(int section, ValueId valueId)
  {
    if (section != kNone)
      valueOverrides_[section].erase(valueId);
  }

  json Skin::stateToJson()
  {
    json data;
    for (int i = 0; i < kNumColors; ++i)
      data[kColorNames[i]] = colors_[i].toString().toStdString();

    for (int i = 0; i < kNumSkinValueIds; ++i)
      data[kValueNames[i]] = values_[i];

    json overrides;
    for (int override_index = 0; override_index < kNumSectionOverrides; ++override_index)
    {
      json override_section;
      for (const auto &color : colorOverrides_[override_index])
      {
        int index = color.first - Skin::kInitialColor;
        override_section[kColorNames[index]] = color.second.toString().toStdString();
      }

      for (const auto &value : valueOverrides_[override_index])
      {
        int index = value.first;
        override_section[kValueNames[index]] = value.second;
      }

      overrides[kOverrideNames[override_index]] = override_section;
    }

    data["overrides"] = overrides;
    data["Plugin Version"] = ProjectInfo::versionNumber;

    return data;
  }

  String Skin::stateToString()
  { return stateToJson().dump(); }

  void Skin::saveToFile(File destination)
  { destination.replaceWithText(stateToString()); }

  json Skin::updateJson(json data)
  {
    int version = 0;
    if (data.count("Plugin Version"))
      version = data["Plugin Version"];

    // if skin versions upgrades are ever needed, insert them here

    return data;
  }

  void Skin::jsonToState(json data)
  {
    clearSkin();
    data = updateJson(data);

    if (data.count("overrides"))
    {
      json overrides = data["overrides"];
      for (int overrideIndex = 0; overrideIndex < kNumSectionOverrides; ++overrideIndex)
      {
        std::string_view name = kOverrideNames[overrideIndex];
        colorOverrides_[overrideIndex].clear();
        valueOverrides_[overrideIndex].clear();

        if (overrides.count(name) == 0)
          continue;

        json overrideSection = overrides[name];
        for (int i = 0; i < kNumColors; ++i)
        {
          if (overrideSection.count(kColorNames[i]))
          {
            ColorId colorId = static_cast<Skin::ColorId>(i + Skin::kInitialColor);

            std::string colorString = overrideSection[kColorNames[i]];
            colorOverrides_[overrideIndex][colorId] = Colour::fromString(colorString);
          }
        }

        for (int i = 0; i < kNumSkinValueIds; ++i)
        {
          if (overrideSection.count(kValueNames[i]))
          {
            Skin::ValueId valueId = static_cast<Skin::ValueId>(i);
            float value = overrideSection[kValueNames[i]];
            valueOverrides_[overrideIndex][valueId] = value;
          }
        }
      }
    }

    for (int i = 0; i < kNumColors; ++i)
    {
      if (data.count(kColorNames[i]))
      {
        std::string colorString = data[kColorNames[i]];
        colors_[i] = Colour::fromString(colorString);
      }
    }

    for (int i = 0; i < kNumSkinValueIds; ++i)
    {
      if (data.count(kValueNames[i]))
        values_[i] = data[kValueNames[i]];
      else
        values_[i] = 0.0f;
    }
  }

  bool Skin::stringToState(String skin_string)
  {
    try
    {
      json data = json::parse(skin_string.toStdString(), nullptr, false);
      jsonToState(data);
    }
    catch (const json::exception &e)
    {
      return false;
    }
    return true;
  }

  bool Skin::loadFromFile(const File &source)
  { return stringToState(source.loadFileAsString()); }

}
