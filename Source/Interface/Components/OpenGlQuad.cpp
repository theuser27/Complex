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

  OpenGlMultiQuad::OpenGlMultiQuad(usize maxQuads, Shaders::FragmentShader shader, String name) :
    OpenGlComponent{ COMPLEX_MOVE(name) }, fragmentShader_{ shader }, maxQuads_{ maxQuads }, numQuads_{ maxQuads }
  {
    data_ = utils::shared_value<float[]>{ maxQuads * kNumFloatsPerQuad };
    auto quadData = getQuadData();
    for (usize i = 0; i < maxQuads; i++)
    {
      quadData.setCoordinates(i, -1.0f, -1.0f, 2.0f, 2.0f);
      quadData.setShaderValue(i, 1.0f);
    }

    setInterceptsMouseClicks(false, false);
  }

  OpenGlMultiQuad::~OpenGlMultiQuad() { destroy(); }

  static void updateDimensions(utils::shared_value<float[]>::span &data, 
    Rectangle<int> viewportBounds, usize numQuads)
  {
    float fullWidth = (float)viewportBounds.getWidth();
    float fullHeight = (float)viewportBounds.getHeight();

    for (usize i = 0; i < numQuads; ++i)
    {
      usize index = i * OpenGlMultiQuad::kNumFloatsPerQuad;
      float quadWidth = data[2 * OpenGlMultiQuad::kNumFloatsPerVertex + index] - data[index];
      float quadHeight = data[2 * OpenGlMultiQuad::kNumFloatsPerVertex + index + 1] - data[index + 1];
      float w = quadWidth * fullWidth / 2.0f;
      float h = quadHeight * fullHeight / 2.0f;
      data[index + 2] = w;
      data[index + 3] = h;
      data[OpenGlMultiQuad::kNumFloatsPerVertex + index + 2] = w;
      data[OpenGlMultiQuad::kNumFloatsPerVertex + index + 3] = h;
      data[2 * OpenGlMultiQuad::kNumFloatsPerVertex + index + 2] = w;
      data[2 * OpenGlMultiQuad::kNumFloatsPerVertex + index + 3] = h;
      data[3 * OpenGlMultiQuad::kNumFloatsPerVertex + index + 2] = w;
      data[3 * OpenGlMultiQuad::kNumFloatsPerVertex + index + 3] = h;
    }
  }

  void OpenGlMultiQuad::init(OpenGlWrapper &openGl)
  {
    static constexpr int triangles[] =
    {
      0, 1, 2,
      2, 3, 0
    };

    COMPLEX_ASSERT(!isInitialised_.load(std::memory_order_acquire), "Init method more than once");

    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    auto maxQuads = maxQuads_.get();
    GLsizeiptr vertSize = (GLsizeiptr)(maxQuads * kNumFloatsPerQuad * sizeof(float));
    
    {
      auto data = data_.read();
      BaseComponent *component = targetComponent_ ? targetComponent_ : this;
      auto customViewportBounds = customViewportBounds_.get();
      auto viewportBounds = (!customViewportBounds.isEmpty()) ? customViewportBounds : component->getLocalBoundsSafe();
      updateDimensions(data, viewportBounds, numQuads_);
      glBufferData(GL_ARRAY_BUFFER, vertSize, data.data(), GL_STATIC_DRAW);
    }

    glGenBuffers(1, &indicesBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    GLsizeiptr barSize = (GLsizeiptr)(maxQuads * kNumIndicesPerQuad * sizeof(int));
    auto indices = utils::up<int[]>::create(maxQuads * kNumIndicesPerQuad);
    for (usize i = 0; i < maxQuads; i++)
      for (usize j = 0; j < kNumIndicesPerQuad; j++)
        indices[i * kNumIndicesPerQuad + j] = triangles[j] + (int)i * (int)kNumVertices;
    
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, barSize, indices.get(), GL_STATIC_DRAW);

    shader_ = openGl.shaders->getShaderProgram(Shaders::kPassthroughVertex, fragmentShader_);
    shader_.use();
    colorUniform_ = getUniform(shader_, "color");
    altColorUniform_ = getUniform(shader_, "alt_color");
    modColorUniform_ = getUniform(shader_, "mod_color");
    backgroundColorUniform_ = getUniform(shader_, "background_color");
    thumbColorUniform_ = getUniform(shader_, "thumb_color");
    position_ = getAttribute(shader_, "position");
    dimensions_ = getAttribute(shader_, "dimensions");
    coordinates_ = getAttribute(shader_, "coordinates");
    shader_values_ = getAttribute(shader_, "shader_values");
    thicknessUniform_ = getUniform(shader_, "thickness");
    roundingUniform_ = getUniform(shader_, "rounding");
    maxArcUniform_ = getUniform(shader_, "max_arc");
    thumbAmountUniform_ = getUniform(shader_, "thumb_amount");
    startPositionUniform_ = getUniform(shader_, "start_pos");
    overallAlphaUniform_ = getUniform(shader_, "overall_alpha");

    isInitialised_.store(true, std::memory_order_release);
  }

  void OpenGlMultiQuad::destroy()
  {
    if (!isInitialised_.load(std::memory_order_acquire))
      return;

    shader_ = {};
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

    auto numQuads = numQuads_.get();
    if (data_.hasUpdate())
    {
      auto data = data_.read();
      updateDimensions(data, viewportBounds, numQuads);

      glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

      GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumFloatsPerQuad * maxQuads_ * sizeof(float));
      glBufferData(GL_ARRAY_BUFFER, vertSize, data.data(), GL_STATIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    shader_.use();

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
    glDrawElements(GL_TRIANGLES, (GLsizei)(numQuads * kNumIndicesPerQuad), GL_UNSIGNED_INT, nullptr);

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
