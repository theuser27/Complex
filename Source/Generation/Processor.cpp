
// Created: 2022-07-11 03:35:27

#include "Processor.hpp"

#include "Framework/metadata.hpp"
#include "Framework/parameters.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/LookAndFeel/Component.hpp"
#include "Interface/Components/Control.hpp"

void deleteProcessor(Processor *processor)
{
  COMPLEX_ASSERT(processor->state != nullptr);
  COMPLEX_ASSERT(processor->stateId != 0);

  // if the GUI for this processor is fading out, delay deletion until it disappears
  if (processor->component->componentFlags.animateFadeAway && processor->component->fadeawayRatio != 0)
  {
    // when the timer on the fade out ends, only then delete
    getUiRelated()->renderer->callbacks.add(processor->component, [processor](Component *component)
      {
        if (component->fadeawayRatio)
          return false;

        auto g = processor->state->plugin->acquireProcessingLock();
        deleteProcessor(processor);
        return true;
      });
    return;
  }

  for (auto child = processor->children; child; child = child->next)
    deleteProcessor(child);

  processor->state->deregisterProcessorForDynamicParameters(processor);

  // unlink all parameters from parameter bridges
  processor->remapParameters({}, false, false);

  if (processor->component)
  {
    if (processor->component->parent)
      processor->component->parent->removeChildComponent(processor->component);
    deleteComponent(processor->component);
  }

  processor->state->allProcessors.data.erase({ processor->stateId, processor });
  utils::Pool::remove(processor->pool);
}

Processor::Processor(utils::Pool *pool, State *state, ProcessorMetadata *metadata) :
  metadata{ metadata }, state{ state }, stateId{ ++state->stateIdCounter }, pool{ pool }, name{ pool }
{
  COMPLEX_ASSERT(metadata);
  COMPLEX_ASSERT(state);
  COMPLEX_ASSERT(stateId > 0);
  COMPLEX_ASSERT(pool);
}

void resetProcessorGeneric(Processor *processor)
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

    showNativeMessageBox("Erroneous processor insert", string.data(), MessageBoxType::Warning);
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

SimdBuffer *
Processor::createBuffer(const SimdBuffer *other)
{
  auto channels = (other) ? other->channels : kChannelsPerInOut;
  auto *buffer = SimdBuffer::create(pool, channels, state->getMaxBinCount());

  if (other)
    applyToThisNoMask<utils::MathOperations::Assign>(buffer,
      other, buffer->channels, buffer->size);

  return buffer;
}

Parameter *
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


AddProcessorUpdate::AddProcessorUpdate(Processor *processorToAdd,
  u64 destinationParentStateId, usize destinationIndex) : state{ *processorToAdd->state }, processor{ processorToAdd },
  destinationParentStateId{ destinationParentStateId }, destinationIndex{ destinationIndex }
{
  destructor = [](UndoAction *a)
  {
    auto *self = (AddProcessorUpdate *)a;
    if (self->processor)
      deleteProcessor(self->processor);
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

        CommandMessages::ProcessorInsertion info{ .processor = self->processor,
          .index = (u32)self->destinationIndex, .useIndex = true };
        CommandMessages::tryProcessorInsertion(destinationPointer->component, info);
      }
      else
      {
        auto g = self->state.plugin->acquireProcessingLock();
        destinationPointer->addChildProcessor(*self->processor, self->destinationIndex);
      }
    }

    self->state.registerProcessorForDynamicParameters(self->processor);
    self->state.plugin->markPluginDirty();

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
    self->state.plugin->markPluginDirty();
  };
}

MoveProcessorUpdate::MoveProcessorUpdate(State *state,
  u64 sourceParentStateId, usize sourceIndex,
  u64 destinationParentStateId, usize destinationIndex, bool isDone) : state{ *state },
  destinationParentStateId{ destinationParentStateId }, destinationIndex{ destinationIndex },
  sourceParentStateId{ sourceParentStateId }, sourceIndex{ sourceIndex }, isDone{ isDone }
{
  static constexpr auto insertComponent = [](Processor *movedProcessor,
    Processor *destinationProcessor, u32 index)
  {
    if (movedProcessor->component && movedProcessor->component->parent)
      movedProcessor->component->parent->removeChildComponent(movedProcessor->component);

    if (destinationProcessor->component)
    {
      if (!movedProcessor->component)
        movedProcessor->component = movedProcessor->createUI();

      CommandMessages::ProcessorInsertion info{ .processor = movedProcessor,
        .index = index, .useIndex = true };
      CommandMessages::tryProcessorInsertion(destinationProcessor->component, info);
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

    Processor *movedProcessor = Processor::getChild(
      sourcePointer->children, self->sourceIndex);
    insertComponent(movedProcessor, destinationPointer, (u32)self->destinationIndex);

    self->state.plugin->markPluginDirty();
  };

  undo = [](UndoAction *a)
  {
    auto *self = (MoveProcessorUpdate *)a;

    if (!self->isDone)
      return;
    self->isDone = !self->isDone;

    auto destinationPointer = self->state.getProcessor(self->destinationParentStateId);
    auto sourcePointer = self->state.getProcessor(self->sourceParentStateId);

    Processor *movedProcessor = Processor::getChild(
      destinationPointer->children, self->destinationIndex);
    insertComponent(movedProcessor, sourcePointer, (u32)self->sourceIndex);

    self->state.plugin->markPluginDirty();
  };
}

DeleteProcessorUpdate::DeleteProcessorUpdate(Processor *processorToDelete) :
  state{ *processorToDelete->state }
{
  parentStateId = processorToDelete->parent->stateId;
  index = processorToDelete->getIndex();

  destructor = [](UndoAction *a)
  {
    auto *self = (DeleteProcessorUpdate *)a;
    if (self->processor)
      deleteProcessor(self->processor);
  };

  redo = [](UndoAction *a)
  {
    auto *self = (DeleteProcessorUpdate *)a;
    auto parentPointer = self->state.getProcessor(self->parentStateId);

    auto g = self->state.plugin->acquireProcessingLock();
    if (!self->processor)
      self->processor = &parentPointer->removeChildProcessor(self->index);

    auto recurseParameters = [](const auto &self, Processor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      auto child = processor->children;
      for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
        self(self, child);
    };
    recurseParameters(recurseParameters, self->processor);

    self->state.deregisterProcessorForDynamicParameters(self->processor);
    self->state.plugin->markPluginDirty();

    self->processor->component->parent->removeChildComponent(self->processor->component);
  };

  undo = [](UndoAction *a)
  {
    auto *self = (DeleteProcessorUpdate *)a;
    auto destinationPointer = self->state.getProcessor(self->parentStateId);
    auto g = self->state.plugin->acquireProcessingLock();

    CommandMessages::ProcessorInsertion info{ .processor = self->processor,
      .index = (u32)self->index, .useIndex = true };
    CommandMessages::tryProcessorInsertion(destinationPointer->component, info);

    auto recurseParameters = [](const auto &self, Processor *processor) -> void
    {
      processor->remapParameters({}, true, true);
      auto child = processor->children;
      for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
        self(self, child);
    };
    recurseParameters(recurseParameters, self->processor);

    self->state.registerProcessorForDynamicParameters(self->processor);
    self->state.plugin->markPluginDirty();

    self->processor = nullptr;
  };
}
