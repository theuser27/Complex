
// Created: 2022-07-11 03:35:27

#include "BaseProcessor.hpp"

#include "Framework/parameter_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"

namespace Generation
{
  BaseProcessor::BaseProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept :
    metadata{ metadata }, state{ state }, stateId{ state->stateIdCounter++ }, arena{ arena } { }

  BaseProcessor::BaseProcessor(const BaseProcessor &other, utils::bumpArena *arena) noexcept :
    metadata{ other.metadata }, state{ other.state }, stateId{ state->stateIdCounter++ }, arena{ arena }
  {
    using T = utils::remove_reference_t<decltype(*parameters)>;

    if (other.parameters)
    {
      parameterCount = other.parameterCount;

      auto *memory = arranew(arena, T, parameterCount);
      parameters = memory;

      auto *parameter = parameters;
      auto *otherParameter = other.parameters;
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

    if (other.children)
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

    if (other.dataBuffer)
    {
      dataBuffer = Framework::SimdBuffer::create(arena, other.dataBuffer->channels, state->getMaxBinCount());
      Framework::applyToThisNoMask<utils::MathOperations::Assign>(dataBuffer,
        other.dataBuffer, other.dataBuffer->channels, other.dataBuffer->size);
    }
  }

  void BaseProcessor::initialise() noexcept
  {
    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      parameter->object.initialise();
  }

  // the following functions are to be called outside of processing time
  bool BaseProcessor::insertSubProcessor([[maybe_unused]] usize index,
    [[maybe_unused]] BaseProcessor &newSubProcessor, [[maybe_unused]] bool callListeners)
  {
    COMPLEX_HARD_ASSERT_FALSE("insertSubProcessor is not implemented for %zu", metadata->id);
  }

  BaseProcessor &
  BaseProcessor::deleteSubProcessor([[maybe_unused]] usize index, [[maybe_unused]] bool callListeners)
  {
    COMPLEX_HARD_ASSERT_FALSE("deleteSubProcessor is not implemented for %zu", metadata->id);
  }

  utils::pair<BaseProcessor &, bool>
  BaseProcessor::updateSubProcessor([[maybe_unused]] usize index,
    [[maybe_unused]] BaseProcessor &newSubProcessor, [[maybe_unused]] bool callListeners)
  {
    COMPLEX_HARD_ASSERT_FALSE("updateSubProcessor is not implemented for %zu", metadata->id);
  }

  Framework::ParameterValue *
  BaseProcessor::getParameter(uuid parameterId) const noexcept
  {
    COMPLEX_HARD_ASSERT(parameters, "No parameters contained in processor %v (%zu) to find",
      metadata->name, metadata->id);

    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      if (parameter->object.getParameterId() == parameterId)
        return &parameter->object;

    COMPLEX_HARD_ASSERT("Parameter (%zu) was not found", parameterId);
    return nullptr;
  }

  void BaseProcessor::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters) noexcept
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
    bool bridgeValueFromParameters, bool remapOnlyBridges) noexcept
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

  void BaseProcessor::insertProcessorAt(BaseProcessor &processor, usize index)
  {
    COMPLEX_ASSERT(index <= childrenCount,
      "You're trying to insert at %zu, when size is %zu", index, childrenCount);

    ++childrenCount;
    processor.parent = this;

    if (!children)
    {
      children = &processor;
      return;
    }

    auto childAfter = children;
    for (usize i = 0; i < index; (++i), (childAfter = childAfter->next)) { }

    processor.previous = childAfter->previous;
    processor.next = childAfter;

    if (processor.previous)
      processor.previous->next = &processor;
    if (processor.next)
      processor.next->previous = &processor;
  }

  void BaseProcessor::reportUnexpectedProcessorInsert(BaseProcessor &attempedInsert,
    utils::span<const uuid> acceptedProcessorIds)
  {
    auto string = utils::string::create("Attempted insert of %v(id: %zu) inside an %v(id: %zu). \
      If this shows to you as a user, report it to the dev. \n\nSupported subprocessors by this type are: ",
      attempedInsert.metadata->name, attempedInsert.metadata->id, metadata->name, metadata->id
    );

    for (auto id : acceptedProcessorIds)
    {
      auto *acceptedMetadata = state->findProcessorMetadata(id);
      string.appendFormat("%v(id: %zu), ", acceptedMetadata->name, acceptedMetadata->id);
    }

    Interface::showNativeMessageBox("Erroneous processor insert", string.data(), Interface::MessageBoxType::Warning);
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
