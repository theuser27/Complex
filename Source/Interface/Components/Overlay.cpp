/*
  ==============================================================================

    Overlay.cpp
    Created: 14 Dec 2022 5:11:59am
    Author:  theuser27

  ==============================================================================
*/

#include "Overlay.h"


namespace Interface
{
  using namespace juce::gl;

  OverlayBackgroundRenderer::OverlayBackgroundRenderer()
  {
    data_[0] = -1.0f;
    data_[1] = -1.0f;
    data_[2] = -1.0f;
    data_[3] = 1.0f;
    data_[4] = 1.0f;
    data_[5] = -1.0f;
    data_[6] = 1.0f;
    data_[7] = 1.0f;

    indices_[0] = 0;
    indices_[1] = 1;
    indices_[2] = 2;
    indices_[3] = 1;
    indices_[4] = 2;
    indices_[5] = 3;

    setInterceptsMouseClicks(false, false);
  }

  void OverlayBackgroundRenderer::init(OpenGlWrapper &openGl)
  {
    glGenBuffers(1, &dataBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, dataBuffer_);

    GLsizeiptr vert_size = static_cast<GLsizeiptr>(kTotalFloats * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, vert_size, data_, GL_STATIC_DRAW);

    glGenBuffers(1, &indicesBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    GLsizeiptr bar_size = static_cast<GLsizeiptr>(kIndices * sizeof(int));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bar_size, indices_, GL_STATIC_DRAW);

    shader_ = openGl.shaders->getShaderProgram(Shaders::kPassthroughVertex, Shaders::kColorFragment);
    shader_->use();
    colorUniform_ = getUniform(*shader_, "color");
    position_ = getAttribute(*shader_, "position");
  }

  void OverlayBackgroundRenderer::render(OpenGlWrapper &openGl, [[maybe_unused]] bool animate)
  {
    drawOverlay(openGl);
  }

  void OverlayBackgroundRenderer::destroy()
  {
    shader_ = nullptr;
    position_ = nullptr;
    colorUniform_ = nullptr;
    glDeleteBuffers(1, &dataBuffer_);
    glDeleteBuffers(1, &indicesBuffer_);

    dataBuffer_ = 0;
    indicesBuffer_ = 0;
  }

  void OverlayBackgroundRenderer::drawOverlay(OpenGlWrapper &openGl)
  {
    if (!setViewPort(openGl))
      return;

    if (shader_ == nullptr)
      init(openGl);

    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    if (additiveBlending_)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_->use();
    colorUniform_->set(color_.getFloatRed(), color_.getFloatGreen(),
      color_.getFloatBlue(), color_.getFloatAlpha());

    glBindBuffer(GL_ARRAY_BUFFER, dataBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    glVertexAttribPointer(position_->attributeID, kNumFloatsPerVertex, GL_FLOAT,
      GL_FALSE, kNumFloatsPerVertex * sizeof(float), nullptr);
    glEnableVertexAttribArray(position_->attributeID);

    glDrawElements(GL_TRIANGLES, kIndices, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(position_->attributeID);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }

  Overlay::Overlay(std::string_view name) : BaseSection(name)
  {
    background_ = makeOpenGlComponent<OverlayBackgroundRenderer>();
    setSkinOverride(Skin::kOverlay);
    addOpenGlComponent(background_);
  }
}
