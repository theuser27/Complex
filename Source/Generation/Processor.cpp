
// Created: 2022-07-11 03:35:27

#include "Processor.hpp"

#include "Framework/metadata.hpp"
#include "Framework/parameters.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/LookAndFeel/Component.hpp"
#include "Interface/Components/Control.hpp"

namespace Generation
{
  Processor::Processor(utils::Pool *arena, Plugin::State *state, Framework::ProcessorMetadata *metadata) : 
    metadata{ metadata }, state{ state }, stateId{ ++state->stateIdCounter }, arena{ arena }, name{ arena } { }

  void resetProcessorGeneric(Generation::Processor *processor)
  {
    for (auto *parameter = processor->parameters; parameter; parameter = parameter->next)
      parameter->reset();
  }

  // the following functions are to be called outside of processing time
  bool
  Processor::addChildProcessor(Processor &newChildProcessor, Processor *insertBefore)
  {
    COMPLEX_ASSERT(!newChildProcessor.parent);

    if (!metadata->acceptsChild(newChildProcessor.metadata))
    {
      auto string = utils::string::create(getLocalScratch(),
        "Attempted insert of %v(id: %zu) inside an %v(id: %zu). \
        If this shows to you as a user, report it to the dev. \n\nSupported subprocessors by this type are: ",
        newChildProcessor.metadata->name, newChildProcessor.metadata->id, metadata->name, metadata->id
      );

      for (auto *acceptedChild = metadata->children; acceptedChild; acceptedChild = acceptedChild->next)
      {
        auto *format = (acceptedChild->next) ? "%v(id: %zu), " : "%v(id: %zu).";
        string.appendFormat(format, acceptedChild->name, acceptedChild->id);
      }

      Interface::showNativeMessageBox("Erroneous processor insert", string.data(), Interface::MessageBoxType::Warning);
      return false;
    }

    ++childrenCount;
    newChildProcessor.parent = this;
    utils::insertDllHalfConnected(&newChildProcessor, insertBefore, children);

    return true;
  }

  void Processor::removeChildProcessor(Processor &removedChildProcessor)
  {
    COMPLEX_ASSERT(removedChildProcessor.parent == this);

    --childrenCount;
    removedChildProcessor.parent = nullptr;
    utils::removeDllHalfConnected(&removedChildProcessor, children);
  }

  void Processor::createChildren(const Processor &other)
  {
    childrenCount = other.childrenCount;

    children = other.children->createCopy();
    children->parent = this;
    for (auto otherChild = other.children->next, child = children; otherChild;
      (otherChild = otherChild->next), (child = child->next))
    {
      child->next = otherChild->createCopy();
      child->next->parent = this;
      child->next->previous = child;
    }
  }

  Framework::SimdBuffer *
  Processor::createBuffer(const Framework::SimdBuffer *other)
  {
    auto channels = (other) ? other->channels : kChannelsPerInOut;
    auto *buffer = Framework::SimdBuffer::create(arena, channels, state->getMaxBinCount());

    if (other)
      Framework::applyToThisNoMask<utils::MathOperations::Assign>(buffer,
        other, buffer->channels, buffer->size);

    return buffer;
  }

  Framework::Parameter *
  Processor::getParameter(uuid parameterId) const
  {
    COMPLEX_HARD_ASSERT(parameters, "No parameters contained in processor %v (%zu) to find",
      metadata->name, metadata->id);

    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      if (parameter->details.id == parameterId)
        return parameter;

    COMPLEX_HARD_ASSERT("Parameter (%zu) was not found", parameterId);
    return nullptr;
  }

  void Processor::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters)
  {
    if (flag == UpdateFlag::NoUpdates)
      return;

    auto parameter = parameters;
    for (usize i = 0; i < parameterCount; (i++), (parameter = parameter->next))
      if (parameter->details.updateFlag == flag)
        parameter->updateValue(sampleRate);

    if (updateChildrenParameters)
      for (auto child = children; child; child = child->next)
        child->updateParameters(flag, sampleRate);
  }

  void Processor::remapParameters(utils::span<Framework::HostParameter *> bridges,
    bool bridgeValueFromParameters, bool remapOnlyBridges)
  {
    if (!bridges.size())
    {
      auto *parameter = parameters;
      for (usize i = 0; i < parameterCount; (++i), (parameter = parameter->next))
      {
        auto bridge = parameter->hostParameter;
        if (!bridge)
          continue;

        if (remapOnlyBridges)
        {
          if (bridge->getParameter())
            bridge->resetParameter(nullptr);
          else
            bridge->resetParameter(parameter, bridgeValueFromParameters);
        }
        else
        {
          bridge->resetParameter(nullptr);
          parameter->hostParameter = nullptr;
        }
      }
    }
    else
    {
      COMPLEX_ASSERT(bridges.size() == parameterCount);

      auto *parameter = parameters;
      for (usize i = 0; i < parameterCount; (++i), (parameter = parameter->next))
      {
        auto *newBridge = bridges[i];
        if (auto *oldBridge = parameter->hostParameter = newBridge)
          oldBridge->resetParameter(nullptr);

        newBridge->resetParameter(parameter, bridgeValueFromParameters);
      }
    }

    Framework::HostParameter::notifyParameterChange();
  }
}

namespace Framework
{
  AddProcessorUpdate::AddProcessorUpdate(Generation::Processor *processorToAdd,
    u64 destinationParentStateId, usize destinationIndex) : state{ *processorToAdd->state }, processor{ processorToAdd },
    destinationParentStateId{ destinationParentStateId }, destinationIndex{ destinationIndex }
  {
    destructor = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      if (self->processor)
        self->state.deleteProcessor(self->processor);
    };

    redo = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);

      if (self->processor->parent != destinationPointer)
      {
        if (destinationPointer->component)
        {
          if (!self->processor->component)
            self->processor->component = self->processor->createUI();

          Interface::CommandMessages::ProcessorInsertion info{ .processor = self->processor,
            .index = (u32)self->destinationIndex, .useIndex = true };
          Interface::CommandMessages::tryProcessorInsertion(destinationPointer->component, info);
        }
        else
        {
          auto g = self->state.plugin->acquireProcessingLock();
          destinationPointer->addChildProcessor(*self->processor, self->destinationIndex);
        }
      }

      self->state.registerProcessorForDynamicParameters(self->processor);

      self->processor = nullptr;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);

      {
        auto g = self->state.plugin->acquireProcessingLock();
        self->processor = &destinationPointer->removeChildProcessor(self->destinationIndex);
      }

      if (self->processor->component)
        self->processor->component->parent->removeChildComponent(self->processor->component);

      self->state.deregisterProcessorForDynamicParameters(self->processor);
    };
  }

  MoveProcessorUpdate::MoveProcessorUpdate(Plugin::State *state,
    u64 sourceParentStateId, usize sourceIndex,
    u64 destinationParentStateId, usize destinationIndex, bool isDone) : state{ *state },
    destinationParentStateId{ destinationParentStateId }, destinationIndex{ destinationIndex },
    sourceParentStateId{ sourceParentStateId }, sourceIndex{ sourceIndex }, isDone{ isDone }
  {
    static constexpr auto insertComponent = [](Generation::Processor *movedProcessor,
      Generation::Processor *destinationProcessor, u32 index)
    {
      if (movedProcessor->component && movedProcessor->component->parent)
        movedProcessor->component->parent->removeChildComponent(movedProcessor->component);

      if (destinationProcessor->component)
      {
        if (!movedProcessor->component)
          movedProcessor->component = movedProcessor->createUI();

        Interface::CommandMessages::ProcessorInsertion info{ .processor = movedProcessor,
          .index = index, .useIndex = true };
        Interface::CommandMessages::tryProcessorInsertion(destinationProcessor->component, info);
      }
      else
      {
        auto g = movedProcessor->state->plugin->acquireProcessingLock();
        (void)movedProcessor->parent->removeChildProcessor(*movedProcessor);
        destinationProcessor->addChildProcessor(*movedProcessor, index);
      }
    };

    redo = [](UndoAction *a)
    {
      auto *self = (MoveProcessorUpdate *)a;

      if (self->isDone)
        return;
      self->isDone = !self->isDone;

      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);
      auto sourcePointer = self->state.getProcessor(self->sourceParentStateId);

      Generation::Processor *movedProcessor = Generation::Processor::getChild(
        sourcePointer->children, self->sourceIndex);
      insertComponent(movedProcessor, destinationPointer, (u32)self->destinationIndex);
    };

    undo = [](UndoAction *a)
    {
      auto *self = (MoveProcessorUpdate *)a;

      if (!self->isDone)
        return;
      self->isDone = !self->isDone;

      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);
      auto sourcePointer = self->state.getProcessor(self->sourceParentStateId);

      Generation::Processor *movedProcessor = Generation::Processor::getChild(
        destinationPointer->children, self->destinationIndex);
      insertComponent(movedProcessor, sourcePointer, (u32)self->sourceIndex);
    };
  }

  DeleteProcessorUpdate::DeleteProcessorUpdate(Generation::Processor *processorToDelete) :
    state{ *processorToDelete->state }
  {
    parentStateId = processorToDelete->parent->stateId;
    index = processorToDelete->getIndex();

    destructor = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      if (self->processor)
        self->state.deleteProcessor(self->processor);
    };

    redo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto parentPointer = self->state.getProcessor(self->parentStateId);

      auto g = self->state.plugin->acquireProcessingLock();
      if (!self->processor)
        self->processor = &parentPointer->removeChildProcessor(self->index);

      auto recurseParameters = [](const auto &self, Generation::Processor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->processor);

      self->state.deregisterProcessorForDynamicParameters(self->processor);

      self->processor->component->parent->removeChildComponent(self->processor->component);
    };

    undo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->parentStateId);
      auto g = self->state.plugin->acquireProcessingLock();

      Interface::CommandMessages::ProcessorInsertion info{ .processor = self->processor,
        .index = (u32)self->index, .useIndex = true };
      Interface::CommandMessages::tryProcessorInsertion(destinationPointer->component, info);

      auto recurseParameters = [](const auto &self, Generation::Processor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->processor);

      self->state.registerProcessorForDynamicParameters(self->processor);

      self->processor = nullptr;
    };
  }
}
