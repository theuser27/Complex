/*
  ==============================================================================

    SoundEngine.cpp
    Created: 12 Aug 2021 2:12:59am
    Author:  theuser27

  ==============================================================================
*/

#include "SoundEngine.hpp"

#include "Framework/fourier_transform.hpp"
#include "Framework/parameter_value.hpp"
#include "EffectsState.hpp"
#include "../Plugin/ProcessorTree.hpp"

namespace Generation
{
  SoundEngine::SoundEngine(Plugin::ProcessorTree *processorTree) noexcept :
    BaseProcessor{ processorTree, Framework::Processors::SoundEngine::id().value() }
  {
    using namespace Framework;

    auto [minOrder, maxOrder] = processorTree->getMinMaxFFTOrder();
    transforms = utils::up<FFT>::create(minOrder, maxOrder);

    // input buffer size, kind of arbitrary but it must be longer than kMaxProcessingBufferLength
    u32 maxInputBufferLength = 1 << (maxOrder + 5);
    // pre- and post-processing FFT buffers size 
    // (+ 2 for nyquist real/imaginary parts and then rounded up to next simd size)
    u32 maxProcessingBufferLength = (1 << maxOrder) + utils::roundUpToMultiple(2U, (u32)simd_float::size);
    // output buffer size, must be longer than kMaxProcessingBufferLength
    u32 maxOutputBufferLength = (1 << maxOrder) * 2;

    u32 maxInOuts = (u32)utils::max(processorTree->getInputSidechains(), processorTree->getOutputSidechains()) + 1;

    inputBuffer.reserve(maxInOuts * kChannelsPerInOut, maxInputBufferLength);
    // needs to be double the max FFT, otherwise we get out of bounds errors
    FFTBuffer_.reserve(maxInOuts * kChannelsPerInOut, maxProcessingBufferLength);
    outBuffer.reserve(maxInOuts * kChannelsPerInOut, maxOutputBufferLength);
  }

  SoundEngine::~SoundEngine() noexcept = default;

  u32 SoundEngine::getProcessingDelay() const noexcept 
  { return FFTSamples_ + processorTree_->getSamplesPerBlock(); }

  void SoundEngine::resetBuffers() noexcept
  {
    FFTSamplesAtReset_ = FFTSamples_;
    nextOverlapOffset_ = 0;
    blockPosition_ = 0;
    inputBuffer.reset();
    outBuffer.reset();
  }

  void SoundEngine::copyBuffers(const float *const *buffer, u32 inputs, u32 samples) noexcept
  {
    // assume that we don't get blocks bigger than our buffer size
    inputBuffer.writeToBufferEnd(buffer, inputs, samples);

    // we update them here because we could get broken up blocks if done inside the loop
    usedInputChannels_ = effectsState_->getUsedInputChannels();
    usedOutputChannels_ = effectsState_->getUsedOutputChannels();
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
    u32 availableSamples = inputBuffer.newSamplesToRead(nextOverlapOffset_);
    if (availableSamples < FFTSamples_)
      return;

    u32 prevFFTNumSamples = FFTSamples_;
    // how many samples we're processing currently
    FFTSamples_ = 1 << FFTOrder_;
    
    i32 FFTChangeOffset = (i32)prevFFTNumSamples - (i32)FFTSamples_;

    // clearing upper samples that could remain
    // after changing from a higher to lower FFTSize
    if (FFTChangeOffset > 0)
      for (usize i = 0; i < FFTBuffer_.getChannels(); i++)
        std::memset(FFTBuffer_.get(i) + FFTSamples_, 0, (u32)FFTChangeOffset * sizeof(float));

    inputBuffer.readBuffer(FFTBuffer_, (u32)FFTBuffer_.getChannels(), usedInputChannels_, 
      FFTSamples_, inputBuffer.BlockBegin, (i32)nextOverlapOffset_ + FFTChangeOffset);

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
      nextOverlap_ = getParameter(Processors::SoundEngine::Overlap::id().value())->getInternalValue<float>(sampleRate);
      windowType_ = Processors::SoundEngine::WindowType::enum_value_by_id(
        getParameter(Processors::SoundEngine::WindowType::id().value())->getInternalValue<Framework::IndexedData>(sampleRate).first->id).value();
      alpha_ = utils::lerp(kAlphaLowerBound, kAlphaUpperBound, getParameter(Processors::SoundEngine::WindowAlpha::id().value())->getInternalValue<float>(sampleRate));

      // getting the next overlapOffset
      nextOverlapOffset_ = (u32)floorf((float)FFTSamples_ * (1.0f - nextOverlap_));

      break;
    case UpdateFlag::BeforeProcess:
      mix_ = getParameter(Processors::SoundEngine::MasterMix::id().value())->getInternalValue<float>(sampleRate);
      FFTOrder_ = getParameter(Processors::SoundEngine::BlockSize::id().value())->getInternalValue<u32>(sampleRate);
      outGain_ = (float)utils::dbToAmplitude(getParameter(Processors::SoundEngine::OutGain::id().value())->getInternalValue<float>(sampleRate));

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

    processorTree_->setUpdateFlag(flag);
  }

  void SoundEngine::doFFT() noexcept
  {
    // windowing
    windows.applyWindow(FFTBuffer_, FFTBuffer_.getChannels(), usedInputChannels_, FFTSamples_, windowType_, alpha_);

    // in-place FFT
    // FFT-ed only if the input is used
    for (u32 i = 0; i < FFTBuffer_.getChannels(); i++)
      if (usedInputChannels_[i])
        transforms->transformRealForward(FFTOrder_, FFTBuffer_.get(i), i);
  }

  void SoundEngine::processFFT(float sampleRate) noexcept
  {
    // + 1 for nyquist
    effectsState_->binCount = FFTSamples_ / 2 + 1;
    effectsState_->sampleRate = sampleRate;
    effectsState_->blockPosition = blockPosition_;
    effectsState_->blockPhase = (float)((double)blockPosition_ / (double)FFTSamples_);

    effectsState_->writeInputData(FFTBuffer_);
    effectsState_->processLanes();
    effectsState_->sumLanesAndWriteOutput(FFTBuffer_);
  }

  void SoundEngine::doIFFT() noexcept
  {
    // in-place IFFT
    for (u32 i = 0; i < FFTBuffer_.getChannels(); i++)
      if (usedOutputChannels_[i])
        transforms->transformRealInverse(FFTOrder_, FFTBuffer_.get(i), i);

    // if the FFT size is big enough to guarantee that even with max overlap 
    // a block >= samplesPerBlock can be finished, we don't offset
    // otherwise, we offset 2 block sizes back
    outBuffer.setLatencyOffset(2 * (i32)processorTree_->getSamplesPerBlock());

    // overlap-adding
    outBuffer.addOverlapBuffer(FFTBuffer_, outBuffer.getChannelCount(), 
      usedOutputChannels_, FFTSamples_, nextOverlapOffset_, windowType_);
  }

  // when the overlap is more than what the window requires
  // there will be an increase in gain, so we need to offset that
  void SoundEngine::scaleDown(u32 start, u32 samples) noexcept
  {
    using namespace Framework;
    using WindowType = Processors::SoundEngine::WindowType::type;

    // TODO: use an extra overlap_ variable to store the overlap param 
    // from previous scaleDown run in order to apply extra attenuation 
    // when moving the overlap control (essentially becomes linear interpolation)

    float mult;
    switch (windowType_)
    {
    case WindowType::Lerp: return;
    case WindowType::Rectangle:
      mult = 1.0f - currentOverlap_;
      break;
    case WindowType::Hann:
    case WindowType::Triangle:
      if (currentOverlap_ <= 0.5f)
        return;

      mult = (1.0f - currentOverlap_) * 2.0f;
      break;
    case WindowType::Hamming:
      if (currentOverlap_ <= 0.5f)
        return;

      // https://www.desmos.com/calculator/z21xz7r2c9
      mult = (1.0f - currentOverlap_) * 1.84f;
      break;
    case WindowType::Sine:
      if (currentOverlap_ <= 0.33333333f)
        return;

      // https://www.desmos.com/calculator/mmjwlj0gqe
      mult = (1.0f - currentOverlap_) * 1.57f;
      break;
    case WindowType::Exp:
      if (currentOverlap_ <= 0.1235f)
        return;

      // not optimal but it works somewhat ok
      // https://www.desmos.com/calculator/ozcckbnyvl
      mult = (1.0f - currentOverlap_) * 3.25f * sqrtf(alpha_ * currentOverlap_);
      break;
    case WindowType::HannExp:
    case WindowType::Lanczos:
      if (currentOverlap_ <= 0.1235f)
        return;

      // TODO: add optimal scaling for these
      mult = (1.0f - currentOverlap_) * 3.25f * sqrtf(alpha_ * currentOverlap_);
      break;
    default:
      COMPLEX_ASSERT_FALSE("Missing case");
      return;
    }

    for (u32 i = 0; i < outBuffer.getChannelCount(); i++)
    {
      if (!usedOutputChannels_[i])
        continue;

      // TODO: optimise this with aligned simd multiply
      for (u32 j = 0; j < samples; j++)
      {
        u32 sampleIndex = (start + j) % outBuffer.getSize();
        outBuffer.multiply(mult, i, sampleIndex);
      }
    }
  }

  void SoundEngine::mixOut(u32 samples) noexcept
  {
    if (!hasEnoughSamples_)
      return;

    u32 start = outBuffer.getToScaleOutput();
    u32 toScaleSamples = outBuffer.getToScaleOutputToAddOverlap();
    outBuffer.advanceToScaleOutput(toScaleSamples);
    
    i32 FFTChangeOffset = (i32)FFTSamplesAtReset_ - (i32)FFTSamples_;
    i32 latencyOffset = FFTChangeOffset - outBuffer.getLatencyOffset();

    // only dry
    if (mix_ == 0.0f)
    {
      inputBuffer.outBufferRead(outBuffer.getBuffer(), outBuffer.getChannelCount(), 
        usedOutputChannels_, samples, outBuffer.getBeginOutput(), latencyOffset);

      // advancing buffer indices
      inputBuffer.advanceLastOutputBlock(samples);

      return;
    }

    scaleDown(start, toScaleSamples);

    // only wet
    if (mix_ == 1.0f)
    {
      inputBuffer.advanceLastOutputBlock(samples);
      return;
    }
      
    // mix both
    auto &dryBuffer = inputBuffer.getBuffer();

    u32 outBufferSize = outBuffer.getSize();
    u32 beginOutput = outBuffer.getBeginOutput();
    
    u32 inputBufferLastBlock = inputBuffer.getLastOutputBlock();
    u32 inputBufferSize = inputBuffer.getSize();
    u32 beginInput = (inputBufferSize + inputBufferLastBlock + latencyOffset) % inputBufferSize;

    for (u32 i = 0; i < outBuffer.getChannelCount(); i++)
    {
      if (!usedOutputChannels_[i])
        continue;

      // TODO: optimise this with simd
      for (u32 j = 0; j < samples; j++)
      {
        u32 outIndex = (beginOutput + j) % outBufferSize;
        u32 inIndex = (beginInput + j) % inputBufferSize;

        float value = utils::lerp(dryBuffer.read(i, inIndex), outBuffer.read(i, outIndex), mix_);
        outBuffer.write(value, i, outIndex);
      }
    }
    inputBuffer.advanceLastOutputBlock(samples);
  }

  void SoundEngine::fillOutput(float *const *buffer, u32 outputs, u32 samples) noexcept
  {
    // if we don't have enough samples we simply output silence
    if (!hasEnoughSamples_)
    {
      for (u32 i = 0; i < outBuffer.getChannelCount(); i++)
        std::memset(buffer[i], 0, sizeof(float) * samples);
      return;
    }

    outBuffer.readOutput(buffer, outputs, usedOutputChannels_, samples, outGain_);
    outBuffer.advanceBeginOutput(samples);
  }

  void SoundEngine::insertSubProcessor(usize, BaseProcessor &newSubProcessor, bool) noexcept
  {
    COMPLEX_ASSERT(newSubProcessor.getProcessorType() == Framework::Processors::EffectsState::id().value());

    newSubProcessor.setParentProcessorId(processorId_);
    effectsState_ = utils::as<EffectsState>(&newSubProcessor);
    subProcessors_.emplace_back(effectsState_);
  }

  void SoundEngine::process(float *const *buffer, u32 samples,
    float sampleRate, u32 numInputs, u32 numOutputs) noexcept
  {
    COMPLEX_ASSERT(FFTSamples_ != 0, "Number of fft samples has not been set in advance");

    // copying input in the main circular buffer
    copyBuffers(buffer, numInputs, samples);

    while (true)
    {
      isReadyToPerform(samples);
      if (!isPerforming_)
        break;
      
      updateParameters(UpdateFlag::Realtime, sampleRate, true);
      doFFT();
      processFFT(sampleRate);
      doIFFT();
    }

    // copying and scaling the dry signal to the output
    mixOut(samples);
    // copying output to buffer
    fillOutput(buffer, numOutputs, samples);
  }
}