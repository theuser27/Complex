/*
  ==============================================================================

    Skin.cpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#include "Skin.hpp"

#include "Third Party/json/json.hpp"
#include "Third Party/visage/file_system.h"
//#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

#include "../Sections/BaseSection.hpp"

using json = nlohmann::json;

namespace
{
  inline constexpr utils::array<std::string_view, Interface::Skin::kSectionsCount> kOverrideNames = {
    "All",
    "Overlays",
    "Effects Lane",
    "Popup Browser",
    "Filter Module",
    "Dynamics Module",
    "Phase Module",
    "Pitch Module",
    "Destroy Module",
  };

  inline constexpr utils::array<std::string_view, Interface::Skin::kValueIdCount> kValueNames = {
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

    "Knob Arc Size",
    "Knob Arc Thickness",
    "Knob Body Size",
    "Knob Handle Length",
    "Knob Shadow Width",
    "Knob Shadow Offset",
  };

  inline constexpr utils::array<std::string_view, Interface::Skin::kColorIdCount> kColorNames = {
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

    "Popup Display Background",
    "Popup Display Border",
    "Popup Selector Background",
    "Popup Selector Delimiter",

    "Text Component Background",
    "Text Component Text 1",
    "Text Component Text 2",

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
    "Text Editor Selection",
  };
}

namespace Interface
{
  namespace
  {
    visage::File getDefaultSkin()
    {
      auto pluginFolder = visage::appDataDirectory().append(JucePlugin_Name);
      if (!std::filesystem::exists(pluginFolder))
      {
        std::filesystem::create_directory(pluginFolder);
      }
      return pluginFolder.append(JucePlugin_Name ".skin");
    }

    json updateJson(json data)
    {
      [[maybe_unused]] int version = 0;
      if (data.count("Plugin Version"))
        version = data["Plugin Version"];

      // if skin versions upgrades are ever needed, insert them here

      return data;
    }
  }

  Skin::Skin()
  {
    auto defaultSkin = getDefaultSkin();

    // temporary solution to ensure there's a skin file
    // if this throws put Complex.skin at Users\(user)\AppData\Roaming\Complex
    if (std::filesystem::exists(defaultSkin))
    {
      auto stringState = visage::loadFileAsString(defaultSkin);
      if (!stringToState(stringState))
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
  }

  void Skin::clearSkin()
  {
    for (int i = 0; i < kSectionsCount; ++i)
      colorOverrides_[i].data.clear();
    for (int i = 0; i < kSectionsCount; ++i)
      valueOverrides_[i].data.clear();
  }

  void Skin::setColour(ColourId colorId, const juce::Colour &color) noexcept
  {
    auto index = colorId - kInitialColor;
    COMPLEX_ASSERT(index < kColorIdCount);
    colors_[index] = color.getARGB();
  }
  u32 Skin::getColour(ColourId colorId) const
  {
    auto index = colorId - kInitialColor;
    COMPLEX_ASSERT(index < kColorIdCount);
    return colors_[index];
  }



  u32 Skin::getColour(SectionOverride section, ColourId colorId) const
  {
    if (section == kNone)
      return getColour(colorId);

    if (auto iter = colorOverrides_[section].find(colorId); iter != colorOverrides_[section].data.end())
      return iter->second;

    return getColour(colorId);
  }

  u32 Skin::getColour(const OpenGlContainer *section, ColourId colorId) const
  {
    SectionOverride sectionOverride;
    do
    {
      sectionOverride = section->getSectionOverride();
      if (auto iter = colorOverrides_[sectionOverride].find(colorId); 
        iter != colorOverrides_[sectionOverride].data.end())
        return iter->second;

      section = dynamic_cast<OpenGlContainer *>(section->getParentSafe());
    } while (sectionOverride != kNone && section != nullptr);

    return getColour(colorId);
  }

  void Skin::setValue(ValueId valueId, float value)
  {
    COMPLEX_ASSERT(valueId < kValueIdCount);
    values_[valueId] = value;
  }

  float Skin::getValue(ValueId valueId) const
  {
    COMPLEX_ASSERT(valueId < kValueIdCount);
    return values_[valueId];
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

  void Skin::addColourOverride(int section, ColourId colorId, const juce::Colour &color)
  {
    if (section == kNone)
      setColour(colorId, color);
    else
      colorOverrides_[section][colorId] = color.getARGB();
  }

  void Skin::removeColourOverride(int section, ColourId colorId)
  {
    if (section != kNone)
      colorOverrides_[section].erase(colorId);
  }

  bool Skin::overridesColour(int section, ColourId colorId) const
  {
    if (section == kNone)
      return true;

    return colorOverrides_[section].find(colorId) != colorOverrides_[section].data.end();
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

  bool Skin::overridesValue(int section, ValueId valueId) const
  {
    if (section == kNone)
      return true;

    return valueOverrides_[section].find(valueId) != valueOverrides_[section].data.end();
  }

  void Skin::saveToFile(const juce::File &destination)
  {
    json data;
    for (int i = 0; i < kColorIdCount; ++i)
      data[kColorNames[i]] = juce::Colour{ colors_[i] }.toString().toStdString();

    for (int i = 0; i < kValueIdCount; ++i)
      data[kValueNames[i]] = values_[i];

    json overrides;
    for (usize overrideIndex = 0; overrideIndex < kSectionsCount; ++overrideIndex)
    {
      json overrideSection;
      for (const auto &[key, value] : colorOverrides_[overrideIndex].data)
        overrideSection[kColorNames[key - kInitialColor]] = juce::Colour{ value }.toString().toStdString();

      for (const auto &[key, value] : valueOverrides_[overrideIndex].data)
        overrideSection[kValueNames[key]] = value;

      overrides[kOverrideNames[overrideIndex]] = overrideSection;
    }

    data["overrides"] = overrides;
    data["Plugin Version"] = JucePlugin_VersionCode;

    (void)destination.replaceWithText(data.dump());
  }

  void Skin::jsonToState(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);

    clearSkin();
    data = updateJson(data);

    if (data.count("overrides"))
    {
      json overrides = data["overrides"];
      for (usize overrideIndex = 0; overrideIndex < kSectionsCount; ++overrideIndex)
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
            ColourId colorId = static_cast<Skin::ColourId>(i + Skin::kInitialColor);

            std::string_view colorString = overrideSection[kColorNames[i]].get<std::string_view>();
            colorOverrides_[overrideIndex].add(colorId, juce::Colour::fromString(colorString.data()).getARGB());
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

    for (usize i = 0; i < kColorIdCount; ++i)
    {
      if (data.contains(kColorNames[i]))
      {
        std::string_view colorString = data[kColorNames[i]].get<std::string_view>();
        colors_[i] = juce::Colour::fromString(colorString.data()).getARGB();
      }
    }

    for (usize i = 0; i < kValueIdCount; ++i)
    {
      if (data.contains(kValueNames[i]))
        values_[i] = data[kValueNames[i]];
      else
        values_[i] = 0.0f;
    }
  }

  bool Skin::stringToState(utils::string_view skinString)
  {
    try
    {
      json data = json::parse(skinString, nullptr, false);
      jsonToState(&data);
    }
    catch (const json::exception &)
    {
      return false;
    }
    return true;
  }

  void Skin::loadDefaultSkin()
  {
    auto defaultSkin = utils::string_view{ BinaryData::Complex_skin, BinaryData::Complex_skinSize };

    try
    {
      json data = json::parse(defaultSkin, nullptr, false);
      jsonToState(&data);
    }
    catch (const json::exception &)
    {
      COMPLEX_ASSERT_FALSE("Couldn't parse default skin");
    }
  }
}
