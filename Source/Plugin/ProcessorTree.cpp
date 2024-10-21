/*
  ==============================================================================

    ProcessorTree.cpp
    Created: 31 May 2023 4:07:02am
    Author:  theuser27

  ==============================================================================
*/

#include "ProcessorTree.hpp"

#include <juce_data_structures/juce_data_structures.h>

#include "Framework/update_types.hpp"
#include "Generation/BaseProcessor.hpp"
#include "Generation/EffectsState.hpp"

namespace Plugin
{
  ProcessorTree::ProcessorTree(u32 inSidechains, u32 outSidechains) : 
    undoManager_{ utils::up<juce::UndoManager>::create(0, 100) }, allProcessors_{ 64 }, 
    inSidechains_(inSidechains), outSidechains_(outSidechains) { }
  ProcessorTree::~ProcessorTree()
  {
    isBeingDestroyed_ = true;
    //undoManager_->~UndoManager();
  }

  u64 ProcessorTree::getId(Generation::BaseProcessor *newProcessor) noexcept
  {
    if (newProcessor->processorId_.has_value())
    {
      // this should be taken as compiler error
      COMPLEX_ASSERT_FALSE("We are adding a processor that already exists??");
      std::abort();
    }

    utils::up<Generation::BaseProcessor> newModule{ newProcessor };
    u64 newModuleId = processorIdCounter_.fetch_add(1, std::memory_order_acq_rel);

    allProcessors_.add(newModuleId, COMPLEX_MOV(newModule));
    if ((float)allProcessors_.data.size() / (float)allProcessors_.data.capacity() >= expandThreshold)
    {
      Framework::VectorMap<u64, utils::up<Generation::BaseProcessor>>
        newAllParameters{ allProcessors_.data.size() * expandAmount };

      auto swap = [this, &newAllParameters]() 
      {
        for (auto &[id, processor] : allProcessors_.data)
          newAllParameters.add(id, utils::up<Generation::BaseProcessor>{ processor.release() });
        
        allProcessors_.data.swap(newAllParameters.data);
      };
      executeOutsideProcessing(swap);
    }

    return newModuleId;
  }

  auto ProcessorTree::getProcessor(u64 processorId) const noexcept
    -> Generation::BaseProcessor *
  {
    auto processorIter = allProcessors_.find(processorId);
    return (processorIter != allProcessors_.data.end()) ?
      processorIter->second.get() : nullptr;
  }

  auto ProcessorTree::deleteProcessor(u64 processorId) noexcept
    -> utils::up<Generation::BaseProcessor>
  {
    auto iter = allProcessors_.find(processorId);
    utils::up<Generation::BaseProcessor> deletedModule = COMPLEX_MOV(iter->second);
    allProcessors_.data.erase(iter);
    return deletedModule;
  }

  auto ProcessorTree::getProcessorParameter(u64 parentProcessorId, std::string_view parameterName) const noexcept
    -> Framework::ParameterValue *
  {
    auto *processorPointer = getProcessor(parentProcessorId);
    if (!processorPointer)
      return {};

    return processorPointer->getParameter(parameterName);
  }

  void ProcessorTree::pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction)
  {
    std::function waitFunction = [this]()
    {
      // check if we're in the middle of an audio callback
      while (updateFlag_.load(std::memory_order_acquire) != UpdateFlag::AfterProcess) { utils::millisleep(); }
      return utils::ScopedLock{ processingLock_, utils::WaitMechanism::Spin };
    };
    
    action->setWaitFunction(COMPLEX_MOV(waitFunction));
    if (isNewTransaction)
      undoManager_->beginNewTransaction();
    undoManager_->perform(action);
  }

  void ProcessorTree::undo() { undoManager_->undo(); }
  void ProcessorTree::redo() { undoManager_->redo(); }
}
