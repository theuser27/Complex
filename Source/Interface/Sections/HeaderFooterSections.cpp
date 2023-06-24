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
		mixNumberBox_->setMaxDisplayCharacters(5);
		mixNumberBox_->setMaxDecimalPlaces(2);
		addSlider(mixNumberBox_.get());

		gainNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::OutGain));
		gainNumberBox_->setMaxDisplayCharacters(5);
		gainNumberBox_->setMaxDecimalPlaces(2);
		gainNumberBox_->setShouldUsePlusMinusPrefix(true);
		addSlider(gainNumberBox_.get());

		blockSizeNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::BlockSize));
		blockSizeNumberBox_->setMaxDisplayCharacters(5);
		blockSizeNumberBox_->setMaxDecimalPlaces(0);
		blockSizeNumberBox_->setAlternativeMode(true);
		addSlider(blockSizeNumberBox_.get());

		overlapNumberBox_ = std::make_unique<NumberBox>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::Overlap));
		overlapNumberBox_->setMaxDisplayCharacters(4);
		overlapNumberBox_->setMaxDecimalPlaces(2);
		overlapNumberBox_->setAlternativeMode(true);
		addSlider(overlapNumberBox_.get());

		windowTypeSelector_ = std::make_unique<TextSelector>(
			soundEngine.getParameterUnchecked((u32)PluginParameters::WindowType),
			Fonts::instance()->getDDinFont());
		windowTypeSelector_->setDrawArrow(true);
		windowTypeSelector_->setTextSelectorListener(this);
		addSlider(windowTypeSelector_.get());

		setSkinOverride(Skin::kHeaderFooter);
	}

	void HeaderFooterSections::paintBackground(Graphics &g)
	{
		auto bounds = getLocalBounds().toFloat();

		g.setColour(findColour(Skin::kBackground, true));
		g.fillRect(bounds.withHeight(kHeaderHeight));
		g.fillRect(bounds.withTop(bounds.getBottom() - kFooterHeight));

		drawSliderLabel(g, mixNumberBox_.get());
		drawSliderLabel(g, gainNumberBox_.get());
		drawSliderLabel(g, blockSizeNumberBox_.get());
		drawSliderLabel(g, overlapNumberBox_.get());
		drawSliderLabel(g, windowTypeSelector_.get());
	}

	void HeaderFooterSections::resized()
	{
		arrangeHeader();
		arrangeFooter();
	}

	void HeaderFooterSections::arrangeHeader()
	{
		auto currentBound = Rectangle{ getLocalBounds().getWidth() - kHorizontalEdgePadding,
			(kHeaderHeight - kNumberBoxHeight) / 2, 0 , kNumberBoxHeight };

		auto mixNumberBoxWidth = mixNumberBox_->setHeight(kNumberBoxHeight);
		currentBound.setX(currentBound.getX() - mixNumberBoxWidth - kHeaderNumberBoxPadding);
		currentBound.setWidth(mixNumberBoxWidth);
		mixNumberBox_->setBounds(currentBound);

		auto mixNumberBoxLabelWidth = mixNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - mixNumberBoxLabelWidth - kLabelToControlMargin);
		currentBound.setWidth(mixNumberBoxLabelWidth);
		mixNumberBox_->getLabelComponent()->setBounds(currentBound);

		auto gainNumberBoxWidth = gainNumberBox_->setHeight(kNumberBoxHeight);
		currentBound.setX(currentBound.getX() - gainNumberBoxWidth - 2 * kHeaderNumberBoxPadding);
		currentBound.setWidth(gainNumberBoxWidth);
		gainNumberBox_->setBounds(currentBound);

		auto gainNumberBoxLabelWidth = gainNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - gainNumberBoxLabelWidth - kLabelToControlMargin);
		currentBound.setWidth(gainNumberBoxLabelWidth);
		gainNumberBox_->getLabelComponent()->setBounds(currentBound);
	}

	void HeaderFooterSections::arrangeFooter()
	{
		auto bounds = getLocalBounds();
		auto yOffset = kFooterHeight - (kFooterHeight - kNumberBoxHeight) / 2;
		auto currentBound = Rectangle{ kHorizontalEdgePadding + kFooterHorizontalEdgePadding, bounds.getBottom() - yOffset,
			bounds.getWidth() - 2 * (kHorizontalEdgePadding + kFooterHorizontalEdgePadding), kNumberBoxHeight };

		// set some random size to trigger the width calculation
		windowTypeSelector_->setBounds(currentBound);

		auto blockSizeLabelWidth = blockSizeNumberBox_->getLabelComponent()->getLabelTextWidth();
		auto blockSizeNumberBoxWidth = blockSizeNumberBox_->setHeight(kNumberBoxHeight);
		auto overlapLabelWidth = overlapNumberBox_->getLabelComponent()->getLabelTextWidth();
		auto overlapNumberBoxWidth = overlapNumberBox_->setHeight(kNumberBoxHeight);
		auto windowTypeLabelWidth = windowTypeSelector_->getLabelComponent()->getLabelTextWidth();
		auto windowTypeSelectorWidth = windowTypeSelector_->getTotalDrawWidth();

		auto elementOffset = (currentBound.getWidth() - blockSizeLabelWidth - kLabelToControlMargin - blockSizeNumberBoxWidth
			- overlapLabelWidth - kLabelToControlMargin - overlapNumberBoxWidth - windowTypeLabelWidth - kLabelToControlMargin 
			- windowTypeSelectorWidth) / 2;

		blockSizeNumberBox_->getLabelComponent()->setBounds(currentBound.removeFromLeft(blockSizeLabelWidth));
		currentBound.removeFromLeft(kLabelToControlMargin);
		blockSizeNumberBox_->setBounds(currentBound.removeFromLeft(blockSizeNumberBoxWidth + 2 * kLabelToControlMargin));

		currentBound.removeFromLeft(elementOffset);
		overlapNumberBox_->getLabelComponent()->setBounds(currentBound.removeFromLeft(overlapLabelWidth));
		currentBound.removeFromLeft(kLabelToControlMargin);
		overlapNumberBox_->setBounds(currentBound.removeFromLeft(overlapNumberBoxWidth + 2 * kLabelToControlMargin));

		currentBound.removeFromLeft(elementOffset);
		windowTypeSelector_->getLabelComponent()->setBounds(currentBound.removeFromLeft(windowTypeLabelWidth));
		currentBound.removeFromLeft(kLabelToControlMargin);
		windowTypeSelector_->setBounds(currentBound);
	}
}
