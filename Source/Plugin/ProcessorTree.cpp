/*
  ==============================================================================

    ProcessorTree.cpp
    Created: 31 May 2023 4:07:02am
    Author:  theuser27

  ==============================================================================
*/

#include "ProcessorTree.h"

#include "AppConfig.h"
#include <juce_data_structures/juce_data_structures.h>
#include "Generation/BaseProcessor.h"
#include "Framework/update_types.h"

namespace Plugin
{
	ProcessorTree::ProcessorTree() : undoManager_(std::make_unique<juce::UndoManager>(0, 100)), allProcessors_(64) { }
	ProcessorTree::~ProcessorTree()
	{
		isBeingDestroyed_ = true;
		//undoManager_.~UndoManager();
	}

	u64 ProcessorTree::getId(Generation::BaseProcessor *newProcessor) noexcept
	{
		// litmus test that getId is not called on an already initialised module
		COMPLEX_ASSERT(newProcessor->getParentProcessorId() == 0 && "We are adding a module that already exists??");

		std::unique_ptr<Generation::BaseProcessor> newModule{ newProcessor };
		u64 newModuleId = processorIdCounter_.fetch_add(1, std::memory_order_acq_rel);

		allProcessors_.add(newModuleId, std::move(newModule));
		if ((float)allProcessors_.data.size() / (float)allProcessors_.data.capacity() >= expandThreshold)
		{
			Framework::VectorMap<u64, std::unique_ptr<Generation::BaseProcessor>>
				newAllParameters{ allProcessors_.data.size() * expandAmount };

			std::ranges::move(allProcessors_.data, newAllParameters.data.begin());

			auto swap = [this, &newAllParameters]() { allProcessors_.data.swap(newAllParameters.data); };
			executeOutsideProcessing(swap);
		}

		return newModuleId;
	}

	std::unique_ptr<Generation::BaseProcessor> ProcessorTree::deleteProcessor(u64 processorId) noexcept
	{
		auto iter = allProcessors_.find(processorId);
		std::unique_ptr<Generation::BaseProcessor> deletedModule = std::move(iter->second);
		allProcessors_.data.erase(iter);
		return std::move(deletedModule);
	}

	Generation::BaseProcessor *ProcessorTree::getProcessor(u64 processorId) const noexcept
	{
		auto moduleIter = allProcessors_.find(processorId);
		return (moduleIter != allProcessors_.data.end()) ?
			moduleIter->second.get() : nullptr;
	}

	Framework::ParameterValue *ProcessorTree::getProcessorParameter(u64 parentProcessorId, std::string_view parameterName) const noexcept
	{
		auto *processorPointer = getProcessor(parentProcessorId);
		if (!processorPointer)
			return {};

		return processorPointer->getParameter(parameterName);
	}

	Generation::BaseProcessor * ProcessorTree::copyProcessor(Generation::BaseProcessor *processor)
	{
		auto copyProcessor = [processor]() { return processor->createCopy({}); };
		return executeOutsideProcessing(copyProcessor);
	}

	void ProcessorTree::pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction)
	{
		std::function waitFunction = [this]()
		{
			// check if we're in the middle of an audio callback
			while (updateFlag_.load(std::memory_order_acquire) != UpdateFlag::AfterProcess) { utils::millisleep(); }
			return utils::ScopedLock{ processingLock_, utils::WaitMechanism::Spin };
		};
		
		action->setWaitFunction(std::move(waitFunction));
		if (isNewTransaction)
			undoManager_->beginNewTransaction();
		undoManager_->perform(action);
	}

	void ProcessorTree::undo() { undoManager_->undo(); }
	void ProcessorTree::redo() { undoManager_->redo(); }
}
