
// Created: 2021-08-12 02:12:59

#include "SoundEngine.hpp"

#include "Framework/fourier_transform.hpp"
#include "Framework/parameter_value.hpp"
#include "EffectsState.hpp"
#include "Plugin/Complex.hpp"

namespace Generation
{
  void SoundEngine::InputBuffer::reserve(SoundEngine *engine, u32 newChannels, u32 newSize)
  {
    COMPLEX_ASSERT(newChannels > 0 && newSize > 0);

    if (newChannels <= channels && newSize <= size)
      return;

    auto *newData = arranew(engine->arena, float, newChannels * newSize);
    auto *oldData = data;

    {
      utils::ScopedLock g{ lock, utils::WaitMechanism::Wait };

      Framework::copyCircular({ .channels = newChannels, .size = newSize, .data = newData }, *this, end);

      if (size)
      {
        // recalculating indices based on the new size
        blockEnd_ = utils::min((newSize - getBlockEndToEnd()) % newSize, newSize - 1);
        blockBegin_ = utils::min((blockEnd_ - getBlockBeginToBlockEnd()) % newSize, newSize - 1);
        lastOutputBlock_ = utils::min((blockBegin_ - getLastOutputBlockToBlockBegin()) % newSize, newSize - 1);
        end = 0;
      }

      data = newData;
      size = newSize;
      channels = newChannels;
    }

    if (oldData)
      engine->arena->remove(oldData);
  }

  void SoundEngine::OutputBuffer::reserve(SoundEngine *engine, u32 newChannels, u32 newSize)
  {
    COMPLEX_ASSERT(newChannels > 0 && newSize > 0);

    // assumes SPSC relationship, therefore we don't need to lock to access these variables
    if (newChannels <= channels && newSize <= size)
      return;

    auto *newData = arranew(engine->arena, float, newChannels * newSize);
    auto *oldData = data;

    {
      utils::ScopedLock g{ lock, utils::WaitMechanism::Wait };

      Framework::copyCircular({ .channels = newChannels, .size = newSize, .data = newData }, *this, end);

      if (size)
      {
        // recalculating indices based on the new size
        addOverlap_ = utils::clamp((newSize - getAddOverlapToEnd()) % newSize, 0U, newSize - 1);
        toScaleOutput_ = utils::clamp((addOverlap_ - getToScaleOutputToAddOverlap()) % newSize, 0U, newSize - 1);
        beginOutput_ = utils::clamp((toScaleOutput_ - getBeginOutputToToScaleOutput()) % newSize, 0U, newSize - 1);
        end = 0;
      }

      data = newData;
      size = newSize;
      channels = newChannels;
    }

    if (oldData)
      engine->arena->remove(oldData);
  }
}

template<> Generation::SoundEngine *
createProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(1));
  return anew(arena, Generation::SoundEngine, { state, metadata, arena });
}

namespace Generation
{
  SoundEngine::SoundEngine(Plugin::State *state, Framework::ProcessorMetadata *metadata,
    utils::bumpArena *arena) noexcept : BaseProcessor{ state, metadata, arena }
  {
    using namespace Framework;

    auto [minOrder, maxOrder] = state->getMinMaxFFTOrder();

    resizeBuffers(maxOrder, state->plugin->inSidechains, state->plugin->outSidechains);
  }

  void SoundEngine::resizeBuffers(u32 maxOrder, u32 maxSidechainInputs, u32 maxSidechainOutputs)
  {
    // input buffer size, kind of arbitrary but it must be longer than kMaxProcessingBufferLength
    u32 maxInputBufferLength = 1 << (maxOrder + 5);
    // pre- and post-processing FFT buffers size
    // (+ 2 for nyquist real/imaginary parts and then rounded up to next simd size)
    u32 maxProcessingBufferLength = utils::roundUpToMultiple((1U << maxOrder) + 2, (u32)simd_float::size);
    // output buffer size, must be longer than kMaxProcessingBufferLength
    u32 maxOutputBufferLength = (1 << maxOrder) * 2;

    u32 maxInOuts = utils::max(maxSidechainInputs, maxSidechainOutputs) + 1;

    inBuffer.reserve(this, maxInOuts * kChannelsPerInOut, maxInputBufferLength);
    outBuffer.reserve(this, maxInOuts * kChannelsPerInOut, maxOutputBufferLength);

    if (FFTBuffer_.channels < maxInOuts * kChannelsPerInOut || FFTBuffer_.size < maxProcessingBufferLength)
    {
      auto newFFTBuffer = Framework::Buffer
      {
        .channels = maxInOuts * kChannelsPerInOut,
        .size = maxProcessingBufferLength,
        .data = arranew(arena, float, (maxInOuts * kChannelsPerInOut) * maxProcessingBufferLength, {})
      };

      auto *oldBuffer = FFTBuffer_.data;
      {
        utils::ScopedLock g{ FFTBufferLock_, utils::WaitMechanism::Wait };
        FFTBuffer_ = newFFTBuffer;
      }

      if (oldBuffer)
        arena->remove(oldBuffer);
    }
  }

  u32 SoundEngine::getProcessingDelay() const noexcept
  { return FFTSamples_ + state->plugin->getSamplesPerBlock(); }

  u32 SoundEngine::getFFTSize() const noexcept
  {
    return 1 << getParameter(Parameters::BlockSize)->getInternalValue<u32>();
  }

  utils::pair<u32, u32> SoundEngine::getMinMaxFFTOrder()
  {
    auto *parameter = getParameter(Parameters::BlockSize);
    auto details = parameter->getParameterDetails();
    return { (u32)details.minValue, (u32)details.maxValue };
  }

  void SoundEngine::resetBuffers() noexcept
  {
    utils::ScopedLock g{ inBuffer.lock, utils::WaitMechanism::Spin };
    utils::ScopedLock gg{ outBuffer.lock, utils::WaitMechanism::Spin };

    FFTSamplesAtReset_ = FFTSamples_;
    nextOverlapOffset_ = 0;
    blockPosition_ = 0;
    inBuffer.reset();
    outBuffer.reset();
  }

  void SoundEngine::copyBuffers(const float *const *buffer, u32 inputs, u32 samples) noexcept
  {
    // assume that we don't get blocks bigger than our buffer size
    inBuffer.writeAtEnd(buffer, inputs, samples);

    auto &effectsState = getEffectsState();
    // we update them here because we could get broken up blocks if done inside the loop
    usedInputChannels_ = effectsState.getUsedInputChannels();
    usedOutputChannels_ = effectsState.getUsedOutputChannels();
  }

  void SoundEngine::isReadyToPerform(u32 samples) noexcept
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

  void SoundEngine::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters) noexcept
  {
    using namespace Framework;

    BaseProcessor::updateParameters(flag, sampleRate, updateChildrenParameters);

    switch (flag)
    {
    case UpdateFlag::Realtime:
      currentOverlap_ = nextOverlap_;
      nextOverlap_ = getParameter(Overlap)->getInternalValue<float>(sampleRate);
      windowTypeId_ = getParameter(WindowType)->getInternalValue<Framework::IndexedData>(sampleRate).first->id;
      alpha_ = utils::lerp(kAlphaLowerBound, kAlphaUpperBound, getParameter(WindowAlpha)->getInternalValue<float>(sampleRate));

      // getting the next overlapOffset
      nextOverlapOffset_ = (u32)::floorf((float)FFTSamples_ * (1.0f - nextOverlap_));

      break;
    case UpdateFlag::BeforeProcess:
      mix_ = getParameter(Mix)->getInternalValue<float>(sampleRate);
      FFTOrder_ = getParameter(BlockSize)->getInternalValue<u32>(sampleRate);
      outGain_ = (float)utils::dbToAmplitude(getParameter(OutGain)->getInternalValue<float>(sampleRate));

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

  void SoundEngine::doFFT(Framework::FFT &ffts) noexcept
  {
    // windowing
    windows.applyWindow(FFTBuffer_, FFTBuffer_.channels, usedInputChannels_, FFTSamples_, windowTypeId_, alpha_);

    // in-place FFT
    // FFT-ed only if the input is used
    for (u32 i = 0; i < FFTBuffer_.channels; i++)
      if (usedInputChannels_[i])
        ffts.transformRealForward(FFTOrder_, FFTBuffer_.get(i), i);
  }

  void SoundEngine::processFFT(float sampleRate) noexcept
  {
    auto &effectsState = getEffectsState();
    // + 1 for nyquist
    effectsState.binCount = FFTSamples_ / 2 + 1;
    effectsState.sampleRate = sampleRate;
    effectsState.blockPosition = blockPosition_;
    effectsState.blockPhase = (float)((double)blockPosition_ / (double)FFTSamples_);

    effectsState.writeInputData(FFTBuffer_);
    effectsState.processLanes();
    effectsState.sumLanesAndWriteOutput(FFTBuffer_);
  }

  void SoundEngine::doIFFT(Framework::FFT &ffts) noexcept
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

  void SoundEngine::mixOut(u32 samples) noexcept
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
      usedOutputChannels_, start, toScaleSamples, windowTypeId_, currentOverlap_, alpha_);

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

  void SoundEngine::fillOutput(float *const *buffer, u32 outputs, u32 samples) noexcept
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

  bool SoundEngine::insertSubProcessor(usize, BaseProcessor &newSubProcessor, bool)
  {
    if (newSubProcessor.metadata->id != Processors::EffectsState)
    {
      static constexpr uuid acceptedProcessorIds[] = { Processors::EffectsState };
      reportUnexpectedProcessorInsert(newSubProcessor, acceptedProcessorIds);

      return false;
    }

    newSubProcessor.parent = this;
    if (children)
    {
      auto childMetadata = state->findProcessorMetadata(Processors::EffectsState);
      auto string = utils::string::create(localScratch,
        "Attempted insert of more than one %v(id: %zu) inside an %v(id: %zu), \
        but processor supports only one of such subprocessors. If this shows to you as a user, report it to the dev.",
        childMetadata->name, childMetadata->id, metadata->name, metadata->id
      );

      Interface::showNativeMessageBox("Erroneous processor insert", string.data(), Interface::MessageBoxType::Warning);
      return false;
    }

    children = &newSubProcessor;
    return true;
  }

  void SoundEngine::initialiseParameters()
  {
    parameters = createParameters(metadata->parametersCount, metadata->parameters);
    parameterCount = metadata->parametersCount;
  }

  void SoundEngine::process(float *const *in, float *const *out, u32 samples,
    float sampleRate, u32 numInputs, u32 numOutputs, Framework::FFT &ffts) noexcept
  {
    COMPLEX_ASSERT(FFTSamples_ != 0, "Number of fft samples has not been set in advance");

    // copying input in the main circular buffer
    copyBuffers(in, numInputs, samples);

    while (true)
    {
      isReadyToPerform(samples);
      if (!isPerforming_)
        break;

      updateParameters(UpdateFlag::Realtime, sampleRate, true);
      doFFT(ffts);
      processFFT(sampleRate);
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
      ParameterScale::Linear, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess,
      [](char *string, usize size, double value, const ParameterDetails &) { return (usize)::stbsp_snprintf(string, (int)size, "%zu", ((usize)1 << (usize)::round(value))); }),
    COMPLEX_STRUCTURE_PARAMETER("Overlap", SoundEngine::Overlap, kMinWindowOverlap, kMaxWindowOverlap, kDefaultWindowOverlap,
      kDefaultWindowOverlap, ParameterScale::Clamp, "%", ParameterDetails::Automatable),
    COMPLEX_STRUCTURE_PARAMETER("Window", SoundEngine::WindowType,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lerp", .id = Lerp),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hann", .id = Hann),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hamming", .id = Hamming),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Triangle", .id = Triangle),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sine", .id = Sine),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Rectangle", .id = Rectangle),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Exponential", .id = Exponential, .userFlags = Window::HasAlpha),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Hann-Exp", .id = HannExp, .userFlags = Window::HasAlpha),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lanczos", .id = Lanczos, .userFlags = Window::HasAlpha)),
        .defaultOptionId = Hann
      }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Alpha", SoundEngine::WindowAlpha, 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear,
      "%", ParameterDetails::Automatable, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Gain", SoundEngine::OutGain, -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear,
      " dB", ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess)
  );

  soundEngine.children = (ProcessorMetadata *)initialiseTypeStructure<EffectsState>(&soundEngine, structure);

  return &soundEngine.computeCounts();
}
