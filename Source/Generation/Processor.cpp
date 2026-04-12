
// Created: 2022-07-11 03:35:27

#include "Processor.hpp"

#include "Framework/parameter_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/LookAndFeel/Component.hpp"

namespace Generation
{
  Processor::Processor(utils::bumpArena *arena, Plugin::State *state, 
    Framework::ProcessorMetadata *metadata, const Processor *other) : metadata{ metadata },
    state{ state }, stateId{ ++state->stateIdCounter }, arena{ arena }, name{ arena }
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

  void Processor::reset()
  {
    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      parameter->object.reset();
  }

  // the following functions are to be called outside of processing time
  bool 
  Processor::addChildProcessor(Processor &newChildProcessor, Processor *insertBefore)
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

  void Processor::removeChildProcessor(Processor &removedChildProcessor)
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
  Processor::getParameter(uuid parameterId) const
  {
    COMPLEX_HARD_ASSERT(parameters, "No parameters contained in processor %v (%zu) to find",
      metadata->name, metadata->id);

    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      if (parameter->object.getParameterId() == parameterId)
        return &parameter->object;

    COMPLEX_HARD_ASSERT("Parameter (%zu) was not found", parameterId);
    return nullptr;
  }

  void Processor::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters)
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

  void Processor::remapParameters(utils::span<Framework::ParameterBridge *> bridges,
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
  Processor::createParameters(usize count, Framework::ParameterMetadata *pararmeterMetadata,
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

        // register dynamic parameters 
        state->registerDynamicParameter(&slot->object);
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

        // register dynamic parameters 
        state->registerDynamicParameter(&slot->object);
      }
    }

    memory[0].previous = &memory[count - 1];
    memory[count - 1].next = nullptr;

    return memory;
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
    for (auto child = processorToDelete->parent->children; child != processorToDelete; child = child->next)
      ++index;

    // has it already been deleted? 
    // if so, we need to skip removal in redo
    if (!processorToDelete->parent)
      processor = processorToDelete;

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
      self->processor->component->componentFlags.isVisible = false;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (DeleteProcessorUpdate *)a;
      auto destinationPointer = self->state.getProcessor(self->parentStateId);
      self->processor->component->componentFlags.isVisible = true;

      auto g = self->state.plugin->acquireProcessingLock();
      destinationPointer->addChildProcessor(*self->processor, self->index);
      auto recurseParameters = [](const auto &self, Generation::Processor *processor) -> void
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
