
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
  class ProcessorSection;
}

namespace Generation
{
  COMPLEX_ENUM(Processors,
    ( SoundEngine, 1757890316114499300),
    (EffectsState, 1758070326562653000),
    ( EffectsLane, 1758070341249565300),
    (EffectModule, 1758070362397345100),
  )

  class BaseProcessor
  {
  public:
    BaseProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena);
    BaseProcessor(const BaseProcessor &other, utils::bumpArena *arena);
    BaseProcessor(BaseProcessor &&) = default;

    BaseProcessor() = delete;
    BaseProcessor(const BaseProcessor &) = delete;
    BaseProcessor &operator=(const BaseProcessor &) = delete;
    BaseProcessor &operator=(BaseProcessor &&) = delete;

    virtual void initialise();
    virtual void initialiseParameters() { }

    virtual void serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise = {}) const;
    virtual void deserialiseFromJson(void *jsonData);
    BaseProcessor *
    createCopy() const { return metadata->create(state, metadata, this); }

    // the following functions are to be called outside of processing time
    virtual bool insertSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true);
    virtual BaseProcessor &
    deleteSubProcessor(usize index, bool callListeners = true);
    virtual utils::pair<BaseProcessor &, bool>
    updateSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true);

    Framework::ParameterValue *
    getParameter(uuid parameterId) const;
    utils::dll<Framework::ParameterValue> *
    createParameters(usize count, Framework::ParameterMetadata *metadata,
      utils::dll<Framework::ParameterValue> *copy = nullptr);

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

    static BaseProcessor *
    getChild(BaseProcessor *children, usize index)
    {
      for (usize i = 0; i < index && children; (++i), (children = children->next)) { }
      return children;
    }
    static BaseProcessor *
    getChild(BaseProcessor *children, usize index, uuid id)
    {
      for (usize i = 0; children;)
      {
        if (id != children->metadata->id)
        {
          children = children->next;
          continue;
        }
        if (i >= index)
          break;
        ++i;
        children = children->next;
      }
      return children;
    }

    Framework::ProcessorMetadata *metadata{};
    Plugin::State *state;
    const u64 stateId = 0;

    BaseProcessor *parent = nullptr;
    BaseProcessor *previous = nullptr;
    BaseProcessor *next = nullptr;
    BaseProcessor *children = nullptr;
    u32 childrenCount{};

    u32 parameterCount{};
    // first node's previous points to the last, last node's next is always nullptr
    utils::dll<Framework::ParameterValue> *parameters{};
    Framework::SimdBuffer *dataBuffer = nullptr;

    utils::bumpArena *arena = nullptr;

  protected:
    void insertProcessorAt(BaseProcessor &processor, usize index);
    void reportUnexpectedProcessorInsert(BaseProcessor &attempedInsert,
      utils::span<const uuid> acceptedProcessorIds);
  };

  static_assert(utils::is_trivially_destructible_v<BaseProcessor>);

  void deserialiseParametersFromJson(void *jsonData, Framework::ProcessorMetadata *metadata,
    utils::dll<Framework::ParameterValue> *&parameters, BaseProcessor *processor, bool validateParameters);
}
