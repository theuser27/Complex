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
	PinBoundsBox::PinBoundsBox(std::string_view name) : BaseSection(name)
	{
		using namespace Framework;

		highlight_ = std::make_unique<OpenGlImageComponent>("highlight");
		addOpenGlComponent(highlight_.get());
		highlight_->setComponent(this);
		highlight_->paintEntireComponent(false);
		highlight_->setInterceptsMouseClicks(false, false);

		lowBound_ = std::make_unique<PinSlider>(baseEffectParameterList[(u32)BaseEffectParameters::LowBound].name.data());
		lowBound_->setIsImageOnTop(true);
		lowBound_->getImageComponent()->setAlwaysOnTop(true);
		addSlider(lowBound_.get());

		highBound_ = std::make_unique<PinSlider>(baseEffectParameterList[(u32)BaseEffectParameters::HighBound].name.data());
		highBound_->setIsImageOnTop(true);
		highBound_->getImageComponent()->setAlwaysOnTop(true);
		addSlider(highBound_.get());

		roundedCorners_ = std::make_unique<OpenGlCorners>();
		addOpenGlComponent(roundedCorners_.get());
		roundedCorners_->setTopCorners(getBounds(), topRounding_);
		roundedCorners_->setBottomCorners(getBounds(), bottomRounding_);
		roundedCorners_->setInterceptsMouseClicks(false, false);

	}

	void PinBoundsBox::paint(Graphics &g)
	{
		// painting highlight box
		auto width = (float)getWidth();
		auto highlightColour = findColour(Skin::kPinSlider, true).withAlpha(0.3f);
		auto lowBound = (float)lowBound_->getValue();
		auto highBound = (float)highBound_->getValue();

		if (lowBound <= highBound)
		{
			auto lowPixel = lowBound * width;
			auto highlightWidth = highBound * width - lowPixel;
			Rectangle bounds{ (float)getX() + lowPixel, (float)getY(), highlightWidth, (float)getHeight() };

			g.setColour(highlightColour);
			//g.fillPath(getRoundedPath(bounds));
			g.fillRect(bounds);
		}
		else
		{
			auto lowPixel = lowBound * width;
			auto upperHighlightWidth = width - lowPixel;
			auto lowerHighlightWidth = highBound * width;

			Rectangle upperBounds{ (float)getX() + lowPixel, (float)getY(), upperHighlightWidth, (float)getHeight() };
			Rectangle lowerBounds{ (float)getX(), (float)getY(), lowerHighlightWidth, (float)getHeight() };

			g.setColour(highlightColour);
			//g.fillPath(getRoundedPath(upperBounds));
			//g.fillPath(getRoundedPath(lowerBounds));
			g.fillRect(upperBounds);
			g.fillRect(lowerBounds);
		}
		
	}

	void PinBoundsBox::resized()
	{
		auto width = (float)getWidth();
		auto lowBound = (float)lowBound_->getValue();
		auto lowBoundWidth = lowBound_->getWidth();
		auto highBound = (float)highBound_->getValue();
		auto highBoundWidth = highBound_->getWidth();

		lowBound_->setTopLeftPosition(getX() + (int)(lowBound * width) - lowBoundWidth / 2, getY());
		highBound_->setTopLeftPosition(getX() + (int)(highBound * width) - highBoundWidth / 2, getY());

		repaintBackground();
	}

	void PinBoundsBox::sliderValueChanged(Slider *slider)
	{
		if (lowBound_.get() == slider || highBound_.get() == slider)
			repaintBackground();
	}


}
