
// Created: 2023-06-23 19:07:51

#include "Graphics.hpp"

#define NANOVG_GL3
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"

#include "Data/BinaryData.hpp"
#include "Framework/platform_definitions.hpp"
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
}
