
// Created: 2021-08-12 02:12:59

#pragma once

#include "Framework/utils.hpp"
#include "Framework/circular_buffer.hpp"
#include "Framework/windows.hpp"
#include "Processor.hpp"

namespace Framework
{
  class FFT;
}

namespace Plugin
{
  struct State;
}

namespace Generation
{
  class EffectsLane;

  class SoundEngine final : public Processor
  {
  public:
    COMPLEX_ENUM_LOCAL(Parameters,
      (        Mix, 1757890335887),
      (  BlockSize, 1757890343009),
      (    Overlap, 1757890352148),
      ( WindowType, 1757856325668),
      (WindowAlpha, 1758060737254),
      (    OutGain, 1758060942415),
    )

    SoundEngine(utils::bumpArena *arena, Plugin::State *state, 
      Framework::ProcessorMetadata *metadata, void *serialisedSave);

  private:
    //=========================================================================================
    // Data
    //
    // pre FFT-ed data buffer; size is as big as it can be (while still being reasonable)
    struct InputBuffer : Framework::CircularBuffer
    {
      u32 lastOutputBlock_ = 0;
      u32 blockBegin_ = 0;
      u32 blockEnd_ = 0;

      utils::bumpArena *arena{};

      // lock must be acquired manually to have access to this struct (except in reserve())

      enum BeginPoint { LastOutputBlock, BlockBegin, BlockEnd, End };

      void reset()
      {
        lastOutputBlock_ = 0;
        blockBegin_ = 0;
        blockEnd_ = 0;
        end = 0;
      }

      void reserve(u32 newChannels, u32 newSize);

      strict_inline void advanceLastOutputBlock(u32 samples)
      { lastOutputBlock_ = (lastOutputBlock_ + samples) % size; }

      // used for manually advancing the block to a desired position
      strict_inline void advanceBlock(u32 newBegin, i32 samples)
      {
        blockBegin_ = newBegin;
        blockEnd_ = (newBegin + (u32)samples) % size;
      }

      // returns how many samples in the buffer can be read
      // starting at blockBegin_/blockEnd_until end_
      strict_inline u32 newSamplesToRead(BeginPoint beginPoint, u32 overlapOffset = 0) const
      {
        u32 begin = (beginPoint == BlockBegin) ? blockBegin_ : blockEnd_;

        // calculating the start position of the current block
        u32 currentBufferStart = (begin + overlapOffset) % size;

        // calculating how many samples can be read from the current start position
        auto ret = (size + end - currentBufferStart) % size;
        COMPLEX_ASSERT(ret < size - ret);
        return ret;
      }

      strict_inline u32 getIndex(BeginPoint beginPoint, i32 offset = 0)
      {
        u32 index = (beginPoint == LastOutputBlock) ? lastOutputBlock_ :
                    (beginPoint ==      BlockBegin) ? blockBegin_ :
                    (beginPoint ==        BlockEnd) ? blockEnd_ :
                    (beginPoint ==             End) ? end : 0;
        return (offset == 0) ? index : (size + index + (u32)offset) % size;
      }

      strict_inline u32 getLastOutputBlockToBlockBegin() const { return (size + blockBegin_ - lastOutputBlock_) % size; }
      strict_inline u32 getBlockBeginToBlockEnd() const { return (size + blockEnd_ - blockBegin_) % size; }
      strict_inline u32 getBlockEndToEnd() const { return (size + end - blockEnd_) % size; }
    } inBuffer{};
    //
    // FFT-ed data buffer, size is double the max FFT block
    // even/odd indices - real/imaginary
    Framework::Buffer FFTBuffer_;
    //
    // if an input isn't used there's no need to process it at all
    utils::span<bool> usedInputChannels_{};
    utils::span<bool> usedOutputChannels_{};
    //
    // output buffer containing dry and wet data
    struct OutputBuffer : Framework::CircularBuffer
    {
      // static offset equal to the additional latency caused by overlap
      i32 latencyOffset_ = 0;
      // index of the first new sample that can be output
      u32 beginOutput_ = 0;
      // index of the first add-overlapped sample that hasn't been scaled
      u32 toScaleOutput_ = 0;
      // index of the first sample of the last add-overlapped block
      u32 addOverlap_ = 0;

      utils::bumpArena *arena{};
      
      // lock must be acquired manually to have access to this struct (except in reserve())

      void reset()
      {
        beginOutput_ = 0;
        toScaleOutput_ = 0;
        addOverlap_ = 0;
        end = 0;
      }

      void reserve(u32 newChannels, u32 newSize);

      strict_inline void setLatencyOffset(i32 newLatencyOffset)
      {
        if (latencyOffset_ == newLatencyOffset)
          return;

        beginOutput_ = (u32)((i32)size - newLatencyOffset) % size;
        toScaleOutput_ = 0;
        addOverlap_ = 0;
        end = 0;

        latencyOffset_ = newLatencyOffset;

        clear();
      }

      strict_inline void advanceBeginOutput(u32 samples) { beginOutput_ = (beginOutput_ + samples) % size; }
      strict_inline void advanceToScaleOutput(u32 samples) { toScaleOutput_ = (toScaleOutput_ + samples) % size; }
      strict_inline void advanceAddOverlap(u32 samples) { addOverlap_ = (addOverlap_ + samples) % size; }

      strict_inline u32 getBeginOutputToToScaleOutput() const { return (size + toScaleOutput_ - beginOutput_) % size; }
      strict_inline u32 getToScaleOutputToAddOverlap() const { return (size + addOverlap_ - toScaleOutput_) % size; }
      strict_inline u32 getAddOverlapToEnd() const { return (size + end - addOverlap_) % size; }
    } outBuffer{};
    //
    // windows pointer for accessing windowing types
    Framework::Window windows{};

    //=========================================================================================
    // Methods
    //
    void resizeBuffers(u32 maxSidechainInputs, u32 maxSidechainOutputs);
    void copyBuffers(const float *const *buffer, u32 inputs, u32 samples);
    void isReadyToPerform(u32 samples);
    void doFFT(Framework::FFT &ffts);
    void interleaveInputs(const Framework::Buffer &inputBuffer);
    void processLanes();
    void sumLanesAndDeinterleaveOutputs(Framework::Buffer &outputBuffer);
    void doIFFT(Framework::FFT &ffts);
    void mixOut(u32 samples);
    void fillOutput(float *const *buffer, u32 outputs, u32 samples);

    void checkUsage();
    void distributeWork() const;
    void processIndividualLanes(EffectsLane *lane) const;

  public:
    Interface::Component *createUI() override;

    // initialising pointers and FFT plans
    void resetBuffers();
    void updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters = true);
    void process(float *const *in, float *const *out, u32 samples, float sampleRate,
      u32 numInputs, u32 numOutputs, Framework::FFT &ffts);

    u32 getProcessingDelay() const;
    float getOverlap() const { return currentOverlap_.load(satomi::memory_order_relaxed); }
    u32 getFFTSize() const;
    u32 getMaxBinCount() const;
    utils::pair<u32, u32> getMinMaxFFTOrder();
    u32 getBlockPosition() const { return blockPosition_; }
    const Framework::SimdBuffer *getInterleavedOutputBuffer() const { return interleavedOutputBuffer; }
  private:
    //=========================================================================================
    // Parameters
    //
    // 1. Master Mix
    // 2. Block Size
    // 3. Overlap
    // 4. Window Type
    // 5. Window Alpha
    // 6. Out Gain
    //
    //=========================================================================================
    // Variables
    //
    // mix amount with dry signal
    float mix_ = 1.0f;
    //
    // FFT order
    u32 FFTOrder_ = 0;
    //
    // amount of overlap with the next block
    float nextOverlap_ = kDefaultWindowOverlap;
    satomi::atomic<float> currentOverlap_ = kDefaultWindowOverlap;
    //
    // window type
    uuid windowTypeId_{};
    //
    // window alpha
    float alpha_ = 0.0f;
    //
    // output gain
    float outGain_ = 1.0f;
    //
    // have we performed for this last run?
    bool isPerforming_ = false;
    //
    // do we have enough processed samples to output?
    bool hasEnoughSamples_ = false;
    //
    // current FFT plan in samples
    u32 FFTSamples_ = 0;
    u32 FFTSamplesAtReset_ = 0;
    //
    // how many samples we are moving forward in the outBuffer after the current block
    u32 nextOverlapOffset_ = 0;
    //
    // without any overlap, every processed block starts at phase 0 (minus a static phase offset),
    // however with overlap every consecutive block starts earlier than the FFT size,
    // which implies that the phase for those blocks is no longer aligned at 0,
    // hence the need to store the current index to calculate the phase shift that occurs
    u32 blockPosition_ = 0;
    //
    //
    bool isInitialised_ = false;

    u32 binCount = 0;
    float sampleRate = 0.0f;
    float blockPhase = 0.0f;

    // if an input/output isn't used there's no need to process it at all
    utils::span<float> outputScaleMultipliers_{};

    Framework::SimdBuffer *interleavedInputBuffer{};
    Framework::SimdBuffer *interleavedOutputBuffer{};

    satomi::atomic<bool> shouldWorkersProcess_ = false;
  };

  static_assert(utils::is_trivially_destructible_v<SoundEngine>);
}

extern template Generation::Processor *createProcessor<Generation::SoundEngine>(Plugin::State *, Framework::ProcessorMetadata *, const void *, void *);
extern template void *initialiseTypeStructure<Generation::SoundEngine>(void *metadata, Framework::PluginStructure &structure);
