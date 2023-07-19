/*
	==============================================================================

		EffectsLaneSection.cpp
		Created: 3 Feb 2023 6:42:55pm
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "EffectsLaneSection.h"
#include "EffectsStateSection.h"

namespace Interface
{
	EffectsLaneSection::EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *parentState, String name) :
		ProcessorSection(typeid(EffectsLaneSection).name(), effectsLane), laneTitle_("Lane Title", std::move(name)),
		effectsLane_(effectsLane), parentState_(parentState)
	{
		using namespace Framework;

		effectsLane_->addListener(this);

		addOpenGlComponent(&laneTitle_);

		addAndMakeVisible(&scrollBar_);
		addOpenGlComponent(scrollBar_.getGlComponent());
		scrollBar_.addListener(this);

		laneActivator_ = std::make_unique<BaseButton>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::LaneEnabled));
		laneActivator_->addButtonListener(this);
		setActivator(laneActivator_.get());
		addButton(laneActivator_.get());

		gainMatchingButton_ = std::make_unique<BaseButton>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::GainMatching));
		gainMatchingButton_->setRadioButton();
		gainMatchingButton_->createLabel();
		addButton(gainMatchingButton_.get());

		inputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::Input),
			Fonts::instance()->getInterVFont());
		inputSelector_->setPopupPrefix("From: ");
		inputSelector_->setDrawArrow(true);
		inputSelector_->setTextSelectorListener(this);
		inputSelector_->setScrollWheelEnabled(true);
		addSlider(inputSelector_.get());

		outputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::Output),
			Fonts::instance()->getInterVFont());
		outputSelector_->setPopupPrefix("To: ");
		outputSelector_->setDrawArrow(true);
		outputSelector_->setTextSelectorListener(this);
		outputSelector_->setScrollWheelEnabled(true);
		addSlider(outputSelector_.get());

		addAndMakeVisible(viewport_);
		viewport_.addListener(this);
		viewport_.setScrollBarsShown(false, false, true, false);

		auto effectModuleSection = std::make_unique<EffectModuleSection>(effectsLane_->getEffectModule(0));
		effectModules_.emplace_back(effectModuleSection.get());
		viewport_.container_.addSubSection(effectModuleSection.get());
		parentState_->registerModule(std::move(effectModuleSection));

		// the show argument is false because we need the container to be a child of the viewport
		// that is because every time we set its bounds juce::Viewport resets its position to (0,0)??
		// hence why we nest the container inside the viewport, whose position is where it's supposed to be
		addSubSection(&viewport_.container_, false);

		setOpaque(false);
		setSkinOverride(Skin::kEffectsLane);
	}

	std::unique_ptr<EffectsLaneSection> EffectsLaneSection::createCopy()
	{
		auto *newEffectsLane = static_cast<Generation::EffectsLane *>(effectsLane_->getProcessorTree()->copyProcessor(effectsLane_));
		return  std::make_unique<EffectsLaneSection>(newEffectsLane, parentState_, laneTitle_.getText() + " - Copy");
	}

	void EffectsLaneSection::resized()
	{
		ScopedLock lock(openGlCriticalSection_);

		BaseSection::resized();

		auto topBarHeight = scaleValueRoundInt(kTopBarHeight);
		auto bottomBarHeight = scaleValueRoundInt(kBottomBarHeight);
		auto leftEdgePadding = scaleValueRoundInt(kLeftEdgePadding);
		auto rightEdgePadding = scaleValueRoundInt(kRightEdgePadding);
		auto textSelectorHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);

		auto inputSelectorWidth = inputSelector_->setHeight(textSelectorHeight);
		inputSelector_->setBounds(laneActivator_->getX() - rightEdgePadding - inputSelectorWidth,
			(topBarHeight - textSelectorHeight) / 2, inputSelectorWidth, textSelectorHeight);

		laneTitle_.setTextHeight(scaleValue(Fonts::kInterVDefaultHeight));
		laneTitle_.setFontType(PlainTextComponent::kTitle);
		laneTitle_.setJustification(Justification::centredLeft);
		laneTitle_.setBounds(leftEdgePadding, (topBarHeight - textSelectorHeight) / 2,
			inputSelector_->getX() - 2 * leftEdgePadding, textSelectorHeight);

		auto gainMatchDimensions = scaleValueRoundInt(kGainMatchButtonDimensions);
		gainMatchingButton_->getGlComponent()->background().setRounding(scaleValue(kGainMatchButtonDimensions / 5.0f));
		gainMatchingButton_->setBounds(leftEdgePadding, getHeight() - (bottomBarHeight + gainMatchDimensions) / 2,
			gainMatchDimensions, gainMatchDimensions);
		auto gainMatchLabelWidth = gainMatchingButton_->getLabelComponent()->getTotalWidth();
		gainMatchingButton_->getLabelComponent()->setBounds(gainMatchingButton_->getRight() + rightEdgePadding,
			getHeight() - (bottomBarHeight + textSelectorHeight) / 2, gainMatchLabelWidth, textSelectorHeight);

		auto outputSelectorWidth = outputSelector_->setHeight(textSelectorHeight);
		outputSelector_->setBounds(getWidth() - rightEdgePadding - outputSelectorWidth,
			getHeight() - (bottomBarHeight + textSelectorHeight) / 2, outputSelectorWidth, textSelectorHeight);

		auto viewportXOffset = scaleValueRoundInt(kModulesHorizontalVerticalPadding + kOutlineThickness);
		auto viewportY = scaleValueRoundInt(kTopBarHeight);
		viewport_.setBounds(viewportXOffset, viewportY, getWidth() - 2 * viewportXOffset, 
			getHeight() - viewportY - bottomBarHeight);

		setEffectPositions();

		scrollBar_.setBounds(getWidth() - kModulesHorizontalVerticalPadding + kModulesHorizontalVerticalPadding / 2,
			kTopBarHeight + kModulesHorizontalVerticalPadding, kModulesHorizontalVerticalPadding / 2, 
			getHeight() - kTopBarHeight - kBottomBarHeight - 2 * kModulesHorizontalVerticalPadding);
		scrollBar_.setColor(getColour(Skin::kLightenScreen));
	}

	void EffectsLaneSection::paintBackground(Graphics &g)
	{
		auto bounds = getLocalBounds().toFloat();

		auto insideRounding = scaleValue(kInsideRouding);

		auto colour = getColour(Skin::kBody);
		g.setColour(colour);
		g.fillRoundedRectangle(bounds, insideRounding);

		g.setColour(getColour(Skin::kBackground));
		g.fillRoundedRectangle(bounds.withTrimmedLeft(kOutlineThickness).withTrimmedRight(kOutlineThickness)
			.withTrimmedTop(kTopBarHeight).withTrimmedBottom(kBottomBarHeight), insideRounding);

		drawButtonLabel(g, gainMatchingButton_.get());
	}

	void EffectsLaneSection::insertedSubProcessor(size_t index, Generation::BaseProcessor *newSubProcessor)
	{
		// SAFETY: static_cast is safe because this is the type we just inserted
		auto newModule = std::make_unique<EffectModuleSection>
			(static_cast<Generation::EffectModule *>(newSubProcessor));
		effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)index, newModule.get());
		parentState_->registerModule(std::move(newModule));
		setEffectPositions();
	}

	void EffectsLaneSection::deletedSubProcessor(size_t index, [[maybe_unused]] Generation::BaseProcessor *deletedSubProcessor)
	{
		auto deletedModule = parentState_->unregisterModule(effectModules_[index]);
		effectModules_.erase(effectModules_.begin() + index);
		viewport_.container_.removeSubSection(deletedModule.get());
		setEffectPositions();
	}

	void EffectsLaneSection::insertModule(size_t index, std::string_view newModuleType)
	{
		COMPLEX_ASSERT(magic_enum::enum_contains<Framework::EffectTypes>(newModuleType)
			&& "An invalid module type was provided to insert");

		effectsLane_->getProcessorTree()->pushUndo(new Framework::AddProcessorUpdate(
			effectsLane_->getProcessorTree(), effectsLane_->getProcessorId(), index, newModuleType));
	}

	void EffectsLaneSection::insertModule(size_t index, EffectModuleSection *movedModule)
	{
		COMPLEX_ASSERT(movedModule && "A module was not provided to insert");

		effectModules_.insert(effectModules_.begin() + index, movedModule);
		viewport_.container_.addSubSection(movedModule);
	}

	EffectModuleSection *EffectsLaneSection::deleteModule(size_t index, bool createUpdate)
	{
		if (createUpdate)
		{
			effectsLane_->getProcessorTree()->pushUndo(new Framework::DeleteProcessorUpdate(
				effectsLane_->getProcessorTree(), effectsLane_->getProcessorId(), index));
		}

		auto removedModule = effectModules_[index];
		effectModules_.erase(effectModules_.begin() + index);
		viewport_.container_.removeSubSection(removedModule);
		return removedModule;
	}

	void EffectsLaneSection::moveModule(size_t oldIndex, size_t newIndex)
	{
		auto movedModule = effectModules_[oldIndex];
		effectModules_.erase(effectModules_.begin() + oldIndex);
		effectModules_.insert(effectModules_.begin() + newIndex, movedModule);
	}

	void EffectsLaneSection::setEffectPositions()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		int paddingBetweenModules = scaleValueRoundInt(kBetweenModulePadding);
		int effectWidth = scaleValueRoundInt(EffectModuleSection::kWidth);
		// TODO: change this when you get to the expandable spectral mask
		int effectHeight = EffectModuleSection::kMinHeight;
		int outerPadding = scaleValueRoundInt(kModulesHorizontalVerticalPadding);
		int y = outerPadding;

		Point<int> position = viewport_.getViewPosition();

		for (auto *effectModule : effectModules_)
		{
			effectModule->setBounds(0, y, effectWidth, effectHeight);
			y += effectHeight + paddingBetweenModules;
		}

		y += scaleValueRoundInt(kAddModuleButtonHeight) + outerPadding;

		viewport_.container_.setBounds(0, 0, viewport_.getWidth(), y);
		viewport_.setViewPosition(position);

		setScrollBarRange();
		repaintBackground();
	}

	size_t EffectsLaneSection::getIndexFromScreenPosition(juce::Point<int> point) const noexcept
	{
		auto yPoint = viewport_.container_.getLocalPoint(this, point).y;
		size_t index = 0;
		for (; index < effectModules_.size(); index++)
		{
			if (effectModules_[index]->getBounds().getCentreY() >= yPoint)
				break;
		}

		return index;
	}
}
