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
		ProcessorSection(typeid(EffectsLaneSection).name(), effectsLane), effectsLane_(effectsLane), parentState_(parentState)
	{
		using namespace Framework;

		laneName_ = std::move(name);

		addAndMakeVisible(viewport_);
		viewport_.setViewedComponent(&container_);
		viewport_.addListener(this);
		viewport_.setScrollBarsShown(false, false, true, false);

		addSubSection(&container_, false);

		addAndMakeVisible(&scrollBar_);
		addOpenGlComponent(scrollBar_.getGlComponent());
		scrollBar_.addListener(this);


		laneActivator_ = std::make_unique<GeneralButton>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::LaneEnabled));
		laneActivator_->addButtonListener(this);
		laneActivator_->setTextButton();
		setActivator(laneActivator_.get());

		gainMatchingButton_ = std::make_unique<GeneralButton>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::GainMatching));
		gainMatchingButton_->setPowerButton();
		addButton(gainMatchingButton_.get());

		inputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::Input),
			Fonts::instance()->getInterVFont());
		inputSelector_->setPopupPrefix("From: ");
		inputSelector_->setDrawArrow(true);
		inputSelector_->setTextSelectorListener(this);
		addSlider(inputSelector_.get());

		outputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameterUnchecked((u32)EffectsLaneParameters::Output),
			Fonts::instance()->getInterVFont());
		inputSelector_->setPopupPrefix("To: ");
		inputSelector_->setDrawArrow(true);
		inputSelector_->setTextSelectorListener(this);
		addSlider(inputSelector_.get());


		setOpaque(false);
		setSkinOverride(Skin::kEffectsLane);
		effectsLane_->addListener(this);
	}

	std::unique_ptr<EffectsLaneSection> EffectsLaneSection::createCopy()
	{
		auto *newEffectsLane = static_cast<Generation::EffectsLane *>(effectsLane_->getProcessorTree()->copyProcessor(effectsLane_));
		return  std::make_unique<EffectsLaneSection>(newEffectsLane, parentState_, laneName_ + " - Copy");
	}

	void EffectsLaneSection::resized()
	{
		ScopedLock lock(openGlCriticalSection_);

		// TODO: arrange buttons and text selectors

		viewport_.setBounds(kModulesHorizontalVerticalPadding, kTopBarHeight,
			getWidth() - 2 * kModulesHorizontalVerticalPadding, getHeight() - kTopBarHeight - kBottomBarHeight);
		setEffectPositions();

		scrollBar_.setBounds(getWidth() - kModulesHorizontalVerticalPadding + kModulesHorizontalVerticalPadding / 2,
			kTopBarHeight + kModulesHorizontalVerticalPadding, kModulesHorizontalVerticalPadding / 2, 
			getHeight() - kTopBarHeight - kBottomBarHeight - 2 * kModulesHorizontalVerticalPadding);
		scrollBar_.setColor(findColour(Skin::kLightenScreen, true));

		BaseSection::resized();
	}

	void EffectsLaneSection::paintBackground(Graphics &g)
	{
		auto bounds = getLocalBounds().toFloat();

		g.setColour(findColour(Skin::kBody, true));
		g.fillRoundedRectangle(bounds, kInsideRouding);

		g.setColour(findColour(Skin::kBackground, true));
		g.fillRoundedRectangle(bounds.withTrimmedLeft(kOutlineThickness).withTrimmedRight(kOutlineThickness)
			.withTrimmedTop(kTopBarHeight).withTrimmedBottom(kBottomBarHeight), kInsideRouding);

		redoBackgroundImage();
	}

	void EffectsLaneSection::redoBackgroundImage()
	{
		int height = std::max(container_.getHeight(), getHeight());
		Image backgroundImage = Image(Image::ARGB, container_.getWidth(), height, true);

		Graphics backgroundGraphics(backgroundImage);
		container_.paintBackground(backgroundGraphics);
		background_.setOwnImage(backgroundImage);
	}

	void EffectsLaneSection::initOpenGlComponents(OpenGlWrapper &openGl)
	{
		background_.init(openGl);
		BaseSection::initOpenGlComponents(openGl);
	}

	void EffectsLaneSection::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		ScopedLock lock(openGlCriticalSection_);

		OpenGlComponent::setViewPort(&viewport_, openGl);

		float imageWidth = utils::nextPowerOfTwo((float)background_.getImageWidth());
		float imageHeight = utils::nextPowerOfTwo((float)background_.getImageHeight());
		float widthRatio = imageWidth / (float)container_.getWidth();
		float heightRatio = imageHeight / (float)viewport_.getHeight();
		float yOffset = 2.0f * (float)viewport_.getViewPositionY() / (float)getHeight();

		background_.setTopLeft(-1.0f, 1.0f + yOffset);
		background_.setTopRight(-1.0f + 2.0f * widthRatio, 1.0f + yOffset);
		background_.setBottomLeft(-1.0f, 1.0f - 2.0f * heightRatio + yOffset);
		background_.setBottomRight(-1.0f + 2.0f * widthRatio, 1.0f - 2.0f * heightRatio + yOffset);

		background_.setColor(Colours::white);
		background_.render();
		BaseSection::renderOpenGlComponents(openGl, animate);
	}

	void EffectsLaneSection::destroyOpenGlComponents(OpenGlWrapper &openGl)
	{
		background_.destroy();
		BaseSection::destroyOpenGlComponents(openGl);
	}

	void EffectsLaneSection::insertedSubProcessor(size_t index, Generation::BaseProcessor *newSubProcessor)
	{
		// SAFETY: static_cast is safe because this is the type we just inserted
		auto newModule = std::make_unique<EffectModuleSection>
			(static_cast<Generation::EffectModule *>(newSubProcessor));
		effectModules_.insert(effectModules_.begin() + index, newModule.get());
		parentState_->registerModule(std::move(newModule));
		setEffectPositions();
	}

	void EffectsLaneSection::deletedSubProcessor(size_t index, [[maybe_unused]] Generation::BaseProcessor *deletedSubProcessor)
	{
		auto deletedModule = parentState_->unregisterModule(effectModules_[index]);
		effectModules_.erase(effectModules_.begin() + index);
		container_.removeSubSection(deletedModule.get());
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
		container_.addSubSection(movedModule);
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
		container_.removeSubSection(removedModule);
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

		int paddingBetweenModules = kBetweenModulePadding;
		int effect_width = EffectModuleSection::kEffectModuleWidth;
		int effect_height = EffectModuleSection::kEffectModuleHeight;
		int y = kModulesHorizontalVerticalPadding;

		Point<int> position = viewport_.getViewPosition();

		for (auto *effectModule : effectModules_)
		{
			effectModule->setBounds(0, y, effect_width, effect_height);
			y += effect_height + paddingBetweenModules;
		}

		y += kAddModuleButtonHeight + paddingBetweenModules;

		container_.setBounds(0, 0, viewport_.getWidth(), y);
		viewport_.setViewPosition(position);

		setScrollBarRange();
		repaintBackground();
	}

	size_t EffectsLaneSection::getIndexFromScreenPosition(juce::Point<int> point) const noexcept
	{
		auto yPoint = container_.getLocalPoint(this, point).y;
		size_t index = 0;
		for (; index < effectModules_.size(); index++)
		{
			if (effectModules_[index]->getBounds().getCentreY() >= yPoint)
				break;
		}

		return index;
	}

}
