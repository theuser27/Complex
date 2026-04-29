
// Created: 2022-07-11 03:35:27

#pragma once

#include "Framework/simd_buffer.hpp"
#include "Framework/parameter_types.hpp"

namespace Framework
{
  class ParameterBridge;
  class ParameterValue;
  struct ParameterMetadata;
  struct ProcessorMetadata;
}

namespace Plugin
{
  struct State;
}

namespace Interface
{
  class Component;
}

namespace Generation
{
  COMPLEX_ENUM(Processors,
    ( SoundEngine, 1757890316114),
    ( EffectsLane, 1758070341249),
    (EffectModule, 1758070362397),
  )

  class Processor
  {
  public:
    Processor(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, const Processor *other);
    Processor(Processor &&) = default;

    Processor() = delete;
    Processor(const Processor &) = delete;
    Processor &operator=(const Processor &) = delete;
    Processor &operator=(Processor &&) = delete;

    virtual Interface::Component *createUI() = 0;
    virtual void reset();

    virtual void serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise = {}) const;
    void deserialiseFromJson(void *jsonData);
    Processor *createCopy() const { return metadata->create(state, metadata, this, nullptr); }

    // the following functions are to be called outside of processing time
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

    Framework::ParameterValue *getParameter(uuid parameterId) const;
    Framework::ParameterValue *createParameters(usize count,
      Framework::ParameterMetadata *metadata, Framework::ParameterValue *copy = nullptr);

    void updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters = true);
    // remaps parameters from the current bridges (if they exist) to new ones (if they exist)
    // if no bridges are provided they are assumed to be nullptr
    // if remapOnlyBridges is also provided it will unmap/remap only the bridges from the parameters
    // while the parameters still keep a reference to them
    void remapParameters(utils::span<Framework::ParameterBridge *> bridges,
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

    Framework::ProcessorMetadata *metadata{};
    Plugin::State *state;
    const u64 stateId = 0;

    Processor *parent = nullptr;
    Processor *previous = nullptr;
    Processor *next = nullptr;
    Processor *children = nullptr;
    u32 childrenCount{};

    u32 parameterCount{};
    // first node's previous points to the last, last node's next is always nullptr
    Framework::ParameterValue *parameters{};
    Framework::SimdBuffer *dataBuffer = nullptr;

    utils::bumpArena *arena = nullptr;
    Interface::Component *component = nullptr;

    utils::stringnd name{};
  };

  static_assert(utils::is_trivially_destructible_v<Processor>);

  void deserialiseParametersFromJson(void *jsonData, Framework::ProcessorMetadata *metadata,
    Framework::ParameterValue *&parameters, Processor *processor, bool validateParameters);
}

namespace Framework
{
  struct AddProcessorUpdate : public Framework::UndoAction
  {
    AddProcessorUpdate(Generation::Processor *processorToAdd,
      u64 destinationParentStateId, usize destinationIndex);

    Plugin::State &state;
    Generation::Processor *processor;
    u64 destinationParentStateId{};
    usize destinationIndex{};
  };

  struct MoveProcessorUpdate final : public Framework::UndoAction
  {
    MoveProcessorUpdate(Plugin::State *state,
      u64 sourceParentStateId, usize sourceIndex,
      u64 destinationParentStateId, usize destinationIndex, bool isDone = false);

    Plugin::State &state;
    u64 destinationParentStateId{};
    usize destinationIndex{};
    u64 sourceParentStateId{};
    usize sourceIndex{};
    bool isDone{};
  };

  struct DeleteProcessorUpdate final : public UndoAction
  {
    DeleteProcessorUpdate(Generation::Processor *processorToDelete);

    Plugin::State &state;
    Generation::Processor *processor{};
    u64 parentStateId{};
    usize index{};
  };
}
