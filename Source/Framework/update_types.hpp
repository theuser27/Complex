/*
  ==============================================================================

    update_types.hpp
    Created: 16 Nov 2022 2:11:06am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "utils.hpp"
#include "sync_primitives.hpp"
#include "parameters.hpp"

namespace Plugin
{
  class ProcessorTree;
}

namespace Generation
{
  class BaseProcessor;
}

namespace Interface
{
  class BaseControl;
}

namespace Framework
{
  enum class UpdateType { AddProcessor, MoveProcessor, DeleteProcessor, ParameterUpdate, PresetUpdate };

  class UndoableAction
  {
  protected:
    UndoableAction() = default;
  public:
    UpdateType type{};
    virtual ~UndoableAction() = default;

    virtual bool perform() = 0;
    virtual bool undo() = 0;

    /// Allows multiple actions to be coalesced into a single action object, to reduce storage space.
    /// The combined action can be the current, next or an entire new action
    virtual UndoableAction *combineActions([[maybe_unused]] UndoableAction *nextAction) { return nullptr; }
  };

  class UndoManager
  {
  public:
    UndoManager(usize transactionsToKeep);

    ~UndoManager();

    void clearUndoHistory();
    void setTransationStorage(usize transactionsToKeep);

    /// Performs an action and adds it to the undo history list.
    bool perform(UndoableAction *action);

    /// Starts a new group of actions that together will be treated as a single transaction.
    void beginNewTransaction();

    // Returns true if there's at least one action in the list to undo.
    bool canUndo() const { return undoActionsCount; }

    /// Tries to roll-back the last transaction, 
    /// returns true if the transaction can be undone, and false if it fails, or
    /// if there aren't any transactions to undo
    /// 
    /// if undoCurrentTransactionOnly == true, then the transaction index won't be changed
    /// when undone and things can immediately be added to the current transaction
    bool undo(bool undoCurrentTransactionOnly = false);

    /// Returns the number of UndoableAction objects that have been performed during the
    /// transaction that is currently open.
    usize getNumActionsInCurrentTransaction() const { return transactions[currentIndex].size(); }

    /// Returns true if there's at least one action in the list to redo.
    bool canRedo() const { return redoActionsCount; }

    /// Tries to redo the last transaction that was undone.
    /// returns true if the transaction can be redone, and false if it fails, or
    /// if there aren't any transactions to redo
    bool redo();

    /// Returns true if the caller code is in the middle of an undo or redo action.
    bool isPerformingUndoRedo() const noexcept { return isInsideUndoRedoCall; }

  private:
    std::vector<std::vector<utils::up<UndoableAction>>> transactions{};
    usize currentIndex = 0, undoActionsCount = 0, redoActionsCount = 0;
    bool isInsideUndoRedoCall = false;
  };


  // most of the updates need to happen outside of processing, but parts of them could be unbounded in time
  // (i.e. allocations) however those can happen regardless if we're currently processing or not
  // so we can specify *when* we need to wait by injecting a synchronisation function into the update
  class WaitingUpdate : public UndoableAction
  {
  public:
    void setWaitFunction(clg::small_fn<utils::ScopedLock()> waitFunction) { waitFunction_ = COMPLEX_MOVE(waitFunction); }
  protected:
    auto wait() const { return waitFunction_(); }
    clg::small_fn<utils::ScopedLock()> waitFunction_ = []() -> utils::ScopedLock
    {
      COMPLEX_ASSERT_FALSE("Wait function for update hasn't been set");
      std::abort();
    };
  };

  // updates use IDs to point to processors since they are guaranteed to be unique
  // which allows implementation to move memory around (wouldn't be possible with pointers)
  
  class AddProcessorUpdate final : public WaitingUpdate
  {
  public:
    AddProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
      usize destinationSubProcessorIndex, Generation::BaseProcessor &newProcessor) noexcept : 
      processorTree_(processorTree), addedProcessor_(&newProcessor),
      destinationProcessorId_(destinationProcessorId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { type = UpdateType::AddProcessor; }

    ~AddProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    Generation::BaseProcessor *addedProcessor_;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
  };

  class MoveProcessorUpdate final : public WaitingUpdate
  {
  public:
    MoveProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
      usize destinationSubProcessorIndex, u64 sourceProcessorId, usize sourceSubProcessorIndex) noexcept :
      processorTree_(processorTree), destinationProcessorId_(destinationProcessorId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex), sourceProcessorId_(sourceProcessorId),
      sourceSubProcessorIndex_(sourceSubProcessorIndex) { type = UpdateType::MoveProcessor; }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
    u64 sourceProcessorId_;
    usize sourceSubProcessorIndex_;
  };

  class DeleteProcessorUpdate final : public WaitingUpdate
  {
  public:
    DeleteProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
      usize destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
      destinationProcessorId_(destinationProcessorId), 
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { type = UpdateType::DeleteProcessor; }

    ~DeleteProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    Generation::BaseProcessor *deletedProcessor_ = nullptr;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
  };

  class ParameterUpdate final : public WaitingUpdate
  {
  public:
    ParameterUpdate(Interface::BaseControl *parameterUI, double oldValue, double newValue) :
      baseParameter_(parameterUI), oldValue_(oldValue), newValue_(newValue) { type = UpdateType::ParameterUpdate; }

    bool perform() override;
    bool undo() override;

    void setDetailsChange(ParameterDetails details) noexcept { details_ = details; }

    UndoableAction *combineActions(UndoableAction *nextAction) override
    {
      auto parameterUpdate = static_cast<ParameterUpdate *>(nextAction);
      if (nextAction->type != UpdateType::ParameterUpdate || baseParameter_ != parameterUpdate->baseParameter_)
        return nullptr;

      newValue_ = parameterUpdate->newValue_;
      return this;
    }

  private:
    Interface::BaseControl *baseParameter_;
    std::optional<ParameterDetails> details_{};
    double oldValue_;
    double newValue_;
    bool firstTime_ = true;
  };

  class PresetUpdate final : public WaitingUpdate
  {
  public:
    PresetUpdate(Plugin::ProcessorTree &processorTree, utils::whatever newSavedState) :
      processorTree_(processorTree), newSavedState_{ COMPLEX_MOVE(newSavedState) } 
    { type = UpdateType::PresetUpdate; }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;
    utils::whatever oldSavedState_{};
    utils::whatever newSavedState_{};
    bool firstTime_ = true;
  };
}
