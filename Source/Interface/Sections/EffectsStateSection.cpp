/*
	==============================================================================

		EffectsStateSection.cpp
		Created: 3 Feb 2023 6:41:53pm
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "EffectsStateSection.h"

namespace Interface
{
	EffectsStateSection::EffectsStateSection(Generation::EffectsState &state) :
		ProcessorSection(typeid(EffectsLaneSection).name(), &state), state_(state)
	{
		
	}


	void EffectsStateSection::resized()
	{
		auto y = kTopToLaneSelectorMargin;
		laneSelector_.setBounds(0, y, getWidth(), kLaneSelectorHeight);
	}

	void EffectsStateSection::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
	{
		// if we're just scrolling lanes normally
		if (!currentlyMovedModule_)
		{
			for (auto &lane : lanes_)
			{
				if (!lane)
					continue;

				if (auto event = e.getEventRelativeTo(lane.get()); 
					lane->hitTest((int)event.position.x, (int)event.position.y))
				{
					lane->scrollLane(event, wheel);
					break;
				}
			}
			return;
		}

		// if we're already dragging a component and we're trying to scroll
		auto point = currentlyMovedModule_->getBounds().getCentre();
		for (auto &lane : lanes_)
		{
			if (!lane)
				continue;

			auto translatedPoint = lane->getLocalPoint(currentlyMovedModule_, point);
			if (lane->hitTest(translatedPoint.x, translatedPoint.y))
			{
				lane->scrollLane(e, wheel);
				break;
			}
		}

	}

	void EffectsStateSection::prepareToMove(EffectModuleSection *effectModule, const MouseEvent &e, bool isCopying)
	{
		COMPLEX_ASSERT(effectModule && "Component to move cannot be nullptr");

		auto *containingLane = effectModule->getParentComponent();
		auto laneIterator = std::ranges::find_if(lanes_, [containingLane](const std::unique_ptr<EffectsLaneSection> &lane)
			{ return containingLane == lane.get(); });
		COMPLEX_ASSERT(laneIterator != lanes_.end() && "Somehow we are preparing to move a module that isn't part of a lane??");

		dragStartIndices_.first = laneIterator - lanes_.begin();
		dragStartIndices_.second = laneIterator->get()->getModuleIndex(effectModule).value();
		dragEndIndices_ = dragStartIndices_;

		isCopyingModule_ = isCopying;
		if (isCopyingModule_)
		{
			auto newModule = effectModule->createCopy();
			currentlyMovedModule_ = newModule.get();
			registerModule(std::move(newModule));
		}
		else
		{
			currentlyMovedModule_ = effectModule;
			laneIterator->get()->deleteModule(dragStartIndices_.second, false);
		}

		addSubSection(currentlyMovedModule_);
		movedModulePosition_ = e.getEventRelativeTo(this).getPosition();
	}

	void EffectsStateSection::draggingComponent(EffectModuleSection *effectModule, const MouseEvent &e)
	{
		COMPLEX_ASSERT(effectModule == currentlyMovedModule_ &&
			"Somehow we're dragging 2 effectModules at the same time");

		currentlyMovedModule_->setTopLeftPosition(currentlyMovedModule_->getPosition() + 
			e.getEventRelativeTo(this).getPosition() - movedModulePosition_);
		auto position = getLocalPoint(currentlyMovedModule_, currentlyMovedModule_->getBounds().getCentre());

		std::pair<size_t, size_t> newIndices = { 0, 0 };
		for (; newIndices.first < lanes_.size(); newIndices.first++)
		{
			auto *lane = lanes_[newIndices.first].get();
			auto translatedPosition = lane->getLocalPoint(this, position);
			if (lane->hitTest(translatedPosition.x, translatedPosition.y))
				break;
		}

		newIndices.second = lanes_[newIndices.first]->getIndexFromScreenPosition(
			lanes_[newIndices.first]->getLocalPoint(this, position));
		dragEndIndices_ = newIndices;

		movedModulePosition_ = position;
	}

	void EffectsStateSection::releaseComponent(EffectModuleSection *effectModule, const MouseEvent &e)
	{
		COMPLEX_ASSERT(effectModule == currentlyMovedModule_ &&
			"Somehow we're releasing a different component from the one we were moving");

		lanes_[dragEndIndices_.first]->insertModule(dragEndIndices_.second, currentlyMovedModule_);

		Framework::WaitingUpdate *update;
		if (isCopyingModule_)
		{
			update = new Framework::CopyProcessorUpdate(state_.getProcessorTree(), *currentlyMovedModule_->getProcessor(),
				lanes_[dragEndIndices_.first]->getProcessorId().value(), dragEndIndices_.second);
		}
		else
		{
			update = new Framework::MoveProcessorUpdate(state_.getProcessorTree(), lanes_[dragEndIndices_.first]->getProcessorId().value(),
				dragEndIndices_.second, lanes_[dragStartIndices_.first]->getProcessorId().value(), dragStartIndices_.second);
		}

		state_.getProcessorTree()->pushUndo(update);
		currentlyMovedModule_ = nullptr;
	}
}
