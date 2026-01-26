
// Created: 2023-06-23 19:07:51

#include "Graphics.hpp"

#define NANOVG_GL3
#include "nanovg/nanovg_gl.h"
#include "nanovg/nanovg_gl_utils.h"

#include "Data/BinaryData.h"
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
      (unsigned char *)BinaryData::DDINBold_ttf, BinaryData::DDINBold_ttfSize, false);
    InterFontId = nvgCreateFontMem(context, "DDin",
      (unsigned char *)BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize, false);

    // TODO: how big should the framebuffer be
    planeArea = { 640, 480 };
    textureFBO = nvgluCreateFramebuffer(context, planeArea.w, planeArea.h, 0);
  }

  Graphics::~Graphics()
  {
    nvgluDeleteFramebuffer(textureFBO);
    nvgDeleteGL3(context);
  }

  float Graphics::getFontAscentFromHeight(int fontId, float height) const noexcept
  {
    if (fontId == DDinFontId)
      return height * 8.0f / kDDinDefaultHeight;
    if (fontId == InterFontId)
      return height * 8.0f / kInterVDefaultHeight;

    COMPLEX_ASSERT_FALSE("Unknown font was provided to get ascent for");
    return 1.0f;
  }

  float Graphics::getFontHeightFromAscent(int fontId, float ascent) const noexcept
  {
    if (fontId == DDinFontId)
      return ascent * kDDinDefaultHeight / 8.0f;
    if (fontId == InterFontId)
      return ascent * kInterVDefaultHeight / 8.0f;

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
      kerning = kInterVDefaultKerning * (height / kInterVDefaultHeight);
    else COMPLEX_ASSERT_FALSE("Unknown font was provided to set");
    
    nvgTextLetterSpacing(context, kerning);
  }

  void Graphics::setFontHeightFromAscent(float ascent) const
  {
    setFontHeight(getFontHeightFromAscent(nvgGetFontFaceId(context), ascent));
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
