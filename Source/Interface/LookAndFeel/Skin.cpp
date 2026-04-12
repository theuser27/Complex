/*
  ==============================================================================

    Skin.cpp
    Created: 16 Nov 2022 6:55:11am
    Author:  theuser27

  ==============================================================================
*/

#include "Skin.hpp"

#include "cplug/config.h"

#include "Third Party/cjson/cjson.h"
#include "Third Party/xhl/xhl_files.h"

#include "Data/BinaryData.hpp"

#include "Framework/load_save.hpp"
#include "Component.hpp"

namespace
{
  inline constexpr utils::string_view kOverrideNames[Interface::Skin::kSectionsCount] =
  {
    "All",
    "Effects Lane",
    "Filter Module",
    "Dynamics Module",
    "Phase Module",
    "Pitch Module",
    "Destroy Module",
  };

  inline constexpr utils::string_view kValueNames[Interface::Skin::kValueIdCount] =
  {
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

    "Scroll Hitbox Width",
    "Scroll Padding",
    "Scroll Shrink Percent",
  };

  inline constexpr utils::string_view kColorNames[Interface::Skin::kColorIdCount] =
  {
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

extern thread_local utils::bumpArena *jsonArena;
extern thread_local utils::bumpArena *localScratch;

namespace Interface
{
  namespace
  {
    void updateJson([[maybe_unused]] cjson *data)
    {
      //[[maybe_unused]] int version = 0;
      //if (cjson *pluginVersion = cjson_GetObjectItem(data, "Plugin Version"))
      //  version = (int)pluginVersion->vuint;

      // if skin versions upgrades are ever needed, insert them here
    }
  }

  Skin::Skin()
  {
    auto skinFile = Framework::LoadSave::getConfigFilePath(CPLUG_PLUGIN_NAME ".skin");

    // temporary solution to ensure there's a skin file
    // if this throws put Complex.skin at Users\(user)\AppData\Roaming\Complex
    if (xfiles_exists(skinFile.data()))
    {
      char *string;
      usize stringSize;
      if (xfiles_read(skinFile.data(), (void **)&string, &stringSize))
      {
        if (stringToState({ string, stringSize }))
          return;
      }
    }

    auto defaultSkin = utils::string_view{ (const char *)BinaryData::Complex_skin, BinaryData::Complex_skinSize };
    if (stringToState(defaultSkin))
    {
      xfiles_write(skinFile.data(), defaultSkin.data(), defaultSkin.size());
      return;
    }

    COMPLEX_HARD_ASSERT_FALSE("Couldn't parse default skin");
  }

  static usize getSkinOverrideIndex(const Component *component)
  {
    while (component->skinOverride == Skin::kUseParentOverride && component->parent)
      component = component->parent;

    COMPLEX_ASSERT(component->skinOverride <= Skin::kUseParentOverride);

    return (component->skinOverride == Skin::kUseParentOverride) ?
      Skin::kNone : component->skinOverride;
  }

  Colour
  Skin::getColour(ColourId colorId, const Component *component) const
  {
    COMPLEX_ASSERT(colorId < kColorIdCount);
    return colours[getSkinOverrideIndex(component)][colorId];
  }

  float
  Skin::getValue(ValueId valueId, const Component *component) const
  {
    COMPLEX_ASSERT(valueId < kValueIdCount);
    return values[getSkinOverrideIndex(component)][valueId];
  }

  void Skin::saveToFile(char *saveFile)
  {
    jsonArena = utils::bumpArena::createNested(localScratch, COMPLEX_KB(128));

    cjson *data = cjson_Create(cjson_Object);
    cjson_AddTo(data, "Plugin Version", cjson_String, CPLUG_PLUGIN_VERSION);

    char buffer[48];

    for (usize i = 0; i < kColorIdCount; ++i)
    {
      (void)colours[0][i].toString(buffer, sizeof(buffer));
      cjson_AddTo(data, kColorNames[i].data(), cjson_String, buffer);
    }

    for (usize i = 0; i < kValueIdCount; ++i)
      cjson_AddTo(data, kValueNames[i].data(), cjson_Float, (double)values[0][i]);

    cjson *overrides = cjson_AddTo(data, "overrides", cjson_Object);
    for (usize overrideIndex = kNone + 1; overrideIndex < kSectionsCount; ++overrideIndex)
    {
      cjson *overrideSection = cjson_AddTo(overrides, kOverrideNames[overrideIndex].data(), cjson_Object);
      for (usize i = 0; i < kColorIdCount; ++i)
      {
        if (colours[overrideIndex][i] != colours[0][i])
        {
          (void)colours[overrideIndex][i].toString(buffer, sizeof(buffer));
          cjson_AddTo(overrideSection, kColorNames[i].data(), cjson_String, buffer);
        }
      }

      for (usize i = 0; i < kValueIdCount; ++i)
        if (values[overrideIndex][i] != values[0][i])
          cjson_AddTo(overrideSection, kValueNames[i].data(), cjson_Float, (double)values[overrideIndex][i]);
    }

    usize stringSize;
    char *string = cjson_Print(data, &stringSize);
    xfiles_write(saveFile, string, stringSize);

    utils::bumpArena::destroy(jsonArena);
    jsonArena = nullptr;
  }

  void Skin::jsonToState(void *jsonData)
  {
    cjson *data = (cjson *)jsonData;

    updateJson(data);

    for (usize i = 0; i < kColorIdCount; ++i)
    {
      if (cjson *c = cjson_GetObjectItem(data, kColorNames[i].data()))
        colours[0][i] = Colour::fromString(c->vstring);
      else
        colours[0][i] = Colours::black;
    }

    for (usize i = 0; i < kValueIdCount; ++i)
    {
      if (cjson *v = cjson_GetObjectItem(data, kValueNames[i].data()))
        values[0][i] = (float)v->vdouble;
      else
        values[0][i] = 0.0f;
    }

    // default every override to base skin
    for (usize i = kNone + 1; i < kSectionsCount; ++i)
    {
      ::memcpy(colours[i], colours[0], sizeof(Skin::colours[0]));
      ::memcpy(values[i], values[0], sizeof(Skin::values[0]));
    }

    if (cjson *overrides = cjson_GetObjectItem(data, "overrides"))
    {
      for (usize overrideIndex = kNone; overrideIndex < kSectionsCount; ++overrideIndex)
      {
        cjson *section = cjson_GetObjectItem(overrides, kOverrideNames[overrideIndex].data());
        if (!section)
          continue;

        for (usize i = 0; i < kColorIdCount; ++i)
          if (cjson *c = cjson_GetObjectItem(section, kColorNames[i].data()))
            colours[overrideIndex][i] = Colour::fromString(c->vstring);

        for (usize i = 0; i < kValueIdCount; ++i)
          if (cjson *v = cjson_GetObjectItem(section, kValueNames[i].data()))
            values[overrideIndex][i] = (float)v->vdouble;
      }
    }
  }

  bool Skin::stringToState(utils::string_view skinString)
  {
    jsonArena = utils::bumpArena::createNested(localScratch, COMPLEX_KB(128));

    const char *potentialError = nullptr;
    cjson *data = cjson_ParseWithOpts(skinString.data(), skinString.size(), &potentialError, false);
    if (!data)
    {
      Interface::showNativeMessageBox("Couldn't parse skin preset correctly.",
        potentialError, Interface::MessageBoxType::Warning);

      return false;
    }

    jsonToState(data);

    utils::bumpArena::destroy(jsonArena);
    jsonArena = nullptr;

    return true;
  }
  
  float 
  getValue(Skin::ValueId valueId, bool isScaled, Skin::Override skinOverride)
  {
    if (uiRelated.skin)
    {
      COMPLEX_ASSERT(skinOverride < Skin::kSectionsCount);
      COMPLEX_ASSERT(valueId < Skin::kValueIdCount);
      auto value = uiRelated.skin->values[skinOverride][valueId];
      return (isScaled) ? scaleValue(value) : value;
    }

    return 0.0f;
  }

  float 
  getValue(Skin::ValueId valueId, bool isScaled, Component *component)
  {
    if (uiRelated.skin)
    {
      auto value = uiRelated.skin->getValue(valueId, component);
      return (isScaled) ? scaleValue(value) : value;
    }

    return 0.0f;
  }

  Colour 
  getColour(Skin::ColourId colorId, Skin::Override skinOverride)
  {
    if (uiRelated.skin)
    {
      COMPLEX_ASSERT(skinOverride < Skin::kSectionsCount);
      COMPLEX_ASSERT(colorId < Skin::kColorIdCount);
      return uiRelated.skin->colours[skinOverride][colorId];
    }

    return Colours::black;
  }

  Colour 
  getColour(Skin::ColourId colorId, Component *component)
  {
    if (uiRelated.skin)
      return uiRelated.skin->getColour(colorId, component);

    return Colours::black;
  }
}
