/*
  ==============================================================================

    EffectsLaneSection.cpp
    Created: 3 Feb 2023 6:42:55pm
    Author:  theuser27

  ==============================================================================
*/

#include "EffectsLaneSection.hpp"

#include "Plugin/ProcessorTree.hpp"
#include "Framework/update_types.hpp"
#include "Generation/EffectModules.hpp"
#include "Generation/EffectsState.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "../Components/BaseButton.hpp"
#include "../Components/BaseSlider.hpp"
#include "EffectsStateSection.hpp"
#include "EffectModuleSection.hpp"

namespace Interface
{
  EffectsContainer::EffectsContainer() : BaseSection{ "Effects Container" }
  {
    using namespace Framework;

    static constexpr auto moduleOptions = Processors::BaseEffect::
      enum_names_and_ids_filter<kGetActiveEffectPredicate, true>(true);

    setSkinOverride(Skin::kEffectsLane);

    addModulesButton_ = utils::up<OptionsButton>::create(nullptr, "Add Modules Button", "Add Modules");
    addModulesButton_->removeLabel();
    {
      PopupItems popupItems{};
      popupItems.addDelimiter("Choose Module to add");

      for (size_t i = 0; i < moduleOptions.size(); ++i)
        popupItems.addEntry((int)i, moduleOptions[i].first.data(), {}, true);

      addModulesButton_->setOptions(COMPLEX_MOVE(popupItems));
    }
    addModulesButton_->setPopupPlacement(Placement::below);
    addModulesButton_->setPopupHandler([this](int selection)
      { 
        COMPLEX_ASSERT(lane_);
        lane_->insertModule(lane_->getNumModules(), moduleOptions[(usize)selection].second);
      });
    addControl(addModulesButton_.get());
  }

  EffectsContainer::~EffectsContainer() = default;

  EffectsLaneSection::EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *parentState, String name) :
    ProcessorSection{ "Effects Lane Section", effectsLane }, laneTitle_{ "Lane Title", COMPLEX_MOVE(name) },
    effectsLane_{ effectsLane }, parentState_{ parentState }
  {
    using namespace Framework;

    effectsLane->addListener(this);

    addOpenGlComponent(&outerRectangle_);
    addOpenGlComponent(&innerRectangle_);
    addOpenGlComponent(&laneTitle_);

    scrollBar_.addListener(this);
    // always on top because we're not using a texture to render background
    scrollBar_.setAlwaysOnTop(true);
    scrollBar_.setViewport(&viewport_);
    addSubOpenGlContainer(&scrollBar_);

    laneActivator_ = utils::up<PowerButton>::create(
      effectsLane->getParameter(Processors::EffectsLane::LaneEnabled::id().value()));
    laneActivator_->addListener(this);
    setActivator(laneActivator_.get());
    addControl(laneActivator_.get());

    gainMatchingButton_ = utils::up<RadioButton>::create(
      effectsLane->getParameter(Processors::EffectsLane::GainMatching::id().value()));
    addControl(gainMatchingButton_.get());

    inputSelector_ = utils::up<TextSelector>::create(
      effectsLane->getParameter(Processors::EffectsLane::Input::id().value()),
      Fonts::instance()->getInterVFont());
    inputSelector_->setPopupPrefix("From: ");
    inputSelector_->setCanUseScrollWheel(true);
    inputSelector_->removeLabel();
    inputSelector_->setItemIgnoreFunction([this](const Framework::IndexedData &indexedData, int index)
      {
        if (indexedData.dynamicUpdateUuid != Framework::kLaneCountChange)
          return true;

        return parentState_->getLaneIndex(this).value() != (usize)index;
      });
    addControl(inputSelector_.get());

    outputSelector_ = utils::up<TextSelector>::create(
      effectsLane->getParameter(Processors::EffectsLane::Output::id().value()),
      Fonts::instance()->getInterVFont());
    outputSelector_->setPopupPrefix("To: ");
    outputSelector_->setPopupPlacement(Placement::above);
    outputSelector_->setCanUseScrollWheel(true);
    outputSelector_->removeLabel();
    outputSelector_->setAnchor(Placement::right);
    addControl(outputSelector_.get());

    for (size_t i = 0; i < effectsLane->getEffectModuleCount(); ++i)
    {
      auto effectModuleSection = utils::up<EffectModuleSection>::create(effectsLane->getEffectModule(i), this);
      effectModuleSection->getDraggableComponent().setListener(parentState_);
      effectModuleSection->getDraggableComponent().setIgnoreClip(parentState_);
      container_.addSubOpenGlContainer(effectModuleSection.get());
      effectModules_.emplace_back(COMPLEX_MOVE(effectModuleSection));
    }

    // the show argument is false because we need the container to be a child of the viewport
    // that is because every time we set its bounds juce::Viewport resets its position to (0,0)??
    // hence why we nest the container inside the viewport, whose position is where it's supposed to be
    container_.setLane(this);
    // always on top because we're not using a texture to render background
    container_.setAlwaysOnTop(true);
    addSubOpenGlContainer(&container_, false);

    viewport_.addListener(this);
    viewport_.setScrollBarsShown(false, false, true, false);
    viewport_.setSingleStepSizes(12, 12);
    viewport_.setViewedComponent(&container_, false);
    viewport_.addAndMakeVisible(&container_);
    addAndMakeVisible(viewport_);

    setOpaque(false);
    setSkinOverride(Skin::kEffectsLane);
  }

  EffectsLaneSection::~EffectsLaneSection() = default;

  utils::up<EffectsLaneSection> EffectsLaneSection::createCopy()
  {
    auto *newEffectsLane = effectsLane_->getProcessorTree()->copyProcessor(effectsLane_);
    return utils::up<EffectsLaneSection>::create(newEffectsLane, parentState_, laneTitle_.getText() + " - Copy");
  }

  void EffectsLaneSection::resized()
  {
    ProcessorSection::resized();

    int topBarHeight = scaleValueRoundInt(kEffectsLaneTopBarHeight);
    int bottomBarHeight = scaleValueRoundInt(kEffectsLaneBottomBarHeight);
    float rectangleRounding = scaleValue(kInsideRouding);
    int outlineThickness = scaleValueRoundInt(kEffectsLaneOutlineThickness);

    outerRectangle_.setColor(getColour(Skin::kBody));
    outerRectangle_.setRounding(rectangleRounding);
    outerRectangle_.setBounds(getLocalBounds());

    innerRectangle_.setColor(getColour(Skin::kBackground));
    innerRectangle_.setRounding(rectangleRounding);
    innerRectangle_.setBounds(getLocalBounds().withTrimmedLeft(outlineThickness).withTrimmedRight(outlineThickness)
      .withTrimmedTop(topBarHeight).withTrimmedBottom(bottomBarHeight));

    int leftEdgePadding = scaleValueRoundInt(kLeftEdgePadding);
    int rightEdgePadding = scaleValueRoundInt(kRightEdgePadding);
    int textSelectorHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);

    int inputSelectorWidth = inputSelector_->setSizes(textSelectorHeight).getWidth();
    inputSelector_->setPosition(Point{ laneActivator_->getX() - rightEdgePadding - inputSelectorWidth,
      (topBarHeight - textSelectorHeight) / 2 });

    laneTitle_.setTextHeight(Fonts::kInterVDefaultHeight);
    laneTitle_.setFontType(PlainTextComponent::kTitle);
    laneTitle_.setJustification(Justification::centredLeft);
    laneTitle_.setBounds(leftEdgePadding, (topBarHeight - textSelectorHeight) / 2,
      inputSelector_->getX() - 2 * leftEdgePadding, textSelectorHeight);

    int gainMatchDimensions = scaleValueRoundInt(kGainMatchButtonDimensions);
    gainMatchingButton_->setRounding(scaleValue(kGainMatchButtonDimensions / 5.0f));
    gainMatchingButton_->setSizes(gainMatchDimensions, gainMatchDimensions);
    gainMatchingButton_->setPosition(Point{ leftEdgePadding, getHeight() - (bottomBarHeight + gainMatchDimensions) / 2 });

    int outputSelectorWidth = outputSelector_->setSizes(textSelectorHeight).getWidth();
    outputSelector_->setPosition(Point{ getWidth() - rightEdgePadding - outputSelectorWidth,
      getHeight() - (bottomBarHeight + textSelectorHeight) / 2 });

    int viewportX = scaleValueRoundInt(kHVModuleToLaneMargin + kEffectsLaneOutlineThickness);
    int viewportY = scaleValueRoundInt(kEffectsLaneTopBarHeight);
    viewport_.setBounds(viewportX, viewportY, getWidth() - 2 * viewportX, 
      getHeight() - viewportY - bottomBarHeight);
    container_.setClipBounds(viewport_.getBounds());

    setEffectPositions();

    scrollBar_.setColor(getColour(Skin::kLightenScreen));
    int scrollBarWidth = scaleValueRoundInt(kHVModuleToLaneMargin);
    int scrollBarHeight = getHeight() - scaleValueRoundInt(kEffectsLaneTopBarHeight + kEffectsLaneBottomBarHeight + 2 * kHVModuleToLaneMargin);
    scrollBar_.setRenderInset({ 0, scrollBarWidth / 4, 0, scrollBarWidth / 4 });
    scrollBar_.setBounds(getWidth() - viewportX, scaleValueRoundInt(kEffectsLaneTopBarHeight + kHVModuleToLaneMargin),
      scrollBarWidth, scrollBarHeight);
  }

  void EffectsLaneSection::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    auto position = e.getPosition();
    int viewportX = scaleValueRoundInt(kEffectsLaneOutlineThickness);
    int viewportY = scaleValueRoundInt(kEffectsLaneTopBarHeight);
    int bottomBarHeight = scaleValueRoundInt(kEffectsLaneBottomBarHeight);
    juce::Rectangle area{ viewportX, viewportY, getWidth() - 2 * viewportX, getHeight() - viewportY - bottomBarHeight };
    if (area.contains(position))
    {
      auto mouseEvent = e.getEventRelativeTo(&viewport_);
      viewport_.mouseWheelMove(mouseEvent, wheel);
    }
  }

  void EffectsLaneSection::insertedSubProcessor(size_t index, Generation::BaseProcessor &newSubProcessor)
  {
    utils::up<EffectModuleSection> section = nullptr;
    if (auto &savedSection = newSubProcessor.getSavedSection(); savedSection.get())
      section = COMPLEX_MOVE(savedSection);
    else
      section = utils::up<EffectModuleSection>::create(utils::as<Generation::EffectModule>(&newSubProcessor), this);

    section->getDraggableComponent().setListener(parentState_);
    section->getDraggableComponent().setIgnoreClip(parentState_);
    container_.addSubOpenGlContainer(section.get());
    effectModules_.insert(effectModules_.begin() + (ptrdiff_t)index, COMPLEX_MOVE(section));
    setEffectPositions();
  }

  void EffectsLaneSection::deletedSubProcessor(size_t index, Generation::BaseProcessor &deletedSubProcessor)
  {
    utils::up<EffectModuleSection> deletedSection{ COMPLEX_MOVE(effectModules_[index]) };
    container_.removeSubOpenGlContainer(deletedSection.get());
    deletedSubProcessor.setSavedSection(COMPLEX_MOVE(deletedSection));
    effectModules_.erase(effectModules_.begin() + (ptrdiff_t)index);
    setEffectPositions();
  }

  void EffectsLaneSection::movedSubProcessor(Generation::BaseProcessor &,
    Generation::BaseProcessor &sourceProcessor, usize sourceIndex,
    Generation::BaseProcessor &destinationProcessor, usize destinationIndex)
  {
    if (&sourceProcessor == &destinationProcessor)
    {
      utils::up<EffectModuleSection> movedSection{ COMPLEX_MOVE(effectModules_[sourceIndex]) };
      effectModules_.erase(effectModules_.begin() + (isize)sourceIndex);
      effectModules_.insert(effectModules_.begin() + (isize)destinationIndex, COMPLEX_MOVE(movedSection));
      setEffectPositions();
    }
    else
    {
      // TODO: handle once multiple lanes are available
    }
  }

  void EffectsLaneSection::insertModule(size_t index, utils::string_view newModuleType)
  {
    COMPLEX_ASSERT(Framework::Processors::BaseEffect::enum_value_by_id(newModuleType).has_value()
      && "An invalid module type was provided to insert");

    auto &processorTree = *effectsLane_->getProcessorTree();
    auto *effectModule = processorTree.createProcessor(Framework::Processors::EffectModule::id().value());
    effectModule->insertSubProcessor(0, *processorTree.createProcessor(newModuleType));

    effectsLane_->getProcessorTree()->pushUndo(new Framework::AddProcessorUpdate{ processorTree,
      effectsLane_->getProcessorId(), index, *effectModule });
  }

  utils::up<EffectModuleSection> EffectsLaneSection::deleteModule(
    const EffectModuleSection *instance, bool createUpdate)
  {
    size_t i = 0;
    for (; i < effectModules_.size(); ++i)
      if (effectModules_[i].get() == instance)
        break;

    if (i >= effectModules_.size())
      return nullptr;

    if (createUpdate)
    {
      effectsLane_->getProcessorTree()->pushUndo(new Framework::DeleteProcessorUpdate(
        *effectsLane_->getProcessorTree(), effectsLane_->getProcessorId(), i));
      return nullptr;
    }

    utils::up<EffectModuleSection> removedModule{ COMPLEX_MOVE(effectModules_[i]) };
    container_.removeSubOpenGlContainer(removedModule.get());
    effectModules_.erase(effectModules_.begin() + (std::ptrdiff_t)i);
    return removedModule;
  }

  void EffectsLaneSection::setEffectPositions()
  {
    if (getWidth() <= 0 || getHeight() <= 0)
      return;

    int marginBetweenModules = scaleValueRoundInt(kVModuleToModuleMargin);
    int effectWidth = scaleValueRoundInt(kEffectModuleWidth);
    int effectHeight = scaleValueRoundInt(kEffectModuleMinHeight);
    int outerPadding = scaleValueRoundInt(kHVModuleToLaneMargin);
    int y = outerPadding;

    Point<int> position = viewport_.getViewPosition();

    for (auto &effectModule : effectModules_)
    {
      effectModule->setBounds(0, y, effectWidth, effectHeight);
      y += effectHeight + marginBetweenModules;
    }

    int addModuleButtonHeight = scaleValueRoundInt(kAddModuleButtonHeight);
    utils::ignore = container_.addModulesButton_->setSizes(addModuleButtonHeight, effectWidth);
    container_.addModulesButton_->setPosition(Point{ 0, y });
    y += addModuleButtonHeight + outerPadding;

    container_.setBounds(0, 0, viewport_.getWidth(), y);
    viewport_.setViewPosition(position);

    setScrollBarRange();
  }

  usize EffectsLaneSection::getIndexFromScreenPositionIgnoringSelf(
    juce::Rectangle<int> bounds, const EffectModuleSection *moduleSection) const noexcept
  {
    auto centrePoint = bounds.getCentre();
    if (auto target = effectModules_.front().get(); target != moduleSection &&
      centrePoint.y <= getLocalArea(target, target->getLocalBounds()).getCentreY())
      return 0;

    if (auto target = effectModules_.back().get(); target != moduleSection &&
      centrePoint.y >= getLocalArea(target, target->getLocalBounds()).getCentreY())
      return effectModules_.size() - 1;

    usize previousIndex = 0;
    for (usize i = 0; i < effectModules_.size(); ++i)
    {
      if (moduleSection == effectModules_[i].get())
      {
        previousIndex = i;
        continue;
      }

      auto nextBounds = getLocalArea(effectModules_[i].get(),
        effectModules_[i]->getLocalBounds());

      if (nextBounds.contains(centrePoint))
        return i;
    }

    return previousIndex;
  }

  void EffectsLaneSection::setLaneName(String newName) { laneTitle_.setText(COMPLEX_MOVE(newName)); }
}
