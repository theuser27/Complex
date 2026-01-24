/*
  ==============================================================================

    Fonts.hpp
    Created: 5 Dec 2022 2:08:35am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "nanovg/nanovg.h"

#include "Framework/memory.hpp"
#include "gui_utils.hpp"

extern "C"
{
  typedef struct NVGLUframebuffer NVGLUframebuffer;
  typedef struct NVGcontext NVGcontext;
}

namespace Interface
{
  using FontId = int;

  struct Font
  {
    FontId id;
    float height;
    float kerning;
  };

  class Graphics
  {
  public:
    static constexpr float kDDinDefaultHeight = 11.5f;
    static constexpr float kInterVDefaultHeight = 12.0f;

    static constexpr float kDDinDefaultKerning = 0.5f;
    static constexpr float kInterVDefaultKerning = 0.5f;

    Graphics();
    ~Graphics();

    Font getDDinFont() const { return Font{ .id = DDinFontId, .height = kDDinDefaultHeight, .kerning = kDDinDefaultKerning }; }
    Font getInterFont() const { return Font{ .id = InterFontId, .height = kInterVDefaultHeight, .kerning = kInterVDefaultKerning }; }

    float getKerningForHeight(int fontId, float height) const noexcept
    {
      if (fontId == DDinFontId)
        return height / kDDinDefaultHeight * kDDinDefaultKerning;
      else if (fontId == InterFontId)
        return height / kInterVDefaultHeight * kInterVDefaultKerning / 8.0f;

      COMPLEX_ASSERT_FALSE("Unknown font to get kerning for");

      return kDDinDefaultKerning;
    }

    Font getFont(FontId fontId, float height) const
    {
      Font font{ fontId, scaleValue(height) };
      font.kerning = uiRelated.cache->getKerningForHeight(font.id, font.height);
      return font;
    }

    float getFontAscentFromHeight(int fontId, float height) const noexcept;
    float getFontHeightFromAscent(int fontId, float ascent) const noexcept;

    void setFontHeight(float height) const;
    void setFontHeightFromAscent(float ascent) const;

    float getStringWidthFloat(utils::string_view string)
    {
      return nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
        string.data() + string.size(), nullptr);
    }

    Area<float> getStringBounds(utils::string_view string)
    {
      float bounds[4];
      (void)nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
        string.data() + string.size(), bounds);

      return { bounds[2] - bounds[0], bounds[3] - bounds[1] };
    }

    Area<float> getStringBoundsMultiline(utils::string_view string, float breakRowWidth)
    {
      float bounds[4];
      (void)nvgTextBoxBounds(context, 0.0f, 0.0f, breakRowWidth, 
        string.data(), string.data() + string.size(), bounds);

      return { bounds[2] - bounds[0], bounds[3] - bounds[1] };
    }

    void setFont(Font font)
    {
      nvgFontFaceId(context, font.id);
      nvgFontSize(context, font.height);
      nvgTextLetterSpacing(context, font.kerning);
    }

    void bindTextureFramebuffer();
    void unbindTextureFramebuffer();

    Rectangle<i32> reserveSlotForTexture(u64 id, Area<i32> desiredArea);

    Rectangle<i32> getTextureWithArea(u64 id, Area<i32> area)
    {
      auto iter = textureBounds.get_first_of(id);
      while (iter != textureBounds.data.end() && iter->first == id)
      {
        if (Area{ iter->second.w, iter->second.h } == area)
          return iter->second;
        ++iter;
      }

      return Rectangle<i32>{};
    }
    Rectangle<i32> resizeTexture(u64 id,
      Rectangle<i32> currentBounds, Area<i32> desiredArea)
    {
      auto iter = textureBounds.binary_search({ id, currentBounds });
      if (iter == textureBounds.data.end())
        return reserveSlotForTexture(id, desiredArea);

      return (iter == textureBounds.data.end()) ? Rectangle<i32>{} : iter->second;
    }

    u64 getStaticTextureId(utils::type_id_t staticId)
    {
      auto iter = staticTextureIds.get_first_of(staticId);
      if (iter == staticTextureIds.data.end())
        iter = staticTextureIds.add_ordered(staticId, idGenerator++);
      
      return iter->second;
    }

    operator NVGcontext *() const { return context; }

    utils::vector_map<u64, Rectangle<i32>> textureBounds{};
    utils::vector_map<utils::type_id_t, u64> staticTextureIds{};

    u64 idGenerator{};
    Area<i32> planeArea{};

    NVGcontext *context = nullptr;
    NVGLUframebuffer *textureFBO = nullptr;
    FontId DDinFontId;
    FontId InterFontId;
  };
}