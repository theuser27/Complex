
// Created: 2022-12-05 02:08:35

#pragma once

#include "nanovg/nanovg.h"

#include "Framework/simd_values.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/memory.hpp"
#include "gui_utils.hpp"

extern "C"
{
  typedef struct NVGLUframebuffer NVGLUframebuffer;
  typedef struct NVGcontext NVGcontext;
  typedef struct NSVGimage NSVGimage;
}

namespace Plugin
{
  struct State;
}

namespace Interface
{
  enum class FontId : u8 { DDinType, InterType };

  class Renderer;
  class Skin;

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
      float width = 0.0f;
      if (!string.empty())
        width = nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
          string.data() + string.size(), nullptr);

      return width;
    }

    Area<float> 
    getStringBounds(utils::string_view string)
    {
      float bounds[4]{};
      if (!string.empty())
        (void)nvgTextBounds(context, 0.0f, 0.0f, string.data(), 
          string.data() + string.size(), bounds);

      return { bounds[2] - bounds[0], bounds[3] - bounds[1] };
    }

    usize 
    getStringNumberOfLines(utils::string_view string, float breakRowWidth)
    {
      usize lines = 1;
      if (!string.empty())
        lines = (usize)nvgTextBreakLines(context, string.data(), string.data() + string.size(),
          breakRowWidth, nullptr, 0);

      return lines;
    }

    Area<float> 
    getStringBoundsMultiline(utils::string_view string, float breakRowWidth)
    {
      float bounds[4]{};
      if (!string.empty())
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

  struct InterfaceRelated
  {
    Plugin::State *state = nullptr;
    Renderer *renderer = nullptr;
    Graphics *cache = nullptr;
    Skin *skin = nullptr;
    float scale = 1.0f;
    float deltaTime = 0.0f;
    double steadyTime = 0.0;
  };

  // thread_local variable for the message thread so that we don't need to pass pointers around
  extern thread_local InterfaceRelated uiRelated;
  strict_inline float scaleValue(float value) { return uiRelated.scale * value; }
  strict_inline Rectangle<float>
  scaleValue(Rectangle<float> bounds)
  {
    auto values = utils::bit_cast<simd_float>(bounds);
    values = uiRelated.scale * utils::toFloat(values);
    return utils::bit_cast<Rectangle<float>>(values);
  }
  strict_inline float scaleValueRound(float value) { return ::roundf(uiRelated.scale * value); }
  strict_inline Rectangle<float>
  scaleValueRound(Rectangle<float> bounds)
  {
    auto values = utils::bit_cast<simd_float>(bounds);
    values = simd_float::round(uiRelated.scale * utils::toFloat(values));
    return utils::bit_cast<Rectangle<float>>(values);
  }
  strict_inline i32 scaleValueRoundInt(float value) { return (int)::roundf(uiRelated.scale * value); }
  strict_inline Rectangle<i32> 
  scaleValueRoundInt(Rectangle<i32> bounds)
  {
    auto values = utils::bit_cast<simd_int>(bounds);
    values = utils::toInt(simd_float::round(uiRelated.scale * utils::toFloat(values)));
    return utils::bit_cast<Rectangle<i32>>(values);
  }
  strict_inline float unscaleValue(float value) { return value / uiRelated.scale; }

  inline void fillRect(NVGcontext *context, Rectangle<float> bounds,
    Colour colour = Colours::white, float cornerRounding = 0.0f)
  {
    nvgBeginPath(context);
    if (cornerRounding == 0.0f)
      nvgRect(context, bounds.x, bounds.y, bounds.w, bounds.h);
    else
      nvgRoundedRect(context, bounds.x, bounds.y, bounds.w, bounds.h, cornerRounding);
    nvgFillColor(context, colour);
    nvgFill(context);
  }

  inline void fillRect(NVGcontext *context, Rectangle<float> bounds,
    Colour colour, float topLeftRounding, float topRightRounding, 
    float bottomLeftRounding, float bottomRightRounding)
  {
    nvgBeginPath(context);
    nvgRoundedRectVarying(context, bounds.x, bounds.y, bounds.w, bounds.h,
      topLeftRounding, topRightRounding, bottomRightRounding, bottomLeftRounding);
    nvgFillColor(context, colour);
    nvgFill(context);
  }

  inline void strokeRect(NVGcontext *context, Rectangle<float> bounds,
    float thickness, Colour colour = Colours::white, float cornerRounding = 0.0f)
  {
    // stroke paints around the specified lines, 
    // therefore we need to shrink by half the thickess to get the result we need
    bounds.trim(thickness * 0.5f);

    nvgBeginPath(context);
    if (cornerRounding == 0.0f)
      nvgRect(context, bounds.x, bounds.y, bounds.w, bounds.h);
    else
      nvgRoundedRect(context, bounds.x, bounds.y, bounds.w, bounds.h, cornerRounding);

    nvgStrokeWidth(context, thickness);
    nvgStrokeColor(context, colour);
    nvgStroke(context);
  }

  inline void strokePolygon(NVGcontext *context, float thickness,
    utils::span<const Point<float>> points, Colour colour = Colours::white)
  {
    if (points.size() < 2)
      return;

    nvgBeginPath(context);
    nvgMoveTo(context, points[0].x, points[0].y);

    for (usize i = 1; i < points.size(); ++i)
      nvgLineTo(context, points[i].x, points[i].y);
    nvgClosePath(context);

    nvgStrokeColor(context, colour);
    nvgStrokeWidth(context, thickness);
    nvgStroke(context);
  }

  inline void renderText(utils::string_view text, FontId font,
    Rectangle<float> bounds, Graphics &context, Colour colour, 
    Placement placement = Placement::centered, bool wrapText = false)
  {
    nvgBeginPath(context.context);
    context.setFont(font, (float)bounds.h);
    float ascent, lineHeight;
    nvgFillColor(context.context, colour);
    nvgTextMetrics(context.context, &ascent, nullptr, &lineHeight);
    int alignmentFlags = NVG_ALIGN_BASELINE;
    alignmentFlags |= ((placement == Placement::left) ? NVG_ALIGN_LEFT : 
      (placement == Placement::right) ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER);
    nvgTextAlign(context.context, alignmentFlags);

    auto x = (float)bounds.x;
    x += ((placement == Placement::left) ? 0.0f :
      (placement == Placement::right) ? (float)bounds.w : (float)bounds.w * 0.5f);
    auto y = ::ceilf((float)bounds.y + ((float)bounds.h - lineHeight) * 0.5f + ascent);
    if (wrapText)
      nvgTextBox(context.context, x, y, x + (float)bounds.w, text.data(), text.data() + text.size());
    else
      nvgText(context.context, x, y, text.data(), text.data() + text.size());
  }

  struct ViewportChange
  {
    Component *component = nullptr;
    Rectangle<int> change{};
    bool isClipping = true;

    constexpr bool operator==(const ViewportChange &) const noexcept = default;
  };

  class Shaders;

  struct OpenGlWrapper
  {
    utils::vector<ViewportChange> parentStack{};
    Shaders *shaders = nullptr;
    Graphics *cache = nullptr;
    NVGcontext *g = nullptr;
    int topLevelHeight = 0;
    bool isDestroyingOpenGl = false;

    operator NVGcontext *() { return g; }
    operator Graphics &() { return *cache; }
  };

  bool setViewport(Point<int> positionInViewport, Rectangle<int> viewportBounds,
    Rectangle<int> scissorBounds, const OpenGlWrapper &openGl, const Component *ignoreClipIncluding);

#if COMPLEX_DEBUG
  void checkGLError(const char *file, const int line);
  #define COMPLEX_CHECK_OPENGL_ERROR() checkGLError(__FILE__, __LINE__)
#else
  #define COMPLEX_CHECK_OPENGL_ERROR()
#endif

  void drawSVG(NSVGimage *image, Graphics &g, Colour colour,
    Rectangle<float> bounds, float strokeWidth);

  namespace Paths
  {
    using DrawingFn = void(Graphics &g, utils::span<const Colour> colours,
      Rectangle<float> bounds, float strokeWidth);

    void enterValueIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);

    void filterIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);
    void dynamicsIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);
    void phaseIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);
    void pitchIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);
    void destroyIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours);

    utils::pair<DrawingFn *, Rectangle<i32>> copyNormalisedValueIcon();
    utils::pair<DrawingFn *, Rectangle<i32>> copyScaledValueIcon();
    utils::pair<DrawingFn *, Rectangle<i32>> pasteValueIcon();
    utils::pair<DrawingFn *, Rectangle<i32>> powerButtonIcon();
  }
}