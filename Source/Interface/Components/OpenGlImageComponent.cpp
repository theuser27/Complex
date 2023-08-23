/*
  ==============================================================================

    OpenGlImageComponent.cpp
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlImageComponent.h"
#include "../Sections/BaseSection.h"

namespace Interface
{
  void OpenGlImageComponent::redrawImage(bool clearOnRedraw)
  {
    if (!active_)
      return;

    Component *component = targetComponent_ ? targetComponent_ : this;

    int width = component->getWidth();
    int height = component->getHeight();
    if (width <= 0 || height <= 0)
      return;

    bool newImage = drawImage_ == nullptr || drawImage_->getWidth() != width || drawImage_->getHeight() != height;
    if (!newImage && (staticImage_ /* || !isDirty_*/))
      return;

    if (newImage)
      drawImage_ = std::make_unique<Image>(Image::ARGB, width, height, false);

    if (clearOnRedraw)
      drawImage_->clear(Rectangle{ 0, 0, width, height });

    Graphics g(*drawImage_);
    paintToImage(g, component);

    image_.lock();
    image_.setImage(drawImage_.get());

    image_.setTopLeft(-1.0f, 1.0f);
    image_.setTopRight(1.0f, 1.0f);
    image_.setBottomLeft(-1.0f, -1.0f);
    image_.setBottomRight(1.0f, -1.0f);
    image_.unlock();

    isDirty_ = false;
  }

  void OpenGlImageComponent::render(OpenGlWrapper &openGl, bool animate)
  {
    Component *targetComponent = targetComponent_ ? targetComponent_ : this;
    if (!active_ || !setViewPort(targetComponent, openGl) || !targetComponent->isVisible())
      return;

    image_.render(openGl, animate);
  }

  void OpenGlBackground::paintToImage(Graphics &g, [[maybe_unused]] Component *target)
  {
    std::visit([&](auto &&componentToRedraw)
      {
        Rectangle<int> bounds = targetComponent_->getLocalArea(componentToRedraw, 
          componentToRedraw->getLocalBounds());
        g.reduceClipRegion(bounds);
        g.setOrigin(bounds.getTopLeft());
        componentToRedraw->paintBackground(g);
      }, componentToRedraw_);
  }

  void PlainTextComponent::paintToImage(Graphics &g, Component *target)
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

    Fonts::instance()->setFontFromHeight(font, parent_->scaleValue(textSize_));
    font_ = std::move(font);
  }

  void PlainShapeComponent::paintToImage(Graphics &g, Component *target)
  {
    Rectangle<float> bounds = target->getLocalBounds().toFloat();

    g.setColour(Colours::white);
    auto transform = strokeShape_.getTransformToScaleToFit(bounds, true, justification_);
    if (!strokeShape_.isEmpty())
      g.strokePath(strokeShape_, PathStrokeType{ 1.0f, PathStrokeType::JointStyle::beveled, PathStrokeType::EndCapStyle::butt },
        transform);
    if (!fillShape_.isEmpty())
      g.fillPath(fillShape_, transform);
  }

}