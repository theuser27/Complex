/*
  ==============================================================================

    update_types.cpp
    Created: 16 Nov 2022 2:11:06am
    Author:  theuser27

  ==============================================================================
*/

#include "update_types.hpp"

#include "Generation/BaseProcessor.hpp"
#include "Plugin/ProcessorTree.hpp"
#include "Framework/parameter_value.hpp"
#include "Interface/Components/BaseControl.hpp"
#include "Interface/Sections/BaseSection.hpp"

namespace Framework
{
  AddProcessorUpdate::~AddProcessorUpdate()
  {
    if (!processorTree_.isBeingDestroyed() && addedProcessor_ && !isDone_)
      processorTree_.deleteProcessor(addedProcessor_->getProcessorId());
  }

  bool AddProcessorUpdate::perform()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    // if we've already undone once and we're redoing, we need to pass the already created module
    if (!addedProcessor_)
      addedProcessor_ = instantiationFunction_(processorTree_);

    auto g = wait();
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, *addedProcessor_);

    isDone_ = true;

    return true;
  }

  bool AddProcessorUpdate::undo()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    addedProcessor_ = &destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

    isDone_ = false;

    return true;
  }

  CopyProcessorUpdate::~CopyProcessorUpdate()
  {
    if (!processorTree_.isBeingDestroyed() && !isDone_)
      processorTree_.deleteProcessor(processorCopy.getProcessorId());
  }

  bool CopyProcessorUpdate::perform()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, processorCopy);

    isDone_ = true;

    return true;
  }

  bool CopyProcessorUpdate::undo()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

    isDone_ = false;

    return true;
  }

  bool MoveProcessorUpdate::perform()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);
    auto sourcePointer = processorTree_.getProcessor(sourceProcessorId_);

    auto g = wait();
    auto &movedProcessor = sourcePointer->deleteSubProcessor(sourceSubProcessorIndex_);
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, movedProcessor);

    return true;
  }

  bool MoveProcessorUpdate::undo()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);
    auto sourcePointer = processorTree_.getProcessor(sourceProcessorId_);

    auto g = wait();
    auto &movedProcessor = destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);
    sourcePointer->insertSubProcessor(sourceSubProcessorIndex_, movedProcessor);

    return true;
  }

  UpdateProcessorUpdate::~UpdateProcessorUpdate()
  {
    if (!processorTree_.isBeingDestroyed() && savedProcessor_)
      processorTree_.deleteProcessor(savedProcessor_->getProcessorId());
  }

  bool UpdateProcessorUpdate::perform()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    // if we've already undone once and we're redoing, we need to pass the already created module
    if (!savedProcessor_)
      savedProcessor_ = instantiationFunction_(processorTree_);

    auto g = wait();
    savedProcessor_ = &destinationPointer->updateSubProcessor(destinationSubProcessorIndex_, *savedProcessor_);

    return true;
  }

  bool UpdateProcessorUpdate::undo()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    savedProcessor_ = &destinationPointer->updateSubProcessor(destinationSubProcessorIndex_, *savedProcessor_);

    return true;
  }

  DeleteProcessorUpdate::~DeleteProcessorUpdate()
  {
    if (!processorTree_.isBeingDestroyed() && deletedProcessor_ && isDone_)
      processorTree_.deleteProcessor(deletedProcessor_->getProcessorId());
  }

  bool DeleteProcessorUpdate::perform()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    deletedProcessor_ = &destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);
    auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      processor->setSavedSection(nullptr);
      for (usize i = 0; i < processor->getSubProcessorCount(); ++i)
        self(self, processor->getSubProcessor(i));
    };
    recurseParameters(recurseParameters, deletedProcessor_);

    isDone_ = true;

    return true;
  }

  bool DeleteProcessorUpdate::undo()
  {
    auto destinationPointer = processorTree_.getProcessor(destinationProcessorId_);

    auto g = wait();
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, *deletedProcessor_);
    auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      for (usize i = 0; i < processor->getSubProcessorCount(); ++i)
        self(self, processor->getSubProcessor(i));
    };
    recurseParameters(recurseParameters, deletedProcessor_);

    isDone_ = false;

    return true;
  }

  bool ParameterUpdate::perform()
  {
    // if this is being performed right after being added to the UndoManager, then the value change was already made
    // on the other hand, if this is being called on a redo, we need to change the value appropriately
    if (!firstTime_)
    {
      if (details_.has_value())
      {
        auto details = baseParameter_->getParameterDetails();
        // TODO: currently  no BaseControl implements setParameterDetails correctly
        baseParameter_->setParameterDetails(details_.value());
        details_ = details;
      }
      baseParameter_->setValueRaw(newValue_);
      baseParameter_->setValueToHost();
    }
    
    firstTime_ = false;
    return true;
  }

  bool ParameterUpdate::undo()
  {
    if (details_.has_value())
    {
      auto details = baseParameter_->getParameterDetails();
      baseParameter_->setParameterDetails(details_.value());
      details_ = details;
    }
    
    baseParameter_->setValueRaw(oldValue_);
    baseParameter_->setValueToHost();
    return true;
  }
}
