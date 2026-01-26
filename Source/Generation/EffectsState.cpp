
// Created: 2021-10-02 20:53:05

#include "EffectsState.hpp"

#include "Framework/sync_primitives.hpp"
#include "Framework/circular_buffer.hpp"
#include "Framework/simd_math.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/parameter_value.hpp"
#include "Plugin/Complex.hpp"
#include "EffectModules.hpp"

namespace Generation
{
  EffectsLane::EffectsLane(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept :
    BaseProcessor{ state, metadata, arena }
  {
    auto maxInOuts = utils::max(state->plugin->inSidechains, state->plugin->outSidechains) + 1;

    laneDataSource_.scratchBuffer = Framework::SimdBuffer::create(arena, maxInOuts * kChannelsPerInOut, state->getMaxBinCount());
    dataBuffer = Framework::SimdBuffer::create(arena, maxInOuts * kChannelsPerInOut, state->getMaxBinCount());
  }

  bool
  EffectsLane::insertSubProcessor(usize index, BaseProcessor &newSubProcessor, [[maybe_unused]] bool callListeners)
  {
    if (newSubProcessor.metadata->id != Processors::EffectModule)
    {
      static constexpr uuid acceptedProcessorIds[] = { Processors::EffectModule };
      reportUnexpectedProcessorInsert(newSubProcessor, acceptedProcessorIds);

      return false;
    }

    insertProcessorAt(newSubProcessor, index);
    return true;
  }

  BaseProcessor &
  EffectsLane::deleteSubProcessor(usize index, [[maybe_unused]] bool callListeners) noexcept
  {
    COMPLEX_ASSERT(index < childrenCount);

    auto *child = getChild(children, index);
    child->previous->next = child->next;
    child->next->previous = child->previous;

    return *child;
  }

  void EffectsLane::initialiseParameters()
  {
    parameters = createParameters(metadata->parametersCount, metadata->parameters);
    parameterCount = metadata->parametersCount;
  }

  EffectsState::EffectsState(Plugin::State *state, Framework::ProcessorMetadata *metadata, utils::bumpArena *arena) noexcept :
    BaseProcessor{ state, metadata, arena }
  {
    auto inSidechains = state->plugin->inSidechains;
    auto outSidechains = state->plugin->outSidechains;

    usedInputChannels_ = { arranew(arena, bool,
      (inSidechains + 1) * kChannelsPerInOut, {}), (inSidechains + 1) * kChannelsPerInOut };

    usedOutputChannels_ = { arranew(arena, bool,
      (outSidechains + 1) * kChannelsPerInOut, {}), (outSidechains + 1) * kChannelsPerInOut };

    outputScaleMultipliers_ = { arranew(arena, float,
      (outSidechains + 1) * kChannelsPerInOut, {}), (outSidechains + 1) * kChannelsPerInOut };

    // size is half the max because a single SIMD package stores both real and imaginary parts

    dataBuffer = Framework::SimdBuffer::create(arena, (inSidechains + 1) * kChannelsPerInOut, state->getMaxBinCount());
    outputBuffer_ = Framework::SimdBuffer::create(arena, (outSidechains + 1) * kChannelsPerInOut, state->getMaxBinCount());
  }

  bool
  EffectsState::insertSubProcessor(usize index, BaseProcessor &newSubProcessor, [[maybe_unused]] bool callListeners)
  {
    if (newSubProcessor.metadata->id != Processors::EffectsLane)
    {
      static constexpr uuid acceptedProcessorIds[] = { Processors::EffectsLane };
      reportUnexpectedProcessorInsert(newSubProcessor, acceptedProcessorIds);

      return false;
    }

    newSubProcessor.parent = this;

    insertProcessorAt(newSubProcessor, index);
    return true;
  }

  BaseProcessor &EffectsState::deleteSubProcessor(usize index, [[maybe_unused]] bool callListeners)
  {
    COMPLEX_ASSERT(index < childrenCount);

    auto *child = children;
    for (usize i = 0; i < index; (++i), (child = child->next)) { }

    child->previous->next = child->next;
    child->next->previous = child->previous;

    return *child;
  }

  void EffectsState::initialiseParameters()
  {
    parameters = createParameters(metadata->parametersCount, metadata->parameters);
    parameterCount = metadata->parametersCount;
  }

  utils::span<bool> EffectsState::getUsedInputChannels() noexcept
  {
    using namespace Framework;

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
          startIndex = kChannelsPerInOut * (index + 1);
        else if (laneIndexedData->id == EffectsLane::InputOptionsLane)
          continue;
        else COMPLEX_ASSERT_FALSE("Missing case");

        for (usize i = 0; i < kChannelsPerInOut; ++i)
          usedInputChannels_[startIndex + i] = true;
      }
    }

    return usedInputChannels_;
  }

  utils::span<bool> EffectsState::getUsedOutputChannels() noexcept
  {
    using namespace Framework;

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
          startIndex = kChannelsPerInOut * (index + 1);
        else if (laneIndexedData->id == EffectsLane::OutputOptionsNone)
          continue;
        else COMPLEX_ASSERT_FALSE("Missing case");

        for (usize i = 0; i < kChannelsPerInOut; ++i)
          usedOutputChannels_[startIndex + i] = true;
      }
    }

    return usedOutputChannels_;
  }

  void EffectsState::writeInputData(const Framework::Buffer &inputBuffer) noexcept
  {
    using namespace Framework;
    using namespace utils;

    COMPLEX_ASSERT(dataBuffer->dataLock.lock.load() >= 0);
    ScopedLock g{ dataBuffer->dataLock, true, WaitMechanism::Spin };

    auto values = utils::array<simd_float, SimdBuffer::kRelativeSize>{};
    auto valueSources = utils::array<utils::ca<const float>, decltype(values)::size()>{};

    for (u32 i = 0; i < inputBuffer.channels; i += (u32)values.size())
    {
      // if the input is not used we skip it
      if (!usedInputChannels_[i])
        continue;

      for (u32 k = 0; k < valueSources.size(); ++k)
        valueSources[k] = inputBuffer.get((u32)(i + k)).offset(0, 2 * binCount);

      auto data = dataBuffer->get(i / (u32)valueSources.size());

      // skipping every n-th complex pair (currently simd_float can take 2 pairs)
      for (u32 j = 0; j < binCount - 1; j += (u32)values.size())
      {
        // skipping every second sample (complex signal)
        for (u32 k = 0; k < values.size(); ++k)
          values[k] = toSimdFloatFromUnaligned(&valueSources[k][j * 2]);

        complexTranspose(values);

        for (u32 k = 0; k < values.size(); ++k)
          data[j + k] = values[k];
      }

      simd_float::array_t nyquist{};
      for (u32 k = 0; k < valueSources.size(); ++k)
      {
        nyquist[2 * k] = valueSources[k][(binCount - 1) * 2];
        nyquist[2 * k + 1] = valueSources[k][(binCount - 1) * 2 + 1];
      }
      data[binCount - 1] = toSimdFloatFromUnaligned(nyquist.data());
    }
  }

  void EffectsState::processLanes() noexcept
  {
    shouldWorkersProcess_.store(true, satomi::memory_order_release);
    // sequential consistency just in case
    // triggers the chains to run again
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
      lane->status_.store(EffectsLane::LaneStatus::Ready, satomi::memory_order_seq_cst);

    distributeWork();

    // waiting for chains to finish
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
    {
      while (lane->status_.load(satomi::memory_order_acquire) != EffectsLane::LaneStatus::Finished)
      { utils::longPause<5>(); }
    }
  }

  void EffectsState::checkUsage()
  {
    // TODO: decide on a heuristic when to add worker threads
    if (false)
    {
      auto &worker = state->reserveFreeWorker(typeId(EffectsState));
      worker.start([this](satomi::atomic<bool> &shouldStop)
        {
          while (!shouldStop.load(satomi::memory_order_acquire))
          {
            while (!shouldWorkersProcess_.load(satomi::memory_order_acquire))
              utils::millisleep();

            distributeWork();
          }
        });
    }
  }

  void EffectsState::distributeWork() const noexcept
  {
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
    {
      if (lane->status_.load(satomi::memory_order_relaxed) == EffectsLane::LaneStatus::Ready)
      {
        auto expected = EffectsLane::LaneStatus::Ready;
        if (lane->status_.compare_exchange_strong(expected, EffectsLane::LaneStatus::Running, satomi::memory_order_seq_cst))
          processIndividualLanes(lane);
      }
    }
  }

  void EffectsState::processIndividualLanes(EffectsLane *thisLane) const noexcept
  {
    using namespace Framework;
    using namespace utils;

    thisLane->currentEffectIndex_.store(0, satomi::memory_order_release);
    thisLane->volumeScale_ = 1.0f;
    auto &laneDataSource = thisLane->laneDataSource_;
    laneDataSource.blockPhase = blockPhase;
    laneDataSource.blockPosition = blockPosition;
    bool isLaneOn = thisLane->getParameter(EffectsLane::LaneEnabled)->getInternalValue<u32>();
    i32 lockValue;

    // Lane Input
    // if this lane's input is another's output and that lane can be used,
    // we wait until it is finished and then copy its data
    if (auto inputIndex = thisLane->getParameter(EffectsLane::Input)->getInternalValue<Framework::IndexedData>();
      inputIndex.first->id == EffectsLane::InputOptionsLane)
    {
      auto *otherLane = (EffectsLane *)getChild(children, inputIndex.second, Processors::EffectsLane);
      COMPLEX_ASSERT(thisLane != otherLane, "A lane cannot use its own output as input");

      while (true)
      {
        auto otherLaneStatus = otherLane->status_.load(satomi::memory_order_acquire);
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
        laneDataSource.sourceBuffer = otherLane->laneDataSource_.sourceBuffer;

        thisLane->status_.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
        return;
      }

      lockValue = utils::lockAtomic(otherLane->laneDataSource_.sourceBuffer->dataLock, false, false, WaitMechanism::Spin);
      laneDataSource.sourceBuffer = otherLane->laneDataSource_.sourceBuffer;
    }
    // input is not from a lane, we can begin processing
    else
    {
      // if this lane is turned off, we set it as finished and grab view to the original dataBuffer's data
      // and we don't actually read the data located there, hence why we don't lock

      if (inputIndex.first->id == EffectsLane::InputOptionsMain)
      {
        laneDataSource.sourceBuffer = dataBuffer;
        laneDataSource.simdChannelOffset = 0;
      }
      else if (inputIndex.first->id == EffectsLane::InputOptionsSidechain)
      {
        laneDataSource.sourceBuffer = dataBuffer;
        laneDataSource.simdChannelOffset = (u32)inputIndex.second + 1;
      }
      else COMPLEX_ASSERT_FALSE("Missing case");

      if (isLaneOn)
      {
        // getting shared access to the state's transformed data
        COMPLEX_ASSERT(dataBuffer->dataLock.lock.load() >= 0);
        lockValue = lockAtomic(dataBuffer->dataLock, false, false, WaitMechanism::Spin);
      }
      else
      {
        thisLane->status_.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
        return;
      }
    }

    simd_float inputVolume = 0.0f;
    simd_float loudnessScale = 1.0f / (float)binCount;
    u32 isGainMatching = thisLane->getParameter(EffectsLane::GainMatching)->getInternalValue<u32>();

    auto getLoudness = [](const ComplexDataSource &laneDataSource, simd_float loudnessScale, u32 binCount)
    {
      simd_float loudness = 0.0f;
      // this is because we're squaring and then scaling inside the loops
      loudnessScale *= loudnessScale;
      auto data = laneDataSource.sourceBuffer->get(laneDataSource.simdChannelOffset);
      for (u32 i = 0; i < binCount - 1; i += 2)
      {
        // magnitudes are: [left channel, right channel, left channel + 1, right channel + 1]
        simd_float values = complexMagnitude({ data[i], data[i + 1] }, false);
        // [left channel,     left channel + 1, right channel,     right channel + 1] +
        // [left channel + 1, left channel,     right channel + 1, right channel    ]
        loudness += groupEven(values) + groupEvenReverse(values);
      }

      loudness += complexMagnitude(data[binCount - 1], false);
      return loudness / loudnessScale;
    };

    if (isGainMatching)
    {
      inputVolume = getLoudness(laneDataSource, loudnessScale, binCount);
      inputVolume = merge(inputVolume, simd_float(1.0f), simd_float::equal(inputVolume, 0.0f));
    }
    else
      thisLane->volumeScale_ = 1.0f;

    // main processing loop
    for (auto *child = thisLane->children; child; child = child->next)
    {
      // this is safe because by design only EffectModules are contained in a lane
      utils::as<EffectModule>(child)->processEffect(laneDataSource, binCount, sampleRate);

      // incrementing where we are currently
      thisLane->currentEffectIndex_.fetch_add(1, satomi::memory_order_acq_rel);
    }

    if (isGainMatching)
    {
      simd_float outputVolume = getLoudness(laneDataSource, loudnessScale, binCount);
      outputVolume = merge(outputVolume, simd_float(1.0f), simd_float::equal(outputVolume, 0.0f));
      simd_float scale = inputVolume / outputVolume;

      // some arbitrary limits taken from dtblkfx
      scale = merge(scale, simd_float{ 1.0f }, simd_float::greaterThan(scale, 1.0e30f));
      scale = merge(scale, simd_float{ 0.0f }, simd_float::lessThan(scale, 1.0e-30f));

      thisLane->volumeScale_ = simd_float::sqrt(scale);
    }

    // unlocking the last module's buffer
    utils::unlockAtomic(laneDataSource.sourceBuffer->dataLock, false, WaitMechanism::Spin, lockValue);

    COMPLEX_ASSERT(dataBuffer->dataLock.lock.load() >= 0);

    // to let other threads know that the data is in its final state
    thisLane->status_.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
  }

  void EffectsState::sumLanesAndWriteOutput(Framework::Buffer &out) noexcept
  {
    using namespace Framework;
    using namespace utils;

    // TODO: redo when you get to multiple outputs

    ScopedLock g{ outputBuffer_->dataLock, true, WaitMechanism::Spin };
    outputBuffer_->clear();

    // multipliers for scaling the multiple chains going into the same output
    ::zeroset(outputScaleMultipliers_.data(), outputScaleMultipliers_.size());

    for (auto *lane = getChild(children, 0, Processors::EffectsLane); lane;
      lane = getChild(lane, 1, Processors::EffectsLane))
    {
      auto [outputOption, index] = lane->getParameter(EffectsLane::Output)
        ->getInternalValue<Framework::IndexedData>();

      if (outputOption->id != EffectsLane::OutputOptionsNone)
        outputScaleMultipliers_[index]++;
    }

    // for every lane we add its scaled output to the main sourceBuffer_ at the designated output channels
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
    {
      auto [outputOption, index] = lane->getParameter(EffectsLane::Output)
        ->getInternalValue<Framework::IndexedData>();
      if (outputOption->id == EffectsLane::OutputOptionsNone)
        continue;

      ScopedLock g1{ lane->laneDataSource_.sourceBuffer->dataLock, false, WaitMechanism::Spin };

      simd_float multiplier = simd_float::max(1.0f, outputScaleMultipliers_[index]);
      Framework::applyToThisNoMask<MathOperations::Add>(outputBuffer_,
        lane->laneDataSource_.sourceBuffer, kChannelsPerInOut, binCount, (u32)index * kChannelsPerInOut, 0, 0, 0,
        lane->volumeScale_.get() / multiplier);
    }

    auto values = utils::array<simd_float, SimdBuffer::kRelativeSize>{};
    auto valueDestinations = utils::array<utils::ca<float>, decltype(values)::size()>{};

    for (u32 i = 0; i < out.channels; i += (u32)values.size())
    {
      if (!usedOutputChannels_[i])
        continue;

      auto data = outputBuffer_->get(i / (u32)valueDestinations.size());

      for (u32 k = 0; k < valueDestinations.size(); ++k)
        valueDestinations[k] = out.get((u32)(i + k)).offset(0, 2 * binCount);

      // skipping every n-th complex pair (currently simd_float can take 2 pairs)
      for (u32 j = 0; j < binCount - 1; j += (u32)values.size())
      {
        for (u32 k = 0; k < values.size(); ++k)
          values[k] = data[j + k];

        complexTranspose(values);

        // skipping every second sample (complex signal)
        for (u32 k = 0; k < values.size(); k++)
          ::memcpy(&valueDestinations[k][j * 2], &values[k], sizeof(simd_float));
      }

      auto rest = data[binCount - 1].getArrayOfValues();
      for (u32 k = 0; k < valueDestinations.size(); ++k)
      {
        valueDestinations[k][(binCount - 1) * 2] = rest[2 * k];
        valueDestinations[k][(binCount - 1) * 2 + 1] = rest[2 * k + 1];
      }
    }
  }
}

template<> Generation::EffectsLane *
createProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *copy)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(1));
  if (copy)
    return anew(state->processorStorage, Generation::EffectsLane, { *(const Generation::EffectsLane *)copy, arena });
  else
    return anew(state->processorStorage, Generation::EffectsLane, { state, metadata, arena });
}
template<> void *
initialiseTypeStructure<Generation::EffectsLane>(void *, Framework::PluginStructure &structure)
{
  using namespace Framework;
  using namespace Generation;

  auto arena = structure.getNewArena(COMPLEX_KB(2));

  ProcessorMetadata &effectsLane = COMPLEX_STRUCTURE_PROCESSOR(Generation::EffectsLane, "Effects Lane", Processors::EffectsLane);
  effectsLane.parameters =
  (
    COMPLEX_STRUCTURE_PARAMETER("Lane Enabled", EffectsLane::LaneEnabled, 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, {},
      ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess, Framework::printToggleValues),
    COMPLEX_STRUCTURE_PARAMETER("Input", EffectsLane::Input,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main", .id = EffectsLane::InputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::InputOptionsSidechain,
            .dynamicUpdateUuid = ParameterChangeReason::inputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lane", .id = EffectsLane::InputOptionsLane,
            .dynamicUpdateUuid = ParameterChangeReason::laneCount)),
        .defaultOptionId = EffectsLane::InputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Output", EffectsLane::Output,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA().addChildren(
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main", .id = EffectsLane::OutputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::OutputOptionsSidechain,
            .dynamicUpdateUuid = ParameterChangeReason::outputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "None", .id = EffectsLane::OutputOptionsNone)),
        .defaultOptionId = EffectsLane::OutputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Gain Matching", EffectsLane::GainMatching, 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, {},
      ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess, Framework::printToggleValues)
  );

  effectsLane.children = (ProcessorMetadata *)initialiseTypeStructure<EffectModule>(&effectsLane, structure);

  return &effectsLane.computeCounts();
}

template<> Generation::EffectsState *
createProcessor(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(1));
  return anew(arena, Generation::EffectsState, { state, metadata, arena });
}
template<> void *
initialiseTypeStructure<Generation::EffectsState>(void *, Framework::PluginStructure &structure)
{
  using namespace Framework;
  using namespace Generation;

  auto arena = structure.getNewArena(256);

  ProcessorMetadata &effectsState = COMPLEX_STRUCTURE_PROCESSOR(EffectsState, "Effects State", Processors::EffectsState);
  effectsState.children = (ProcessorMetadata *)initialiseTypeStructure<EffectsLane>(&effectsState, structure);

  return &effectsState.computeCounts();
}
