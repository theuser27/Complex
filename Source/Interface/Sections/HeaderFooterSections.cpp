/*
	==============================================================================

		HeaderFooterSections.cpp
		Created: 23 May 2023 6:00:44am
		Author:  theuser27

	==============================================================================
*/

#include "HeaderFooterSections.h"

namespace Interface
{
	HeaderFooterSections::HeaderFooterSections(Generation::SoundEngine &soundEngine) :
		BaseSection(typeid(HeaderFooterSections).name())
	{
		using namespace Framework;

		/*backgroundColour_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		backgroundColour_->setTargetComponent(this);
		addOpenGlComponent(backgroundColour_);

		bottomBarColour_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		bottomBarColour_->setTargetComponent(this);
		addOpenGlComponent(bottomBarColour_);*/
		
		mixNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::MasterMix::self()));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		mixNumberBox_->setCanUseScrollWheel(true);
		addControl(mixNumberBox_.get());

		gainNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::OutGain::self()));
		gainNumberBox_->setMaxTotalCharacters(5);
		gainNumberBox_->setMaxDecimalCharacters(2);
		gainNumberBox_->setShouldUsePlusMinusPrefix(true);
		gainNumberBox_->setCanUseScrollWheel(true);
		addControl(gainNumberBox_.get());

		blockSizeNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::BlockSize::self()));
		blockSizeNumberBox_->setMaxTotalCharacters(5);
		blockSizeNumberBox_->setMaxDecimalCharacters(0);
		blockSizeNumberBox_->setAlternativeMode(true);
		blockSizeNumberBox_->setCanUseScrollWheel(true);
		addControl(blockSizeNumberBox_.get());

		overlapNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::Overlap::self() ));
		overlapNumberBox_->setMaxTotalCharacters(4);
		overlapNumberBox_->setMaxDecimalCharacters(2);
		overlapNumberBox_->setAlternativeMode(true);
		overlapNumberBox_->setCanUseScrollWheel(true);
		addControl(overlapNumberBox_.get());

		windowTypeSelector_ = std::make_unique<TextSelector>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::WindowType::self()),
			Fonts::instance()->getDDinFont());
		windowTypeSelector_->setCanUseScrollWheel(true);
		windowTypeSelector_->addLabel();
		windowTypeSelector_->setLabelPlacement(BubbleComponent::BubblePlacement::left);
		addControl(windowTypeSelector_.get());

		windowAlphaNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameter(BaseProcessors::SoundEngine::WindowAlpha::self()));
		windowAlphaNumberBox_->setMaxTotalCharacters(4);
		windowAlphaNumberBox_->setMaxDecimalCharacters(2);
		windowAlphaNumberBox_->setAlternativeMode(true);
		windowAlphaNumberBox_->setCanUseScrollWheel(true);
		windowAlphaNumberBox_->removeLabel();
		addControl(windowAlphaNumberBox_.get());
	}

	void HeaderFooterSections::paintBackground(Graphics &g)
	{
		auto bounds = getLocalBounds().toFloat();

		float footerHeight = scaleValue(kFooterHeight);

		g.setColour(getColour(Skin::kBackground));
		g.fillRect(bounds.withHeight(bounds.getHeight() - footerHeight));
		g.setColour(getColour(Skin::kBody));
		g.fillRect(bounds.withTop(bounds.getBottom() - footerHeight));

		if (!showAlpha_)
			return;

		float numberBoxHeight = scaleValue(NumberBox::kDefaultNumberBoxHeight);
		float dotDiameter = numberBoxHeight * 0.25f;
		float dotMargin = numberBoxHeight * 0.125f;
		auto dotBounds = Rectangle{ (float)windowAlphaNumberBox_->getX() - dotMargin - dotDiameter,
			bounds.getBottom() - (footerHeight + dotDiameter) * 0.5f, dotDiameter, dotDiameter };

		g.setColour(getColour(Skin::kBackgroundElement));
		g.fillEllipse(dotBounds);

		BaseSection::paintBackground(g);
	}

	void HeaderFooterSections::resized()
	{
		/*auto bounds = getLocalBounds();
		int footerHeight = scaleValueRoundInt(kFooterHeight);
		
		backgroundColour_->setColor(getColour(Skin::kBackground));
		backgroundColour_->setCustomDrawBounds(bounds.withHeight(bounds.getHeight() - footerHeight));
		backgroundColour_->setBounds(bounds.withHeight(bounds.getHeight() - footerHeight));

		bottomBarColour_->setColor(getColour(Skin::kBody));
		bottomBarColour_->setCustomDrawBounds(bounds.withTop(bounds.getBottom() - footerHeight));
		bottomBarColour_->setBounds(bounds.withTop(bounds.getBottom() - footerHeight));*/

		arrangeHeader();
		arrangeFooter();
		repaintBackground();
	}

	void HeaderFooterSections::sliderValueChanged(BaseSlider *movedSlider)
	{
		static constexpr float dynamicWindowsStart = (float)Framework::WindowTypes::Exp / 
			(float)Framework::WindowTypes::enum_count();

		if (movedSlider != windowTypeSelector_.get())
			return;

		showAlpha_ = movedSlider->getValue() >= dynamicWindowsStart;
		if (showAlpha_)
		{
			windowTypeSelector_->setDrawArrow(false);
			windowAlphaNumberBox_->setVisible(true);
		}
		else
		{
			windowTypeSelector_->setDrawArrow(true);
			windowAlphaNumberBox_->setVisible(false);
		}

		arrangeFooter();
		repaintBackground();
	}

	void HeaderFooterSections::arrangeHeader()
	{
		int numberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		int headerNumberBoxMargin = scaleValueRoundInt(kHeaderNumberBoxMargin);

		auto currentPoint = Point{ getWidth() - scaleValueRoundInt(kHeaderHorizontalEdgePadding),
			centerVertically(0, numberBoxHeight, scaleValueRoundInt(kHeaderHeight)) };

		auto mixNumberBoxBounds = mixNumberBox_->getBoundsForSizes(numberBoxHeight);
		mixNumberBox_->setOverallBounds(currentPoint - Point{ mixNumberBoxBounds.getRight(), 0 });
		
		currentPoint.x -= mixNumberBoxBounds.getWidth() + headerNumberBoxMargin;

		auto gainNumberBoxBounds = gainNumberBox_->getBoundsForSizes(numberBoxHeight);
		gainNumberBox_->setOverallBounds(currentPoint - Point{ gainNumberBoxBounds.getRight(), 0 });
	}

	void HeaderFooterSections::arrangeFooter()
	{
		int footerHeight = scaleValueRoundInt(kFooterHeight);
		int numberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		int textSelectorHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);
		int footerHorizontalEdgePadding = scaleValueRoundInt(kFooterHorizontalEdgePadding);

		auto bounds = getLocalBounds();
		auto yOffset = footerHeight - (footerHeight - numberBoxHeight) / 2;
		auto currentPoint = Point{ footerHorizontalEdgePadding, bounds.getBottom() - yOffset };

		auto blockSizeNumberBoxBounds = blockSizeNumberBox_->getBoundsForSizes(numberBoxHeight);
		auto overlapNumberBoxBounds = overlapNumberBox_->getBoundsForSizes(numberBoxHeight);
		auto windowTypeSelectorBounds = windowTypeSelector_->getBoundsForSizes(textSelectorHeight);
		auto windowAlphaBounds = windowAlphaNumberBox_->getBoundsForSizes(numberBoxHeight);

		auto totalElementsLength = blockSizeNumberBoxBounds.getWidth() +
			overlapNumberBoxBounds.getWidth() + windowTypeSelectorBounds.getWidth();

		if (showAlpha_)
		{
			auto dotRadius = numberBoxHeight / 4;
			auto dotMargin = numberBoxHeight / 8;
			totalElementsLength += windowAlphaBounds.getWidth() + dotRadius + 2 * dotMargin;
		}

		auto elementOffset = (bounds.getWidth() - 2 * footerHorizontalEdgePadding - totalElementsLength) / 2;

		blockSizeNumberBox_->setOverallBounds(currentPoint + Point{ -blockSizeNumberBoxBounds.getX(), 0 });
		currentPoint.x += blockSizeNumberBoxBounds.getWidth() + elementOffset;

		overlapNumberBox_->setOverallBounds(currentPoint + Point{ -overlapNumberBoxBounds.getX(), 0 });
		currentPoint.x += overlapNumberBoxBounds.getWidth() + elementOffset;

		windowTypeSelector_->setOverallBounds(currentPoint + Point{ -windowTypeSelectorBounds.getX(), 0 });
		currentPoint.x += windowTypeSelectorBounds.getWidth() + elementOffset;

		if (!showAlpha_)
			return;

		windowAlphaNumberBox_->setOverallBounds({ bounds.getRight() - footerHorizontalEdgePadding - 
			windowAlphaBounds.getWidth(), bounds.getBottom() - yOffset });
	}
}
