
// Created: 2023-06-23 19:07:51

#include "Graphics.hpp"

#define NANOVG_GL3
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"

#include "nanovg/nanosvg.h"

#include "Data/BinaryData.hpp"
#include "Framework/platform.hpp"
#include "Framework/utils.hpp"

namespace Interface
{
  Graphics::Graphics()
  {
    context = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES
    #if COMPLEX_DEBUG
      | NVG_DEBUG
    #endif
    );

    if (!context)
    {
      showNativeMessageBox("Graphics Initialisation",
        "Couldn't create nanovg context.", MessageBoxType::Error);
      COMPLEX_TRAP();
    }

    DDinFontId = nvgCreateFontMem(context, "DDin",
      (unsigned char *)BinaryData::D_DIN_Bold_otf, (int)BinaryData::D_DIN_Bold_otfSize, false);
    InterFontId = nvgCreateFontMem(context, "DDin",
      (unsigned char *)BinaryData::Inter_Medium_ttf, (int)BinaryData::Inter_Medium_ttfSize, false);

    // TODO: how big should the framebuffer be
    planeArea = { 640, 480 };
    textureFBO = nvgluCreateFramebuffer(context, planeArea.w, planeArea.h, 0);
  }

  Graphics::~Graphics()
  {
    nvgluDeleteFramebuffer(textureFBO);
    nvgDeleteGL3(context);
  }

  static constexpr float kDDinDefaultHeight = 11.5f;
  static constexpr float kInterDefaultHeight = 10.5f;

  static constexpr float kDDinDefaultKerning = 0.0f;
  static constexpr float kInterDefaultKerning = 0.0f;

  float
  Graphics::getFontLineHeightFromFontHeight(FontId fontId, float height)
  {
    if (fontId == FontId::DDinType)
      return height * 8.0f / kDDinDefaultHeight;
    if (fontId == FontId::InterType)
      return height * 8.0f / kInterDefaultHeight;

    COMPLEX_ASSERT_FALSE("Unknown font was provided to get ascent for");
    return 1.0f;
  }

  float
  Graphics::getFontKerningFromFontHeight(FontId fontId, float height)
  {
    if (fontId == FontId::DDinType)
      return height / kDDinDefaultHeight * kDDinDefaultKerning;
    else if (fontId == FontId::InterType)
      return height / kInterDefaultHeight * kInterDefaultKerning;

    COMPLEX_ASSERT_FALSE("Unknown font to get kerning for");

    return kDDinDefaultKerning;
  }

  float
  Graphics::getFontHeightFromLineHeight(FontId fontId, float lineHeight)
  {
    if (fontId == FontId::DDinType)
      return lineHeight * kDDinDefaultHeight / 16.0f;
    if (fontId == FontId::InterType)
      return lineHeight * kInterDefaultHeight / 16.0f;

    COMPLEX_ASSERT_FALSE("Unknown font was provided to get height for");
    return 1.0f;
  }

  void Graphics::setFontHeight(float height) const
  {
    nvgFontSize(context, height);

    int id = nvgGetFontFaceId(context);
    float kerning = 0.0f;

    if (id == DDinFontId)
      kerning = kDDinDefaultKerning * (height / kDDinDefaultHeight);
    else if (id == InterFontId)
      kerning = kInterDefaultKerning * (height / kInterDefaultHeight);
    else COMPLEX_ASSERT_FALSE("Unknown font was provided to set");

    nvgTextLetterSpacing(context, kerning);
  }

  void Graphics::setFont(FontId fontid, float lineHeight)
  {
    float height = getFontHeightFromLineHeight(fontid, lineHeight);
    float kerning = getFontKerningFromFontHeight(fontid, height);

    int id = (fontid == FontId::DDinType) ? DDinFontId : InterFontId;
    nvgFontFaceId(context, id);
    nvgFontSize(context, height);
    nvgTextLetterSpacing(context, kerning);
    nvgTextAlign(context, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  }

  void Graphics::bindTextureFramebuffer()
  {
    nvgluBindFramebuffer(textureFBO);
  }
  void Graphics::unbindTextureFramebuffer()
  {
    nvgluBindFramebuffer(nullptr);
  }
  Rectangle<i32> Graphics::reserveSlotForTexture([[maybe_unused]] u64 id, [[maybe_unused]] Area<i32> desiredArea)
  {

    //for (auto &[id_, bounds] : textureBounds.data)
    //{

    //}
    return Rectangle<i32>();
  }

  constinit thread_local InterfaceRelated uiRelated{};

  //void Shape::drawAll(Graphics &g, Rectangle<float> bounds, float scale,
  //  float strokeWidth, utils::span<Colour> colours) const
  //{
  //  for (usize i = 0; i < paths.size(); ++i)
  //  {
  //    auto &[fn, predefinedColour] = paths[i];
  //    auto colour = (predefinedColour == Colour{}) ? colours[i] : predefinedColour;
  //    fn(g, colour, bounds, scale, strokeWidth);
  //  }
  //}

  void drawSVG(NSVGimage *image, Graphics &g, Colour colour,
    Rectangle<float> bounds, float scale, float strokeWidth)
  {
    nvgSave(g);

    nvgStrokeWidth(g, strokeWidth);
    nvgStrokeColor(g, colour);

    if (scale == 0.0f)
      scale = utils::min(bounds.w / image->width, bounds.h / image->height);

    float x = bounds.x + ::roundf((bounds.w - image->width * scale) * 0.5f);
    float y = bounds.y + ::roundf((bounds.h - image->height * scale) * 0.5f);

    nvgBeginPath(g);
    for (NSVGshape *shape = image->shapes; shape != nullptr; shape = shape->next)
    {
      // important: implicitly casting NSVGlineCap to NVGlineCap, ensure the enum values correspond
      nvgLineCap(g, shape->strokeLineCap);
      for (NSVGpath *path = shape->paths; path != nullptr; path = path->next)
      {
        nvgMoveTo(g, x + scale * path->pts[0], y + scale * path->pts[1]);
        for (int i = 0; i < path->npts - 1; i += 3)
        {
          float *p = &path->pts[i * 2];
          nvgBezierTo(g,
            x + scale * p[2], y + scale * p[3],
            x + scale * p[4], y + scale * p[5],
            x + scale * p[6], y + scale * p[7]);
        }

        if (path->closed)
          nvgClosePath(g);
      }
    }
    nvgStroke(g);

    nvgRestore(g);
  }

  namespace Paths
  {
    void pasteValueIcon()
    {
      //static const Shape shape = []()
      //{
      //  Path one;
      //  one.startNewSubPath({ 4.5f, 11.5f });
      //  one.quadraticTo({ 3.0f, 12.0f }, { 3.5f, 10.5f });
      //  one.lineTo({ 3.5f, 4.5f });
      //  one.quadraticTo({ 3.0f, 3.0f }, { 4.5f, 3.5f });
      //  one.lineTo({ 5.5f, 3.5f });

      //  one.startNewSubPath({ 6.0f, 4.5f });
      //  one.quadraticTo({ 5.0f, 5.0f }, { 5.5f, 4.0f });
      //  one.lineTo({ 5.5f, 3.0f });
      //  one.quadraticTo({ 5.0f, 2.0f }, { 6.0f, 2.5f });
      //  one.lineTo({ 8.0f, 2.5f });
      //  one.quadraticTo({ 9.0f, 2.0f }, { 8.5f, 3.0f });
      //  one.lineTo({ 8.5f, 4.0f });
      //  one.quadraticTo({ 9.0f, 5.0f }, { 8.0f, 4.5f });
      //  one.closeSubPath();

      //  one.startNewSubPath({ 8.5f, 3.5f });
      //  one.lineTo({ 9.5f, 3.5f });
      //  one.quadraticTo({ 11.0f, 3.0f }, { 10.5f, 4.5f });

      //  one.addRoundedRectangle(juce::Rectangle{ 6.5f, 6.5f, 6.0f, 7.0f }, 1.5f);

      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(one), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();
    }

    void enterValueIcon()
    {
      //static const Shape shape = []()
      //{
      //  juce::Path one;
      //  one.startNewSubPath({ 3.5f, 12.5f });
      //  one.lineTo({ 3.5f, 11.0f });
      //  one.quadraticTo({ 3.0f, 10.0f }, { 4.0f, 10.0f });
      //  one.lineTo({ 8.5f, 5.5f });
      //  one.lineTo({ 10.5f, 7.5f });
      //  one.lineTo({ 6.0f, 12.0f });
      //  one.quadraticTo({ 6.0f, 13.0f }, { 5.0f, 12.5f });
      //  one.closeSubPath();

      //  juce::Path two;
      //  two.startNewSubPath({ 10.0f, 4.0f });
      //  two.quadraticTo({ 11.0f, 2.5f }, { 12.5f, 3.5f });
      //  two.quadraticTo({ 13.5f, 5.0f }, { 12.0f, 6.0f });
      //  two.closeSubPath();

      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(one), Shape::Stroke, juce::Colour{});
      //  result.paths.emplace_back(COMPLEX_MOVE(two), Shape::Fill, juce::Colour{});
      //  return result;
      //}();
    }



    void copyNormalisedValueIcon(Graphics &g, Rectangle<float> bounds,
      float strokeWidth, utils::span<Colour> colours)
    {
      COMPLEX_ASSERT(colours.size() >= 1);
      COMPLEX_ASSERT(bounds.w == bounds.y);

      static constexpr float size = 16.0f;



      nvgSave(g);

      nvgTranslate(g, bounds.x, bounds.y);
      nvgStrokeWidth(g, scaleValue(strokeWidth));
      nvgStrokeColor(g, colours[0]);

      nvgBeginPath(g);

      //nvgMoveTo(g, x + scale * path->pts[0], y + scale * path->pts[1]);

      //static const Shape shape = []()
      //{
      //  juce::Path path;
      //  path.startNewSubPath({ 3.5f, 6.0f });
      //  path.lineTo({ 3.5f, 11.0f });
      //  path.quadraticTo({ 3.5f, 12.5f }, { 5.0f, 12.5f });
      //  path.lineTo({ 10.0f, 12.5f });

      //  path.addRoundedRectangle(juce::Rectangle{ 5.5f, 3.5f, 6.0f, 7.0f }, 1.5f);
      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(path), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();

      nvgRestore(g);
    }

    void copyScaledValueIcon()
    {
      //static const Shape shape = []()
      //{
      //  static constexpr float fStartX = 9.5f;
      //  static constexpr float fStartY = 7.5f;
      //  static constexpr float fWidth = 2.0f;
      //  static constexpr float fHeight = 4.0f;
      //  static constexpr float fRounding = 1.0f;

      //  juce::Path one;
      //  one.startNewSubPath({ 3.5f, 6.0f });
      //  one.lineTo({ 3.5f, 11.0f });
      //  one.quadraticTo({ 3.5f, 12.5f }, { 5.0f, 12.5f });
      //  one.lineTo({ 10.0f, 12.5f });

      //  one.startNewSubPath({ 7.5f, 3.5f });
      //  one.lineTo({ 6.5f, 3.5f });
      //  one.quadraticTo({ 5.5f, 3.5f }, { 5.5f, 4.5f });
      //  one.lineTo({ 5.5f, 9.5f });
      //  one.quadraticTo({ 5.5f, 10.5f }, { 6.5f, 10.5f });
      //  one.lineTo({ 10.5f, 10.5f });
      //  one.quadraticTo({ 11.5f, 10.5f }, { 11.5f, 9.5f });
      //  one.lineTo({ 11.5f, 8.5f });

      //  juce::Path two;
      //  two.startNewSubPath({ fStartX, fStartY });
      //  two.lineTo({ fStartX, fStartY - (fHeight - fRounding) });
      //  two.quadraticTo({ fStartX, fStartY - fHeight }, { fStartX + fRounding, fStartY - fHeight });
      //  two.lineTo({ fStartX + fWidth, fStartY - fHeight });
      //  two.startNewSubPath({ fStartX, 5.5f });
      //  two.lineTo({ fStartX + fWidth, 5.5f });

      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(one), Shape::Stroke, juce::Colour{});
      //  result.paths.emplace_back(COMPLEX_MOVE(two), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();
    }

    void filterIcon()
    {
      //static const auto shape = []()
      //{
      //  Shape result;
      //  result.paths.emplace_back(fromSvgData(BinaryData::Icon_Filter_svg,
      //    BinaryData::Icon_Filter_svgSize), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();
    }

    void dynamicsIcon()
    {
      //static const auto shape = []()
      //{
      //  Shape result;
      //  result.paths.emplace_back(fromSvgData(BinaryData::Icon_Dynamics_svg,
      //    BinaryData::Icon_Dynamics_svgSize), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();
    }

    void phaseIcon()
    {
      //static const auto shape = []()
      //{
      //  Shape result;
      //  auto draw = [](Graphics &g, Colour colour,
      //    Rectangle<float> bounds, float strokeWidth)
      //  {
      //    static const auto path = Path{ BinaryData::Icon_Phase_svg,
      //      BinaryData::Icon_Phase_svgSize };
      //    path.drawPath(g, colour, bounds, strokeWidth);
      //  };
      //  result.paths.emplace_back(draw, Colour{});
      //  return result;
      //}();
    }

    void pitchIcon()
    {
      //static const auto shape = []()
      //{
      //  Shape result;
      //  auto draw = [](Graphics &g, Colour colour,
      //    Rectangle<float> bounds, float strokeWidth)
      //  {
      //    static const auto path = Path{ BinaryData::Icon_Pitch_svg,
      //      BinaryData::Icon_Pitch_svgSize };
      //    path.drawPath(g, colour, bounds, strokeWidth);
      //  };
      //  return result;
      //}();
    }

    void destroyIcon()
    {
      //static const auto shape = []()
      //{
      //  static constexpr float width = 9.0f;
      //  static constexpr float height = 10.0f;
      //  static constexpr float centerElementWidth = width / 3.0f;
      //  static constexpr float bodyX = 1.5f;
      //  static constexpr float bodyY = 3.0f;
      //  static constexpr float bodyWidth = width - 2.0f * bodyX;
      //  static constexpr float bodyHeight = height - bodyY;

      //  juce::Path strokePath;

      //  // hat
      //  strokePath.startNewSubPath(0.0f, 1.0f);
      //  strokePath.lineTo(width, 1.0f);
      //  strokePath.closeSubPath();
      //  strokePath.startNewSubPath((width - centerElementWidth) * 0.5f, 0.0f);
      //  strokePath.lineTo((width - centerElementWidth) * 0.5f + centerElementWidth, 0.0f);
      //  strokePath.closeSubPath();

      //  // body outline
      //  strokePath.addRoundedRectangle(bodyX, bodyY, bodyWidth, bodyHeight, 2.0f, 2.0f, false, false, true, true);
      //
      //  // vertical lines
      //  strokePath.startNewSubPath((width - centerElementWidth + 1.0f) * 0.5f, bodyY + 2.0f);
      //  strokePath.lineTo((width - centerElementWidth + 1.0f) * 0.5f, bodyY + bodyHeight - 2.0f);
      //  strokePath.closeSubPath();
      //  strokePath.startNewSubPath((width + centerElementWidth + 1.0f) * 0.5f - 1.0f, bodyY + 2.0f);
      //  strokePath.lineTo((width + centerElementWidth + 1.0f) * 0.5f - 1.0f, bodyY + bodyHeight - 2.0f);
      //  strokePath.closeSubPath();

      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(strokePath), Shape::Stroke, juce::Colour{});
      //  return result;
      //}();
    }

    void contrastIcon()
    {
      //static const auto shape = []()
      //{
      //  float width = 14.0f;
      //  float height = 14.0f;
      //  float rounding = 6.0f;

      //  juce::Path strokePath;
      //  juce::Path fillPath;

      //  strokePath.startNewSubPath(width - rounding, 0.0f);
      //  strokePath.quadraticTo(width, 0.0f, width, rounding);
      //  strokePath.lineTo(width, height - rounding);
      //  strokePath.quadraticTo(width, height, width - rounding, height);
      //  strokePath.lineTo(rounding, height);
      //  strokePath.quadraticTo(0.0f, height, 0.0f, height - rounding);
      //  strokePath.lineTo(0.0f, rounding);
      //  strokePath.quadraticTo(0.0f, 0.0f, rounding, 0.0f);
      //  strokePath.closeSubPath();

      //  fillPath.startNewSubPath(width * 0.5f, 0.0f);
      //  fillPath.lineTo(width - rounding, 0.0f);
      //  fillPath.quadraticTo(width, 0.0f, width, rounding);
      //  fillPath.lineTo(width, height - rounding);
      //  fillPath.quadraticTo(width, height, width - rounding, height);
      //  fillPath.lineTo(width * 0.5f, height);
      //  fillPath.closeSubPath();

      //  Shape result;
      //  result.paths.emplace_back(COMPLEX_MOVE(strokePath), Shape::Stroke, juce::Colour{});
      //  result.paths.emplace_back(COMPLEX_MOVE(fillPath), Shape::Fill, juce::Colour{});
      //  return result;
      //}();
    }

    utils::pair<DrawingFn *, Rectangle<i32>>
    powerButtonIcon()
    {
      return {};
      /*static const auto shape = []()
      {
        static constexpr float kAngle = 0.8f * k2Pi;
        static constexpr float kAngleStart = kPi - kAngle * 0.5f;
        juce::Path path;

        path.startNewSubPath(5.5f, 0.0f);
        path.lineTo(5.5f, 5.0f);
        path.closeSubPath();

        path.addArc(0.0f, 2.0f, 11.0f, 11.0f, kAngleStart, kAngle + kAngleStart, true);

        Shape result;
        result.paths.emplace_back(COMPLEX_MOVE(path), Shape::Stroke, juce::Colour{});
        return result;
      }();*/
    }
  }
}
