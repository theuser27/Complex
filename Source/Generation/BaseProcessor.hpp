/*
  ==============================================================================

    BaseProcessor.hpp
    Created: 11 Jul 2022 3:35:27am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <optional>
#include <span>

#include "Interface/LookAndFeel/Miscellaneous.hpp"
#include "Framework/vector_map.hpp"
#include "Framework/simd_buffer.hpp"

namespace Framework
{
  class ParameterBridge;
  class ParameterValue;
}

namespace Plugin
{
  class ProcessorTree;
}

namespace Interface
{
  class ProcessorSection;
}

namespace Generation
{
  class BaseProcessor
  {
  public:
    BaseProcessor() = delete;

    BaseProcessor(Plugin::ProcessorTree *processorTree, std::string_view processorType) noexcept;
    BaseProcessor(const BaseProcessor &other) noexcept;
    BaseProcessor &operator=(const BaseProcessor &other) noexcept;
    BaseProcessor(BaseProcessor &&other) noexcept;
    BaseProcessor &operator=(BaseProcessor &&other) noexcept;

    virtual ~BaseProcessor() noexcept;

    virtual void initialise() noexcept;

    virtual void serialiseToJson(void *jsonData) const;
    static void deserialiseFromJson(std::span<const std::string_view> parameterIds, 
      BaseProcessor *processor, void *jsonData);
    virtual BaseProcessor *createCopy() const = 0;
    void clearSubProcessors() noexcept { subProcessors_.clear(); }

    BaseProcessor *getSubProcessor(usize index) const noexcept { return subProcessors_[index]; }
    auto getSubProcessorCount() const noexcept { return subProcessors_.size(); }
    usize getSubProcessorIndex(const BaseProcessor *subProcessor) noexcept
    { return std::ranges::find(subProcessors_, subProcessor) - subProcessors_.begin(); }

    // the following functions are to be called outside of processing time
    virtual void insertSubProcessor([[maybe_unused]] usize index,
      [[maybe_unused]] BaseProcessor &newSubProcessor) noexcept
    {
      COMPLEX_ASSERT_FALSE("insertSubProcessor is not implemented for this type");
      (void)std::fprintf(stderr, "insertSubProcessor is not implemented for %s", getProcessorType().data());
      std::abort();
    }
    virtual BaseProcessor &deleteSubProcessor([[maybe_unused]] usize index) noexcept
    {
      COMPLEX_ASSERT_FALSE("deleteSubProcessor is not implemented for this type");
      (void)std::fprintf(stderr, "deleteSubProcessor is not implemented for %s", getProcessorType().data());
      std::terminate();
    }
    virtual BaseProcessor &updateSubProcessor([[maybe_unused]] usize index,
      [[maybe_unused]] BaseProcessor &newSubProcessor) noexcept
    {
      COMPLEX_ASSERT_FALSE("updateSubProcessor is not implemented for this type");
      (void)std::fprintf(stderr, "updateSubProcessor is not implemented for %s", getProcessorType().data());
      std::terminate();
    }
    
    Framework::ParameterValue *getParameter(std::string_view parameterId) const noexcept;
    Framework::ParameterValue *getParameterUnchecked(usize index) const noexcept;
    usize getParameterCount() const noexcept;
    usize getParameterIndex(const Framework::ParameterValue *parameter) noexcept
    {
      return std::ranges::find_if(processorParameters_.data, [&](const auto &element)
        { return element.second.get() == parameter; }) - processorParameters_.data.begin();
    }

    virtual void updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters = true) noexcept;
    // remaps parameters from the current bridges (if they exist) to new ones (if they exist)
    // if no bridges are provided they are assumed to be nullptr
    // if remapOnlyBridges is also provided it will unmap/remap only the bridges from the parameters 
    // while the parameters still keep a reference to them
    void remapParameters(std::optional<std::span<Framework::ParameterBridge *>> bridges, 
      bool bridgeValueFromParameters, bool remapOnlyBridges = false) noexcept;

    //void randomiseParameters();
    //void setAllParametersRandomisation(bool toRandomise = true);
    //void setParameterRandomisation(std::string_view name, bool toRandomise = true);

    std::string_view getProcessorType() const noexcept { return processorType_; }
    u64 getProcessorId() const noexcept { return processorId_; }
    Plugin::ProcessorTree *getProcessorTree() const noexcept { return processorTree_; }
    auto getDataBuffer() const noexcept { return Framework::SimdBufferView{ dataBuffer_ }; }
    
    u64 getParentProcessorId() const noexcept { return parentProcessorId_; }
    void setParentProcessorId(u64 newParentModuleId) noexcept { parentProcessorId_ = newParentModuleId; }


    void addListener(BaseProcessorListener *listener) { listeners_.push_back(listener); }

    utils::up<Interface::ProcessorSection> &getSavedSection() noexcept;
    void setSavedSection(utils::up<Interface::ProcessorSection> savedSection) noexcept;

    virtual void initialiseParameters() { }

  protected:
    void createProcessorParameters(std::span<const std::string_view> parameterIds);

    // data contextual to every individual module
    Framework::SimdBuffer<Framework::complex<float>, simd_float> dataBuffer_{};
    std::vector<BaseProcessor *> subProcessors_{};
    Framework::VectorMap<std::string_view, utils::up<Framework::ParameterValue>> processorParameters_;

    // data contextual to the base BaseProcessor
    Plugin::ProcessorTree *const processorTree_;
    const std::string_view processorType_;
    utils::DeferredConstant<u64> processorId_;
    u64 parentProcessorId_ = 0;

    std::vector<BaseProcessorListener *> listeners_{};
    utils::up<Interface::ProcessorSection> savedSection_;

    friend class Plugin::ProcessorTree;
  };
}
