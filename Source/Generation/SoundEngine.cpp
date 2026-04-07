
// Created: 2021-08-12 02:12:59

#include "SoundEngine.hpp"

#include "Framework/fourier_transform.hpp"
#include "Framework/parameter_value.hpp"
#include "Effects.hpp"
#include "Plugin/Complex.hpp"

namespace Generation
{
  static bool 
  reserveBuffer(Framework::Buffer &buffer, utils::bumpArena *arena, u32 newChannels, u32 newSize, u32 *end)
  {
    newChannels = newChannels ? newChannels : buffer.channels;
    newSize = newSize ? newSize : buffer.size;      

    if (newChannels <= buffer.channels && newSize <= buffer.size)
      return false;

    auto *newData = arranew(arena, float, newChannels * newSize);
    auto *oldData = buffer.data;
    bool copiedData = buffer.size && end;

    {
      utils::ScopedLock g{ buffer.lock, utils::WaitMechanism::Wait };

      if (copiedData)
      {
        Framework::Buffer newBuffer{ .channels = newChannels, .size = newSize, .data = newData };
        Framework::copyCircular(newBuffer, buffer, *end);
        *end = 0;
      }

      buffer.data = newData;
      buffer.size = newSize;
      buffer.channels = newChannels;
    }

    if (oldData)
      utils::bumpArena::remove(oldData);
    return copiedData;
  }
  
  void SoundEngine::InputBuffer::reserve(u32 newChannels, u32 newSize)
  {
    bool copiedData = reserveBuffer(*this, arena, newChannels, newSize, &end);
    if (copiedData)
    {
      // recalculating indices based on the new size
      blockEnd_ = utils::min((newSize - getBlockEndToEnd()) % newSize, newSize - 1);
      blockBegin_ = utils::min((blockEnd_ - getBlockBeginToBlockEnd()) % newSize, newSize - 1);
      lastOutputBlock_ = utils::min((blockBegin_ - getLastOutputBlockToBlockBegin()) % newSize, newSize - 1);
    }
  }

  void SoundEngine::OutputBuffer::reserve(u32 newChannels, u32 newSize)
  {
    bool copiedData = reserveBuffer(*this, arena, newChannels, newSize, &end);
    if (copiedData)
    {
      // recalculating indices based on the new size
      addOverlap_ = utils::clamp((newSize - getAddOverlapToEnd()) % newSize, 0U, newSize - 1);
      toScaleOutput_ = utils::clamp((addOverlap_ - getToScaleOutputToAddOverlap()) % newSize, 0U, newSize - 1);
      beginOutput_ = utils::clamp((toScaleOutput_ - getBeginOutputToToScaleOutput()) % newSize, 0U, newSize - 1);
    }
  }
}

template<> Generation::Processor *
createProcessor<Generation::SoundEngine>(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *, void *serialisedSave)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(4));
  return anew(arena, Generation::SoundEngine, { arena, state, metadata, serialisedSave });
}

namespace Generation
{
  SoundEngine::SoundEngine(utils::bumpArena *arena, Plugin::State *state, 
    Framework::ProcessorMetadata *metadata, void *serialisedSave) : Processor{ arena, state, metadata, nullptr }
  {
    using namespace Framework;

    if (serialisedSave)
      deserialiseFromJson(serialisedSave);
    else
    {
      parameters = createParameters(metadata->parametersCount, metadata->parameters);
      parameterCount = metadata->parametersCount;
    }
    
    inBuffer.arena = arena;
    outBuffer.arena = arena;
    resizeBuffers(state->plugin->inSidechains, state->plugin->outSidechains);
  }

  void SoundEngine::resizeBuffers(u32 maxSidechainInputs, u32 maxSidechainOutputs)
  {
    auto maxOrder = (u32)getParameter(Parameters::BlockSize)->getParameterDetails().maxValue;
    
    // input buffer size, kind of arbitrary but it must be longer than maxProcessingBufferLength
    u32 maxInputBufferLength = 1 << (maxOrder + 4);
    // pre- and post-processing FFT buffers size
    // (+ 2 for nyquist real/imaginary parts and then rounded up to next simd size)
    u32 maxProcessingBufferLength = utils::roundUpToMultiple((1U << maxOrder) + 2, (u32)simd_float::size);
    // output buffer size, must be longer than maxProcessingBufferLength
    u32 maxOutputBufferLength = (1 << maxOrder) * 2;

    u32 maxInChannels = utils::kChannelsPerInOut * (maxSidechainInputs + 1);
    u32 maxOutChannels = utils::kChannelsPerInOut * (maxSidechainOutputs + 1);
    u32 maxInOutChannels = utils::max(maxInChannels, maxOutChannels);

    inBuffer.reserve(maxInOutChannels, maxInputBufferLength);
    outBuffer.reserve(maxInOutChannels, maxOutputBufferLength);
    (void)reserveBuffer(FFTBuffer_, arena, maxInOutChannels, maxProcessingBufferLength, nullptr);

    usedInputChannels_ = { arranew(arena, bool, maxInChannels, {}), maxInChannels };
    usedOutputChannels_ = { arranew(arena, bool, maxOutChannels, {}), maxOutChannels };
    outputScaleMultipliers_ = { arranew(arena, float, maxOutChannels, {}), maxOutChannels };

    auto maxBinCount = (1 << (maxOrder - 1)) + 1;
    interleavedInputBuffer = Framework::SimdBuffer::create(arena, maxInChannels, maxBinCount);
    interleavedOutputBuffer = Framework::SimdBuffer::create(arena, maxOutChannels, maxBinCount);
  }

  u32 SoundEngine::getProcessingDelay() const { return FFTSamples_ + state->plugin->getSamplesPerBlock(); }
  u32 SoundEngine::getFFTSize() const { return 1 << getParameter(Parameters::BlockSize)->getInternalValue<u32>(); }
  u32 
  SoundEngine::getMaxBinCount() const
  {
    return (1 << ((u32)getParameter(Parameters::BlockSize)->getParameterDetails().maxValue - 1)) + 1;
  }

  utils::pair<u32, u32> 
  SoundEngine::getMinMaxFFTOrder()
  {
    auto *parameter = getParameter(Parameters::BlockSize);
    auto details = parameter->getParameterDetails();
    return { (u32)details.minValue, (u32)details.maxValue };
  }

  void SoundEngine::resetBuffers()
  {
    utils::ScopedLock g{ inBuffer.lock, utils::WaitMechanism::Spin };
    utils::ScopedLock gg{ outBuffer.lock, utils::WaitMechanism::Spin };

    FFTSamplesAtReset_ = FFTSamples_;
    nextOverlapOffset_ = 0;
    blockPosition_ = 0;
    inBuffer.reset();
    outBuffer.reset();
  }

  void SoundEngine::copyBuffers(const float *const *buffer, u32 inputs, u32 samples)
  {
    // assume that we don't get blocks bigger than our buffer size
    inBuffer.writeAtEnd(buffer, inputs, samples);

    // update used in/outputs here because we could get broken up blocks if done inside the loop
    
    // update inputs
    {
      ::zeroset(usedInputChannels_.data(), usedInputChannels_.size());

      for (auto *lane = getChild(children, 0, Processors::EffectsLane); lane;
        lane = getChild(lane, 1, Processors::EffectsLane))
      {
        // if the input is not another lane's output and the chain is enabled
        if (lane->getParameter(EffectsLane::LaneEnabled)->getInternalValue<u32>())
        {
          auto [laneIndexedData, index] = lane->getParameter(EffectsLane::Input)->getInternalValue<Framework::IndexedData>();
          usize startIndex = 0;
          if (laneIndexedData->id == EffectsLane::InputOptionsMain) { }
          else if (laneIndexedData->id == EffectsLane::InputOptionsSidechain)
            startIndex = utils::kChannelsPerInOut * (index + 1);
          else if (laneIndexedData->id == EffectsLane::InputOptionsLane)
            continue;
          else COMPLEX_ASSERT_FALSE("Missing case");

          for (usize i = 0; i < utils::kChannelsPerInOut; ++i)
            usedInputChannels_[startIndex + i] = true;
        }
      }
    }

    // update outputs
    {
      ::zeroset(usedOutputChannels_.data(), usedOutputChannels_.size());

      for (auto *lane = getChild(children, 0, Processors::EffectsLane); lane;
        lane = getChild(lane, 1, Processors::EffectsLane))
      {
        if (lane->getParameter(EffectsLane::LaneEnabled)->getInternalValue<u32>())
        {
          auto [laneIndexedData, index] = lane->getParameter(EffectsLane::Output)->getInternalValue<Framework::IndexedData>();
          usize startIndex = 0;
          if (laneIndexedData->id == EffectsLane::OutputOptionsMain) { }
          else if (laneIndexedData->id == EffectsLane::OutputOptionsSidechain)
            startIndex = utils::kChannelsPerInOut * (index + 1);
          else if (laneIndexedData->id == EffectsLane::OutputOptionsNone)
            continue;
          else COMPLEX_ASSERT_FALSE("Missing case");

          for (usize i = 0; i < utils::kChannelsPerInOut; ++i)
            usedOutputChannels_[startIndex + i] = true;
        }
      }
    }
  }

  void SoundEngine::isReadyToPerform(u32 samples)
  {
    isPerforming_ = false;
    hasEnoughSamples_ = false;

    // if there are scaled and/or processed samples
    // that haven't already been output we don't need to perform
    u32 samplesReady = outBuffer.getBeginOutputToToScaleOutput() +
      outBuffer.getToScaleOutputToAddOverlap();
    if (samplesReady >= samples)
    {
      hasEnoughSamples_ = true;
      return;
    }

    // are there enough samples ready to be processed?
    u32 availableSamples = inBuffer.newSamplesToRead(InputBuffer::BlockBegin, nextOverlapOffset_);
    if (availableSamples < FFTSamples_)
      return;

    u32 prevFFTNumSamples = FFTSamples_;
    // how many samples we're processing currently
    FFTSamples_ = 1 << FFTOrder_;

    i32 FFTChangeOffset = (i32)prevFFTNumSamples - (i32)FFTSamples_;

    // clearing upper samples that could remain
    // after changing from a higher to lower FFTSize
    if (FFTChangeOffset > 0)
      for (u32 i = 0; i < FFTBuffer_.channels; i++)
        ::zeroset(FFTBuffer_.get(i) + FFTSamples_, (u32)FFTChangeOffset);

    u32 start = inBuffer.getIndex(InputBuffer::BlockBegin, (i32)nextOverlapOffset_ + FFTChangeOffset);
    inBuffer.readAt(FFTBuffer_, FFTBuffer_.channels, FFTSamples_, start, 0, usedInputChannels_);
    inBuffer.advanceBlock(start, FFTSamples_);

    blockPosition_ += nextOverlapOffset_ + (u32)FFTChangeOffset;

    isPerforming_ = true;
  }

  void SoundEngine::updateParameters(UpdateFlag flag, float currentSampleRate, bool updateChildrenParameters)
  {
    using namespace Framework;

    Processor::updateParameters(flag, currentSampleRate, updateChildrenParameters);

    switch (flag)
    {
    case UpdateFlag::Realtime:
      currentOverlap_.store(nextOverlap_, satomi::memory_order_relaxed);
      nextOverlap_ = getParameter(Overlap)->getInternalValue<float>(currentSampleRate);
      windowTypeId_ = getParameter(WindowType)->getInternalValue<Framework::IndexedData>(currentSampleRate).first->id;
      alpha_ = utils::lerp(kAlphaLowerBound, kAlphaUpperBound, getParameter(WindowAlpha)->getInternalValue<float>(currentSampleRate));

      // getting the next overlapOffset
      nextOverlapOffset_ = (u32)::floorf((float)FFTSamples_ * (1.0f - nextOverlap_));

      break;
    case UpdateFlag::BeforeProcess:
      mix_ = getParameter(Mix)->getInternalValue<float>(currentSampleRate);
      FFTOrder_ = getParameter(BlockSize)->getInternalValue<u32>(currentSampleRate);
      outGain_ = (float)utils::dbToAmplitude(getParameter(OutGain)->getInternalValue<float>(currentSampleRate));

      if (!isInitialised_)
      {
        isInitialised_ = true;
        FFTSamples_ = 1 << FFTOrder_;
        FFTSamplesAtReset_ = FFTSamples_;
      }

      break;
    default:
      break;
    }
  }

  void SoundEngine::doFFT(Framework::FFT &ffts)
  {
    // windowing
    windows.applyWindow(FFTBuffer_, FFTBuffer_.channels, usedInputChannels_, FFTSamples_, windowTypeId_, alpha_);

    // in-place FFT
    // FFT-ed only if the input is used
    for (u32 i = 0; i < FFTBuffer_.channels; i++)
      if (usedInputChannels_[i])
        ffts.transformRealForward(FFTOrder_, FFTBuffer_.get(i), i);
  }

  void SoundEngine::doIFFT(Framework::FFT &ffts)
  {
    using namespace Framework;

    // in-place IFFT
    for (u32 i = 0; i < FFTBuffer_.channels; i++)
      if (usedOutputChannels_[i])
        ffts.transformRealInverse(FFTOrder_, FFTBuffer_.get(i), i);

    // if the FFT size is big enough to guarantee that even with max overlap
    // a block >= samplesPerBlock can be finished, we don't offset
    // otherwise, we offset 2 block sizes back
    outBuffer.setLatencyOffset(2 * (i32)state->plugin->getSamplesPerBlock());

    // overlap-adding
    {
      u32 bufferSize = outBuffer.size;
      u32 oldEnd = outBuffer.end;
      outBuffer.end = (outBuffer.addOverlap_ + FFTSamples_) % bufferSize;

      // getting how many samples are going to be overlapped
      // we clamp the value to max samples in case the FFT size was changed
      u32 overlappedSamples = utils::min(utils::circularDifference(
        outBuffer.addOverlap_, oldEnd, bufferSize), FFTSamples_);

      if (overlappedSamples)
        windows.addOverlap(outBuffer, FFTBuffer_, outBuffer.channels,
          usedOutputChannels_, overlappedSamples, outBuffer.addOverlap_, windowTypeId_);

      // writing stuff that isn't overlapped
      if (u32 assignSamples = FFTSamples_ - overlappedSamples; assignSamples)
      {
        //buffer_.clear((bufferSize + newEnd - assignSamples) % bufferSize, assignSamples);
        applyToBuffer(CircularBuffer::assignBuffersFn, outBuffer,
          FFTBuffer_, outBuffer.channels, assignSamples,
          (bufferSize + outBuffer.end - assignSamples) % bufferSize, overlappedSamples, usedOutputChannels_);
      }

      outBuffer.advanceAddOverlap(nextOverlapOffset_);
    }
  }

  void SoundEngine::mixOut(u32 samples)
  {
    if (!hasEnoughSamples_)
      return;

    u32 start = outBuffer.toScaleOutput_;
    u32 toScaleSamples = outBuffer.getToScaleOutputToAddOverlap();
    outBuffer.advanceToScaleOutput(toScaleSamples);

    i32 FFTChangeOffset = (i32)FFTSamplesAtReset_ - (i32)FFTSamples_;
    i32 latencyOffset = FFTChangeOffset - outBuffer.latencyOffset_;

    // only dry
    if (mix_ == 0.0f)
    {
      u32 inStart = inBuffer.getIndex(InputBuffer::LastOutputBlock, latencyOffset);
      inBuffer.readAt(outBuffer, outBuffer.channels, samples, inStart,
        outBuffer.beginOutput_, usedOutputChannels_);

      // advancing buffer indices
      inBuffer.advanceLastOutputBlock(samples);

      return;
    }

    // when the overlap is more than what the window requires
    // there will be an increase in gain, so we need to offset that
    windows.scaleDown(outBuffer, outBuffer.channels,
      usedOutputChannels_, start, toScaleSamples, windowTypeId_, 
      currentOverlap_.load(satomi::memory_order_relaxed), alpha_);

    // only wet
    if (mix_ == 1.0f)
    {
      inBuffer.advanceLastOutputBlock(samples);
      return;
    }

    // mix both
    u32 beginInput = (inBuffer.size + inBuffer.lastOutputBlock_ + latencyOffset) % inBuffer.size;

    for (u32 i = 0; i < outBuffer.channels; i++)
    {
      if (!usedOutputChannels_[i])
        continue;

      auto in = inBuffer.get(i);
      auto out = outBuffer.get(i);

      // TODO: optimise this with simd
      for (u32 j = 0; j < samples; j++)
      {
        u32 outIndex = (outBuffer.beginOutput_ + j) % outBuffer.size;
        u32 inIndex = (beginInput + j) % inBuffer.size;
        out[outIndex] = utils::lerp(in[inIndex], out[outIndex], mix_);
      }
    }
    inBuffer.advanceLastOutputBlock(samples);
  }

  void SoundEngine::fillOutput(float *const *buffer, u32 outputs, u32 samples)
  {
    // if we don't have enough samples we simply output silence
    // TODO: hasEnoughSamples_ is only for FFT-ing data, not outputting??
    if (!hasEnoughSamples_)
    {
      for (u32 i = 0; i < outBuffer.channels; i++)
        ::zeroset(buffer[i], samples);
      return;
    }

    COMPLEX_ASSERT(outputs <= outBuffer.channels);
    outBuffer.readAt(buffer, outputs, samples, outBuffer.beginOutput_, usedOutputChannels_);
    // zero out non-copied channels and scaling
    for (u32 i = 0; i < outputs; i++)
    {
      if (!usedOutputChannels_[i])
      {
        ::zeroset(buffer[i], samples);
        continue;
      }

      if (outGain_ != 1.0f)
        for (u32 j = 0; j < samples; ++j)
          buffer[i][j] *= outGain_;
    }

    outBuffer.advanceBeginOutput(samples);
  }

  void SoundEngine::process(float *const *in, float *const *out, u32 samples,
    float currentSampleRate, u32 numInputs, u32 numOutputs, Framework::FFT &ffts)
  {
    COMPLEX_ASSERT(FFTSamples_ != 0, "Number of fft samples has not been set in advance");

    // copying input in the main circular buffer
    copyBuffers(in, numInputs, samples);

    while (true)
    {
      isReadyToPerform(samples);
      if (!isPerforming_)
        break;

      updateParameters(UpdateFlag::Realtime, currentSampleRate, true);
      doFFT(ffts);
      {
        // + 1 for nyquist
        binCount = FFTSamples_ / 2 + 1;
        sampleRate = currentSampleRate;
        blockPhase = (float)((double)blockPosition_ / (double)FFTSamples_);

        interleaveInputs(FFTBuffer_);
        processLanes();
        sumLanesAndDeinterleaveOutputs(FFTBuffer_);
      }
      doIFFT(ffts);
    }

    // copying and scaling the dry signal to the output
    mixOut(samples);
    // copying output to buffer
    fillOutput(out, numOutputs, samples);
  }
}

template<> void *
initialiseTypeStructure<Generation::SoundEngine>(void *, Framework::PluginStructure &structure)
{
  using namespace Framework;
  using namespace Generation;
  using enum Window::Types;

  auto *arena = structure.getNewArena(COMPLEX_KB(3));

  ProcessorMetadata &soundEngine = COMPLEX_STRUCTURE_PROCESSOR(SoundEngine, "Sound Engine", Processors::SoundEngine);
  soundEngine.parameters =
  (
    COMPLEX_STRUCTURE_PARAMETER("Mix", SoundEngine::Mix, 0.0f, 1.0f, 1.0f, 1.0f,
      ParameterScale::Linear, "%", ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Block Size", SoundEngine::BlockSize, kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder,
      (float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder),
      ParameterScale::Linear, {}, ParameterDetails::Automatable | ParameterDetails::Extensible | ParameterDetails::RoundToInt, UpdateFlag::BeforeProcess,
      [](char *string, usize size, double value, const ParameterDetails &) { return (usize)::stbsp_snprintf(string, (int)size, "%zu", ((usize)1 << (usize)::round(value))); }),
    COMPLEX_STRUCTURE_PARAMETER("Overlap", SoundEngine::Overlap, kMinWindowOverlap, kMaxWindowOverlap, kDefaultWindowOverlap,
      kDefaultWindowOverlap, ParameterScale::Clamp, "%", ParameterDetails::Automatable),
    COMPLEX_STRUCTURE_PARAMETER("Window", SoundEngine::WindowType,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lerp", .id = Lerp),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hann", .id = Hann),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hamming", .id = Hamming),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Triangle", .id = Triangle),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sine", .id = Sine),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Rectangle", .id = Rectangle),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Exponential", .id = Exponential, .userFlags = Window::HasAlpha),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hann-Exp", .id = HannExp, .userFlags = Window::HasAlpha),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lanczos", .id = Lanczos, .userFlags = Window::HasAlpha) }}),
        .defaultOptionId = Hann
      }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Alpha", SoundEngine::WindowAlpha, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear,
      "%", ParameterDetails::Automatable, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Gain", SoundEngine::OutGain, -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear,
      " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess)
  );

  soundEngine.children = (ProcessorMetadata *)initialiseTypeStructure<EffectsLane>(&soundEngine, structure);

  return &soundEngine.computeCounts();
}
