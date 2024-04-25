/*
  ==============================================================================

    Skin.cpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#include "Skin.h"

#include "Third Party/json/json.hpp"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"
#include "DefaultLookAndFeel.h"
#include "../Sections/BaseSection.h"
#include "Framework/load_save.h"

using json = nlohmann::json;

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
  namespace
  {
    json updateJson(json data)
    {
      int version = 0;
      if (data.count("Plugin Version"))
        version = data["Plugin Version"];

      // if skin versions upgrades are ever needed, insert them here

      return data;
    }
  }

  Skin::Skin()
  {
	  juce::File defaultSkin = Framework::LoadSave::getDefaultSkin();

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
      colorOverrides_[i].data.clear();
    for (int i = 0; i < kSectionsCount; ++i)
      valueOverrides_[i].data.clear();
  }

  void Skin::setColor(ColorId colorId, const juce::Colour &color) noexcept { colors_[colorId - kInitialColor] = color.getARGB(); }
  u32 Skin::getColor(ColorId colorId) const { return colors_[colorId - kInitialColor]; }

  bool Skin::overridesColor(int section, ColorId colorId) const
  {
    if (section == kNone)
      return true;

    return colorOverrides_[section].find(colorId) != colorOverrides_[section].data.end();
  }

  bool Skin::overridesValue(int section, ValueId valueId) const
  {
    if (section == kNone)
      return true;

    return valueOverrides_[section].find(valueId) != valueOverrides_[section].data.end();
  }

  void Skin::copyValuesToLookAndFeel(juce::LookAndFeel *lookAndFeel) const
  {
    lookAndFeel->setColour(juce::PopupMenu::backgroundColourId, juce::Colour{ getColor(Skin::kPopupBackground) });
    lookAndFeel->setColour(juce::PopupMenu::textColourId, juce::Colour{ getColor(Skin::kNormalText) });
    lookAndFeel->setColour(juce::TooltipWindow::textColourId, juce::Colour{ getColor(Skin::kNormalText) });

    lookAndFeel->setColour(juce::BubbleComponent::backgroundColourId, juce::Colour{ getColor(Skin::kPopupBackground) });
    lookAndFeel->setColour(juce::BubbleComponent::outlineColourId, juce::Colour{ getColor(Skin::kPopupBorder) });

    for (int i = kInitialColor; i < kFinalColor; ++i)
      lookAndFeel->setColour(i, juce::Colour{ getColor((ColorId)i) });
  }

  u32 Skin::getColor(SectionOverride section, ColorId colorId) const
  {
    if (section == kNone)
      return getColor(colorId);

    if (auto iter = colorOverrides_[section].find(colorId); iter != colorOverrides_[section].data.end())
      return iter->second;

    return juce::Colours::black.getARGB();
  }

  u32 Skin::getColor(const OpenGlContainer *section, ColorId colorId) const
  {
    SectionOverride sectionOverride;
    do
    {
      sectionOverride = section->getSectionOverride();
      if (auto iter = colorOverrides_[sectionOverride].find(colorId); iter != colorOverrides_[sectionOverride].data.end())
        return iter->second;

      section = dynamic_cast<OpenGlContainer *>(section->getParentSafe());
    } while (sectionOverride != kNone && section != nullptr);

    return getColor(colorId);
  }

  float Skin::getValue(SectionOverride section, ValueId valueId) const
  {
    if (auto iter = valueOverrides_[section].find(valueId); iter != valueOverrides_[section].data.end())
      return iter->second;

    return getValue(valueId);
  }

  float Skin::getValue(const OpenGlContainer *section, ValueId valueId) const
  {
    SectionOverride sectionOverride;
    do
    {
      sectionOverride = section->getSectionOverride();
      if (auto iter = valueOverrides_[sectionOverride].find(valueId); iter != valueOverrides_[sectionOverride].data.end())
        return iter->second;

      section = dynamic_cast<OpenGlContainer *>(section->getParentSafe());
    } while (sectionOverride != kNone && section != nullptr);

    return getValue(valueId);
  }

  void Skin::addOverrideColor(int section, ColorId colorId, const juce::Colour &color)
  {
    if (section == kNone)
      setColor(colorId, color);
    else
      colorOverrides_[section][colorId] = color.getARGB();
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

  std::string Skin::stateToString()
  {
    json data;
    for (int i = 0; i < kColorIdCount; ++i)
      data[kColorNames[i]] = juce::Colour{ colors_[i] }.toString().toStdString();

    for (int i = 0; i < kValueIdCount; ++i)
      data[kValueNames[i]] = values_[i];

    json overrides;
    for (int override_index = 0; override_index < kSectionsCount; ++override_index)
    {
      json override_section;
      for (const auto &[key, value] : colorOverrides_[override_index].data)
        override_section[kColorNames[key - kInitialColor]] = juce::Colour{ value }.toString().toStdString();

      for (const auto &[key, value] : valueOverrides_[override_index].data)
        override_section[kValueNames[key]] = value;

      overrides[kOverrideNames[override_index]] = override_section;
    }

    data["overrides"] = overrides;
    data["Plugin Version"] = JucePlugin_VersionCode;

    return data.dump();
  }

  void Skin::saveToFile(const juce::File &destination)
  { destination.replaceWithText(stateToString()); }

  void Skin::jsonToState(std::any jsonData)
  {
    json data = std::any_cast<json>(std::move(jsonData));

    clearSkin();
    data = updateJson(data);

    if (data.count("overrides"))
    {
      json overrides = data["overrides"];
      for (int overrideIndex = 0; overrideIndex < kSectionsCount; ++overrideIndex)
      {
        std::string_view name = kOverrideNames[overrideIndex];
        colorOverrides_[overrideIndex].data.clear();
        valueOverrides_[overrideIndex].data.clear();

        if (overrides.count(name) == 0)
          continue;

        json overrideSection = overrides[name];
        for (int i = 0; i < kColorIdCount; ++i)
        {
          if (overrideSection.count(kColorNames[i]))
          {
            ColorId colorId = static_cast<Skin::ColorId>(i + Skin::kInitialColor);

            std::string colorString = overrideSection[kColorNames[i]];
            colorOverrides_[overrideIndex].add(colorId, juce::Colour::fromString(colorString).getARGB());
          }
        }

        for (int i = 0; i < kValueIdCount; ++i)
        {
          if (overrideSection.count(kValueNames[i]))
          {
            Skin::ValueId valueId = static_cast<Skin::ValueId>(i);
            float value = overrideSection[kValueNames[i]];
            valueOverrides_[overrideIndex].add(valueId, value);
          }
        }
      }
    }

    for (int i = 0; i < kColorIdCount; ++i)
    {
      if (data.count(kColorNames[i]))
      {
        std::string colorString = data[kColorNames[i]];
        colors_[i] = juce::Colour::fromString(colorString).getARGB();
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

  bool Skin::stringToState(const juce::String &skinString)
  {
    try
    {
      json data = json::parse(skinString.toStdString(), nullptr, false);
      jsonToState(std::move(data));
    }
    catch (const json::exception &)
    {
      return false;
    }
    return true;
  }

  bool Skin::loadFromFile(const juce::File &source)
  { return stringToState(source.loadFileAsString()); }

  void Skin::loadDefaultSkin()
  {
    juce::MemoryInputStream skin{ BinaryData::Complex_skin, BinaryData::Complex_skinSize, false };
    std::string skin_string = skin.readEntireStreamAsString().toStdString();

    try
    {
      json data = json::parse(skin_string, nullptr, false);
      jsonToState(std::move(data));
    }
    catch (const json::exception &)
    {
      COMPLEX_ASSERT_FALSE("Couldn't parse default skin");
    }
  }
}
