/*
  ==============================================================================

    update_types.hpp
    Created: 16 Nov 2022 2:11:06am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <any>
#include <juce_data_structures/juce_data_structures.h>

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
  class ParameterValue;
  class PresetUpdate;

  // most of the updates need to happen outside of processing, but parts of them could be unbounded in time
  // (i.e. allocations) however those can happen regardless if we're currently processing or not
  // so we can specify *when* we need to wait by injecting a synchronisation function into the update
  class WaitingUpdate : public juce::UndoableAction
  {
  public:
    void setWaitFunction(std::function<utils::ScopedLock()> waitFunction) { waitFunction_ = COMPLEX_MOV(waitFunction); }
  protected:
    auto wait() const { return waitFunction_(); }
    std::function<utils::ScopedLock()> waitFunction_ = []()
    { 
      COMPLEX_ASSERT_FALSE("Wait function for update hasn't been set");
      std::atomic<bool> temp{};
      return utils::ScopedLock{ temp, utils::WaitMechanism::Wait };
    };
    bool isDone_ = false;
  };
  
  class AddProcessorUpdate final : public WaitingUpdate
  {
  public:
    AddProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId, usize destinationSubProcessorIndex, 
      clg::small_fn<Generation::BaseProcessor *(Plugin::ProcessorTree &)> instantiationFunction) noexcept : processorTree_(processorTree),
      instantiationFunction_(COMPLEX_MOV(instantiationFunction)), destinationProcessorId_(destinationProcessorId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

    ~AddProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    clg::small_fn<Generation::BaseProcessor *(Plugin::ProcessorTree &processorTree)> instantiationFunction_;
    Generation::BaseProcessor *addedProcessor_ = nullptr;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
  };

  class CopyProcessorUpdate final : public WaitingUpdate
  {
  public:
    CopyProcessorUpdate(Plugin::ProcessorTree &processorTree, Generation::BaseProcessor &processorCopy, 
      u64 destinationProcessorId, usize destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
      processorCopy(processorCopy), destinationProcessorId_(destinationProcessorId),
      destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

    ~CopyProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    Generation::BaseProcessor &processorCopy;

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
      sourceSubProcessorIndex_(sourceSubProcessorIndex) { }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
    u64 sourceProcessorId_;
    usize sourceSubProcessorIndex_;
  };

  class UpdateProcessorUpdate final : public WaitingUpdate
  {
  public:
    UpdateProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId, usize destinationSubProcessorIndex,
      clg::small_fn<Generation::BaseProcessor *(Plugin::ProcessorTree &)> instantiationFunction) noexcept :
      processorTree_(processorTree), instantiationFunction_(COMPLEX_MOV(instantiationFunction)),
      destinationProcessorId_(destinationProcessorId), destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

    ~UpdateProcessorUpdate() override;

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;

    clg::small_fn<Generation::BaseProcessor *(Plugin::ProcessorTree &processorTree)> instantiationFunction_;
    Generation::BaseProcessor *savedProcessor_ = nullptr;

    u64 destinationProcessorId_;
    usize destinationSubProcessorIndex_;
  };

  class DeleteProcessorUpdate final : public WaitingUpdate
  {
  public:
    DeleteProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
      usize destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
      destinationProcessorId_(destinationProcessorId), destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

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
      baseParameter_(parameterUI), oldValue_(oldValue), newValue_(newValue) { }

    bool perform() override;
    bool undo() override;

    void setDetailsChange(ParameterDetails details) noexcept { details_ = details; }

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
    PresetUpdate(Plugin::ProcessorTree &processorTree, std::any newSavedState) :
      processorTree_(processorTree), newSavedState_{ COMPLEX_MOV(newSavedState) }{ }

    bool perform() override;
    bool undo() override;

  private:
    Plugin::ProcessorTree &processorTree_;
    std::any oldSavedState_{};
    std::any newSavedState_{};
    bool firstTime_ = true;
  };
}
