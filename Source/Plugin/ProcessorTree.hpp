/*
  ==============================================================================

    ProcessorTree.hpp
    Created: 31 May 2023 4:07:02am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <span>

#include "Framework/constants.hpp"
#include "Framework/sync_primitives.hpp"
#include "Framework/vector_map.hpp"
#include "Framework/utils.hpp"

namespace juce
{
  class UndoManager;
}

namespace Generation
{
  class BaseProcessor;
}

namespace Framework
{
  struct IndexedData;
  class ParameterValue;
  class ParameterModulator;
  class ParameterBridge;
  class WaitingUpdate;
  class PresetUpdate;
}

namespace Plugin
{
  // global state for keeping track of all processors
  class ProcessorTree
  {
  protected:
    static constexpr size_t expandAmount = 2;
    static constexpr float expandThreshold = 0.75f;

    ProcessorTree(u32 inSidechains, u32 outSidechains);
    virtual ~ProcessorTree();

  public:
    // 0 is reserved to mean "uninitialised" and
    // 1 is reserved for the processorTree itself
    static constexpr u64 processorTreeId = 1;

    ProcessorTree(const ProcessorTree &) = delete;
    ProcessorTree(ProcessorTree &&) = delete;
    ProcessorTree &operator=(const ProcessorTree &other) = delete;
    ProcessorTree &operator=(ProcessorTree &&other) = delete;

    void serialiseToJson(void *jsonData) const;
    virtual void deserialiseFromJson(void *jsonData) = 0;
    
    // do not call this function manually, it's always called when you instantiate a BaseProcessor derived type
    // may block and allocate if expandThreshold has been reached
    auto getId(Generation::BaseProcessor *newProcessor) noexcept -> u64;

    auto getProcessor(u64 processorId) const noexcept -> Generation::BaseProcessor *;
    // creates a default processor or loads processor from save if jsonData != nullptr
    auto createProcessor(std::string_view processorType, void *jsonData = nullptr)
      -> Generation::BaseProcessor *;
    auto copyProcessor(std::derived_from<Generation::BaseProcessor> auto *processor)
    {
      auto copyProcessor = [processor]() { return processor->createCopy(); };
      return executeOutsideProcessing(copyProcessor);
    }
    auto deleteProcessor(u64 processorId) noexcept -> utils::up<Generation::BaseProcessor>;

    auto getProcessorParameter(u64 parentProcessorId, std::string_view parameterName) const noexcept 
      -> Framework::ParameterValue *;
    // see IndexedData::dynamicUpdateUuid
    void registerDynamicParameter(Framework::ParameterValue *parameter);
    void updateDynamicParameters(std::string_view reason) const noexcept;

    auto getUpdateFlag() const noexcept -> UpdateFlag { return updateFlag_.load(std::memory_order_acquire); }
    // only the audio thread changes the updateFlag
    // so we need acq_rel in order for it to see any changes made by the GUI thread
    // because it's only done twice per run i opted for max security with seq_cst just in case
    void setUpdateFlag(UpdateFlag newFlag) noexcept { updateFlag_.store(newFlag, std::memory_order_seq_cst); }

    auto getSampleRate() const noexcept -> float { return sampleRate_.load(std::memory_order_acquire); }
    auto getSamplesPerBlock() const noexcept -> u32 { return samplesPerBlock_.load(std::memory_order_acquire); }
    auto getMinMaxFFTOrder() const noexcept -> std::pair<u32, u32>
    { return { minFFTOrder_.load(std::memory_order_acquire), maxFFTOrder_.load(std::memory_order_acquire) }; }
    auto getMaxBinCount() const noexcept -> u32 { return (1 << (maxFFTOrder_.load(std::memory_order_acquire) - 1)) + 1; }
    auto getInputSidechains() const noexcept -> u32 { return inSidechains_; }
    auto getOutputSidechains() const noexcept -> u32 { return outSidechains_; }
    virtual auto getLaneCount() const -> size_t = 0;

    auto getParameterBridges() noexcept { return std::span{ parameterBridges_ }; }
    auto &getParameterModulators() noexcept { return parameterModulators_; }

    void pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction = true);
    void undo();
    void redo();

    // quick and dirty spinlock to ensure lambdas are executed outside of an audio callback
    auto executeOutsideProcessing(const auto &function)
    {
      // check if we're in the middle of an audio callback
      while (updateFlag_.load(std::memory_order_relaxed) != UpdateFlag::AfterProcess) { utils::millisleep(); }

      utils::ScopedLock g{ processingLock_, utils::WaitMechanism::Spin };
      return function();
    }

    bool isBeingDestroyed() const noexcept { return isBeingDestroyed_.load(std::memory_order_acquire); }
    void clearState();

  protected:
    // all plugin undo steps are stored here
    utils::up<juce::UndoManager> undoManager_;
    // the processor tree is stored in a flattened map
    Framework::VectorMap<u64, utils::up<Generation::BaseProcessor>> allProcessors_;
    // outward facing parameters, which can be mapped to in-plugin parameters
    std::vector<Framework::ParameterBridge *> parameterBridges_{};
    // modulators inside the plugin
    std::vector<Framework::ParameterModulator *> parameterModulators_{};
    // parameters that receive updates upon various plugin changes
    std::vector<std::pair<Framework::IndexedData *, Framework::ParameterValue *>> dynamicParameters_{};
    // used to give out non-repeating ids for all processors
    std::atomic<u64> processorIdCounter_ = processorTreeId + 1;
    // used for checking whether it's ok to update parameters/plugin structure
    std::atomic<UpdateFlag> updateFlag_ = UpdateFlag::AfterProcess;
    // if any updates are supposed to happen to the processing tree/undoManager
    // the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
    mutable utils::ReentrantLock<bool> processingLock_{ false, {} };
    std::atomic<bool> isBeingDestroyed_ = false;

    // might be updated on any thread hence atomic
    std::atomic<u32> samplesPerBlock_ = 0;
    std::atomic<float> sampleRate_ = kDefaultSampleRate;
    std::atomic<u32> minFFTOrder_ = kMinFFTOrder;
    std::atomic<u32> maxFFTOrder_ = kMaxFFTOrder;
    // not atomic because these are only set at plugin instantiation
    const u32 inSidechains_ = 0;
    const u32 outSidechains_ = 0;
  };
}