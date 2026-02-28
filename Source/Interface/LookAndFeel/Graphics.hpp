
// Created: 2022-12-05 02:08:35

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
  enum class FontId : u8 { DDinType, InterType };

  class Graphics
  {
  public:
    Graphics();
    ~Graphics();

    static float getFontKerningFromFontHeight(FontId fontId, float height);
    static float getFontLineHeightFromFontHeight(FontId fontId, float height);
    static float getFontHeightFromLineHeight(FontId fontId, float lineHeight);

    float 
    getStringWidthFloat(utils::string_view string)
    {
      return nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
        string.data() + string.size(), nullptr);
    }

    Area<float> 
    getStringBounds(utils::string_view string)
    {
      float bounds[4];
      (void)nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
        string.data() + string.size(), bounds);

      return { bounds[2] - bounds[0], bounds[3] - bounds[1] };
    }

    Area<float> 
    getStringBoundsMultiline(utils::string_view string, float breakRowWidth)
    {
      float bounds[4];
      (void)nvgTextBoxBounds(context, 0.0f, 0.0f, breakRowWidth, 
        string.data(), string.data() + string.size(), bounds);

      return { bounds[2] - bounds[0], bounds[3] - bounds[1] };
    }

    void setFont(FontId fontid, float lineHeight);
    void setFontHeight(float height) const;

    void bindTextureFramebuffer();
    void unbindTextureFramebuffer();

    Rectangle<i32> reserveSlotForTexture(u64 id, Area<i32> desiredArea);

    Rectangle<i32> 
    getTextureWithArea(u64 id, Area<i32> area)
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

    Rectangle<i32> 
    resizeTexture(u64 id, Rectangle<i32> currentBounds, Area<i32> desiredArea)
    {
      auto iter = textureBounds.binary_search({ id, currentBounds });
      if (iter == textureBounds.data.end())
        return reserveSlotForTexture(id, desiredArea);

      return (iter == textureBounds.data.end()) ? Rectangle<i32>{} : iter->second;
    }

    u64 
    getStaticTextureId(utils::typeInfo staticId)
    {
      auto iter = staticTextureIds.get_first_of(staticId);
      if (iter == staticTextureIds.data.end())
        iter = staticTextureIds.add_ordered(staticId, idGenerator++);
      
      return iter->second;
    }

    operator NVGcontext *() const { return context; }

    utils::vector_map<u64, Rectangle<i32>> textureBounds{};
    utils::vector_map<utils::typeInfo, u64> staticTextureIds{};

    u64 idGenerator{};
    Area<i32> planeArea{};

    NVGcontext *context = nullptr;
    NVGLUframebuffer *textureFBO = nullptr;
    
  private:
    int DDinFontId;
    int InterFontId;
  };

  inline void paintDebugRect(NVGcontext *context, 
    Rectangle<float> bounds, Colour colour = Colours::white)
  {
    nvgBeginPath(context);
    nvgRect(context, 0.0f, 0.0f, bounds.w, bounds.h);
    nvgFillColor(context, colour);
    nvgFill(context);
  }
}