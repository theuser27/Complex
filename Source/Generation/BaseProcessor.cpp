
// Created: 2022-07-11 03:35:27

#include "BaseProcessor.hpp"

#include "Framework/parameter_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/LookAndFeel/BaseComponent.hpp"

namespace Generation
{
  BaseProcessor::BaseProcessor(utils::bumpArena *arena, Plugin::State *state, 
    Framework::ProcessorMetadata *metadata, const BaseProcessor *other) : metadata{ metadata },
    state{ state }, stateId{ state->stateIdCounter++ }, arena{ arena } 
  {
    if (!other)
      return;

    using T = utils::remove_reference_t<decltype(*parameters)>;

    if (other->parameters)
    {
      parameterCount = other->parameterCount;

      auto *memory = arranew(arena, T, parameterCount);
      parameters = memory;

      auto *parameter = parameters;
      auto *otherParameter = other->parameters;
      for (usize i = 0; i < parameterCount; ++i)
      {
        parameter = new (memory) T{ otherParameter->object };
        parameter->object.parentProcessor = this;
        parameter->previous = memory - 1;
        parameter->next = memory + 1;

        ++otherParameter;
        ++memory;
      }

      parameters->previous = parameter;
      parameter->next = nullptr;
    }

    if (other->children)
    {
      childrenCount = other->childrenCount;

      children = other->children->createCopy();
      children->parent = this;
      for (auto otherChild = other->children->next, child = children; otherChild;
        (otherChild = otherChild->next), (child = child->next))
      {
        child->next = otherChild->createCopy();
        child->next->parent = this;
        child->next->previous = child;
      }
    }

    if (other->dataBuffer)
    {
      dataBuffer = Framework::SimdBuffer::create(arena, other->dataBuffer->channels, state->getMaxBinCount());
      Framework::applyToThisNoMask<utils::MathOperations::Assign>(dataBuffer,
        other->dataBuffer, other->dataBuffer->channels, other->dataBuffer->size);
    }
  }

  void BaseProcessor::reset()
  {
    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      parameter->object.reset();
  }

  // the following functions are to be called outside of processing time
  bool 
  BaseProcessor::addChildProcessor(BaseProcessor &newChildProcessor, BaseProcessor *insertBefore)
  {
    COMPLEX_ASSERT(!newChildProcessor.parent);

    if (!metadata->acceptsChild(newChildProcessor.metadata))
    {
      auto string = utils::string::create(localScratch,
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
    newChildProcessor.previous = &newChildProcessor;
    newChildProcessor.next = nullptr;

    if (!children)
    {
      children = &newChildProcessor;
      return true;
    }

    newChildProcessor.next = insertBefore;

    insertBefore = (insertBefore) ? insertBefore : children;
    newChildProcessor.previous = insertBefore->previous;
    insertBefore->previous = &newChildProcessor;

    if (newChildProcessor.previous->next)
      newChildProcessor.previous->next = &newChildProcessor;

    return true;
  }

  void BaseProcessor::removeChildProcessor(BaseProcessor &removedChildProcessor)
  {
    COMPLEX_ASSERT(removedChildProcessor.parent == this);

    if (removedChildProcessor.previous->next)
      removedChildProcessor.previous->next = removedChildProcessor.next;
    if (removedChildProcessor.next)
      removedChildProcessor.next->previous = removedChildProcessor.previous;

    removedChildProcessor.parent = nullptr;
    removedChildProcessor.previous = nullptr;
    removedChildProcessor.next = nullptr;

    if (children == &removedChildProcessor)
      children = removedChildProcessor.next;

    --childrenCount;
  }

  Framework::ParameterValue *
  BaseProcessor::getParameter(uuid parameterId) const
  {
    COMPLEX_HARD_ASSERT(parameters, "No parameters contained in processor %v (%zu) to find",
      metadata->name, metadata->id);

    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      if (parameter->object.getParameterId() == parameterId)
        return &parameter->object;

    COMPLEX_HARD_ASSERT("Parameter (%zu) was not found", parameterId);
    return nullptr;
  }

  void BaseProcessor::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters)
  {
    if (flag == UpdateFlag::NoUpdates)
      return;

    auto parameter = parameters;
    for (usize i = 0; i < parameterCount; (i++), (parameter = parameter->next))
      if (parameter->object.getUpdateFlag() == flag)
        parameter->object.updateValue(sampleRate);

    if (updateChildrenParameters)
      for (auto child = children; child; child = child->next)
        child->updateParameters(flag, sampleRate);
  }

  void BaseProcessor::remapParameters(utils::span<Framework::ParameterBridge *> bridges,
    bool bridgeValueFromParameters, bool remapOnlyBridges)
  {
    if (!bridges.size())
    {
      auto *parameter = parameters;
      for (usize i = 0; i < parameterCount; (++i), (parameter = parameter->next))
      {
        auto bridge = parameter->object.getParameterLink()->hostControl;
        if (!bridge)
          continue;

        if (remapOnlyBridges)
        {
          if (bridge->getParameterLink())
            bridge->resetParameterLink(nullptr);
          else
            bridge->resetParameterLink(parameter->object.getParameterLink(), bridgeValueFromParameters);
        }
        else
        {
          bridge->resetParameterLink(nullptr);
          parameter->object.changeBridge(nullptr);
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
        if (auto *oldBridge = parameter->object.changeBridge(newBridge))
          oldBridge->resetParameterLink(nullptr);

        newBridge->resetParameterLink(parameter->object.getParameterLink(), bridgeValueFromParameters);
      }
    }

    Framework::ParameterBridge::notifyParameterChange();
  }

  utils::dll<Framework::ParameterValue> *
  BaseProcessor::createParameters(usize count, Framework::ParameterMetadata *pararmeterMetadata,
    utils::dll<Framework::ParameterValue> *copy)
  {
    using T = utils::remove_reference_t<decltype(*parameters)>;

    if (!count)
      return nullptr;

    auto *memory = arranew(arena, T, count);

    if (!copy)
    {
      for (usize i = 0; i < count; (++i), (pararmeterMetadata = pararmeterMetadata->next))
      {
        T *slot;
        if (pararmeterMetadata->details.scale == Framework::ParameterScale::Indexed)
        {
          auto details = pararmeterMetadata->details;
          auto *optionStub = Framework::IndexedData::deepCopy(arena, details.options);
          details.options = optionStub;
          slot = new (memory + i) T{ pararmeterMetadata->details };
        }
        else
          slot = new (memory + i) T{ pararmeterMetadata->details };

        slot->object.parentProcessor = this;
        slot->previous = slot - 1;
        slot->next = slot + 1;
      }
    }
    else
    {
      for (usize i = 0; i < count; (++i), (copy = copy->next))
      {
        auto *slot = new (memory + i) T{ copy->object };
        if (auto details = slot->object.getParameterDetails(); details.scale == Framework::ParameterScale::Indexed)
        {
          auto *optionStub = Framework::IndexedData::deepCopy(arena, details.options);
          details.options = optionStub;
          slot->object.setParameterDetails(details);
        }
        slot->object.parentProcessor = this;
        slot->previous = slot - 1;
        slot->next = slot + 1;
      }
    }

    memory[0].previous = &memory[count - 1];
    memory[count - 1].next = nullptr;

    return memory;
  }
}

namespace Framework
{
  AddProcessorUpdate::AddProcessorUpdate(Generation::BaseProcessor *processorToAdd,
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

      auto g = self->state.plugin->acquireProcessingLock();
      destinationPointer->addChildProcessor(*self->processor, self->destinationIndex);
      if (destinationPointer->component)
      {
        self->processor->component = self->processor->createUI();
        destinationPointer->component->addChildComponent(self->processor->component, self->destinationIndex);
      }

      self->processor = nullptr;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (AddProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);

      auto g = self->state.plugin->acquireProcessingLock();
      self->processor = &destinationPointer->removeChildProcessor(self->destinationIndex);
    };
  }

  MoveProcessorUpdate::MoveProcessorUpdate(Generation::BaseProcessor *processorToMove,
    u64 destinationParentStateId, usize destinationIndex) : state{ *processorToMove->state },
    destinationParentStateId{ destinationParentStateId }, destinationIndex{ destinationIndex }
  {
    sourceParentStateId = processorToMove->parent->stateId;
    for (auto child = processorToMove->parent->children; child != processorToMove; child = child->next)
      ++sourceIndex;

    redo = [](UndoAction *a)
    {
      auto *self = (MoveProcessorUpdate *)a;

      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);
      auto sourcePointer = self->state.getProcessor(self->sourceParentStateId);

      auto g = self->state.plugin->acquireProcessingLock();
      auto &movedProcessor = sourcePointer->removeChildProcessor(self->sourceIndex);
      destinationPointer->addChildProcessor(movedProcessor, self->destinationIndex);

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

      auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);
      auto sourcePointer = self->state.getProcessor(self->sourceParentStateId);

      auto g = self->state.plugin->acquireProcessingLock();
      auto &movedProcessor = destinationPointer->removeChildProcessor(self->destinationIndex);
      sourcePointer->addChildProcessor(movedProcessor, self->sourceIndex);

      //if (sourcePointer != destinationPointer)
      //  for (auto listener : sourcePointer->listeners_)
      //    listener->movedSubProcessor(movedProcessor, *destinationPointer, 
      //      destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

      //for (auto listener : destinationPointer->listeners_)
      //  listener->movedSubProcessor(movedProcessor, *destinationPointer, 
      //    destinationSubProcessorIndex_, *sourcePointer, sourceSubProcessorIndex_);

    };
  }

  DeleteProcessorUpdate::DeleteProcessorUpdate(Generation::BaseProcessor *processorToDelete) : 
    state{ *processorToDelete->state }
  {
    parentStateId = processorToDelete->parent->stateId;
    for (auto child = processorToDelete->parent->children; child != processorToDelete; child = child->next)
      ++index;

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
      self->processor = &parentPointer->removeChildProcessor(self->index);
      auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->processor);
      self->processor->component->componentFlags.isVisible = false;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->parentStateId);
      self->processor->component->componentFlags.isVisible = true;

      auto g = self->state.plugin->acquireProcessingLock();
      destinationPointer->addChildProcessor(*self->processor, self->index);
      auto recurseParameters = [](const auto &self, Generation::BaseProcessor *processor) -> void
      {
        processor->remapParameters({}, true, true);
        auto child = processor->children;
        for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
          self(self, child);
      };
      recurseParameters(recurseParameters, self->processor);

      self->processor = nullptr;
    };
  }
}
