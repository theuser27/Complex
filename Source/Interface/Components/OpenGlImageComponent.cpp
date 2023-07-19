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

    Component *component = targetComponent_ ? targetComponent_ : this;

    auto *display = Desktop::getInstance().getDisplays().getDisplayForPoint(getScreenPosition());
    COMPLEX_ASSERT(display && "We're running graphics classes on a system with no display??");
  	double pixelScale = display->scale;
    int width = (int)(component->getWidth() * pixelScale);
    int height = (int)(component->getHeight() * pixelScale);
    if (width <= 0 || height <= 0)
      return;

    bool newImage = drawImage_ == nullptr || drawImage_->getWidth() != width || drawImage_->getHeight() != height;
    if (!newImage && (staticImage_ || !force))
      return;

    image_.lock();

    if (newImage)
      drawImage_ = std::make_unique<Image>(Image::ARGB, width, height, false);

    drawImage_->clear(Rectangle<int>(0, 0, width, height));
    Graphics g(*drawImage_);
    g.addTransform(AffineTransform::scale((float)pixelScale));
    paintToImage(g);
    image_.setImage(drawImage_.get());

    // TODO: figure out what these calculations were supposed to do
  	//float glWidth = utils::nextPowerOfTwo((float)width);
   // float glHeight = utils::nextPowerOfTwo((float)height);
   // float widthRatio = glWidth / (float)width;
   // float heightRatio = glHeight / (float)height;
  	//float right = -1.0f + 2.0f * widthRatio;
    //float bottom = 1.0f - 2.0f * heightRatio;

    image_.setTopLeft(-1.0f, 1.0f);
  	image_.setTopRight(1.0f, 1.0f);
    image_.setBottomLeft(-1.0f, -1.0f);
    image_.setBottomRight(1.0f, -1.0f);
    image_.unlock();
  }

  void OpenGlImageComponent::init(OpenGlWrapper &openGl)
  { image_.init(openGl); }

  void OpenGlImageComponent::render(OpenGlWrapper &openGl, bool animate)
  {
    animator_.tick(animate);

    Component *targetComponent = targetComponent_ ? targetComponent_ : this;
    if (!active_ || !setViewPort(targetComponent, openGl) || !targetComponent->isVisible())
      return;

    image_.render();
  }

  void OpenGlImageComponent::destroy([[maybe_unused]] OpenGlWrapper &openGl)
  { image_.destroy(); }

  void PlainTextComponent::paintToImage(Graphics &g)
  {
    if (font_type_ == kTitle)
    {
      g.setColour(getColour(Skin::kHeadingText));
      g.setFont(Fonts::instance()->getInterVFont().withHeight(text_size_).boldened());
    }
    else if (font_type_ == kText)
    {
      g.setColour(getColour(Skin::kNormalText));
      g.setFont(Fonts::instance()->getInterVFont().withHeight(text_size_));
    }
    else
    {
      g.setColour(getColour(Skin::kWidgetPrimary1));
      g.setFont(Fonts::instance()->getDDinFont().withHeight(text_size_));
    }

    Component *component = targetComponent_ ? targetComponent_ : this;

    g.drawText(text_, buffer_, 0, component->getWidth() - 2 * buffer_,
      component->getHeight(), justification_, true);
  }

	void PlainShapeComponent::paintToImage(Graphics &g)
  {
    Component *component = targetComponent_ ? targetComponent_ : this;
    Rectangle<float> bounds = component->getLocalBounds().toFloat();
    Path shape = shape_;
    shape.applyTransform(shape.getTransformToScaleToFit(bounds, true, justification_));

    g.setColour(Colours::white);
    //g.fillPath(shape);
    g.strokePath(shape, PathStrokeType(1.0f, PathStrokeType::JointStyle::mitered, PathStrokeType::EndCapStyle::rounded));
  }

}