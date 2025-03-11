/*
  ==============================================================================

    ProcessorTree.cpp
    Created: 31 May 2023 4:07:02am
    Author:  theuser27

  ==============================================================================
*/

#include "ProcessorTree.hpp"

#include "Framework/update_types.hpp"
#include "Generation/BaseProcessor.hpp"

namespace Plugin
{
  ProcessorTree::ProcessorTree(u32 inSidechains, u32 outSidechains, usize undoSteps) :
    undoManager_{ utils::up<Framework::UndoManager>::create(undoSteps) }, allProcessors_{ 64 },
    inSidechains_(inSidechains), outSidechains_(outSidechains) { }
  ProcessorTree::~ProcessorTree()
  {
    isBeingDestroyed_.store(true);
    //undoManager_->~UndoManager();
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
    utils::up<Generation::BaseProcessor> deletedModule = COMPLEX_MOVE(iter->second);
    allProcessors_.data.erase(iter);
    return deletedModule;
  }

  auto ProcessorTree::getProcessorParameter(u64 parentProcessorId, utils::string_view parameterName) const noexcept
    -> Framework::ParameterValue *
  {
    auto *processorPointer = getProcessor(parentProcessorId);
    if (!processorPointer)
      return {};

    return processorPointer->getParameter(parameterName);
  }

  void ProcessorTree::pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction)
  {
    clg::small_fn<utils::ScopedLock()> waitFunction = [this]()
    {
      // check if we're in the middle of an audio callback
      utils::millisleep([&]()
        { return updateFlag_.load(std::memory_order_relaxed) != UpdateFlag::AfterProcess; });

      return utils::ScopedLock{ processingLock_, utils::WaitMechanism::Spin };
    };
    
    action->setWaitFunction(COMPLEX_MOVE(waitFunction));
    if (isNewTransaction)
      undoManager_->beginNewTransaction();
    undoManager_->perform(action);
  }

  void ProcessorTree::undo() { undoManager_->undo(); }
  void ProcessorTree::redo() { undoManager_->redo(); }

  void ProcessorTree::addProcessor(utils::up<Generation::BaseProcessor> processor)
  {
    auto processorId = processor->processorId_;
    allProcessors_.add(processorId, COMPLEX_MOVE(processor));
    if ((float)allProcessors_.data.size() / (float)allProcessors_.data.capacity() >= expandThreshold)
    {
      utils::VectorMap<u64, utils::up<Generation::BaseProcessor>>
        newAllParameters{ allProcessors_.data.size() * expandAmount };

      auto swap = [this, &newAllParameters]()
      {
        for (auto &pair : allProcessors_.data)
          newAllParameters.data.emplace_back(COMPLEX_MOVE(pair));

        allProcessors_.data.swap(newAllParameters.data);
      };
      executeOutsideProcessing(swap);
    }
  }
}
