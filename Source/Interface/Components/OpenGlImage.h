/*
  ==============================================================================

    OpenGlImage.h
    Created: 14 Dec 2022 2:18:45am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <mutex>
#include "JuceHeader.h"

#include "OpenGlComponent.h"

namespace Interface
{
  class OpenGlImage : public OpenGlComponent
  {
  public:
    OpenGlImage(String name = typeid(OpenGlImage).name());

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl, bool animate) override;
    void destroy() override;

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }

    auto *getOwnImage() noexcept { return ownedImage_.get(); }
    void setOwnImage(Image image)
    {
      ownedImage_ = std::make_unique<Image>(std::move(image));
      mutex_.lock();
      setImage(ownedImage_.get());
      mutex_.unlock();
    }

    void setImage(Image *image)
    {
      image_ = image;
      imageWidth_ = image->getWidth();
      imageHeight_ = image->getHeight();
    }

    void setColor(Colour color) { color_ = color; }

    void setPosition(float x, float y, int index)
    {
      positionVertices_[index] = x;
      positionVertices_[index + 1] = y;
      hasNewVertices_ = true;
    }
    void setTopLeft(float x, float y) { setPosition(x, y, 0); }
    void setBottomLeft(float x, float y) { setPosition(x, y, 4); }
    void setBottomRight(float x, float y) { setPosition(x, y, 8); }
    void setTopRight(float x, float y) { setPosition(x, y, 12); }

    int getImageWidth() const { return imageWidth_; }
    int getImageHeight() const { return imageHeight_; }

    void setAdditive(bool additive) { additive_ = additive; }
    void setUseAlpha(bool useAlpha) { useAlpha_ = useAlpha; }
    void setScissor(bool scissor) { scissor_ = scissor; }

  private:
    std::mutex mutex_;
    bool hasNewVertices_ = true;

    Image *image_ = nullptr;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    std::unique_ptr<Image> ownedImage_ = nullptr;
    Colour color_ = Colours::white;
    OpenGLTexture texture_;
    bool additive_ = false;
    bool useAlpha_ = false;
    bool scissor_ = false;

    OpenGLShaderProgram *imageShader_ = nullptr;
    std::unique_ptr<OpenGLShaderProgram::Uniform> imageColor_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> imagePosition_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> textureCoordinates_;

    std::unique_ptr<float[]> positionVertices_;
    std::unique_ptr<int[]> positionTriangles_;
    GLuint vertexBuffer_{};
    GLuint triangleBuffer_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlImage)
  };
}