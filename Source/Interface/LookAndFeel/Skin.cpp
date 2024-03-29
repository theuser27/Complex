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
  inline constexpr std::array<std::string_view, Interface::Skin::kSectionsCount> kOverrideNames = {
    "All",
    "Overlays",
    "Effects Lane",
  	"Popup Browser",
    "Filter Module",
    "Dynamics Module",
    "Phase Module"
  };

  inline constexpr std::array<std::string_view, Interface::Skin::kValueIdCount> kValueNames = {
    "Body Rounding Top",
    "Body Rounding Bottom",

  	"Widget Line Width",
    "Widget Line Boost",
    "Widget Fill Center",
    "Widget Fill Fade",
    "Widget Fill Boost",
  	"Widget Margin",
    "Widget Rounded Corner",

    "Label Height",
    "Label Background Height",
    "Label Rounding",
    "Label Offset",
    "Text Component Label Offset",

    "Rotary Option X Offset",
    "Rotary Option Y Offset",
    "Rotary Option Width",

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
    "Modulation Font Size"
  };

  inline constexpr std::array<std::string_view, Interface::Skin::kColorIdCount> kColorNames = {
    "Background",
    "Body",
    "Background Element",
    "Heading Text",
    "Normal Text",
    "Border",

    "Widget Primary 1",
    "Widget Primary 2",
    "Widget Primary Disabled",
    "Widget Secondary 1",
    "Widget Secondary 2",
    "Widget Secondary Disabled",
    "Widget Accent 1",
    "Widget Accent 2",
    "Widget Background 1",
    "Widget Background 2",
    "Widget Center Line",

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

    "Modulation Meter",
    "Modulation Meter Left",
    "Modulation Meter Right",
    "Modulation Meter Control",
    "Modulation Button Selected",
    "Modulation Button Dragging",
    "Modulation Button Unselected",

    "Icon Button Off",
    "Icon Button Off Hover",
    "Icon Button Off Pressed",
    "Icon Button On",
    "Icon Button On Hover",
    "Icon Button On Pressed",

    "Action Button Primary",
    "Action Button Primary Hover",
    "Action Button Primary Press",
    "Action Button Secondary",
    "Action Button Secondary Hover",
    "Action Button Secondary Press",
  	"Action Button Text",

  	"Power Button On",
    "Power Button Off",

    "Text Editor Background",
    "Text Editor Border",
    "Text Editor Caret",
    "Text Editor Selection"
  };
}

namespace Interface
{
  using namespace juce;

  Skin::Skin()
  {
    File defaultSkin = Framework::LoadSave::getDefaultSkin();

    // temporary solution to ensure there's a skin file
    // if this throws put Complex.skin at Users\(user)\AppData\Roaming\Complex
    if (defaultSkin.exists())
    {
      if (!loadFromFile(defaultSkin))
        loadDefaultSkin();
    }
    else
      loadDefaultSkin();

    /*try
    {
      if (!defaultSkin.exists())
        throw std::exception("skin file doesn't exist");
      
      loadFromFile(defaultSkin);

    }
    catch (const std::exception &e)
    {
      std::cerr << e.what() << '\n';
    }*/

    copyValuesToLookAndFeel(DefaultLookAndFeel::instance());
  }

  void Skin::clearSkin()
  {
    for (int i = 0; i < kSectionsCount; ++i)
      colorOverrides_[i].clear();
    for (int i = 0; i < kSectionsCount; ++i)
      valueOverrides_[i].clear();
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
    lookAndFeel->setColour(PopupMenu::textColourId, getColor(Skin::kNormalText));
    lookAndFeel->setColour(TooltipWindow::textColourId, getColor(Skin::kNormalText));

    lookAndFeel->setColour(BubbleComponent::backgroundColourId, getColor(Skin::kPopupBackground));
    lookAndFeel->setColour(BubbleComponent::outlineColourId, getColor(Skin::kPopupBorder));

    for (int i = kInitialColor; i < kFinalColor; ++i)
      lookAndFeel->setColour(i, getColor(static_cast<ColorId>(i)));
  }

  Colour Skin::getColor(int section, ColorId colorId) const
  {
    if (section == kNone)
      return getColor(colorId);

    if (colorOverrides_[section].contains(colorId))
      return colorOverrides_[section].at(colorId);

    return Colours::black;
  }

  Colour Skin::getColor(const BaseSection *section, ColorId colorId) const
  {
    SectionOverride sectionOverride = section->getSectionOverride();
    while (sectionOverride != kNone)
    {
      if (colorOverrides_[sectionOverride].contains(colorId))
        return colorOverrides_[sectionOverride].at(colorId);

      section = section->getParent();
      sectionOverride = section->getSectionOverride();
    }

    return getColor(colorId);
  }

  float Skin::getValue(int section, ValueId valueId) const
  {
    if (valueOverrides_[section].contains(valueId))
      return valueOverrides_[section].at(valueId);

    return getValue(valueId);
  }

  float Skin::getValue(const BaseSection *section, ValueId valueId) const
  {
    SectionOverride sectionOverride = section->getSectionOverride();
    while (sectionOverride != kNone)
    {
      if (valueOverrides_[sectionOverride].contains(valueId))
        return valueOverrides_[sectionOverride].at(valueId);

      section = section->getParent();
      sectionOverride = section->getSectionOverride();
    }

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
    for (int i = 0; i < kColorIdCount; ++i)
      data[kColorNames[i]] = colors_[i].toString().toStdString();

    for (int i = 0; i < kValueIdCount; ++i)
      data[kValueNames[i]] = values_[i];

    json overrides;
    for (int override_index = 0; override_index < kSectionsCount; ++override_index)
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
      for (int overrideIndex = 0; overrideIndex < kSectionsCount; ++overrideIndex)
      {
        std::string_view name = kOverrideNames[overrideIndex];
        colorOverrides_[overrideIndex].clear();
        valueOverrides_[overrideIndex].clear();

        if (overrides.count(name) == 0)
          continue;

        json overrideSection = overrides[name];
        for (int i = 0; i < kColorIdCount; ++i)
        {
          if (overrideSection.count(kColorNames[i]))
          {
            ColorId colorId = static_cast<Skin::ColorId>(i + Skin::kInitialColor);

            std::string colorString = overrideSection[kColorNames[i]];
            colorOverrides_[overrideIndex][colorId] = Colour::fromString(colorString);
          }
        }

        for (int i = 0; i < kValueIdCount; ++i)
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

    for (int i = 0; i < kColorIdCount; ++i)
    {
      if (data.count(kColorNames[i]))
      {
        std::string colorString = data[kColorNames[i]];
        colors_[i] = Colour::fromString(colorString);
      }
    }

    for (size_t i = 0; i < kValueIdCount; ++i)
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
    catch (const json::exception &)
    {
      return false;
    }
    return true;
  }

  bool Skin::loadFromFile(const File &source)
  { return stringToState(source.loadFileAsString()); }

  void Skin::loadDefaultSkin()
  {
    MemoryInputStream skin((const void *)BinaryData::Complex_skin, BinaryData::Complex_skinSize, false);
    std::string skin_string = skin.readEntireStreamAsString().toStdString();

    try
    {
      json data = json::parse(skin_string, nullptr, false);
      jsonToState(data);
    }
    catch (const json::exception &)
    {
      COMPLEX_ASSERT_FALSE("Couldn't parse default skin");
    }
  }
}
