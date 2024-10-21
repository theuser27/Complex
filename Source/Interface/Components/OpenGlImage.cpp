/*
  ==============================================================================

    OpenGlImage.cpp
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlImage.hpp"

#include "Interface/LookAndFeel/Fonts.hpp"
#include "../Sections/BaseSection.hpp"

namespace Interface
{
  using namespace juce::gl;

  static constexpr int kNumPositions = 16;
  static constexpr int kNumTriangleIndices = 6;

  static constexpr int kPositionTriangles[] = 
  {
    0, 1, 2,
    2, 3, 0
  };

  OpenGlImage::OpenGlImage(String name) : OpenGlComponent{ std::move(name) }
  {
    positionVertices_ = std::make_unique<float[]>(kNumPositions);
    float positionVertices[kNumPositions] =
    {
      -1.0f,  1.0f, 0.0f, 1.0f,
      -1.0f, -1.0f, 0.0f, 0.0f,
       1.0f, -1.0f, 1.0f, 0.0f,
       1.0f,  1.0f, 1.0f, 1.0f
    };
    auto vertices = positionVertices_.lock();
    memcpy(vertices, positionVertices, kNumPositions * sizeof(float));
    positionVertices_.unlock();

    setInterceptsMouseClicks(false, false);
  }

  OpenGlImage::~OpenGlImage() { destroy(); }

  void OpenGlImage::redrawImage(juce::Rectangle<int> redrawArea, bool forceRedraw)
  {
    if (!isActive_)
      return;

    BaseComponent *component = targetComponent_ ? targetComponent_ : this;
    juce::Rectangle<int> customDrawBounds = customViewportBounds_.get();
    auto bounds = (!customDrawBounds.isEmpty()) ? customDrawBounds : component->getLocalBounds();
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    if (width <= 0 || height <= 0)
      return;

    if (redrawArea == juce::Rectangle<int>{})
      redrawArea = { width, height };

    auto *drawImage = drawImage_.lock();
    bool newImage = drawImage == nullptr || drawImage->getWidth() != width || drawImage->getHeight() != height;
    if (!newImage && !forceRedraw)
      return;

    if (newImage)
    {
      drawImage_.unlock();
      drawImage_ = std::make_unique<Image>(Image::ARGB, width, height, false);
      drawImage = drawImage_.lock();
    }

    if (clearOnRedraw_)
      drawImage->clear(redrawArea);

    Graphics g(*drawImage);
    if (paintFunction_)
      paintFunction_(g, redrawArea);
    else
      paintToImage(g, component);

    drawImage_.unlock();
    shouldReloadImage_ = true;
  }

  void OpenGlImage::init(OpenGlWrapper &openGl)
  {
    COMPLEX_ASSERT(!isInitialised_.load(std::memory_order_acquire), "Init method more than once");

    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
    auto vertices = positionVertices_.lock();
    glBufferData(GL_ARRAY_BUFFER, vertSize, vertices, GL_STATIC_DRAW);
    positionVertices_.unlock();

    glGenBuffers(1, &triangleBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);

    GLsizeiptr triSize = static_cast<GLsizeiptr>(kNumTriangleIndices * sizeof(float));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triSize, kPositionTriangles, GL_STATIC_DRAW);

    imageShader_ = openGl.shaders->getShaderProgram(Shaders::kImageVertex, Shaders::kTintedImageFragment);

    imageShader_->use();
    imageColour_ = getUniform(*imageShader_, "color");
    imagePosition_ = getAttribute(*imageShader_, "position");
    textureCoordinates_ = getAttribute(*imageShader_, "tex_coord_in");

    isInitialised_.store(true, std::memory_order_release);
  }

  void OpenGlImage::render(OpenGlWrapper &openGl)
  {
    BaseComponent *component = targetComponent_ ? targetComponent_ : this;
    juce::Rectangle<int> customViewportBounds = customViewportBounds_.get();
    juce::Rectangle<int> customScissorBounds = customScissorBounds_.get();
    auto viewportBounds = (!customViewportBounds.isEmpty()) ? customViewportBounds : component->getLocalBoundsSafe();
    auto scissorBounds = (!customScissorBounds.isEmpty()) ? customScissorBounds : viewportBounds;
    if (!isActive_ || !component->isVisibleSafe() || !setViewPort(*component, *this, 
      viewportBounds, scissorBounds, openGl, ignoreClipIncluding_))
      return;

    if (shouldReloadImage_)
    {
      auto *drawImage = drawImage_.lock();
      std::tie(textureWidth_, textureHeight_) = loadImageAsTexture(openGl.context, textureId_, *drawImage, GL_LINEAR);
      shouldReloadImage_ = false;
      drawImage_.unlock();
    }

    glEnable(GL_BLEND);
    if (useScissor_)
      glEnable(GL_SCISSOR_TEST);
    else
      glDisable(GL_SCISSOR_TEST);

    if (isAdditive_)
      glBlendFunc(GL_ONE, GL_ONE);
    else if (useAlpha_)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

    if (hasNewVertices_)
    {
      GLsizeiptr vertSize = static_cast<GLsizeiptr>(kNumPositions * sizeof(float));
      auto vertices = positionVertices_.lock();
      glBufferData(GL_ARRAY_BUFFER, vertSize, vertices, GL_STATIC_DRAW);
      positionVertices_.unlock();
      hasNewVertices_ = false;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleBuffer_);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glActiveTexture(GL_TEXTURE0);

    imageShader_->use();

    Colour colour = colour_.get();
    imageColour_.set(colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), colour.getFloatAlpha());

    glVertexAttribPointer(imagePosition_.attributeId, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(imagePosition_.attributeId);

    glVertexAttribPointer(textureCoordinates_.attributeId, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid *)(2 * sizeof(float)));
    glEnableVertexAttribArray(textureCoordinates_.attributeId);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(imagePosition_.attributeId);
    glDisableVertexAttribArray(textureCoordinates_.attributeId);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }

  void OpenGlImage::destroy()
  {
    if (!isInitialised_.load(std::memory_order_acquire))
      return;

    // preparing the image for next time if openGl reinitialises this object
    shouldReloadImage_ = true;

    imageShader_ = nullptr;
    imageColour_ = {};
    imagePosition_ = {};
    textureCoordinates_ = {};
    textureWidth_ = 0;
    textureHeight_ = 0;

    if (textureId_)
      pushResourcesForDeletion(OpenGlAllocatedResource::Texture, 1, textureId_);
    if (vertexBuffer_)
      pushResourcesForDeletion(OpenGlAllocatedResource::Buffer, 1, vertexBuffer_);
    if (triangleBuffer_)
      pushResourcesForDeletion(OpenGlAllocatedResource::Buffer, 1, triangleBuffer_);

    textureId_ = 0;
    vertexBuffer_ = 0;
    triangleBuffer_ = 0;

    isInitialised_.store(false, std::memory_order_release);
  }

  void OpenGlBackground::paintToImage(Graphics &g, [[maybe_unused]] BaseComponent *target)
  {
    juce::Rectangle<int> bounds = targetComponent_.get()->getLocalArea(
      componentToRedraw_, componentToRedraw_->getLocalBounds());
    g.reduceClipRegion(bounds);
    g.setOrigin(bounds.getTopLeft());

    auto &internalContext = g.getInternalContext();
    internalContext.setFill(Colours::transparentBlack);
    internalContext.fillRect(bounds, true);

    componentToRedraw_->paintBackground(g);
  }

  PlainTextComponent::PlainTextComponent(String name, String text):
    OpenGlImage(std::move(name)), text_(std::move(text)), 
    font_(Fonts::instance()->getInterVFont()) { }

  void PlainTextComponent::resized()
  {
    OpenGlImage::resized();
    redrawImage();
  }

  void PlainTextComponent::paintToImage(Graphics &g, BaseComponent *target)
  {
    updateState();

    g.setFont(font_);
    g.setColour(textColour_);

    g.drawText(text_, 0, 0, target->getWidth(), target->getHeight(), justification_, true);
  }

  void PlainTextComponent::updateState()
  {
    Font font;
    if (fontType_ == kTitle)
    {
      textColour_ = getColour(Skin::kHeadingText);
      font = Fonts::instance()->getInterVFont().boldened();
    }
    else if (fontType_ == kText)
    {
      textColour_ = getColour(Skin::kNormalText);
      font = Fonts::instance()->getInterVFont();
    }
    else
    {
      textColour_ = getColour(Skin::kWidgetPrimary1);
      font = Fonts::instance()->getDDinFont();
    }

    Fonts::instance()->setHeight(font, scaleValue(textSize_));
    font_ = std::move(font);
  }

  void PlainShapeComponent::paintToImage(Graphics &g, BaseComponent *target)
  {
    juce::Rectangle<float> bounds = target->getLocalBounds().toFloat();

    g.setColour(Colours::white);
    for (auto &[path, type, _] : shape_.paths)
    {
      auto transform = path.getTransformToScaleToFit(bounds, true, justification_);
      switch (type)
      {
      case Shape::Stroke:
        g.strokePath(path, { 1.0f, PathStrokeType::JointStyle::beveled,
          PathStrokeType::EndCapStyle::butt }, transform);
        break;
      case Shape::Fill:
        g.fillPath(path, transform);
        break;
      default:
        break;
      }			
    }
  }

}
