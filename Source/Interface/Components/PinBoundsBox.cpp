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
		highlight_->setComponent(this);
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

	void PinBoundsBox::paint(Graphics &g)
	{
		paintHighlightBox(g, (float)lowBound_->getValue(), (float)highBound_->getValue(),
			findColour(Skin::kPinSlider, true).withAlpha(0.15f));
	}

	void PinBoundsBox::resized()
	{
		static constexpr int additionalOffsetY = 8;

		highlight_->setBounds(0, 0, getWidth(), getHeight());

		auto width = (float)getWidth();
		auto lowBound = (float)lowBound_->getValue();
		auto lowBoundWidth = lowBound_->getWidth();
		auto highBound = (float)highBound_->getValue();
		auto highBoundWidth = highBound_->getWidth();

		lowBound_->setBounds((int)(lowBound * width) - lowBoundWidth / 2, -additionalOffsetY, 
			lowBoundWidth, getHeight() + additionalOffsetY);
		highBound_->setBounds((int)(highBound * width) - highBoundWidth / 2, - additionalOffsetY, 
			highBoundWidth, getHeight() + additionalOffsetY);

		repaintBackground();
	}

	void PinBoundsBox::sliderValueChanged(Slider *slider)
	{
		if (lowBound_.get() == slider || highBound_.get() == slider)
			repaintBackground();
	}

	void PinBoundsBox::paintHighlightBox(Graphics &g, float lowBoundValue, float highBoundValue, Colour colour) const
	{
		auto width = (float)getWidth();
		auto height = (float)getHeight();

		if (lowBoundValue <= highBoundValue)
		{
			auto lowPixel = lowBoundValue * width;
			auto highlightWidth = highBoundValue * width - lowPixel;
			Rectangle bounds{ lowPixel, 0.0f, highlightWidth, height };

			g.setColour(colour);
			g.fillRect(bounds);
		}
		else
		{
			auto lowPixel = lowBoundValue * width;
			auto upperHighlightWidth = width - lowPixel;
			auto lowerHighlightWidth = highBoundValue * width;

			Rectangle upperBounds{ lowPixel, 0.0f, upperHighlightWidth, height };
			Rectangle lowerBounds{ 0.0f, 0.0f, lowerHighlightWidth, height };

			g.setColour(colour);
			g.fillRect(upperBounds);
			g.fillRect(lowerBounds);
		}
	}


}
