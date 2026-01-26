
// Created: 2022-11-16 02:11:06

#include "update_types.hpp"

#include "Generation/BaseProcessor.hpp"
#include "Plugin/Complex.hpp"
#include "Framework/parameter_value.hpp"
#include "Interface/Components/BaseControl.hpp"

namespace Framework
{
  UndoManager::UndoManager(usize transactionsToKeep)
  {
    setTransationStorage(transactionsToKeep);
  }
  UndoManager::~UndoManager() = default;

  void UndoManager::setTransationStorage(usize transactionsToKeep)
  {
    transactionsToKeep = utils::max(transactionsToKeep, (usize)1);
    if (transactions.size() == transactionsToKeep)
      return;

    auto newTransactions = decltype(transactions){ utils::generalAllocator, transactionsToKeep };
    if (!transactions.size())
    {
      clearUndoHistory();
      transactions = COMPLEX_MOVE(newTransactions);
      return;
    }

    usize copySize, begin;
    if ((usize)(undoActionsCount + redoActionsCount) <= transactionsToKeep)
    {
      // new buffer can house all currently stored actions
      copySize = undoActionsCount + redoActionsCount;
      begin = ((usize)currentIndex + 1 - undoActionsCount +
        transactions.size()) % transactions.size();
    }
    else
    {
      // new buffer can't house all currently stored actions
      // use half the size for undo and redo actions respectively
      copySize = transactionsToKeep;
      redoActionsCount = transactionsToKeep / 2;
      undoActionsCount = transactionsToKeep - redoActionsCount;
      begin = ((usize)currentIndex + 1 - undoActionsCount +
        transactions.size()) % transactions.size();
    }

    currentIndex = undoActionsCount - 1;
    for (usize i = 0; i < copySize; ++i)
      newTransactions[i] = COMPLEX_MOVE(transactions[(begin + i) % transactions.size()]);
    transactions = COMPLEX_MOVE(newTransactions);
  }

  void UndoManager::clearUndoHistory()
  {
    transactions.clear();
    currentIndex = 0;
    undoActionsCount = 1; // for the currentIndex
    redoActionsCount = 0;
  }

  bool UndoManager::perform(UndoableAction *newAction)
  {
    if (newAction == nullptr)
      return false;

    utils::up action{ newAction };

    if (isPerformingUndoRedo())
    {
      COMPLEX_ASSERT_FALSE("Don't call perform() recursively from the UndoableAction::perform() \
        or undo() methods, or else these actions will be discarded!");
      return false;
    }

    if (!action->perform())
      return false;

    COMPLEX_ASSERT(redoActionsCount == 0, "You need to call beginNewTransaction() \
      if you want to overwrite undone actions");

    auto &actionSet = transactions[currentIndex];
    if (!actionSet.size())
      actionSet.emplace_back(COMPLEX_MOVE(action));
    else
    {
      auto &lastAction = actionSet.back();
      auto coalescedAction = lastAction->combineActions(action.get());
      if (coalescedAction != lastAction.get())
      {
        if (coalescedAction && coalescedAction != action.get())
        {
          action.reset(coalescedAction);
          actionSet.pop_back();
        }
        
        actionSet.emplace_back(COMPLEX_MOVE(action));
      }
    }

    return true;
  }

  void UndoManager::beginNewTransaction()
  {
    // skip making a new action set if current one is empty
    if (!transactions[currentIndex].size())
      return;
    currentIndex = (currentIndex + 1) % transactions.size();
    transactions[currentIndex] = decltype(transactions)::value_type{};

    undoActionsCount = utils::min(undoActionsCount + 1, transactions.size());
    redoActionsCount = 0;
  }

  bool UndoManager::undo(bool undoCurrentTransactionOnly)
  {
    if (undoActionsCount == 0)
      return false;
    
    isInsideUndoRedoCall = true;

    bool success = [](auto &actions)
    {
      for (usize i = actions.size(); i > 0; --i)
        if (!actions[i - 1]->undo())
          return false;

      return true;
    }(transactions[currentIndex]);

    if (success)
    {
      if (!undoCurrentTransactionOnly)
      {
        currentIndex = (currentIndex - 1 + transactions.size()) % transactions.size();
        --undoActionsCount;
        ++redoActionsCount;
      }
    }
    else
      clearUndoHistory(); // very severe edge case, shouldn't happen

    //sendChangeMessage();
    isInsideUndoRedoCall = false;
    return true;
  }

  bool UndoManager::redo()
  {
    if (redoActionsCount == 0)
      return false;

    isInsideUndoRedoCall = true;

    bool success = [](auto &actions)
    {
      for (usize i = actions.size(); i > 0; --i)
        if (!actions[i - 1]->undo())
          return false;

      return true;
    }(transactions[(currentIndex + 1) % transactions.size()]);

    if (success)
    {
      currentIndex = (currentIndex + 1) % transactions.size();
      --redoActionsCount;
      ++undoActionsCount;
    }
    else
      clearUndoHistory(); // very severe edge case, shouldn't happen

    //sendChangeMessage();
    isInsideUndoRedoCall = false;
    return true;
  }

  AddProcessorUpdate::~AddProcessorUpdate()
  {
    if (addedProcessor_)
      state_.deleteProcessor(addedProcessor_);
  }

  bool AddProcessorUpdate::perform()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);
    
    auto g = wait();
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, *addedProcessor_);

    addedProcessor_ = nullptr;

    return true;
  }

  bool AddProcessorUpdate::undo()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);

    auto g = wait();
    addedProcessor_ = &destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

    return true;
  }

  bool MoveProcessorUpdate::perform()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);
    auto sourcePointer = state_.getProcessor(sourceStateId_);

    auto g = wait();
    auto &movedProcessor = sourcePointer->deleteSubProcessor(sourceSubProcessorIndex_, false);
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, movedProcessor, false);

    //if (sourcePointer != destinationPointer)
    //  for (auto listener : sourcePointer->listeners_)
    //    listener->movedSubProcessor(movedProcessor, *sourcePointer, 
    //      sourceSubProcessorIndex_, *destinationPointer, destinationSubProcessorIndex_);
    //
    //for (auto listener : destinationPointer->listeners_)
    //  listener->movedSubProcessor(movedProcessor, *sourcePointer, 
    //    sourceSubProcessorIndex_, *destinationPointer, destinationSubProcessorIndex_);

    return true;
  }

  bool MoveProcessorUpdate::undo()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);
    auto sourcePointer = state_.getProcessor(sourceStateId_);

    auto g = wait();
    auto &movedProcessor = destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_, false);
    sourcePointer->insertSubProcessor(sourceSubProcessorIndex_, movedProcessor, false);

    //if (sourcePointer != destinationPointer)
    //  for (auto listener : sourcePointer->listeners_)
    //    listener->movedSubProcessor(movedProcessor, *destinationPointer, 
    //      destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

    //for (auto listener : destinationPointer->listeners_)
    //  listener->movedSubProcessor(movedProcessor, *destinationPointer, 
    //    destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

    return true;
  }

  DeleteProcessorUpdate::~DeleteProcessorUpdate()
  {
    if (deletedProcessor_)
      state_.deleteProcessor(deletedProcessor_);
  }

  bool DeleteProcessorUpdate::perform()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);

    auto g = wait();
    deletedProcessor_ = &destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);
    auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      auto child = processor->children;
      for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
        self(self, child);
    };
    recurseParameters(recurseParameters, deletedProcessor_);

    return true;
  }

  bool DeleteProcessorUpdate::undo()
  {
    auto destinationPointer = state_.getProcessor(destinationStateId_);

    auto g = wait();
    destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, *deletedProcessor_);
    auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      auto child = processor->children;
      for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
        self(self, child);
    };
    recurseParameters(recurseParameters, deletedProcessor_);

    deletedProcessor_ = nullptr;

    return true;
  }

  bool ParameterUpdate::perform()
  {
    // if this is being performed right after being added to the UndoManager, then the value change was already made
    // on the other hand, if this is being called on a redo, we need to change the value appropriately
    if (!firstTime_)
    {
      if (hasDetails_)
      {
        auto details = baseParameter_->details;
        // TODO: currently  no BaseControl implements setParameterDetails correctly
        baseParameter_->details = details_;
        details_ = details;
      }
      baseParameter_->setValue(newValue_, false);
      baseParameter_->setValueToHost();
    }
    
    firstTime_ = false;
    return true;
  }

  bool ParameterUpdate::undo()
  {
    if (hasDetails_)
    {
      auto details = baseParameter_->details;
      baseParameter_->details = details_;
      details_ = details;
    }
    
    baseParameter_->setValue(oldValue_, false);
    baseParameter_->setValueToHost();
    return true;
  }

  bool PresetUpdate::perform()
  {
    auto g = wait();
    auto oldState = plugin_.exchangeStates(COMPLEX_MOVE(state));
    state = COMPLEX_MOVE(oldState);

    return true;
  }

  bool PresetUpdate::undo()
  {
    auto g = wait();
    auto newState = plugin_.exchangeStates(COMPLEX_MOVE(state));
    state = COMPLEX_MOVE(newState);

    return true;
  }
}
