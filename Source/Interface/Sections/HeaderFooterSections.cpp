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

		mixNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::MasterMix));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		mixNumberBox_->setScrollWheelEnabled(true);
		addSlider(mixNumberBox_.get());

		gainNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::OutGain));
		gainNumberBox_->setMaxTotalCharacters(5);
		gainNumberBox_->setMaxDecimalCharacters(2);
		gainNumberBox_->setShouldUsePlusMinusPrefix(true);
		gainNumberBox_->setScrollWheelEnabled(true);
		addSlider(gainNumberBox_.get());

		blockSizeNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::BlockSize));
		blockSizeNumberBox_->setMaxTotalCharacters(5);
		blockSizeNumberBox_->setMaxDecimalCharacters(0);
		blockSizeNumberBox_->setAlternativeMode(true);
		blockSizeNumberBox_->setScrollWheelEnabled(true);
		addSlider(blockSizeNumberBox_.get());

		overlapNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::Overlap));
		overlapNumberBox_->setMaxTotalCharacters(4);
		overlapNumberBox_->setMaxDecimalCharacters(2);
		overlapNumberBox_->setAlternativeMode(true);
		overlapNumberBox_->setScrollWheelEnabled(true);
		addSlider(overlapNumberBox_.get());

		windowTypeSelector_ = std::make_unique<TextSelector>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::WindowType),
			Fonts::instance()->getDDinFont());
		windowTypeSelector_->setDrawArrow(true);
		windowTypeSelector_->setTextSelectorListener(this);
		windowTypeSelector_->setScrollWheelEnabled(true);
		addSlider(windowTypeSelector_.get());

		windowAlphaNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::WindowAlpha));
		windowAlphaNumberBox_->setMaxTotalCharacters(4);
		windowAlphaNumberBox_->setMaxDecimalCharacters(2);
		windowAlphaNumberBox_->setAlternativeMode(true);
		windowAlphaNumberBox_->setScrollWheelEnabled(true);
		addSlider(windowAlphaNumberBox_.get());
	}

	void HeaderFooterSections::paintBackground(Graphics &g)
	{
		auto bounds = getLocalBounds().toFloat();

		float headerHeight = scaleValue(kHeaderHeight);
		float footerHeight = scaleValue(kFooterHeight);

		g.setColour(getColour(Skin::kBackground));
		g.fillRect(bounds.withHeight(headerHeight));
		g.setColour(getColour(Skin::kBody));
		g.fillRect(bounds.withTop(bounds.getBottom() - footerHeight));

		drawSliderLabel(g, mixNumberBox_.get());
		drawSliderLabel(g, gainNumberBox_.get());
		drawSliderLabel(g, blockSizeNumberBox_.get());
		drawSliderLabel(g, overlapNumberBox_.get());
		drawSliderLabel(g, windowTypeSelector_.get());

		if (!showAlpha_)
			return;

		float numberBoxHeight = scaleValue(NumberBox::kDefaultNumberBoxHeight);
		float dotDiameter = numberBoxHeight * 0.25f;
		float dotMargin = numberBoxHeight * 0.125f;
		auto dotBounds = Rectangle{ (float)windowAlphaNumberBox_->getX() - dotMargin - dotDiameter,
			bounds.getBottom() - (footerHeight + dotDiameter) * 0.5f, dotDiameter, dotDiameter };

		g.setColour(getColour(Skin::kBackgroundElement));
		g.fillEllipse(dotBounds);
	}

	void HeaderFooterSections::resized()
	{
		arrangeHeader();
		arrangeFooter();
	}

	void HeaderFooterSections::sliderValueChanged(Slider *movedSlider)
	{
		static constexpr float dynamicWindowsStart = (float)Framework::WindowTypes::Exp / 
			(float)magic_enum::enum_count<Framework::WindowTypes>();

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
		int headerHorizontalEdgePadding = (int)std::ceil(scaleValue(kHeaderHorizontalEdgePadding));
		int headerHeight = (int)std::ceil(scaleValue(kHeaderHeight));
		int numberBoxHeight = (int)std::ceil(scaleValue(NumberBox::kDefaultNumberBoxHeight));
		int headerNumberBoxMargin = (int)std::ceil(scaleValue(kHeaderNumberBoxMargin));
		int labelToControlMargin = (int)std::ceil(scaleValue(kLabelToControlMargin));

		auto currentBound = Rectangle{ getWidth() - headerHorizontalEdgePadding,
			(headerHeight - numberBoxHeight) / 2, 0 , numberBoxHeight };

		auto mixNumberBoxWidth = mixNumberBox_->setHeight(numberBoxHeight);
		currentBound.setX(currentBound.getX() - mixNumberBoxWidth - headerNumberBoxMargin);
		currentBound.setWidth(mixNumberBoxWidth);
		mixNumberBox_->setBounds(currentBound);

		auto mixNumberBoxLabelWidth = mixNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - mixNumberBoxLabelWidth - labelToControlMargin);
		currentBound.setWidth(mixNumberBoxLabelWidth);
		mixNumberBox_->getLabelComponent()->setBounds(currentBound);

		auto gainNumberBoxWidth = gainNumberBox_->setHeight(numberBoxHeight);
		currentBound.setX(currentBound.getX() - gainNumberBoxWidth - 2 * headerNumberBoxMargin);
		currentBound.setWidth(gainNumberBoxWidth);
		gainNumberBox_->setBounds(currentBound);

		auto gainNumberBoxLabelWidth = gainNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - gainNumberBoxLabelWidth - labelToControlMargin);
		currentBound.setWidth(gainNumberBoxLabelWidth);
		gainNumberBox_->getLabelComponent()->setBounds(currentBound);
	}

	void HeaderFooterSections::arrangeFooter()
	{
		int footerHeight = (int)std::ceil(scaleValue(kFooterHeight));
		int numberBoxHeight = (int)std::ceil(scaleValue(NumberBox::kDefaultNumberBoxHeight));
		int textSelectorHeight = (int)std::ceil(scaleValue(TextSelector::kDefaultTextSelectorHeight));
		int footerHorizontalEdgePadding = (int)std::ceil(scaleValue(kFooterHorizontalEdgePadding));
		int labelToControlMargin = (int)std::ceil(scaleValue(kLabelToControlMargin));

		auto bounds = getLocalBounds();
		auto yOffset = footerHeight - (footerHeight - numberBoxHeight) / 2;
		auto currentBound = Rectangle{ footerHorizontalEdgePadding, bounds.getBottom() - yOffset,
			bounds.getWidth() - 2 * footerHorizontalEdgePadding, numberBoxHeight };

		auto blockSizeLabelWidth = blockSizeNumberBox_->getLabelComponent()->getLabelTextWidth();
		auto blockSizeNumberBoxWidth = blockSizeNumberBox_->setHeight(numberBoxHeight);
		auto overlapLabelWidth = overlapNumberBox_->getLabelComponent()->getLabelTextWidth();
		auto overlapNumberBoxWidth = overlapNumberBox_->setHeight(numberBoxHeight);
		auto windowTypeLabelWidth = windowTypeSelector_->getLabelComponent()->getLabelTextWidth();
		auto windowTypeSelectorWidth = windowTypeSelector_->setHeight(textSelectorHeight);
		auto windowAlphaWidth = windowAlphaNumberBox_->setHeight(numberBoxHeight);

		auto totalElementsLength = blockSizeLabelWidth + labelToControlMargin + blockSizeNumberBoxWidth + overlapLabelWidth
			+ labelToControlMargin + overlapNumberBoxWidth + windowTypeLabelWidth + labelToControlMargin + windowTypeSelectorWidth;

		if (showAlpha_)
		{
			auto dotRadius = numberBoxHeight / 4;
			auto dotMargin = numberBoxHeight / 8;
			totalElementsLength += windowAlphaWidth + dotRadius + 2 * dotMargin;
		}

		auto elementOffset = (currentBound.getWidth() - totalElementsLength) / 2;

		blockSizeNumberBox_->getLabelComponent()->setBounds(currentBound.removeFromLeft(blockSizeLabelWidth));
		currentBound.removeFromLeft(labelToControlMargin);
		blockSizeNumberBox_->setBounds(currentBound.removeFromLeft(blockSizeNumberBoxWidth));

		currentBound.removeFromLeft(elementOffset);
		overlapNumberBox_->getLabelComponent()->setBounds(currentBound.removeFromLeft(overlapLabelWidth));
		currentBound.removeFromLeft(labelToControlMargin);
		overlapNumberBox_->setBounds(currentBound.removeFromLeft(overlapNumberBoxWidth));

		currentBound.removeFromLeft(elementOffset);
		windowTypeSelector_->getLabelComponent()->setBounds(currentBound.removeFromLeft(windowTypeLabelWidth));
		currentBound.removeFromLeft(labelToControlMargin);
		windowTypeSelector_->setBounds(currentBound.removeFromLeft(windowTypeSelectorWidth));

		if (!showAlpha_)
			return;

		currentBound = Rectangle{ bounds.getRight() - footerHorizontalEdgePadding - windowAlphaWidth,
			bounds.getBottom() - yOffset, windowAlphaWidth, numberBoxHeight };
		windowAlphaNumberBox_->setBounds(currentBound);
	}
}
