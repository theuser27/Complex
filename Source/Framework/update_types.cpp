
// Created: 2022-11-16 02:11:06

#include "Generation/BaseProcessor.hpp"
#include "Plugin/Complex.hpp"
#include "Framework/parameter_value.hpp"
#include "Interface/Components/BaseControl.hpp"

namespace Framework
{
  UndoManager::UndoManager(usize transactionsToKeep)
  {
    storage = utils::bumpArena::create(COMPLEX_MB(64), transactionsToKeep * 512);
    setTransationStorage(transactionsToKeep);
  }

  UndoManager::~UndoManager()
  {
    utils::bumpArena::destroy(storage);
  }

  void UndoManager::setTransationStorage(usize transactionsToKeep)
  {
    transactionsToKeep = utils::max(transactionsToKeep, (usize)1);
    if (transactions.size() == transactionsToKeep)
      return;

    if (!transactions.size())
    {
      transactions = { (decltype(transactions)::value_type *)nullptr, transactionsToKeep };
      clear();
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

    auto *newTransactions = arranew(storage, decltype(transactions)::value_type,
      transactionsToKeep, {});
    for (usize i = 0; i < copySize; ++i)
      newTransactions[i] = transactions[(begin + i) % transactions.size()];
    transactions = { newTransactions, transactionsToKeep };
  }

  void UndoManager::clear()
  {
    currentIndex = 0;
    undoActionsCount = 1; // for the currentIndex
    redoActionsCount = 0;

    // this will also deallocate the transactions buffer, so we need to allocate it again
    storage->clear(storage);

    auto *newTransactions = arranew(storage, decltype(transactions)::value_type,
      transactions.size(), {});
    transactions = { newTransactions, transactions.size() };
  }

  bool UndoManager::perform(UndoAction *newAction, bool performOnAdd)
  {
    if (newAction == nullptr)
      return false;

    if (isPerformingUndoRedo())
    {
      COMPLEX_ASSERT_FALSE("Don't call perform() recursively from the UndoAction::perform() \
        or undo() methods, or else these actions will be discarded!");
      return false;
    }

    if (performOnAdd)
      newAction->redo(newAction);

    COMPLEX_ASSERT(redoActionsCount == 0, "You need to call beginNewTransaction() \
      if you want to overwrite undone actions");

    auto &transaction = transactions[currentIndex];
    if (!transaction.second)
      transaction.second = newAction;
    else if (!transaction.second->combineActions)
    {
      newAction->next = transaction.second;
      transaction.second = newAction->next;
    }
    else
    {
      auto *lastAction = transaction.second;
      auto coalescedAction = lastAction->combineActions(
        transaction.first, lastAction, newAction);

      if (!coalescedAction)
      {
        newAction->next = lastAction;
        transaction.second = newAction;
      }
      else
      {
        coalescedAction->next = lastAction->next;
        transaction.second = coalescedAction;

        if (coalescedAction != newAction)
          utils::bumpArena::remove(newAction);
        if (coalescedAction != lastAction)
          utils::bumpArena::remove(lastAction);
      }
    }

    return true;
  }

  utils::bumpArena *
  UndoManager::beginNewTransaction()
  {
    bool currentHasArena = transactions[currentIndex].first;

    // skip making a new action set if current one is empty
    if (!currentHasArena || transactions[currentIndex].second)
    {
      if (currentHasArena)
      {
        currentIndex = (currentIndex + 1) % transactions.size();

        // destroy transaction to be replaced
        for (auto *action = transactions[currentIndex].second; action; action = action->next)
          if (action->destructor)
            action->destructor(action);

        undoActionsCount = utils::min(undoActionsCount + 1, transactions.size());
        redoActionsCount = 0;
      }

      transactions[currentIndex].second = {};
      if (!transactions[currentIndex].first)
        transactions[currentIndex].first = utils::bumpArena::createNested(storage, 512);
    }

    return transactions[currentIndex].first;
  }

  bool UndoManager::undo(bool undoCurrentTransactionOnly)
  {
    if (undoActionsCount == 0)
      return false;
    
    isInsideUndoRedoCall = true;

    COMPLEX_FOREACH_AND_REVERSE_SLL(action, transactions[currentIndex].second)
    {
      action->undo(action);
      transactions[currentIndex].second = action;
    }

    if (!undoCurrentTransactionOnly)
    {
      currentIndex = (currentIndex - 1 + transactions.size()) % transactions.size();
      --undoActionsCount;
      ++redoActionsCount;
    }

    isInsideUndoRedoCall = false;
    return true;
  }

  bool UndoManager::redo()
  {
    if (!redoActionsCount)
      return false;

    isInsideUndoRedoCall = true;

    COMPLEX_FOREACH_AND_REVERSE_SLL(action, transactions[(currentIndex + 1) % transactions.size()].second)
    {
      action->redo(action);
      transactions[currentIndex].second = action;
    }
    
    currentIndex = (currentIndex + 1) % transactions.size();
    --redoActionsCount;
    ++undoActionsCount;

    isInsideUndoRedoCall = false;
    return true;
  }

  AddProcessorUpdate::AddProcessorUpdate(Plugin::State &state, u64 destinationStateId, 
    usize destinationSubProcessorIndex, Generation::BaseProcessor &newProcessor) :
    state_(state), addedProcessor_(&newProcessor), destinationStateId_(destinationStateId),
    destinationSubProcessorIndex_(destinationSubProcessorIndex)
  {
    destructor = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      if (self->addedProcessor_)
        self->state_.deleteProcessor(self->addedProcessor_);
    };

    redo = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      destinationPointer->insertSubProcessor(
        self->destinationSubProcessorIndex_, *self->addedProcessor_);

      self->addedProcessor_ = nullptr;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      self->addedProcessor_ = &destinationPointer->deleteSubProcessor(self->destinationSubProcessorIndex_);

    };
  }

  MoveProcessorUpdate::MoveProcessorUpdate(Plugin::State &state, u64 destinationStateId,
    usize destinationSubProcessorIndex, u64 sourceStateId, usize sourceSubProcessorIndex) :
    state_(state), destinationStateId_(destinationStateId),
    destinationSubProcessorIndex_(destinationSubProcessorIndex), sourceStateId_(sourceStateId),
    sourceSubProcessorIndex_(sourceSubProcessorIndex)
  {
    redo = [](UndoAction *a)
    {
      auto *self = (MoveProcessorUpdate *)a;

      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);
      auto sourcePointer = self->state_.getProcessor(self->sourceStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      auto &movedProcessor = sourcePointer->deleteSubProcessor(self->sourceSubProcessorIndex_, false);
      destinationPointer->insertSubProcessor(self->destinationSubProcessorIndex_, movedProcessor, false);

      //if (sourcePointer != destinationPointer)
      //  for (auto listener : sourcePointer->listeners_)
      //    listener->movedSubProcessor(movedProcessor, *sourcePointer, 
      //      sourceSubProcessorIndex_, *destinationPointer, destinationSubProcessorIndex_);
      //
      //for (auto listener : destinationPointer->listeners_)
      //  listener->movedSubProcessor(movedProcessor, *sourcePointer, 
      //    sourceSubProcessorIndex_, *destinationPointer, destinationSubProcessorIndex_);
    };

    undo = [](UndoAction *a)
    {
      auto *self = (MoveProcessorUpdate *)a;

      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);
      auto sourcePointer = self->state_.getProcessor(self->sourceStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      auto &movedProcessor = destinationPointer->deleteSubProcessor(self->destinationSubProcessorIndex_, false);
      sourcePointer->insertSubProcessor(self->sourceSubProcessorIndex_, movedProcessor, false);

      //if (sourcePointer != destinationPointer)
      //  for (auto listener : sourcePointer->listeners_)
      //    listener->movedSubProcessor(movedProcessor, *destinationPointer, 
      //      destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

      //for (auto listener : destinationPointer->listeners_)
      //  listener->movedSubProcessor(movedProcessor, *destinationPointer, 
      //    destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

    };
  }

  DeleteProcessorUpdate::DeleteProcessorUpdate(Plugin::State &state, 
    u64 destinationStateId, usize destinationSubProcessorIndex) : state_(state),
    destinationStateId_(destinationStateId),
    destinationSubProcessorIndex_(destinationSubProcessorIndex)
  {
    destructor = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      if (self->deletedProcessor_)
        self->state_.deleteProcessor(self->deletedProcessor_);
    };

    redo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      self->deletedProcessor_ = &destinationPointer->deleteSubProcessor(self->destinationSubProcessorIndex_);
      auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->deletedProcessor_);
    };

    undo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto destinationPointer = self->state_.getProcessor(self->destinationStateId_);

      auto g = self->state_.plugin->acquireProcessingLock();
      destinationPointer->insertSubProcessor(self->destinationSubProcessorIndex_, *self->deletedProcessor_);
      auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->deletedProcessor_);

      self->deletedProcessor_ = nullptr;
    };
  }
}
