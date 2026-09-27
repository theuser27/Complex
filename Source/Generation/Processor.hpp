
// Created: 2022-07-11 03:35:27

#pragma once

#include "Framework/simd_buffer.hpp"
#include "Framework/metadata.hpp"

class HostParameter;
class Parameter;
struct ParameterMetadata;
struct ProcessorMetadata;

struct State;

class Component;

COMPLEX_ENUM(Processors,
  ( SoundEngine, 1757890316114),
  ( EffectsLane, 1758070341249),
  (EffectModule, 1758070362397),
)

class Processor
{
public:
  Processor(utils::Pool *arena, State *state, ProcessorMetadata *metadata);
  Processor(Processor &&) = default;

  Processor() = delete;
  Processor(const Processor &) = delete;
  Processor &operator=(const Processor &) = delete;
  Processor &operator=(Processor &&) = delete;

  Component *createUI() { return metadata->createUI(this); }
  void reset() { metadata->reset(this); }
  void serialiseToJson(void *jsonData, utils::span<Parameter *> parametersToSerialise = {}) const
  {
    metadata->serialise(this, jsonData, parametersToSerialise);
  }
  void deserialiseFromJson(void *jsonData);
  Processor *createCopy() const { return metadata->create(state, metadata, this, nullptr, nullptr); }

  // the following functions are to be called outside of processing time
  void createChildren(const Processor &other);
  bool addChildProcessor(Processor &newChildProcessor, Processor *insertBefore = nullptr);
  void removeChildProcessor(Processor &removedChildProcessor);
  bool
  addChildProcessor(Processor &newChildProcessor, usize index)
  {
    COMPLEX_HARD_ASSERT(index <= childrenCount);
    return addChildProcessor(newChildProcessor, getChild(children, index));
  }
  Processor &
  removeChildProcessor(usize index)
  {
    auto *child = getChild(children, utils::min((usize)childrenCount, index));
    removeChildProcessor(*child);
    return *child;
  }

  SimdBuffer *createBuffer(const SimdBuffer *other = nullptr);

  Parameter *getParameter(uuid parameterId) const;
  void updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters = true);
  // remaps parameters from the current bridges (if they exist) to new ones (if they exist)
  // if no bridges are provided they are assumed to be nullptr
  // if remapOnlyBridges is also provided it will unmap/remap only the bridges from the parameters
  // while the parameters still keep a reference to them
  void remapParameters(utils::span<HostParameter *> bridges,
    bool bridgeValueFromParameters, bool remapOnlyBridges = false);

  //void randomiseParameters();
  //void setAllParametersRandomisation(bool toRandomise = true);
  //void setParameterRandomisation(utils::string_view name, bool toRandomise = true);

  usize
  getIndex()
  {
    usize i = 0;
    for (auto *child = parent->children; child != this; child = child->next)
      ++i;
    return i;
  }
  static Processor *
  getChild(Processor *children, usize index)
  {
    for (; index && children; (--index), (children = children->next)) { }
    return children;
  }
  static Processor *
  getChild(Processor *children, usize index, uuid id)
  {
    for (usize i = 0; children;)
    {
      if (id != children->metadata->id)
      {
        children = children->next;
        continue;
      }
      if (i == index)
        break;
      ++i;
      children = children->next;
    }
    return children;
  }

  ProcessorMetadata *metadata{};
  State *state;
  const u64 stateId = 0;

  Processor *parent = nullptr;
  Processor *previous = nullptr;
  Processor *next = nullptr;
  Processor *children = nullptr;
  u32 childrenCount{};

  u32 parameterCount{};
  // half-connected dll: first node's previous points to the last, 
  //                     last node's next is always nullptr
  Parameter *parameters{};
  SimdBuffer *dataBuffer = nullptr;

  utils::Pool *arena = nullptr;
  Component *component = nullptr;

  utils::stringnd name{};
};

static_assert(utils::is_trivially_destructible_v<Processor>);

void deserialiseParametersFromJson(void *jsonData, ProcessorMetadata *metadata,
  Parameter *&parameters, Processor *processor, bool validateParameters);

struct AddProcessorUpdate : public UndoAction
{
  AddProcessorUpdate(Processor *processorToAdd,
    u64 destinationParentStateId, usize destinationIndex);

  State &state;
  Processor *processor;
  u64 destinationParentStateId{};
  usize destinationIndex{};
};

struct MoveProcessorUpdate final : public UndoAction
{
  MoveProcessorUpdate(State *state,
    u64 sourceParentStateId, usize sourceIndex,
    u64 destinationParentStateId, usize destinationIndex, bool isDone = false);

  State &state;
  u64 destinationParentStateId{};
  usize destinationIndex{};
  u64 sourceParentStateId{};
  usize sourceIndex{};
  bool isDone{};
};

struct DeleteProcessorUpdate final : public UndoAction
{
  DeleteProcessorUpdate(Processor *processorToDelete);

  State &state;
  Processor *processor{};
  u64 parentStateId{};
  usize index{};
};
