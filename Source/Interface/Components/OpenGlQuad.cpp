/*
  ==============================================================================

    OpenGlQuad.cpp
    Created: 14 Dec 2022 2:05:11am
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlQuad.hpp"

namespace Interface
{
  using namespace juce::gl;

  OpenGlMultiQuad::OpenGlMultiQuad(int maxQuads, Shaders::FragmentShader shader, String name) :
    OpenGlComponent(std::move(name)), fragmentShader_(shader), maxQuads_(maxQuads), numQuads_(maxQuads)
  {
    static const int triangles[] =
    {
      0, 1, 2,
      2, 3, 0
    };

    data_ = std::make_unique<float[]>(maxQuads_ * kNumFloatsPerQuad);
    indices_ = std::make_unique<int[]>(maxQuads_ * kNumIndicesPerQuad);
    vertexBuffer_ = 0;
    indicesBuffer_ = 0;

    modColor_ = Colours::transparentBlack;

    for (int i = 0; i < (int)maxQuads_; i++)
    {
      setCoordinates(i, -1.0f, -1.0f, 2.0f, 2.0f);
      setShaderValue(i, 1.0f);

      for (int j = 0; j < (int)kNumIndicesPerQuad; j++)
        indices_[i * kNumIndicesPerQuad + j] = triangles[j] + i * (int)kNumVertices;
    }

    setInterceptsMouseClicks(false, false);
  }

  OpenGlMultiQuad::~OpenGlMultiQuad() { destroy(); }

  void OpenGlMultiQuad::init(OpenGlWrapper &openGl)
  {
    COMPLEX_ASSERT(!isInitialised_.load(std::memory_order_acquire), "Init method more than once");

    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    auto data = data_.lock();
    GLsizeiptr vertSize = (GLsizeiptr)(maxQuads_ * kNumFloatsPerQuad * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, vertSize, data, GL_STATIC_DRAW);
    data_.unlock();

    glGenBuffers(1, &indicesBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    GLsizeiptr barSize = (GLsizeiptr)(maxQuads_ * kNumIndicesPerQuad * sizeof(int));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, barSize, indices_.get(), GL_STATIC_DRAW);

    shader_ = openGl.shaders->getShaderProgram(Shaders::kPassthroughVertex, fragmentShader_);
    shader_->use();
    colorUniform_ = getUniform(*shader_, "color");
    altColorUniform_ = getUniform(*shader_, "alt_color");
    modColorUniform_ = getUniform(*shader_, "mod_color");
    backgroundColorUniform_ = getUniform(*shader_, "background_color");
    thumbColorUniform_ = getUniform(*shader_, "thumb_color");
    position_ = getAttribute(*shader_, "position");
    dimensions_ = getAttribute(*shader_, "dimensions");
    coordinates_ = getAttribute(*shader_, "coordinates");
    shader_values_ = getAttribute(*shader_, "shader_values");
    thicknessUniform_ = getUniform(*shader_, "thickness");
    roundingUniform_ = getUniform(*shader_, "rounding");
    maxArcUniform_ = getUniform(*shader_, "max_arc");
    thumbAmountUniform_ = getUniform(*shader_, "thumb_amount");
    startPositionUniform_ = getUniform(*shader_, "start_pos");
    overallAlphaUniform_ = getUniform(*shader_, "overall_alpha");
    valuesUniform_ = getUniform(*shader_, "static_values");

    isInitialised_.store(true, std::memory_order_release);
  }

  void OpenGlMultiQuad::destroy()
  {
    if (!isInitialised_.load(std::memory_order_acquire))
      return;

    shader_ = nullptr;
    position_ = {};
    dimensions_ = {};
    coordinates_ = {};
    shader_values_ = {};
    colorUniform_ = {};
    altColorUniform_ = {};
    modColorUniform_ = {};
    thumbColorUniform_ = {};
    thicknessUniform_ = {};
    roundingUniform_ = {};
    maxArcUniform_ = {};
    thumbAmountUniform_ = {};
    startPositionUniform_ = {};
    overallAlphaUniform_ = {};
    valuesUniform_ = {};
    if (vertexBuffer_)
      pushResourcesForDeletion(OpenGlAllocatedResource::Buffer, 1, vertexBuffer_);
    if (indicesBuffer_)
      pushResourcesForDeletion(OpenGlAllocatedResource::Buffer, 1, indicesBuffer_);

    vertexBuffer_ = 0;
    indicesBuffer_ = 0;

    isInitialised_.store(false, std::memory_order_release);
  }

  void OpenGlMultiQuad::render(OpenGlWrapper &openGl)
  {
    BaseComponent *component = targetComponent_ ? targetComponent_ : this;
    auto customViewportBounds = customViewportBounds_.get();
    auto customScissorBounds = customScissorBounds_.get();
    auto viewportBounds = (!customViewportBounds.isEmpty()) ? customViewportBounds : component->getLocalBoundsSafe();
    auto scissorBounds = (!customScissorBounds.isEmpty()) ? customScissorBounds : viewportBounds;
    float overallAlpha = overallAlpha_;
    if (!active_ || (!drawWhenNotVisible_ && !component->isVisibleSafe()) || (overallAlpha == 0.0f) ||
      !setViewPort(*component, *this, viewportBounds, scissorBounds, openGl, ignoreClipIncluding_))
      return;

    // setup
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    if (additiveBlending_)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (dirty_)
    {
      for (u32 i = 0; i < numQuads_; ++i)
        setDimensions(i, getQuadWidth(i), getQuadHeight(i), 
          (float)viewportBounds.getWidth(), (float)viewportBounds.getHeight());

      glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

      auto data = data_.lock();
      GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumFloatsPerQuad * maxQuads_ * sizeof(float));
      glBufferData(GL_ARRAY_BUFFER, vertSize, data, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      data_.unlock();

      dirty_ = false;
    }

    shader_->use();

    if (overallAlphaUniform_)
      overallAlphaUniform_.set(overallAlpha);

    auto colour = color_.get();
    colorUniform_.set(colour.getFloatRed(), colour.getFloatGreen(),
      colour.getFloatBlue(), colour.getFloatAlpha());

    if (altColorUniform_)
    {
      auto altColour = altColor_.get();
      altColorUniform_.set(altColour.getFloatRed(), altColour.getFloatGreen(),
        altColour.getFloatBlue(), altColour.getFloatAlpha());
    }

    if (modColorUniform_)
    {
      auto modColour = modColor_.get();
      modColorUniform_.set(modColour.getFloatRed(), modColour.getFloatGreen(),
        modColour.getFloatBlue(), modColour.getFloatAlpha());
    }

    if (backgroundColorUniform_)
    {
      auto backgroundColour = backgroundColor_.get();
      backgroundColorUniform_.set(backgroundColour.getFloatRed(), backgroundColour.getFloatGreen(),
        backgroundColour.getFloatBlue(), backgroundColour.getFloatAlpha());
    }

    if (thumbColorUniform_)
    {
      auto thumbColour = thumbColor_.get();
      thumbColorUniform_.set(thumbColour.getFloatRed(), thumbColour.getFloatGreen(),
        thumbColour.getFloatBlue(), thumbColour.getFloatAlpha());
    }

    if (thumbAmountUniform_)
      thumbAmountUniform_.set(thumbAmount_);

    if (startPositionUniform_)
      startPositionUniform_.set(startPosition_);

    if (thicknessUniform_)
      thicknessUniform_.set(thickness_);

    if (roundingUniform_)
      roundingUniform_.set(rounding_);
    if (maxArcUniform_)
      maxArcUniform_.set(maxArc_);

    if (valuesUniform_)
    {
      auto values = values_.get();
      valuesUniform_.set(values[0], values[1], values[2], values[3]);
    }

    COMPLEX_CHECK_OPENGL_ERROR;

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    glVertexAttribPointer(position_.attributeId, 2, GL_FLOAT, GL_FALSE, kNumFloatsPerVertex * sizeof(float), nullptr);
    glEnableVertexAttribArray(position_.attributeId);
    if (dimensions_)
    {
      glVertexAttribPointer(dimensions_.attributeId, 2, GL_FLOAT, GL_FALSE, 
        kNumFloatsPerVertex * sizeof(float), (GLvoid *)(2 * sizeof(float)));
      glEnableVertexAttribArray(dimensions_.attributeId);
    }
    if (coordinates_)
    {
      glVertexAttribPointer(coordinates_.attributeId, 2, GL_FLOAT, GL_FALSE,
        kNumFloatsPerVertex * sizeof(float), (GLvoid *)(4 * sizeof(float)));
      glEnableVertexAttribArray(coordinates_.attributeId);
    }
    if (shader_values_)
    {
      glVertexAttribPointer(shader_values_.attributeId, 4, GL_FLOAT, GL_FALSE, 
        kNumFloatsPerVertex * sizeof(float), (GLvoid *)(6 * sizeof(float)));
      glEnableVertexAttribArray(shader_values_.attributeId);
    }

    // render
    glDrawElements(GL_TRIANGLES, (GLsizei)(numQuads_ * kNumIndicesPerQuad), GL_UNSIGNED_INT, nullptr);

    // clean-up
    glDisableVertexAttribArray(position_.attributeId);
    if (dimensions_)
      glDisableVertexAttribArray(dimensions_.attributeId);
    if (coordinates_)
      glDisableVertexAttribArray(coordinates_.attributeId);
    if (shader_values_)
      glDisableVertexAttribArray(shader_values_.attributeId);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }
}
