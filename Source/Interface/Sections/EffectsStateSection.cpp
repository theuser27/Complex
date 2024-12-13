/*
  ==============================================================================

    EffectsStateSection.cpp
    Created: 3 Feb 2023 6:41:53pm
    Author:  theuser27

  ==============================================================================
*/

#include "EffectsStateSection.hpp"

#include "Plugin/ProcessorTree.hpp"
#include "Framework/update_types.hpp"
#include "Generation/EffectsState.hpp"
#include "EffectModuleSection.hpp"
#include "EffectsLaneSection.hpp"

namespace Interface
{
  EffectsStateSection::EffectsStateSection(Generation::EffectsState &state) :
    ProcessorSection("Effects State", &state), state_(state)
  {
    lanes_.reserve(state.getLaneCount());
    for (size_t i = 0; i < state.getLaneCount(); ++i)
    {
      auto *lane = state_.getEffectsLane(i);
      auto &laneSection = lanes_.emplace_back(utils::up<EffectsLaneSection>::create(lane, this, "Lane A"));
      addSubOpenGlContainer(laneSection.get());
    }
  }

  EffectsStateSection::~EffectsStateSection() = default;


  void EffectsStateSection::resized()
  {
    int laneSelectorHeight = scaleValueRoundInt(kLaneSelectorHeight);
    laneSelector_.setBounds(0, 0, getWidth(), laneSelectorHeight);

    int laneSelectorMargin = scaleValueRoundInt(kLaneSelectorToLanesMargin);
    int lanesX = 0;
    int lanesY = laneSelectorHeight + laneSelectorMargin;
    int lanesWidth = scaleValueRoundInt(kEffectsLaneWidth);
    int lanesHeight = getHeight() - lanesY;
    int laneToLaneMargin = scaleValueRoundInt(kLaneToLaneMargin);

    for (auto &lane : lanes_)
    {
      if (!lane)
        return;

      lane->setBounds(lanesX, lanesY, lanesWidth, lanesHeight);
      lanesX += lanesWidth + laneToLaneMargin;
    }
      


  }

  void EffectsStateSection::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    for (auto &lane : lanes_)
    {
      if (!lane)
        continue;

      if (auto mouseEvent = e.getEventRelativeTo(lane.get()); 
        lane->hitTest((int)mouseEvent.position.x, (int)mouseEvent.position.y))
      {
        lane->scrollLane(mouseEvent, wheel);
        break;
      }
    }
  }

  EffectModuleSection *EffectsStateSection::prepareToMove(EffectModuleSection *movedModule, const MouseEvent &, bool isCopying)
  {
    auto *containingLane = movedModule->getLaneSection();
    auto laneIterator = std::ranges::find_if(lanes_, [containingLane](const auto &lane)
      { return containingLane == lane.get(); });
    COMPLEX_ASSERT(laneIterator != lanes_.end() && "Somehow we are preparing to move a module that isn't part of a lane??");

    dragStartIndices_.first = laneIterator - lanes_.begin();
    dragStartIndices_.second = laneIterator->get()->getModuleIndex(movedModule).value();

    if (isCopying)
    {
      movedModuleCopy_ = movedModule->createCopy();
      movedModuleCopy_->getDraggableComponent().setListener(this);
      movedModuleCopy_->getDraggableComponent().setIgnoreClip(this);
      movedModuleCopy_->setAlwaysOnTop(true);
      addSubOpenGlContainer(movedModuleCopy_.get());
      movedModuleCopy_->setBounds(getLocalArea(movedModule, movedModule->getLocalBounds()));
      return movedModuleCopy_.get();
    }
    else
    {
      COMPLEX_ASSERT(movedModuleCopy_ == nullptr);
      return movedModule;
    }
  }

  static size_t getLaneIndexForModule(juce::Rectangle<int> moduleBounds,
    std::span<utils::up<EffectsLaneSection>> lanes)
  {
    size_t laneIndex = 0;
    int largestAreaOverlap = 0;
    auto horizontalRange = moduleBounds.getHorizontalRange();

    while (true)
    {
      auto horizontalOverlap = lanes[laneIndex]->getBounds().getHorizontalRange()
        .getIntersectionWith(horizontalRange).getLength();
      if (horizontalOverlap > largestAreaOverlap)
        largestAreaOverlap = horizontalOverlap;
      else if (horizontalOverlap < largestAreaOverlap)
        break;
      if (laneIndex + 1 >= lanes.size())
        break;
      laneIndex++;
    }

    return laneIndex;
  }

  void EffectsStateSection::releaseComponent(EffectModuleSection *movedModule, const MouseEvent &)
  {
    auto newIndices = [&]() -> std::pair<size_t, size_t>
    {
      auto bounds = getLocalArea(movedModule, movedModule->getLocalBounds());
      size_t laneIndex = getLaneIndexForModule(bounds, lanes_);

      if (laneIndex >= lanes_.size())
        return dragStartIndices_;

      if (bounds.getCentreX() < lanes_[0]->getX())
        return { laneIndex, 0 };

      size_t effectIndex = lanes_[laneIndex]->getIndexFromScreenPositionIgnoringSelf(
        lanes_[laneIndex]->getLocalPoint(this, bounds.getCentre()), movedModule);
      return { laneIndex, effectIndex };
    }();

    Framework::WaitingUpdate *update;
    if (movedModuleCopy_ != nullptr)
    {
      // save the section inside the processor so it can be used
      removeSubOpenGlContainer(movedModuleCopy_.get());
      movedModuleCopy_->setAlwaysOnTop(false);
      auto *processor = movedModuleCopy_->getProcessor();
      processor->setSavedSection(COMPLEX_MOV(movedModuleCopy_));

      update = new Framework::CopyProcessorUpdate(*state_.getProcessorTree(), *processor,
        lanes_[newIndices.first]->getProcessorId().value(), newIndices.second);
    }
    else
    {
      if (newIndices == dragStartIndices_)
      {
        lanes_[dragStartIndices_.first]->setEffectPositions();
        return;
      }

      update = new Framework::MoveProcessorUpdate(*state_.getProcessorTree(), lanes_[newIndices.first]->getProcessorId().value(),
        newIndices.second, lanes_[dragStartIndices_.first]->getProcessorId().value(), dragStartIndices_.second);
    }

    state_.getProcessorTree()->pushUndo(update);
  }

  juce::Point<int> EffectsStateSection::mouseWheelWhileDragging(EffectModuleSection *movedModule, 
    const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel)
  {
    size_t laneIndex = getLaneIndexForModule(getLocalArea(movedModule, movedModule->getLocalBounds()), lanes_);
    auto deltaY = lanes_[laneIndex]->scrollLane(e.getEventRelativeTo(lanes_[laneIndex].get()), wheel);

    return (movedModuleCopy_) ? juce::Point<int>{} : juce::Point<int>{ 0, deltaY };
  }

  void EffectsStateSection::createLane(EffectsLaneSection *laneToCopy)
  {
    // if a lane is provided then we're copying
    if (laneToCopy)
    {

      return;
    }

    // TODO: add update to parameters expecting lane counts
  }

  void EffectsStateSection::deleteLane(EffectsLaneSection *laneToDelete)
  {
    COMPLEX_ASSERT(laneToDelete && "The lane to delete must not be nullptr");
  }

  std::optional<usize> EffectsStateSection::getLaneIndex(EffectsLaneSection *lane) noexcept
  {
    COMPLEX_ASSERT(lane);
    auto iter = std::ranges::find_if(lanes_, [&](const auto &element) { return element.get() == lane; });
    if (iter != lanes_.end())
      return (usize)(iter - lanes_.begin());
    else
      return {};
  }
}
