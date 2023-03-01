/*
  ==============================================================================

    OpenGlImageComponent.cpp
    Created: 14 Dec 2022 2:07:44am
    Author:  Lenovo

  ==============================================================================
*/

#include "OpenGlImageComponent.h"
#include "Framework/utils.h"

namespace Interface
{
  void OpenGlImageComponent::redrawImage(bool force)
  {
    if (!active_)
      return;

    Component *component = component_ ? component_ : this;

    auto *display = Desktop::getInstance().getDisplays().getDisplayForPoint(getScreenPosition());
    COMPLEX_ASSERT(display && "We're running graphics classes on a system with no display??");
  	double pixel_scale = display->scale;
    int width = (int)(component->getWidth() * pixel_scale);
    int height = (int)(component->getHeight() * pixel_scale);
    if (width <= 0 || height <= 0)
      return;

    bool new_image = drawImage_ == nullptr || drawImage_->getWidth() != width || drawImage_->getHeight() != height;
    if (!new_image && (staticImage_ || !force))
      return;

    image_.lock();

    if (new_image)
      drawImage_ = std::make_unique<Image>(Image::ARGB, width, height, false);

    drawImage_->clear(Rectangle<int>(0, 0, width, height));
    Graphics g(*drawImage_);
    g.addTransform(AffineTransform::scale((float)pixel_scale));
    paintToImage(g);
    image_.setImage(drawImage_.get());

    float gl_width = utils::nextPowerOfTwo((float)width);
    float gl_height = utils::nextPowerOfTwo((float)height);
    float width_ratio = gl_width / (float)width;
    float height_ratio = gl_height / (float)height;

    float right = 2.0f * width_ratio - 1.0f;
    float bottom = 1.0f - 2.0f * height_ratio;
    image_.setTopRight(right, 1.0f);
    image_.setBottomLeft(-1.0f, bottom);
    image_.setBottomRight(right, bottom);
    image_.unlock();
  }

  void OpenGlImageComponent::init(OpenGlWrapper &open_gl)
  { image_.init(open_gl); }

  void OpenGlImageComponent::render(OpenGlWrapper &open_gl, bool animate)
  {
    Component *component = component_ ? component_ : this;
    if (!active_ || !setViewPort(component, open_gl) || !component->isVisible())
      return;

    image_.render();
  }

  void OpenGlImageComponent::destroy(OpenGlWrapper &open_gl)
  { image_.destroy(); }
}