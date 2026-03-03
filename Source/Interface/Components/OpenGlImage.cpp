/*
  ==============================================================================

    OpenGlImage.cpp
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlImage.hpp"

#include "nanovg/nanovg_gl_utils.h"

#include "Interface/LookAndFeel/Graphics.hpp"

namespace Interface
{
  static constexpr int kNumPositions = 16;
  static constexpr int kNumTriangleIndices = 6;

  static constexpr int kPositionTriangles[] = 
  {
    0, 1, 2,
    2, 3, 0
  };

  static void destroyImage(OpenGlImage *image)
  {
    // preparing the image for next time if openGl reinitialises this object
    image->shouldReloadImage = true;

    if (image->vertexShader.indicesBuffer)
      glDeleteBuffers(1, &image->vertexShader.indicesBuffer);
    if (image->fragmentShader.imageId)
      glDeleteTextures(1, &image->fragmentShader.imageId);
    image->vertexShader = {};
    image->fragmentShader = {};

    image->componentFlags.isOpenGlInitialised = false;
    image->componentFlags.isDestroyingOpenGl = false;
  }

  OpenGlImage::OpenGlImage()
  {
    ::memcpy(vertices, vertexShader.KInitPositionVertices, 
      vertexShader.kNumPositions * sizeof(float));
  }

  void OpenGlImage::redrawImage(Rectangle<int> redrawArea, bool forceRedraw)
  {
    auto bounds = getLocalBounds();
    int width = bounds.w;
    int height = bounds.h;
    if (width <= 0 || height <= 0)
      return;

    if (redrawArea == Rectangle<int>{})
      redrawArea = { width, height };

    auto &drawImage = drawImage_.lock();
    bool newImage = drawImage.isNull() || drawImage.getWidth() != width || drawImage.getHeight() != height;
    if (!newImage && !forceRedraw)
      return;

    if (newImage)
    {
      drawImage = Image{ Image::ARGB, width, height, false };
    }

    if (clearOnRedraw)
      drawImage.clear(redrawArea);

    Graphics g{ drawImage };
    if (paintFunction)
      paintFunction(g, redrawArea);
    else
      paintToImage(g, component);

    drawImage_.unlock();
    shouldReloadImage = true;
  }

  bool OpenGlImage::render(OpenGlWrapper &openGl)
  {
    if (componentFlags.isOpenGlInitialised && componentFlags.isDestroyingOpenGl)
    {
      destroyImage(this);

      return true;
    }

    if (!componentFlags.isOpenGlInitialised)
    {
      glGenBuffers(1, &vertexShader.vertexBuffer);
      glBindBuffer(GL_ARRAY_BUFFER, vertexShader.vertexBuffer);

      glBufferData(GL_ARRAY_BUFFER, kNumPositions * sizeof(float), vertices, GL_STATIC_DRAW);

      glGenBuffers(1, &vertexShader.indicesBuffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexShader.indicesBuffer);

      glBufferData(GL_ELEMENT_ARRAY_BUFFER, kNumTriangleIndices * sizeof(float), kPositionTriangles, GL_STATIC_DRAW);

      vertexShader.shaderId = openGl.shaders->addVertexShader(vertexShader.key, vertexShader.code);
      fragmentShader.shaderId = openGl.shaders->addVertexShader(fragmentShader.key, fragmentShader.code);
      shaderProgram = openGl.shaders->getShaderProgram(vertexShader.shaderId, fragmentShader.shaderId);

      shaderProgram.use();
      vertexShader.getVariables(shaderProgram);
      fragmentShader.getVariables(shaderProgram);

      componentFlags.isOpenGlInitialised = true;
    }

    if (!setViewport(getPosition(), getLocalBounds(), 
      getLocalBounds(), openGl, ignoreClipIncluding))
      return false;

    if (shouldReloadImage)
    {
      auto &drawImage = drawImage_.lock();
      auto temp = loadImageAsTexture(fragmentShader.imageId, drawImage, GL_LINEAR);
      textureWidth = temp.first;
      textureHeight = temp.second;
      shouldReloadImage = false;
      drawImage_.unlock();
    }

    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);

    if (additiveBlending)
      glBlendFunc(GL_ONE, GL_ONE);
    else if (useAlpha)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (isDirty)
    {
      glBindBuffer(GL_ARRAY_BUFFER, vertexShader.vertexBuffer);
      glBufferData(GL_ARRAY_BUFFER, kNumPositions * sizeof(float), vertices, GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexShader.indicesBuffer);
    glBindTexture(GL_TEXTURE_2D, fragmentShader.imageId);
    glActiveTexture(GL_TEXTURE0);

    shaderProgram.use();
    vertexShader.enableAttributes();
    fragmentShader.setUniforms();

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    vertexShader.disableAttributes();
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }
}
