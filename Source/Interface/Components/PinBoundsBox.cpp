/*
	==============================================================================

		PinBoundsBox.cpp
		Created: 27 Feb 2023 8:19:37pm
		Author:  theuser27

	==============================================================================
*/

#include "PinBoundsBox.h"

namespace Interface
{
	PinBoundsBox::PinBoundsBox(std::string_view name, Framework::ParameterValue *lowBound, 
		Framework::ParameterValue *highBound) : BaseSection(name)
	{
		using namespace Framework;

		setInterceptsMouseClicks(false, true);

		highlight_ = std::make_unique<OpenGlImageComponent>("highlight");
		addOpenGlComponent(highlight_.get());
		highlight_->setTargetComponent(this);
		highlight_->paintEntireComponent(false);
		highlight_->setInterceptsMouseClicks(false, false);

		lowBound_ = std::make_unique<PinSlider>(lowBound);
		lowBound_->setIsImageOnTop(true);
		lowBound_->getImageComponent()->setAlwaysOnTop(true);
		addSlider(lowBound_.get());

		highBound_ = std::make_unique<PinSlider>(highBound);
		highBound_->setIsImageOnTop(true);
		highBound_->getImageComponent()->setAlwaysOnTop(true);
		addSlider(highBound_.get());

		roundedCorners_ = std::make_unique<OpenGlCorners>();
		addOpenGlComponent(roundedCorners_.get());
		roundedCorners_->setInterceptsMouseClicks(false, false);
	}

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
		highlight_->setBounds(0, 0, getWidth(), getHeight());

		positionSliders();
		roundedCorners_->setBounds(getLocalBounds());

		repaintBackground();
	}

	void PinBoundsBox::sliderValueChanged(Slider *slider)
	{
		if (lowBound_.get() == slider || highBound_.get() == slider)
			positionSliders();
	}

	void PinBoundsBox::positionSliders()
	{
		static constexpr int kAdditionalWidth = 16;

		auto width = (float)getWidth();
		auto pinSlidersWidth = scaleValueRoundInt(PinSlider::kDefaultPinSliderWidth + kAdditionalWidth);
		auto pinSlidersDrawWidthOffset = scaleValueRoundInt(kAdditionalWidth / 2.0f);
		auto lowBoundPosition = (int)std::round((float)lowBound_->getValue() * width);
		auto highBoundPosition = (int)std::round((float)highBound_->getValue() * width);

		lowBound_->setBoundsAndDrawBounds(
			{ lowBoundPosition - pinSlidersWidth / 2, 0, pinSlidersWidth, getHeight() },
			{ pinSlidersDrawWidthOffset, 0, pinSlidersWidth - 2 * pinSlidersDrawWidthOffset, getHeight() });
		lowBound_->setTotalRange(width);
		highBound_->setBoundsAndDrawBounds(
			{ highBoundPosition - pinSlidersWidth / 2, 0, pinSlidersWidth, getHeight() },
			{ pinSlidersDrawWidthOffset, 0, pinSlidersWidth - 2 * pinSlidersDrawWidthOffset, getHeight() });
		highBound_->setTotalRange(width);

		highlight_->redrawImage();
	}

	void PinBoundsBox::paintHighlightBox(Graphics &g, float lowBoundValue, 
		float highBoundValue, Colour colour, float shiftValue) const
	{
		g.setColour(colour);

		auto width = (float)getWidth();
		auto height = (float)getHeight();
		auto lowBoundShifted = std::clamp(lowBoundValue + shiftValue, 0.0f, 1.0f);
		auto highBoundShifted = std::clamp(highBoundValue + shiftValue, 0.0f, 1.0f);

		if (lowBoundValue < highBoundValue)
		{
			auto lowPixel = lowBoundShifted * width;
			auto highlightWidth = highBoundShifted * width - lowPixel;
			Rectangle bounds{ lowPixel, 0.0f, highlightWidth, height };

			g.fillRect(bounds);
		}
		else if (lowBoundValue > highBoundValue)
		{
			auto lowPixel = lowBoundShifted * width;
			auto upperHighlightWidth = width - lowPixel;
			auto lowerHighlightWidth = highBoundShifted * width;

			Rectangle upperBounds{ lowPixel, 0.0f, upperHighlightWidth, height };
			Rectangle lowerBounds{ 0.0f, 0.0f, lowerHighlightWidth, height };

			g.fillRect(upperBounds);
			g.fillRect(lowerBounds);
		}
	}


}
