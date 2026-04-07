
// Created: 2021-10-02 20:53:05

#include "Effects.hpp"

#include "Framework/circular_buffer.hpp"
#include "Framework/simd_math.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/parameter_value.hpp"
#include "Plugin/Complex.hpp"
#include "SoundEngine.hpp"

namespace Generation
{
  EffectsLane::EffectsLane(utils::bumpArena *arena, Plugin::State *state, Framework::ProcessorMetadata *metadata,
    const EffectsLane *other, void *serialisedSave) : Processor{ arena, state, metadata, other }
  {
    if (other)
      return;

    auto maxInOutChannels = utils::kChannelsPerInOut * (utils::max(state->plugin->inSidechains, state->plugin->outSidechains) + 1);
    auto maxBinCount = state->getMaxBinCount();
    
    laneDataSource.scratchBuffer = Framework::SimdBuffer::create(arena, maxInOutChannels, maxBinCount);
    dataBuffer = Framework::SimdBuffer::create(arena, maxInOutChannels, maxBinCount);

    if (serialisedSave)
      deserialiseFromJson(serialisedSave);
    else
    {
      parameters = createParameters(metadata->parametersCount, metadata->parameters);
      parameterCount = metadata->parametersCount;
    }
  }

  void SoundEngine::interleaveInputs(const Framework::Buffer &inputBuffer)
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

  void SoundEngine::processLanes()
  {
    shouldWorkersProcess_.store(true, satomi::memory_order_release);
    // sequential consistency just in case
    // triggers the chains to run again
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
      lane->status.store(EffectsLane::LaneStatus::Ready, satomi::memory_order_seq_cst);

    distributeWork();

    // waiting for chains to finish
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
    {
      while (lane->status.load(satomi::memory_order_acquire) != EffectsLane::LaneStatus::Finished)
      { utils::longPause<5>(); }
    }
  }

  void SoundEngine::checkUsage()
  {
    // TODO: decide on a heuristic when to add worker threads
    if (false)
    {
      auto &worker = state->reserveFreeWorker(typeId(SoundEngine));
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

  void SoundEngine::distributeWork() const
  {
    for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
      lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
    {
      if (lane->status.load(satomi::memory_order_relaxed) == EffectsLane::LaneStatus::Ready)
      {
        auto expected = EffectsLane::LaneStatus::Ready;
        if (lane->status.compare_exchange_strong(expected, EffectsLane::LaneStatus::Running, satomi::memory_order_seq_cst))
          processIndividualLanes(lane);
      }
    }
  }

  void SoundEngine::processIndividualLanes(EffectsLane *thisLane) const
  {
    using namespace Framework;
    using namespace utils;

    thisLane->currentEffectIndex.store(0, satomi::memory_order_release);
    thisLane->volumeScale.store(1.0f, satomi::memory_order_relaxed);
    auto &laneDataSource = thisLane->laneDataSource;
    laneDataSource.blockPhase = blockPhase;
    laneDataSource.blockPosition = blockPosition_;
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
        auto otherLaneStatus = otherLane->status.load(satomi::memory_order_acquire);
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
        laneDataSource.sourceBuffer = otherLane->laneDataSource.sourceBuffer;

        thisLane->status.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
        return;
      }

      lockValue = utils::lockAtomic(otherLane->laneDataSource.sourceBuffer->dataLock, false, false, WaitMechanism::Spin);
      laneDataSource.sourceBuffer = otherLane->laneDataSource.sourceBuffer;
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
        thisLane->status.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
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
      thisLane->volumeScale.store(1.0f, satomi::memory_order_relaxed);

    // main processing loop
    for (auto *child = thisLane->children; child; child = child->next)
    {
      // this is safe because by design only EffectModules are contained in a lane
      utils::as<EffectModule>(child)->processEffect(laneDataSource, binCount, sampleRate);

      // incrementing where we are currently
      thisLane->currentEffectIndex.fetch_add(1, satomi::memory_order_acq_rel);
    }

    if (isGainMatching)
    {
      simd_float outputVolume = getLoudness(laneDataSource, loudnessScale, binCount);
      outputVolume = merge(outputVolume, simd_float(1.0f), simd_float::equal(outputVolume, 0.0f));
      simd_float scale = inputVolume / outputVolume;

      // some arbitrary limits taken from dtblkfx
      scale = merge(scale, simd_float{ 1.0f }, simd_float::greaterThan(scale, 1.0e30f));
      scale = merge(scale, simd_float{ 0.0f }, simd_float::lessThan(scale, 1.0e-30f));

      thisLane->volumeScale.store(simd_float::sqrt(scale), satomi::memory_order_relaxed);
    }

    // unlocking the last module's buffer
    utils::unlockAtomic(laneDataSource.sourceBuffer->dataLock, false, WaitMechanism::Spin, lockValue);

    COMPLEX_ASSERT(dataBuffer->dataLock.lock.load() >= 0);

    // to let other threads know that the data is in its final state
    thisLane->status.store(EffectsLane::LaneStatus::Finished, satomi::memory_order_release);
  }

  void SoundEngine::sumLanesAndDeinterleaveOutputs(Framework::Buffer &out)
  {
    using namespace Framework;
    using namespace utils;

    // TODO: redo when you get to multiple outputs

    ScopedLock g{ interleavedOutputBuffer->dataLock, true, WaitMechanism::Spin };
    interleavedOutputBuffer->clear();

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

      ScopedLock g1{ lane->laneDataSource.sourceBuffer->dataLock, false, WaitMechanism::Spin };

      simd_float multiplier = simd_float::max(1.0f, outputScaleMultipliers_[index]);
      Framework::applyToThisNoMask<MathOperations::Add>(interleavedOutputBuffer,
        lane->laneDataSource.sourceBuffer, kChannelsPerInOut, binCount, (u32)index * kChannelsPerInOut, 0, 0, 0,
        lane->volumeScale.load(satomi::memory_order_relaxed) / multiplier);
    }

    auto values = utils::array<simd_float, SimdBuffer::kRelativeSize>{};
    auto valueDestinations = utils::array<utils::ca<float>, decltype(values)::size()>{};

    for (u32 i = 0; i < out.channels; i += (u32)values.size())
    {
      if (!usedOutputChannels_[i])
        continue;

      auto data = interleavedOutputBuffer->get(i / (u32)valueDestinations.size());

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

template<> Generation::Processor *
createProcessor<Generation::EffectsLane>(Plugin::State *state, Framework::ProcessorMetadata *metadata, const void *toCopy, void *serialisedSave)
{
  auto *arena = utils::bumpArena::createNested(state->processorStorage, COMPLEX_MB(1));
  return anew(state->processorStorage, Generation::EffectsLane, { arena, state, metadata, 
    (const Generation::EffectsLane *)toCopy, serialisedSave });
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
        .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main Input", .id = EffectsLane::InputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::InputOptionsSidechain,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::inputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lanes", .id = EffectsLane::InputOptionsLane,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::laneSources) }}),
        .defaultOptionId = EffectsLane::InputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Output", EffectsLane::Output,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main Output", .id = EffectsLane::OutputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::OutputOptionsSidechain,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::outputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "None", .id = EffectsLane::OutputOptionsNone) }}),
        .defaultOptionId = EffectsLane::OutputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Gain Matching", EffectsLane::GainMatching, 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, {},
      ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess, Framework::printToggleValues)
  );

  effectsLane.children = (ProcessorMetadata *)initialiseTypeStructure<EffectModule>(&effectsLane, structure);

  return &effectsLane.computeCounts();
}
