/*
  ==============================================================================

    EffectsState.hpp
    Created: 2 Oct 2021 8:53:05pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <thread>
#include "Framework/sync_primitives.hpp"
#include "Framework/simd_buffer.hpp"
#include "BaseProcessor.hpp"
#include "Plugin/ProcessorTree.hpp"

namespace Generation
{
  class EffectModule;
  class EffectsState;

  class EffectsLane final : public BaseProcessor
  {
  public:
    EffectsLane() = delete;

    EffectsLane(Plugin::ProcessorTree *processorTree) noexcept;

    EffectsLane(const EffectsLane &other) noexcept : BaseProcessor{ other } { }
    EffectsLane &operator=(const EffectsLane &other) noexcept 
    { BaseProcessor::operator=(other); return *this; }

    void deserialiseFromJson(void *jsonData) override;

    void initialise() noexcept override
    {
      BaseProcessor::initialise();
      status_.store(LaneStatus::Finished, std::memory_order_release);
      currentEffectIndex_.store(0, std::memory_order_release);
    }

    // Inherited via BaseProcessor
    void insertSubProcessor(usize index, BaseProcessor &newSubProcessor) noexcept override;
    BaseProcessor &deleteSubProcessor(usize index) noexcept override;
    void initialiseParameters() override;

    auto *getEffectModule(usize index) const noexcept { return effectModules_[index]; }
    auto getEffectModuleCount() const noexcept { return effectModules_.size(); }

    EffectsLane *createCopy() const override { return processorTree_->createProcessor<EffectsLane>(*this); }

  private:
    Framework::ComplexDataSource laneDataSource_;
    std::vector<EffectModule *> effectModules_{};

    //// Parameters
    // 
    // 1. lane enabled
    // 2. input index
    // 3. output index
    // 4. gain match

    utils::shared_value<simd_float> volumeScale_{};
    std::atomic<u32> currentEffectIndex_ = 0;

    // Finished - finished all processing
    // Ready - ready for a thread to begin work
    // Running - processing is running
    // Stopped - temporarily stopped to wait for data from another lane
    enum class LaneStatus : u32 { Finished, Ready, Running, Stopped };

    std::atomic<LaneStatus> status_ = LaneStatus::Finished;

    friend class EffectsState;
  };

  class EffectsState final : public BaseProcessor
  {
  public:
    // data link between modules in different lanes
    struct EffectsModuleLink
    {
      bool checkForFeedback();

      std::pair<u32, u32> sourceIndex, destinationIndex;
    };

    EffectsState() = delete;
    EffectsState(const EffectsState &) = delete;
    EffectsState(EffectsState &&) = delete;
    EffectsState &operator=(const EffectsState &other) = delete;
    EffectsState &operator=(EffectsState &&other) = delete;

    EffectsState(Plugin::ProcessorTree *processorTree) noexcept;
    ~EffectsState() noexcept override;

    void deserialiseFromJson(void *jsonData) override;

    // Inherited via BaseProcessor
    void insertSubProcessor(usize index, BaseProcessor &newSubProcessor) noexcept override;
    BaseProcessor &deleteSubProcessor(usize index) noexcept override;
    void initialiseParameters() override;

    void writeInputData(Framework::CheckedPointer<float> inputBuffer, usize channels, usize samples) noexcept;
    void processLanes() noexcept;
    void sumLanesAndWriteOutput(Framework::CheckedPointer<float> outputBuffer, usize channels, usize samples) noexcept;

    std::span<char> getUsedInputChannels() noexcept;
    std::span<char> getUsedOutputChannels() noexcept;

    auto getOutputBuffer(u32 channelCount, u32 beginChannel) const noexcept
    { return Framework::SimdBufferView{ outputBuffer_, beginChannel, channelCount }; }

    usize getLaneCount() const noexcept { return lanes_.size(); }
    auto *getEffectsLane(usize index) const noexcept { return lanes_[index]; }

    // current number of FFT bins (real-imaginary pairs)
    u32 binCount = 0;
    float sampleRate = 0.0f;
    float blockPhase = 0.0f;

  private:
    struct Thread;
    friend struct Thread;
    
    BaseProcessor *createCopy() const override
    { COMPLEX_ASSERT_FALSE("You're trying to copy EffectsState, which is not meant to be copied"); return nullptr; }

    void checkUsage();
    
    void distributeWork() const noexcept;
    void processIndividualLanes(usize laneIndex) const noexcept;

    std::vector<EffectsLane *> lanes_{};

    // if an input/output isn't used there's no need to process it at all
    std::vector<char> usedInputChannels_{};
    std::vector<char> usedOutputChannels_{};
    std::vector<float> outputScaleMultipliers_{};

    Framework::SimdBuffer<Framework::complex<float>, simd_float> outputBuffer_{};

    std::vector<Thread> workerThreads_;
  };
}
