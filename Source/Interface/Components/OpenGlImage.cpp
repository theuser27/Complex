/*
  ==============================================================================

    OpenGlImage.cpp
    Created: 14 Dec 2022 2:18:45am
    Author:  theuser27

  ==============================================================================
*/

#include "../LookAndFeel/Shaders.h"
#include "OpenGlImage.h"
#include "OpenGlComponent.h"


namespace Interface
{
  using namespace juce::gl;

  static constexpr int kNumPositions = 16;
  static constexpr int kNumTriangleIndices = 6;

  OpenGlImage::OpenGlImage(String name) : OpenGlComponent(std::move(name))
  {
    positionVertices_ = std::make_unique<float[]>(kNumPositions);
    float position_vertices[kNumPositions] = 
    {
      0.0f, 1.0f, 0.0f, 1.0f,
      0.0f, -1.0f, 0.0f, 0.0f,
      0.1f, -1.0f, 1.0f, 0.0f,
      0.1f, 1.0f, 1.0f, 1.0f
    };
    memcpy(positionVertices_.get(), position_vertices, kNumPositions * sizeof(float));

    positionTriangles_ = std::make_unique<int[]>(kNumTriangleIndices);
    int position_triangles[kNumTriangleIndices] = 
    {
      0, 1, 2,
      2, 3, 0
    };
    memcpy(positionTriangles_.get(), position_triangles, kNumTriangleIndices * sizeof(int));
  }

  void OpenGlImage::init(OpenGlWrapper &openGl)
  {
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, vertSize, positionVertices_.get(), GL_STATIC_DRAW);

    glGenBuffers(1, &triangleBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);

    GLsizeiptr triSize = static_cast<GLsizeiptr>(kNumTriangleIndices * sizeof(float));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triSize, positionTriangles_.get(), GL_STATIC_DRAW);

    imageShader_ = openGl.shaders->getShaderProgram(Shaders::kImageVertex, Shaders::kTintedImageFragment);

    imageShader_->use();
    imageColor_ = getUniform(*imageShader_, "color");
    imagePosition_ = getAttribute(*imageShader_, "position");
    textureCoordinates_ = getAttribute(*imageShader_, "tex_coord_in");
  }

  void OpenGlImage::render([[maybe_unused]] OpenGlWrapper &openGl, [[maybe_unused]] bool animate)
  {
    mutex_.lock();
    if (image_)
    {
      texture_.loadImage(*image_);
      image_ = nullptr;
    }
    mutex_.unlock();

    glEnable(GL_BLEND);
    if (scissor_)
      glEnable(GL_SCISSOR_TEST);
    else
      glDisable(GL_SCISSOR_TEST);

    if (additive_)
      glBlendFunc(GL_ONE, GL_ONE);
    else if (useAlpha_)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    mutex_.lock();
    if (hasNewVertices_)
    {
      GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
    	glBufferData(GL_ARRAY_BUFFER, vertSize, positionVertices_.get(), GL_STATIC_DRAW);
      hasNewVertices_ = false;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);
    texture_.bind();
    glActiveTexture(GL_TEXTURE0);
    mutex_.unlock();

    imageShader_->use();

    imageColor_->set(color_.getFloatRed(), color_.getFloatGreen(), color_.getFloatBlue(), color_.getFloatAlpha());

    glVertexAttribPointer(imagePosition_->attributeID, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(imagePosition_->attributeID);

    glVertexAttribPointer(textureCoordinates_->attributeID, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid *)(2 * sizeof(float)));
    glEnableVertexAttribArray(textureCoordinates_->attributeID);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(imagePosition_->attributeID);
    glDisableVertexAttribArray(textureCoordinates_->attributeID);
    texture_.unbind();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }

  void OpenGlImage::destroy()
  {
    texture_.release();

    imageShader_ = nullptr;
    imageColor_ = nullptr;
    imagePosition_ = nullptr;
    textureCoordinates_ = nullptr;

    glDeleteBuffers(1, &vertexBuffer_);
    glDeleteBuffers(1, &triangleBuffer_);
  }
}