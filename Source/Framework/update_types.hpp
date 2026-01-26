
// Created: 2022-11-16 02:11:06

#pragma once

#include "utils.hpp"
#include "sync_primitives.hpp"
#include "parameter_types.hpp"
#include "Plugin/Complex.hpp"

namespace Plugin
{
  struct ComplexPlugin;
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
  enum UpdateType : u32
  {
    kAddProcessorUpdate,
    kMoveProcessorUpdate,
    kDeleteProcessorUpdate,
    kParameterUpdate,
    kPresetUpdate
  };

  // most of the updates need to happen outside of processing, but parts of them could be unbounded in time
  // (i.e. allocations) however those can happen regardless if we're currently processing or not
  // so we can specify *when* we need to wait by injecting a synchronisation function into the update
  class WaitingUpdate : public UndoableAction
  {
  public:
    utils::smallFn<utils::ScopedLock()> wait{};
  };

  // updates use IDs to point to processors since they are guaranteed to be unique
  // which allows implementation to move memory around (wouldn't be possible with pointers)
  
  class AddProcessorUpdate final : public WaitingUpdate
  {
  public:
    AddProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex, Generation::BaseProcessor &newProcessor) noexcept : 
      state_(state), addedProcessor_(&newProcessor), destinationStateId_(destinationStateId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { type = kAddProcessorUpdate; }

    ~AddProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::State &state_;

    Generation::BaseProcessor *addedProcessor_;

    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
  };

  class MoveProcessorUpdate final : public WaitingUpdate
  {
  public:
    MoveProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex, u64 sourceStateId, usize sourceSubProcessorIndex) noexcept :
      state_(state), destinationStateId_(destinationStateId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex), sourceStateId_(sourceStateId),
      sourceSubProcessorIndex_(sourceSubProcessorIndex) { type = kMoveProcessorUpdate; }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::State &state_;

    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
    u64 sourceStateId_;
    usize sourceSubProcessorIndex_;
  };

  class DeleteProcessorUpdate final : public WaitingUpdate
  {
  public:
    DeleteProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex) noexcept : state_(state),
      destinationStateId_(destinationStateId), 
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { type = kDeleteProcessorUpdate; }

    ~DeleteProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::State &state_;

    Generation::BaseProcessor *deletedProcessor_ = nullptr;

    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
  };

  class ParameterUpdate final : public WaitingUpdate
  {
  public:
    ParameterUpdate(Interface::BaseControl *parameterUI, double oldValue, double newValue, 
      ParameterDetails *optionalDetails = nullptr) : baseParameter_(parameterUI), 
      oldValue_(oldValue), newValue_(newValue)
    {
      type = kParameterUpdate;
      if (optionalDetails)
      {
        details_ = *optionalDetails;
        hasDetails_ = true;
      }
    }

    bool perform() override;
    bool undo() override;

    UndoableAction *combineActions(UndoableAction *nextAction) override
    {
      auto parameterUpdate = static_cast<ParameterUpdate *>(nextAction);
      if (nextAction->type != kParameterUpdate || baseParameter_ != parameterUpdate->baseParameter_)
        return nullptr;

      newValue_ = parameterUpdate->newValue_;
      return this;
    }


  private:
    Interface::BaseControl *baseParameter_;
    double oldValue_;
    double newValue_;
    ParameterDetails details_{};
    bool hasDetails_ = false;
    bool firstTime_ = true;
  };

  class PresetUpdate final : public WaitingUpdate
  {
  public:
    PresetUpdate(Plugin::ComplexPlugin &plugin, utils::sp<Plugin::State> newSavedState) :
      plugin_(plugin), state{ COMPLEX_MOVE(newSavedState) }
    { type = kPresetUpdate; }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ComplexPlugin &plugin_;
    utils::sp<Plugin::State> state;
  };
}
