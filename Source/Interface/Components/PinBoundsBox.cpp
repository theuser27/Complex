/*
  ==============================================================================

    PinBoundsBox.cpp
    Created: 27 Feb 2023 8:19:37pm
    Author:  theuser27

  ==============================================================================
*/

#include "PinBoundsBox.hpp"

#include "Framework/parameter_value.hpp"
#include "OpenGlImage.hpp"
#include "OpenGlQuad.hpp"
#include "BaseSlider.hpp"

namespace Interface
{
  PinBoundsBox::PinBoundsBox(utils::string_view name, Framework::ParameterValue *lowBound,
    Framework::ParameterValue *highBound) : BaseSection{ name }
  {
    using namespace Framework;

    setInterceptsMouseClicks(false, true);

    //highlight_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kHighlightFragment, "highlight");
    addOpenGlComponent(&highlight_);
    /*highlight_.setCustomRenderFunction([this](OpenGlWrapper &openGl, bool animate)
      {
        simd_float lowValues = lowBound_->getInternalValue<simd_float>(kDefaultSampleRate, true);
        simd_float highValues = highBound_->getInternalValue<simd_float>(kDefaultSampleRate, true);
        highlight_.setStaticValues(lowValues[0], 0);
        highlight_.setStaticValues(lowValues[2], 1);
        highlight_.setStaticValues(highValues[0], 2);
        highlight_.setStaticValues(highValues[2], 3);

        highlight_.setColor(primaryColour_);
        highlight_.setAltColor(secondaryColour_);
        highlight_.setModColor(ternaryColour_);

        highlight_.render(openGl, animate);
      });*/
    highlight_.setTargetComponent(this);
    highlight_.paintEntireComponent(false);
    highlight_.setInterceptsMouseClicks(false, false);

    lowBound_ = utils::up<PinSlider>::create(lowBound);
    lowBound_->setAddedHitbox(BorderSize{ 0, kAdditionalPinWidth / 2, 0, kAdditionalPinWidth / 2 });
    addControl(lowBound_.get());

    highBound_ = utils::up<PinSlider>::create(highBound);
    highBound_->setAddedHitbox(BorderSize{ 0, kAdditionalPinWidth / 2, 0, kAdditionalPinWidth / 2 });
    addControl(highBound_.get());

    roundedCorners_.setInterceptsMouseClicks(false, false);
    addOpenGlComponent(&roundedCorners_);
  }

  PinBoundsBox::~PinBoundsBox() = default;

  void PinBoundsBox::paintBackground(Graphics &g)
  {
    g.setColour(getColour(Skin::kBody));
    g.fillRect(getLocalBounds());
  }

  void PinBoundsBox::paint(Graphics &g)
  {
    paintHighlightBox(g, (float)lowBound_->getValue(), (float)highBound_->getValue(),
      getColour(Skin::kWidgetPrimary1).withAlpha(0.15f));
  }

  void PinBoundsBox::resized()
  {
    primaryColour_ = getColour(Skin::kWidgetPrimary1).withAlpha(0.15f);
    secondaryColour_ = getColour(Skin::kModulationMeterLeft).withAlpha(0.15f);
    ternaryColour_ = getColour(Skin::kModulationMeterRight).withAlpha(0.15f);

    //highlight_.setColor(primaryColour_);
    //highlight_.setAltColor(secondaryColour_);
    //highlight_.setModColor(ternaryColour_);
    
    highlight_.setBounds(0, 0, getWidth(), getHeight());

    positionSliders();
    roundedCorners_.setBounds(getLocalBounds());

    repaintBackground();
  }

  void PinBoundsBox::controlValueChanged(BaseControl *control)
  {
    if (lowBound_.get() == control || highBound_.get() == control)
      positionSliders();
  }

  void PinBoundsBox::positionSliders()
  {
    auto width = (float)getWidth();
    auto lowBoundPosition = (int)std::round((float)lowBound_->getValue() * width);
    auto highBoundPosition = (int)std::round((float)highBound_->getValue() * width);

    auto lowBoundWidth = lowBound_->setSizes(getHeight()).getWidth();
    lowBound_->setPosition(Point{ lowBoundPosition - lowBoundWidth / 2, 0 });
    lowBound_->setTotalRange(width);

    auto highBoundWidth = highBound_->setSizes(getHeight()).getWidth();
    highBound_->setPosition(Point{ highBoundPosition - highBoundWidth / 2, 0 });
    highBound_->setTotalRange(width);

    //simd_float lowValues = lowBound_->getInternalValue<simd_float>(kDefaultSampleRate, true);
    //simd_float highValues = highBound_->getInternalValue<simd_float>(kDefaultSampleRate, true);
    //highlight_.setStaticValues(lowValues[0], 0);
    //highlight_.setStaticValues(lowValues[2], 1);
    //highlight_.setStaticValues(highValues[0], 2);
    //highlight_.setStaticValues(highValues[2], 3);
    highlight_.redrawImage();
  }

  void PinBoundsBox::setRounding(float rounding) noexcept { roundedCorners_.setCorners(getLocalBounds(), rounding); }
  void PinBoundsBox::setTopRounding(float topRounding) noexcept { roundedCorners_.setTopCorners(getLocalBounds(), topRounding); }
  void PinBoundsBox::setBottomRounding(float bottomRounding) noexcept { roundedCorners_.setTopCorners(getLocalBounds(), bottomRounding); }
  void PinBoundsBox::setRounding(float topRounding, float bottomRounding) noexcept { roundedCorners_.setCorners(getLocalBounds(), topRounding, bottomRounding); }
  void PinBoundsBox::setRoundedCornerColour(Colour colour) noexcept { roundedCorners_.setColor(colour); }

  void PinBoundsBox::paintHighlightBox(Graphics &g, float lowBoundValue, 
                                       float highBoundValue, Colour colour, float shiftValue) const
  {
    g.setColour(colour);

    auto width = (float)getWidth();
    auto height = (float)getHeight();
    auto lowBoundShifted = utils::clamp(lowBoundValue + shiftValue, 0.0f, 1.0f);
    auto highBoundShifted = utils::clamp(highBoundValue + shiftValue, 0.0f, 1.0f);

    if (lowBoundValue < highBoundValue)
    {
      auto lowPixel = lowBoundShifted * width;
      auto highlightWidth = highBoundShifted * width - lowPixel;
      juce::Rectangle bounds{ lowPixel, 0.0f, highlightWidth, height };

      g.fillRect(bounds);
    }
    else if (lowBoundValue > highBoundValue)
    {
      auto lowPixel = lowBoundShifted * width;
      auto upperHighlightWidth = width - lowPixel;
      auto lowerHighlightWidth = highBoundShifted * width;

      juce::Rectangle upperBounds{ lowPixel, 0.0f, upperHighlightWidth, height };
      juce::Rectangle lowerBounds{ 0.0f, 0.0f, lowerHighlightWidth, height };

      g.fillRect(upperBounds);
      g.fillRect(lowerBounds);
    }
  }


}
