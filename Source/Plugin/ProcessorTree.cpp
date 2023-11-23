/*
  ==============================================================================

    ProcessorTree.cpp
    Created: 31 May 2023 4:07:02am
    Author:  theuser27

  ==============================================================================
*/

#include "ProcessorTree.h"
#include "Framework/update_types.h"

namespace Plugin
{
	u64 ProcessorTree::getId(Generation::BaseProcessor *newModulePointer, bool isRootModule) noexcept
	{
		if (isRootModule)
			return processorIdCounter_.load(std::memory_order_acquire);

		// litmus test that getId is not called on an already initialised module
		COMPLEX_ASSERT(newModulePointer->getParentProcessorId() == 0 && "We are adding a module that already exists??");

		std::unique_ptr<Generation::BaseProcessor> newModule{ newModulePointer };
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

	std::unique_ptr<Generation::BaseProcessor> ProcessorTree::deleteProcessor(u64 moduleId) noexcept
	{
		auto iter = allProcessors_.find(moduleId);
		std::unique_ptr<Generation::BaseProcessor> deletedModule = std::move(iter->second);
		allProcessors_.data.erase(iter);
		return std::move(deletedModule);
	}

	Generation::BaseProcessor *ProcessorTree::getProcessor(u64 moduleId) const noexcept
	{
		auto moduleIter = allProcessors_.find(moduleId);
		return (moduleIter != allProcessors_.data.end()) ?
			moduleIter->second.get() : nullptr;
	}

	Framework::ParameterValue *ProcessorTree::getProcessorParameter(u64 parentModuleId, std::string_view parameter) const noexcept
	{
		auto *processorPointer = getProcessor(parentModuleId);
		if (!processorPointer)
			return {};

		return processorPointer->getParameter(parameter);
	}
	
	void ProcessorTree::pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction)
	{
		std::function waitFunction = [this]()
		{
			// check if we're in the middle of an audio callback
			while (getUpdateFlag() != Framework::UpdateFlag::AfterProcess) { utils::wait(); }
			return utils::ScopedSpinLock{ waitLock_ };
		};
		
		action->setWaitFunction(std::move(waitFunction));
		if (isNewTransaction)
			undoManager_.beginNewTransaction();
		undoManager_.perform(action);
	}
}
