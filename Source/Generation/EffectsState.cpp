/*
  ==============================================================================

    EffectsState.cpp
    Created: 2 Oct 2021 8:53:05pm
    Author:  theuser27

  ==============================================================================
*/

#include "EffectsState.hpp"

#include "Framework/simd_math.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/parameter_value.hpp"
#include "Plugin/ProcessorTree.hpp"
#include "EffectModules.hpp"

namespace Generation
{
  EffectsLane::EffectsLane(Plugin::ProcessorTree *processorTree) noexcept :
    BaseProcessor{ processorTree, Framework::Processors::EffectsLane::id().value() }
  {
    subProcessors_.reserve(32);

    auto maxBinCount = processorTree->getMaxBinCount();
    laneDataSource_.conversionBuffer.reserve(kChannelsPerInOut, maxBinCount);

    auto maxInOuts = std::max(processorTree->getInputSidechains(), processorTree->getOutputSidechains()) + 1;
    dataBuffer_.reserve(maxInOuts * kChannelsPerInOut, maxBinCount);
  }

  void EffectsLane::insertSubProcessor(usize index, BaseProcessor &newSubProcessor) noexcept
  {
    COMPLEX_ASSERT(newSubProcessor.getProcessorType() == Framework::Processors::EffectModule::id().value()
      && "You're trying to move a non-EffectModule into EffectChain");

    newSubProcessor.setParentProcessorId(processorId_);
    effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)index, utils::as<EffectModule>(&newSubProcessor));
    subProcessors_.insert(subProcessors_.begin() + (std::ptrdiff_t)index, &newSubProcessor);

    for (auto *listener : listeners_)
      listener->insertedSubProcessor(index, newSubProcessor);
  }

  BaseProcessor &EffectsLane::deleteSubProcessor(usize index) noexcept
  {
    COMPLEX_ASSERT(index < subProcessors_.size());

    BaseProcessor *deletedProcessor = subProcessors_[index];
    effectModules_.erase(effectModules_.begin() + (ptrdiff_t)index);
    subProcessors_.erase(subProcessors_.begin() + (ptrdiff_t)index);

    for (auto *listener : listeners_)
      listener->deletedSubProcessor(index, *deletedProcessor);

    return *deletedProcessor;
  }

  void EffectsLane::initialiseParameters()
  {
    createProcessorParameters(Framework::Processors::EffectsLane::
      enum_ids_filter<Framework::kGetParameterPredicate, true>());
  }

  EffectsState::EffectsState(Plugin::ProcessorTree *processorTree) noexcept :
    BaseProcessor{ processorTree, Framework::Processors::EffectsState::id().value() }
  {
    static constexpr auto initialLaneCount = 16;

    lanes_.reserve(initialLaneCount);
    subProcessors_.reserve(initialLaneCount);
    workerThreads_.reserve(initialLaneCount);

    auto maxBinCount = processorTree->getMaxBinCount();
    auto inSidechains = processorTree->getInputSidechains();
    auto outSidechains = processorTree->getOutputSidechains();

    usedInputChannels_ = std::vector<char>((inSidechains + 1) * kChannelsPerInOut, false);
    usedOutputChannels_ = std::vector<char>((outSidechains + 1) * kChannelsPerInOut, false);
    outputScaleMultipliers_ = std::vector<float>((outSidechains + 1) * kChannelsPerInOut, 1.0f);

    // size is half the max because a single SIMD package stores both real and imaginary parts
    dataBuffer_.reserve((inSidechains + 1) * kChannelsPerInOut, maxBinCount);
    outputBuffer_.reserve((outSidechains + 1) * kChannelsPerInOut, maxBinCount);
  }

  void EffectsState::insertSubProcessor(usize index, BaseProcessor &newSubProcessor) noexcept
  {
    COMPLEX_ASSERT(newSubProcessor.getProcessorType() == Framework::Processors::EffectsLane::id().value() &&
      "You're trying to insert a non-EffectChain into EffectsState");

    newSubProcessor.setParentProcessorId(processorId_);
    lanes_.emplace_back(utils::as<EffectsLane>(&newSubProcessor));
    subProcessors_.emplace_back(&newSubProcessor);

    for (auto *listener : listeners_)
      listener->insertedSubProcessor(index, newSubProcessor);
  }

  BaseProcessor &EffectsState::deleteSubProcessor(usize index) noexcept
  {
    COMPLEX_ASSERT(index < subProcessors_.size());

    BaseProcessor *deletedLane = lanes_[index];
    lanes_.erase(lanes_.begin() + (ptrdiff_t)index);
    subProcessors_.erase(subProcessors_.begin() + (ptrdiff_t)index);

    for (auto *listener : listeners_)
      listener->deletedSubProcessor(index, *deletedLane);

    return *deletedLane;
  }

  void EffectsState::initialiseParameters()
  {
    createProcessorParameters(Framework::Processors::EffectsState::
      enum_ids_filter<Framework::kGetParameterPredicate, true>());
  }

  std::span<char> EffectsState::getUsedInputChannels() noexcept
  {
    using namespace Framework;

    std::ranges::fill(usedInputChannels_, false);
    for (auto &lane : lanes_)
    {
      static constexpr auto kInputId = Processors::EffectsLane::Input::id().value();
      static constexpr auto kInputMainId = Processors::EffectsLane::Input::Main::id().value();
      static constexpr auto kInputSidechainId = Processors::EffectsLane::Input::Sidechain::id().value();
      static constexpr auto kInputLaneId = Processors::EffectsLane::Input::Lane::id().value();
      static constexpr auto kLaneEnabledId = Processors::EffectsLane::LaneEnabled::id().value();

      auto [laneIndexedData, index] = lane->getParameter(kInputId)->getInternalValue<std::string_view>();
      // if the input is not another lane's output and the chain is enabled
      if (lane->getParameter(kLaneEnabledId)->getInternalValue<u32>())
      {
        usize startIndex = 0;
        if (laneIndexedData->id == kInputMainId) { }
        else if (laneIndexedData->id == kInputSidechainId)
          startIndex = kChannelsPerInOut * (index + 1);
        else if (laneIndexedData->id == kInputLaneId)
          continue;
        else COMPLEX_ASSERT_FALSE("Missing case");

        for (usize i = 0; i < kChannelsPerInOut; ++i)
          usedInputChannels_[startIndex + i] = true;
      }
    }

    return usedInputChannels_;
  }

  std::span<char> EffectsState::getUsedOutputChannels() noexcept
  {
    using namespace Framework;

    std::ranges::fill(usedOutputChannels_, false);
    for (auto &lane : lanes_)
    {
      static constexpr auto kOutputId = Processors::EffectsLane::Output::id().value();
      static constexpr auto kOutputMainId = Processors::EffectsLane::Output::Main::id().value();
      static constexpr auto kOutputSidechainId = Processors::EffectsLane::Output::Sidechain::id().value();
      static constexpr auto kOutputNoneId = Processors::EffectsLane::Output::None::id().value();
      static constexpr auto kLaneEnabledId = Processors::EffectsLane::LaneEnabled::id().value();

      auto [laneIndexedData, index] = lane->getParameter(kOutputId)->getInternalValue<std::string_view>();
      if (lane->getParameter(kLaneEnabledId)->getInternalValue<u32>())
      {
        usize startIndex = 0;
        if (laneIndexedData->id == kOutputMainId) { }
        else if (laneIndexedData->id == kOutputSidechainId)
          startIndex = kChannelsPerInOut * (index + 1);
        else if (laneIndexedData->id == kOutputNoneId)
          continue;
        else COMPLEX_ASSERT_FALSE("Missing case");

        for (usize i = 0; i < kChannelsPerInOut; ++i)
          usedOutputChannels_[startIndex + i] = true;
      }
    }

    return usedOutputChannels_;
  }

  void EffectsState::writeInputData(const float *inputBuffer, usize channels, usize samples) noexcept
  {
    using namespace Framework;
    using namespace utils;

    COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);
    ScopedLock g{ dataBuffer_.getLock(), true, WaitMechanism::Spin };

    auto values = utils::array<simd_float, SimdBuffer<complex<float>, simd_float>::getRelativeSize()>{};
    auto data = dataBuffer_.get();
    usize size = outputBuffer_.getSize();
    for (usize i = 0; i < channels; i += values.size())
    {
      // if the input is not used we skip it
      if (!usedInputChannels_[i])
        continue;

      auto *channelBuffer = inputBuffer + i * samples;

      // skipping every n-th complex pair (currently simd_float can take 2 pairs)
      for (usize j = 0; j < binCount_ - 1; j += values.size())
      {
        // skipping every second sample (complex signal)
        for (u32 k = 0; k < values.size(); ++k)
          values[k] = toSimdFloatFromUnaligned(channelBuffer + k * samples + j * 2);

        complexTranspose(values);

        for (u32 k = 0; k < values.size(); ++k)
          data[i * size + j + k] = values[k];
      }

      simd_float::array_t nyquist{};
      for (u32 k = 0; k < nyquist.size(); k += 2)
      {
        nyquist[k] = channelBuffer[k * samples + (binCount_ - 1) * 2];
        nyquist[k + 1] = channelBuffer[k * samples + (binCount_ - 1) * 2 + 1];
      }
      data[i * size + binCount_ - 1] = nyquist;
    }
  }

  void EffectsState::processLanes() noexcept
  {
    // sequential consistency just in case
    // triggers the chains to run again
    for (auto &lane : lanes_)
      lane->status_.store(EffectsLane::LaneStatus::Ready, std::memory_order_seq_cst);

    distributeWork();

    // waiting for chains to finish
    for (auto &lane : lanes_)
      while (lane->status_.load(std::memory_order_acquire) != EffectsLane::LaneStatus::Finished) { utils::longPause<5>(); }
  }

  EffectsState::Thread::Thread(EffectsState &state) : thread([this, &state]()
    {
      while (true)
      {
        if (shouldStop.load(std::memory_order_acquire))
          return;

        state.distributeWork();
      }
    }) { }

  void EffectsState::checkUsage()
  {
    // TODO: decide on a heuristic when to add worker threads
    //workerThreads_.emplace_back(*this);
  }

  void EffectsState::distributeWork() const noexcept
  {
    for (usize i = 0; i < lanes_.size(); ++i)
    {
      if (lanes_[i]->status_.load(std::memory_order_acquire) == EffectsLane::LaneStatus::Ready)
      {
        auto expected = EffectsLane::LaneStatus::Ready;
        if (lanes_[i]->status_.compare_exchange_strong(expected, EffectsLane::LaneStatus::Running,
          std::memory_order_acq_rel, std::memory_order_relaxed))
          processIndividualLanes(i);
      }
    }
  }

  void EffectsState::processIndividualLanes(usize laneIndex) const noexcept
  {
    using namespace Framework;
    using namespace utils;

    auto *thisLane = lanes_[laneIndex];

    thisLane->currentEffectIndex_.store(0, std::memory_order_release);
    thisLane->volumeScale_ = 1.0f;
    auto &laneDataSource = thisLane->laneDataSource_;
    laneDataSource.normalisedPhase = normalisedPhase_;
    bool isLaneOn = thisLane->getParameter(Processors::EffectsLane::LaneEnabled::id().value())
      ->getInternalValue<u32>();

    // Lane Input 
    // if this lane's input is another's output and that lane can be used,
    // we wait until it is finished and then copy its data
    if (auto inputIndex = thisLane->getParameter(Processors::EffectsLane::Input::id().value())
      ->getInternalValue<std::string_view>(); inputIndex.first->id == Processors::EffectsLane::Input::Lane::id().value())
    {
      COMPLEX_ASSERT(inputIndex.second != laneIndex && "A lane cannot use its own output as input");
      while (true)
      {
        auto otherLaneStatus = lanes_[inputIndex.second]->status_.load(std::memory_order_acquire);
        if (otherLaneStatus == EffectsLane::LaneStatus::Finished)
          break;

        longPause<40>();
      }

      // getting shared access to the lane's output
      // if this lane is turned off, we only grab the view from the other lane's buffer
      // since we won't be reading from it but it's necessary for the outputBuffer to know where the data is
      if (!isLaneOn)
      {
        // TODO: decide if it's even worth to wait, or we can grab a reference to the other lane's SimdBufferView
        laneDataSource.sourceBuffer = lanes_[inputIndex.second]->laneDataSource_.sourceBuffer;
        laneDataSource.dataType = lanes_[inputIndex.second]->laneDataSource_.dataType;

        thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
        return;
      }

      lockAtomic(lanes_[inputIndex.second]->laneDataSource_.sourceBuffer.getLock(), false, WaitMechanism::Spin);
      laneDataSource.sourceBuffer = lanes_[inputIndex.second]->laneDataSource_.sourceBuffer;
      laneDataSource.dataType = lanes_[inputIndex.second]->laneDataSource_.dataType;
    }
    // input is not from a lane, we can begin processing
    else
    {
      // if this lane is turned off, we set it as finished and grab view to the original dataBuffer's data
      // and we don't actually read the data located there, hence why we don't lock
      if (isLaneOn)
      {
        // getting shared access to the state's transformed data
        COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);
        lockAtomic(dataBuffer_.getLock(), false, WaitMechanism::Spin);
      }

      laneDataSource.dataType = ComplexDataSource::Cartesian;
      if (inputIndex.first->id == Processors::EffectsLane::Input::Main::id().value())
        laneDataSource.sourceBuffer = SimdBufferView{ dataBuffer_, 0, kChannelsPerInOut };
      else if (inputIndex.first->id == Processors::EffectsLane::Input::Sidechain::id().value())
        laneDataSource.sourceBuffer = SimdBufferView{ dataBuffer_, (inputIndex.second + 1) * kChannelsPerInOut, kChannelsPerInOut };
      else COMPLEX_ASSERT_FALSE("Missing case");
      
      // finish if lane is off
      if (!isLaneOn)
      {
        thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
        return;
      }
    }

    simd_float inputVolume = 0.0f;
    simd_float loudnessScale = 1.0f / (float)binCount_;
    u32 isGainMatching = thisLane->getParameter(Processors::EffectsLane::GainMatching::id().value())->getInternalValue<u32>();

    auto getLoudness = [](const ComplexDataSource &laneDataSource, simd_float loudnessScale, u32 binCount)
    {
      simd_float loudness = 0.0f;
      // this is because we're squaring and then scaling inside the loops
      loudnessScale *= loudnessScale;
      auto data = laneDataSource.sourceBuffer.get();
      if (laneDataSource.dataType == ComplexDataSource::Cartesian)
      {
        for (u32 i = 0; i < binCount - 1; i += 2)
        {
          // magnitudes are: [left channel, right channel, left channel + 1, right channel + 1]
          simd_float values = complexMagnitude({ data[i], data[i + 1] }, false);
          // [left channel,     left channel + 1, right channel,     right channel + 1] + 
          // [left channel + 1, left channel,     right channel + 1, right channel    ]
          loudness += groupEven(values) + groupEvenReverse(values);
        }

        loudness += complexMagnitude(data[binCount - 1], false);
      }
      else if (laneDataSource.dataType == ComplexDataSource::Polar)
      {
        for (u32 i = 0; i < binCount - 1; ++i)
          loudness += data[i] * data[i];

        loudness = copyFromEven(loudness);
      }
      return loudness / loudnessScale;
    };

    if (isGainMatching)
    {
      inputVolume = getLoudness(laneDataSource, loudnessScale, binCount_);
      inputVolume = merge(inputVolume, simd_float(1.0f), simd_float::equal(inputVolume, 0.0f));
    }
    else
      thisLane->volumeScale_ = 1.0f;

    // main processing loop
    for (auto effectModule : thisLane->subProcessors_)
    {
      // this is safe because by design only EffectModules are contained in a lane
      as<EffectModule>(effectModule)->processEffect(laneDataSource, binCount_, sampleRate_);

      // incrementing where we are currently
      thisLane->currentEffectIndex_.fetch_add(1, std::memory_order_acq_rel);
    }

    if (isGainMatching)
    {
      simd_float outputVolume = getLoudness(laneDataSource, loudnessScale, binCount_);
      outputVolume = merge(outputVolume, simd_float(1.0f), simd_float::equal(outputVolume, 0.0f));
      simd_float scale = inputVolume / outputVolume;

      // some arbitrary limits taken from dtblkfx
      scale = merge(scale, simd_float{ 1.0f }, simd_float::greaterThan(scale, 1.0e30f));
      scale = merge(scale, simd_float{ 0.0f }, simd_float::lessThan(scale, 1.0e-30f));

      thisLane->volumeScale_ = simd_float::sqrt(scale);
    }

    // unlocking the last module's buffer
    if (laneDataSource.sourceBuffer != laneDataSource.conversionBuffer)
      unlockAtomic(laneDataSource.sourceBuffer.getLock(), false, WaitMechanism::Spin);

    COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);

    // to let other threads know that the data is in its final state
    thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
  }

  void EffectsState::sumLanesAndWriteOutput(float *outputBuffer, usize channels, usize samples) noexcept
  {
    using namespace Framework;
    using namespace utils;

    // TODO: redo when you get to multiple outputs
    // checks whether all of the chains are real-imaginary pairs
    // (instead of magnitude-phase pairs) and if not, gets converted
    for (auto &lane : lanes_)
    {
      ScopedLock g1{ lane->laneDataSource_.sourceBuffer.getLock(), false, WaitMechanism::Spin };

      if (lane->laneDataSource_.dataType == ComplexDataSource::Polar)
      {
        convertBuffer<complexPolarToCart>(
          lane->laneDataSource_.sourceBuffer, lane->laneDataSource_.conversionBuffer, binCount_);
        lane->laneDataSource_.sourceBuffer = lane->laneDataSource_.conversionBuffer;
        lane->laneDataSource_.dataType = ComplexDataSource::Cartesian;
      }
    }

    ScopedLock g{ outputBuffer_.getLock(), true, WaitMechanism::Spin };
    outputBuffer_.clear();

    // multipliers for scaling the multiple chains going into the same output
    std::ranges::fill(outputScaleMultipliers_, 0.0f);
    for (u32 i = 0; i < lanes_.size(); i++)
    {
      auto [outputOption, index] = lanes_[i]->getParameter(Processors::EffectsLane::Output::id().value())
        ->getInternalValue<std::string_view>();

      if (outputOption->id != Processors::EffectsLane::Output::None::id().value())
        outputScaleMultipliers_[index]++;
    }

    // for every lane we add its scaled output to the main sourceBuffer_ at the designated output channels
    for (auto &lane : lanes_)
    {
      auto [outputOption, index] = lane->getParameter(Processors::EffectsLane::Output::id().value())
        ->getInternalValue<std::string_view>();
      if (outputOption->id == Processors::EffectsLane::Output::None::id().value())
        continue;

      ScopedLock g1{ lane->laneDataSource_.sourceBuffer.getLock(), false, WaitMechanism::Spin };

      simd_float multiplier = simd_float::max(1.0f, outputScaleMultipliers_[index]);
      SimdBuffer<complex<float>, simd_float>::applyToThisNoMask<MathOperations::Add>(outputBuffer_,
        lane->laneDataSource_.sourceBuffer, kChannelsPerInOut, binCount_, index * kChannelsPerInOut, 0, 0, 0,
        lane->volumeScale_.get() / multiplier);
    }

    auto values = utils::array<simd_float, SimdBuffer<complex<float>, simd_float>::getRelativeSize()>{};
    auto data = outputBuffer_.get();
    usize size = outputBuffer_.getSize();
    for (usize i = 0; i < channels; i += values.size())
    {
      if (!usedOutputChannels_[i])
        continue;

      auto *channelBuffer = outputBuffer + i * samples;

      // skipping every n-th complex pair (currently simd_float can take 2 pairs)
      for (usize j = 0; j < binCount_ - 1; j += values.size())
      {
        for (usize k = 0; k < values.size(); ++k)
          values[k] = data[i * size + j + k];

        complexTranspose(values);

        // skipping every second sample (complex signal)
        for (usize k = 0; k < values.size(); k++)
          std::memcpy(channelBuffer + k * samples + j * 2, &values[k], sizeof(simd_float));
      }

      auto rest = data[i * size + binCount_ - 1].getArrayOfValues();
      for (u32 k = 0; k < values.size(); k += 2)
      {
        channelBuffer[k * samples + (binCount_ - 1) * 2] = rest[k];
        channelBuffer[k * samples + (binCount_ - 1) * 2 + 1] = rest[k + 1];
      }
    }
  }
}