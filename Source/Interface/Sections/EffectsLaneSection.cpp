/*
	==============================================================================

		EffectsLaneSection.cpp
		Created: 3 Feb 2023 6:42:55pm
		Author:  theuser27

	==============================================================================
*/

#include "EffectsLaneSection.h"

#include "Plugin/ProcessorTree.h"
#include "Framework/update_types.h"
#include "Generation/EffectModules.h"
#include "Generation/EffectsState.h"
#include "../LookAndFeel/Fonts.h"
#include "../Components/OpenGlImageComponent.h"
#include "../Components/OpenGlMultiQuad.h"
#include "../Components/BaseButton.h"
#include "../Components/BaseSlider.h"
#include "EffectsStateSection.h"
#include "EffectModuleSection.h"

namespace Interface
{
	EffectsContainer::EffectsContainer() : BaseSection(typeid(EffectsContainer).name())
	{
		setSkinOverride(Skin::kEffectsLane);

		addModulesButton_ = std::make_unique<OptionsButton>(nullptr, "Add Modules Button", "Add Modules");
		addModulesButton_->removeLabel();
		addControl(addModulesButton_.get());
	}

	EffectsContainer::~EffectsContainer() = default;

	void EffectsContainer::buttonClicked(BaseButton *clickedButton)
	{
		if (addModulesButton_.get() != clickedButton)
			return;

		PopupItems popupItems{ "Choose Module to add" };

		constexpr auto moduleOptions = Framework::BaseProcessors::BaseEffect::enum_names<nested_enum::InnerNodes>(true);
		for (size_t i = 0; i < moduleOptions.size(); ++i)
			popupItems.addItem((int)i, std::string{ moduleOptions[i] });

		showPopupSelector(addModulesButton_.get(), addModulesButton_->getLocalBounds().getBottomLeft(),
			std::move(popupItems), [this](int selection) { handlePopupResult(selection); });
	}

	void EffectsContainer::handlePopupResult(int selection) const
	{
		COMPLEX_ASSERT(lane_);

		auto enumValue = Framework::BaseProcessors::BaseEffect::make_enum(selection);
		if (!enumValue.has_value())
			return;

		lane_->insertModule(lane_->getNumModules(), enumValue.value().enum_id());
	}

	EffectsLaneSection::EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *parentState, String name) :
		ProcessorSection(typeid(EffectsLaneSection).name(), effectsLane), effectsLane_(effectsLane), parentState_(parentState)
	{
		using namespace Framework;

		effectsLane_->addListener(this);

		outerRectangle_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment);
		addOpenGlComponent(outerRectangle_);
		innerRectangle_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment);
		addOpenGlComponent(innerRectangle_);

		laneTitle_ = makeOpenGlComponent<PlainTextComponent>("Lane Title", std::move(name));
		addOpenGlComponent(laneTitle_);

		addAndMakeVisible(&scrollBar_);
		addOpenGlComponent(scrollBar_.getGlComponent());
		scrollBar_.addListener(this);
		//scrollBar_.setShrinkLeft(true);

		laneActivator_ = std::make_unique<PowerButton>(
			effectsLane->getParameter(BaseProcessors::EffectsLane::LaneEnabled::name()));
		laneActivator_->addListener(this);
		setActivator(laneActivator_.get());
		addControl(laneActivator_.get());

		gainMatchingButton_ = std::make_unique<RadioButton>(
			effectsLane->getParameter(BaseProcessors::EffectsLane::GainMatching::name()));
		addControl(gainMatchingButton_.get());

		inputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameter(BaseProcessors::EffectsLane::Input::name()),
			Fonts::instance()->getInterVFont());
		inputSelector_->setPopupPrefix("From: ");
		inputSelector_->setCanUseScrollWheel(true);
		inputSelector_->removeLabel();
		addControl(inputSelector_.get());

		outputSelector_ = std::make_unique<TextSelector>(
			effectsLane->getParameter(BaseProcessors::EffectsLane::Output::name()),
			Fonts::instance()->getInterVFont());
		outputSelector_->setPopupPrefix("To: ");
		outputSelector_->setCanUseScrollWheel(true);
		outputSelector_->removeLabel();
		addControl(outputSelector_.get());

		addAndMakeVisible(viewport_);
		viewport_.addListener(this);
		viewport_.setScrollBarsShown(false, false, true, false);
		viewport_.setSingleStepSizes(12, 12);
		scrollBar_.setViewport(&viewport_);

		auto effectModuleSection = std::make_unique<EffectModuleSection>(effectsLane_->getEffectModule(0), this);
		effectModules_.emplace_back(effectModuleSection.get());
		container_.addSubSection(effectModuleSection.get());
		parentState_->registerModule(std::move(effectModuleSection));

		// the show argument is false because we need the container to be a child of the viewport
		// that is because every time we set its bounds juce::Viewport resets its position to (0,0)??
		// hence why we nest the container inside the viewport, whose position is where it's supposed to be
		container_.setLane(this);
		container_.setAlwaysOnTop(true);
		addSubSection(&container_, false);

		viewport_.setViewedComponent(&container_, false);
		viewport_.addAndMakeVisible(&container_);

		setOpaque(false);
		setSkinOverride(Skin::kEffectsLane);
	}

	EffectsLaneSection::~EffectsLaneSection() = default;

	std::unique_ptr<EffectsLaneSection> EffectsLaneSection::createCopy()
	{
		auto *newEffectsLane = static_cast<Generation::EffectsLane *>(effectsLane_->getProcessorTree()->copyProcessor(effectsLane_));
		return std::make_unique<EffectsLaneSection>(newEffectsLane, parentState_, laneTitle_->getText() + " - Copy");
	}

	void EffectsLaneSection::resized()
	{
		BaseSection::resized();

		int topBarHeight = scaleValueRoundInt(kTopBarHeight);
		int bottomBarHeight = scaleValueRoundInt(kBottomBarHeight);
		float rectangleRounding = scaleValue(kInsideRouding);
		int outlineThickness = scaleValueRoundInt(kOutlineThickness);

		outerRectangle_->setColor(getColour(Skin::kBody));
		outerRectangle_->setRounding(rectangleRounding);
		outerRectangle_->setBounds(getLocalBounds());

		innerRectangle_->setColor(getColour(Skin::kBackground));
		innerRectangle_->setRounding(rectangleRounding);
		innerRectangle_->setBounds(getLocalBounds().withTrimmedLeft(outlineThickness).withTrimmedRight(outlineThickness)
			.withTrimmedTop(topBarHeight).withTrimmedBottom(bottomBarHeight));

		int leftEdgePadding = scaleValueRoundInt(kLeftEdgePadding);
		int rightEdgePadding = scaleValueRoundInt(kRightEdgePadding);
		int textSelectorHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);

		int inputSelectorWidth = inputSelector_->setBoundsForSizes(textSelectorHeight).getWidth();
		inputSelector_->setPosition(Point{ laneActivator_->getX() - rightEdgePadding - inputSelectorWidth,
			(topBarHeight - textSelectorHeight) / 2 });

		laneTitle_->setTextHeight(Fonts::kInterVDefaultHeight);
		laneTitle_->setFontType(PlainTextComponent::kTitle);
		laneTitle_->setJustification(Justification::centredLeft);
		laneTitle_->setBounds(leftEdgePadding, (topBarHeight - textSelectorHeight) / 2,
			inputSelector_->getX() - 2 * leftEdgePadding, textSelectorHeight);

		int gainMatchDimensions = scaleValueRoundInt(kGainMatchButtonDimensions);
		gainMatchingButton_->setRounding(scaleValue(kGainMatchButtonDimensions / 5.0f));
		gainMatchingButton_->setBoundsForSizes(gainMatchDimensions, gainMatchDimensions);
		gainMatchingButton_->setPosition(Point{ leftEdgePadding, getHeight() - (bottomBarHeight + gainMatchDimensions) / 2 });

		int outputSelectorWidth = outputSelector_->setBoundsForSizes(textSelectorHeight).getWidth();
		outputSelector_->setPosition(Point{ getWidth() - rightEdgePadding - outputSelectorWidth,
			getHeight() - (bottomBarHeight + textSelectorHeight) / 2 });

		int viewportX = scaleValueRoundInt(kModulesHorizontalVerticalPadding + kOutlineThickness);
		int viewportY = scaleValueRoundInt(kTopBarHeight);
		viewport_.setBounds(viewportX, viewportY, getWidth() - 2 * viewportX, 
			getHeight() - viewportY - bottomBarHeight);

		setEffectPositions();

		int scrollBarWidth = scaleValueRoundInt(kModulesHorizontalVerticalPadding);
		int scrollBarHeight = getHeight() - scaleValueRoundInt(kTopBarHeight + kBottomBarHeight + 2 * kModulesHorizontalVerticalPadding);
		scrollBar_.setBounds(getWidth() - viewportX, scaleValueRoundInt(kTopBarHeight + kModulesHorizontalVerticalPadding), 
			scrollBarWidth, scrollBarHeight);
		scrollBar_.getGlComponent()->setCustomDrawBounds({ scrollBarWidth / 4, 0, scrollBarWidth / 2, scrollBarHeight });
		scrollBar_.setColor(getColour(Skin::kLightenScreen));
	}

	void EffectsLaneSection::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
	{
		auto position = e.getPosition();
		int viewportX = scaleValueRoundInt(kOutlineThickness);
		int viewportY = scaleValueRoundInt(kTopBarHeight);
		int bottomBarHeight = scaleValueRoundInt(kBottomBarHeight);
		Rectangle area{ viewportX, viewportY, getWidth() - 2 * viewportX, getHeight() - viewportY - bottomBarHeight };
		if (area.contains(position))
		{
			auto mouseEvent = e.getEventRelativeTo(&viewport_);
			viewport_.mouseWheelMove(mouseEvent, wheel);
		}
	}

	void EffectsLaneSection::insertedSubProcessor(size_t index, Generation::BaseProcessor *newSubProcessor)
	{
		auto newModule = std::make_unique<EffectModuleSection>(utils::as<Generation::EffectModule>(newSubProcessor), this);
		effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)index, newModule.get());
		container_.addSubSection(newModule.get());
		parentState_->registerModule(std::move(newModule));
		setEffectPositions();
	}

	void EffectsLaneSection::deletedSubProcessor(size_t index, [[maybe_unused]] Generation::BaseProcessor *deletedSubProcessor)
	{
		auto deletedModule = parentState_->unregisterModule(effectModules_[index]);
		effectModules_.erase(effectModules_.begin() + (std::ptrdiff_t)index);
		container_.removeSubSection(deletedModule.get());
		setEffectPositions();
	}

	void EffectsLaneSection::insertModule(size_t index, std::string_view newModuleType)
	{
		COMPLEX_ASSERT(Framework::BaseProcessors::BaseEffect::enum_value_by_id(newModuleType).has_value()
			&& "An invalid module type was provided to insert");

		effectsLane_->getProcessorTree()->pushUndo(new Framework::AddProcessorUpdate(
			*effectsLane_->getProcessorTree(), effectsLane_->getProcessorId(), index, newModuleType));
	}

	void EffectsLaneSection::insertModule(size_t index, EffectModuleSection *movedModule)
	{
		COMPLEX_ASSERT(movedModule && "A module was not provided to insert");

		effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)index, movedModule);
		container_.addSubSection(movedModule);
	}

	EffectModuleSection *EffectsLaneSection::deleteModule(size_t index, bool createUpdate)
	{
		if (createUpdate)
		{
			effectsLane_->getProcessorTree()->pushUndo(new Framework::DeleteProcessorUpdate(
				*effectsLane_->getProcessorTree(), effectsLane_->getProcessorId(), index));
			return nullptr;
		}

		auto *removedModule = effectModules_[index];
		effectModules_.erase(effectModules_.begin() + (std::ptrdiff_t)index);
		container_.removeSubSection(removedModule);
		return removedModule;
	}

	EffectModuleSection *EffectsLaneSection::deleteModule(const EffectModuleSection *instance, bool createUpdate)
	{
		auto iter = std::ranges::find(effectModules_, instance);
		if (iter == effectModules_.end())
			return nullptr;
		return deleteModule((size_t)(iter - effectModules_.begin()), createUpdate);
	}

	void EffectsLaneSection::moveModule(size_t oldIndex, size_t newIndex)
	{
		auto *movedModule = effectModules_[oldIndex];
		effectModules_.erase(effectModules_.begin() + (std::ptrdiff_t)oldIndex);
		effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)newIndex, movedModule);
	}

	void EffectsLaneSection::setEffectPositions()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		int paddingBetweenModules = scaleValueRoundInt(kBetweenModulePadding);
		int effectWidth = scaleValueRoundInt(kEffectModuleWidth);
		// TODO: change this when you get to the expandable spectral mask
		int effectHeight = kEffectModuleMinHeight;
		int outerPadding = scaleValueRoundInt(kModulesHorizontalVerticalPadding);
		int y = outerPadding;

		Point<int> position = viewport_.getViewPosition();

		for (auto *effectModule : effectModules_)
		{
			effectModule->setBounds(0, y, effectWidth, effectHeight);
			y += effectHeight + paddingBetweenModules;
		}

		int addModuleButtonHeight = scaleValueRoundInt(kAddModuleButtonHeight);
		std::ignore = container_.addModulesButton_->setBoundsForSizes(addModuleButtonHeight, effectWidth);
		container_.addModulesButton_->setPosition(Point{ 0, y });
		//viewport_.container_.addModulesButton_->setBounds(0, y, effectWidth, addModuleButtonHeight);
		y += addModuleButtonHeight + outerPadding;

		container_.setBounds(0, 0, viewport_.getWidth(), y);
		viewport_.setViewPosition(position);

		setScrollBarRange();
	}

	size_t EffectsLaneSection::getIndexFromScreenPosition(Point<int> point) const noexcept
	{
		int yPoint = container_.getLocalPoint(this, point).y;
		size_t index = 0;
		for (; index < effectModules_.size(); index++)
		{
			if (effectModules_[index]->getBounds().getCentreY() >= yPoint)
				break;
		}

		return index;
	}

	void EffectsLaneSection::setLaneName(String newName) { laneTitle_->setText(std::move(newName)); }
}
