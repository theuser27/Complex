/*
  ==============================================================================

    OpenGlImage.hpp
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/shader_types.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  // Drawing class that acquires its logic from the paint method
  // of the currently linked component (i.e. the owner of the object)
  // and draws it on an owned image resource
  class OpenGlImage : public Component
  {
  public:
    OpenGlImage();

    void redrawImage(Rectangle<int> redrawArea = {}, bool forceRedraw = true);

    bool render(OpenGlWrapper &openGl) override;

    void setVertexPosition(usize index, float x, float y)
    {
      vertices[index] = x;
      vertices[index + 1] = y;
    }

    void movePosition(float x, float y)
    {
      vertices[0]  = -1.0f + x;
      vertices[1]  =  1.0f + y;
      vertices[4]  = -1.0f + x;
      vertices[5]  = -1.0f + y;
      vertices[8]  =  1.0f + x;
      vertices[9]  = -1.0f + y;
      vertices[12] =  1.0f + x;
      vertices[13] =  1.0f + y;
    }

    ImageVertex vertexShader{};
    TintedImageFragment fragmentShader{};
    OpenGlShaderProgram shaderProgram{};

    u64 textureId{};
    Rectangle<i32> textureBoundsInFramebuffer{};
    
    float vertices[ImageVertex::kNumPositions];

    utils::smallFn<void(Graphics &, Rectangle<i32>)> paintFunction{};
    Component *ignoreClipIncluding = nullptr;
    bool isDirty = false;
    bool additiveBlending = false;
    bool useAlpha = false;
    bool shouldReloadImage = false;
    bool clearOnRedraw = true;
  };

  struct PlainShapeComponent final : public Component
  {
    void (*draw)(OpenGlWrapper &openGl, Component *reference, Component *self) = nullptr;
    Component *reference = nullptr;

    bool render(OpenGlWrapper &openGl)
    {
      draw(openGl, reference, this);
      return true;
    }
  };
}
