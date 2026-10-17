
// Created: 2021-10-02 20:53:05

#include "Effects.hpp"

#include "Third Party/cjson/cjson.h"

#include "Framework/buffer.hpp"
#include "Framework/simd_math.hpp"
#include "Framework/simd_utils.hpp"
#include "Framework/parameters.hpp"
#include "Plugin/Complex.hpp"
#include "SoundEngine.hpp"
#include "Interface/Skin.hpp"
#include "Interface/EffectModuleSection.hpp"

static EffectData *
createEffect(ProcessorMetadata *processorMetadata, EffectModule *module,
  EffectData *copy = nullptr, void *jsonData = nullptr)
{
  if (copy)
    processorMetadata = copy->metadata;

  auto *vtable = (EffectVtable *)processorMetadata->vtable;

  COMPLEX_ASSERT(processorMetadata->parameters);
  COMPLEX_ASSERT(vtable->createEffect);
  COMPLEX_ASSERT(vtable->runEffect);
  COMPLEX_ASSERT(vtable->createUI);

  EffectData *effectData = vtable->createEffect(module, copy);
  effectData->metadata = processorMetadata;

  Parameter *effectParameters{};
  u32 parameterCount;
  if (copy)
  {
    parameterCount = copy->parameterCount;
    effectParameters = createAndInitialiseParameters(
      module, parameterCount, processorMetadata->parameters, copy->parameters);
  }
  else if (jsonData)
  {
    parameterCount = processorMetadata->parameterCount;
    deserialiseParametersFromJson(jsonData, processorMetadata, effectParameters, module, false);
  }
  else
  {
    parameterCount = processorMetadata->parameterCount;
    effectParameters = createAndInitialiseParameters(
      module, parameterCount, processorMetadata->parameters, nullptr);
  }

  effectData->parameters = effectParameters;
  effectData->parameterCount = parameterCount;

  return effectData;
}

static void automationMappingChangedEffectModule(Processor *processor, Parameter *parameter, bool isMapping)
{
  auto *module = (EffectModule *)processor;
  bool isFromModule = utils::findIf(processor->metadata->parameters, 
    [&](const ParameterMetadata *item) { return item->details.id == parameter->details.id; });
  if (isFromModule)
    return;
  
  usize i = 0;
  // because if the half-connected dll property, we can findthe index of the parameter 
  for (auto firstParameter = parameter; firstParameter->previous->next;
    (firstParameter = firstParameter->previous), (++i)) { }

  utils::ScopedLock g{ module->lock, utils::WaitMechanism::Spin };

  if (!isMapping)
    module->mappedParameters.erase_ordered({ i, parameter });
  else
    module->mappedParameters.add_ordered(i, parameter);
}

static Processor *
createEffectModule(State *state, ProcessorMetadata *metadata,
  const void *copy, void *serialisedSave, void *extra)
{
  auto *pool = utils::Pool::createNested(state->processorStorage, COMPLEX_MB(1));
  auto *module = anew(state->processorStorage, EffectModule, { pool, state, metadata });

  auto *other = (EffectModule *)copy;
  module->dataBuffer = module->createBuffer((other) ? other->dataBuffer : nullptr);
  module->mappedParameters.data = { pool };
  if (serialisedSave)
  {
    module->deserialiseFromJson(serialisedSave);

    auto *effectData = cjson_GetObjectItem((cjson *)serialisedSave, "effect");
    auto moduleTypeParameter = module->getParameter(EffectModule::ModuleType);
    auto [option, index] = getOptionFromValue(scaleValue(moduleTypeParameter->normalisedValue.load(satomi::memory_order_relaxed), 
      moduleTypeParameter->details), moduleTypeParameter->details);
    COMPLEX_ASSERT(option->processorMetadata->id == cjson_GetObjectItem(effectData, "id")->vuint);

    module->effects.store(createEffect(option->processorMetadata, module, nullptr, effectData), satomi::memory_order_release);

    initialiseParameters(module->parameters, module->parameterCount, nullptr, true);
  }
  else
  {
    module->parameters = createParameters(module,
      (other) ? other->parameterCount : metadata->parameterCount,
      metadata->parameters, (other) ? other->parameters : nullptr);
    module->parameterCount = metadata->parameterCount;

    if (other)
    {
      initialiseParameters(module->parameters, other->parameterCount, other->parameters);
    }
    else
    {
      // skip the module type parameter so we can initialise it ourselves
      COMPLEX_ASSERT(module->parameters->details.id == EffectModule::ModuleType);
      initialiseParameters(module->parameters->next, module->parameterCount - 1);

      COMPLEX_ASSERT(extra);
      auto option = (IndexedData *)extra;
      auto *moduleTypeParameter = module->getParameter(EffectModule::ModuleType);
      auto normalisedValue = unscaleValue(getValueFromOptionId(option->id,
        moduleTypeParameter->details), moduleTypeParameter->details);
      (void)moduleTypeParameter->updateNormalisedValue(normalisedValue, ParameterAssignment::MainThread);
    }
  }

  return module;
}

static void serialiseEffectModule(const Processor *processor, void *jsonData, utils::span<Parameter *>)
{
  auto *effectModule = (EffectModule *)processor;
  auto *effect = effectModule->currentEffect.load(satomi::memory_order_acquire);

  serialiseProcessorGeneric(processor, jsonData, {});

  cjson *data = (cjson *)jsonData;
  cjson *effectData = cjson_AddTo(data, "effect", cjson_Object);
  cjson_AddTo(effectData, "id", cjson_Unsigned, effect->metadata->id);
  serialiseParameters(effectData, effect->parameters, effect->parameterCount);
}

static void resetEffectModule(Processor *processor)
{
  for (auto *parameter = processor->parameters; parameter; parameter = parameter->next)
    parameter->reset();
  for (auto *parameter = ((EffectModule *)processor)->currentEffect.load(satomi::memory_order_acquire)->parameters;
    parameter; parameter = parameter->next)
    parameter->reset();
}


static EffectData *
changeEffect(EffectModule *module, const IndexedData *effectOption, ParameterAssignment assignmentType)
{
restart:

  // reason for this lock is because both audio-thread and main-thread can call this function
  // and the process of changing effects leaves things temporarily in an invalid state
  // we need to prevent that from happening, but the function itself is bounded in time
  // so spinning for a little shouldn't be too bad in the rare cases it's necessary
  utils::ScopedLock g{ module->lock, utils::WaitMechanism::Spin };

  auto *activeEffect = module->currentEffect.load(satomi::memory_order_acquire);
  if (activeEffect && activeEffect->metadata->id == effectOption->processorMetadata->id)
    return activeEffect;

  // was effect already created?
  auto *effect = module->effects.load(satomi::memory_order_acquire);
  for (; effect; effect = effect->next)
    if (effect->metadata->id == effectOption->processorMetadata->id)
      break;

  if (!effect)
  {
    // skip creation and leave it to the main-thread to handle
    if (assignmentType == ParameterAssignment::AudioThread)
      return nullptr;

    // unlock while we create the effect
    // so that if the audio thread comes along it can proceed in bounded time
    g.~ScopedLock();

    effect = createEffect(effectOption->processorMetadata, module);
    effect->next = module->effects.load(satomi::memory_order_acquire);
    module->effects.store(effect, satomi::memory_order_release);

    // now that the effect is present in the list, we need to restart from the beginning
    // we can technically call this function again, but i think goto has better readability here
    // also gives you enough of a shock to keep you on your toes unlike possibly unbounded recursion
    goto restart;
  }

  // remap host parameters to the new effect
  usize i = 0;
  auto *replacementParameter = effect->parameters;
  for (auto &[index, parameter] : module->mappedParameters.data)
  {
    if (index >= effect->parameterCount)
      continue;

    COMPLEX_DEBUG_LOG("Replacing %v with %v at index %zu\n",
      parameter->details.displayName, replacementParameter->details.displayName, index);

    for (; i < index; (replacementParameter = replacementParameter->next), (++i)) { }
    parameter->hostParameter.load(satomi::memory_order_relaxed)->resetParameter(replacementParameter, false, assignmentType);
    parameter = replacementParameter;
  }

  module->currentEffect.store(effect, satomi::memory_order_release);

  return effect;
}

void EffectModule::process(ComplexDataSource &source, u32 binCount, float sampleRate)
{
  using namespace utils;

  if (!getParameter(ModuleEnabled)->getInternalValue<u32>(sampleRate))
    return;

  auto *effect = currentEffect.load(satomi::memory_order_acquire);

  // getting exclusive access to data
  lockAtomic(dataBuffer->dataLock, false, true, WaitMechanism::Spin);

  ((EffectVtable *)effect->metadata->vtable)->runEffect(this, effect, source, dataBuffer, binCount, sampleRate);

  // if the mix is 100% for all channels, we can skip mixing entirely
  f4 wetMix = getParameter(ModuleMix)->getInternalValue<f4>(sampleRate);
  if (!f4::allEqual(wetMix, 1.0f))
  {
    auto sourceData = source.sourceBuffer->get();
    auto destinationData = dataBuffer->get();
    f4 dryMix = 1.0f - wetMix;
    for (u32 i = 0; i < binCount; i++)
      destinationData[i] = f4::mulAdd(dryMix * sourceData[i], wetMix, destinationData[i]);
  }

  // switching to being a reader and allowing other readers to participate
  // seq_cst because the following atomic could be reordered to happen prior to this one
  dataBuffer->dataLock.lock.store(1, satomi::memory_order_release);
  source.sourceBuffer->dataLock.lock.fetch_sub(1, satomi::memory_order_acq_rel);

  source.sourceBuffer = dataBuffer;
}

static Processor *
createEffectsLane(State *state, ProcessorMetadata *metadata,
  const void *copy, void *serialisedSave, void *)
{
  auto *pool = utils::Pool::createNested(state->processorStorage, COMPLEX_MB(1));
  auto *lane = anew(state->processorStorage, EffectsLane, { pool, state, metadata });

  if (auto *other = (EffectsLane *)copy)
  {
    lane->parameters = createAndInitialiseParameters(
      lane, other->parameterCount, metadata->parameters, other->parameters);
    lane->parameterCount = other->parameterCount;
    lane->dataBuffer = lane->createBuffer(other->dataBuffer);
    lane->createChildren(*other);
  }
  else
  {
    auto maxInOutChannels = kChannelsPerInOut * (utils::max(state->plugin->inSidechains, state->plugin->outSidechains) + 1);
    auto maxBinCount = state->getMaxBinCount();

    lane->laneDataSource.scratchBuffer = SimdBuffer::create(pool, maxInOutChannels, maxBinCount);
    lane->dataBuffer = SimdBuffer::create(pool, maxInOutChannels, maxBinCount);

    if (serialisedSave)
    {
      lane->deserialiseFromJson(serialisedSave);
      initialiseParameters(lane->parameters, lane->parameterCount, nullptr, true);
    }
    else
    {
      lane->parameters = createAndInitialiseParameters(
        lane, metadata->parameterCount, metadata->parameters);
      lane->parameterCount = metadata->parameterCount;
    }
  }

  return lane;
}

void resetEffectsLane(Processor *processor)
{
  resetProcessorGeneric(processor);
  auto *lane = (EffectsLane *)processor;
  lane->status.store(EffectsLane::Status::Finished, satomi::memory_order_release);
  lane->currentEffectIndex.store(0, satomi::memory_order_release);
}

void SoundEngine::interleaveInputs(const Buffer &inputBuffer)
{
  using namespace utils;

  COMPLEX_ASSERT(interleavedInputBuffer->dataLock.lock.load() >= 0);
  ScopedLock g{ interleavedInputBuffer->dataLock, true, WaitMechanism::Spin };

  auto values = utils::array<f4, SimdBuffer::kRelativeSize>{};
  auto valueSources = utils::array<utils::ca<const float>, decltype(values)::size()>{};

  for (u32 i = 0; i < inputBuffer.channels; i += (u32)values.size())
  {
    // if the input is not used we skip it
    if (!usedInputChannels_[i])
      continue;

    for (u32 k = 0; k < valueSources.size(); ++k)
      valueSources[k] = inputBuffer.get((u32)(i + k)).offset(0, 2 * binCount);

    auto data = interleavedInputBuffer->get(i / (u32)valueSources.size());

    // skipping every n-th complex pair (currently f4 can take 2 pairs)
    for (u32 j = 0; j < binCount - 1; j += (u32)values.size())
    {
      // skipping every second sample (complex signal)
      for (u32 k = 0; k < values.size(); ++k)
        values[k] = toSimdFloatFromUnaligned(&valueSources[k][j * 2]);

      complexTranspose(values);

      for (u32 k = 0; k < values.size(); ++k)
        data[j + k] = values[k];
    }

    f4::array_t nyquist{};
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
    lane->status.store(EffectsLane::Status::Ready, satomi::memory_order_seq_cst);

  distributeWork();

  // waiting for chains to finish
  for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
    lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
  {
    while (lane->status.load(satomi::memory_order_acquire) != EffectsLane::Status::Finished)
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
    if (lane->status.load(satomi::memory_order_relaxed) == EffectsLane::Status::Ready)
    {
      auto expected = EffectsLane::Status::Ready;
      if (lane->status.compare_exchange_strong(expected, EffectsLane::Status::Running, satomi::memory_order_seq_cst))
        processIndividualLanes(lane);
    }
  }
}

void SoundEngine::processIndividualLanes(EffectsLane *thisLane) const
{
  using namespace utils;

  thisLane->currentEffectIndex.store(0, satomi::memory_order_release);
  thisLane->volumeScale.store(1.0f, satomi::memory_order_relaxed);
  auto &laneDataSource = thisLane->laneDataSource;
  laneDataSource.blockPhase = blockPhase;
  laneDataSource.blockPosition = blockPosition_;
  bool isLaneOn = thisLane->getParameter(EffectsLane::LaneEnabled)->getInternalValue<u32>();

  // Lane Input
  // if this lane's input is another's output and that lane can be used,
  // we wait until it is finished and then copy its data
  if (auto inputIndex = thisLane->getParameter(EffectsLane::Input)->getInternalValue<IndexedData>();
    inputIndex.first->parent && inputIndex.first->parent->id == EffectsLane::InputOptionsLane)
  {
    auto *otherLane = (EffectsLane *)state->getProcessor(inputIndex.first->stateId);
    COMPLEX_ASSERT(thisLane != otherLane, "A lane cannot use its own output as input");

    while (true)
    {
      auto otherLaneStatus = otherLane->status.load(satomi::memory_order_acquire);
      if (otherLaneStatus == EffectsLane::Status::Finished)
        break;

      // process dependent lane if no one else has taken up the work
      if (otherLaneStatus == EffectsLane::Status::Ready &&
        otherLane->status.compare_exchange_strong(otherLaneStatus,
          EffectsLane::Status::Running, satomi::memory_order_seq_cst))
      {
        processIndividualLanes(otherLane);
        continue;
      }

      longPause<40>();
    }

    // getting shared access to the lane's output
    // if this lane is turned off, we only grab the view from the other lane's buffer
    // since we won't be reading from it but it's necessary for the outputBuffer to know where the data is
    if (!isLaneOn)
    {
      // TODO: decide if it's even worth to wait, or we can grab a reference to the other lane's SimdBufferView
      laneDataSource.sourceBuffer = otherLane->laneDataSource.sourceBuffer;

      thisLane->status.store(EffectsLane::Status::Finished, satomi::memory_order_release);
      return;
    }

    // getting shared access to the lane's output data
    (void)utils::lockAtomic(otherLane->laneDataSource.sourceBuffer->dataLock, false, false, WaitMechanism::Spin);
    laneDataSource.sourceBuffer = otherLane->laneDataSource.sourceBuffer;
  }
  // input is not from a lane, we can begin processing
  else
  {
    // if this lane is turned off, we set it as finished and grab view to the original dataBuffer's data
    // and we don't actually read the data located there, hence why we don't lock

    if (inputIndex.first->id == EffectsLane::InputOptionsMain)
    {
      laneDataSource.sourceBuffer = interleavedInputBuffer;
      laneDataSource.simdChannelOffset = 0;
    }
    else if (inputIndex.first->id == EffectsLane::InputOptionsSidechain)
    {
      laneDataSource.sourceBuffer = interleavedInputBuffer;
      laneDataSource.simdChannelOffset = (u32)inputIndex.second + 1;
    }
    else COMPLEX_ASSERT_FALSE("Missing case");

    if (isLaneOn)
    {
      // getting shared access to the state's transformed data
      COMPLEX_ASSERT(interleavedInputBuffer->dataLock.lock.load() >= 0);
      (void)lockAtomic(interleavedInputBuffer->dataLock, false, false, WaitMechanism::Spin);
    }
    else
    {
      thisLane->status.store(EffectsLane::Status::Finished, satomi::memory_order_release);
      return;
    }
  }

  f4 inputVolume = 0.0f;
  f4 loudnessScale = 1.0f / (float)binCount;
  u32 isGainMatching = thisLane->getParameter(EffectsLane::GainMatching)->getInternalValue<u32>();

  auto getLoudness = [](const ComplexDataSource &laneDataSource, f4 loudnessScale, u32 binCount)
  {
    f4 loudness = 0.0f;
    // this is because we're squaring and then scaling inside the loops
    loudnessScale *= loudnessScale;
    auto data = laneDataSource.sourceBuffer->get(laneDataSource.simdChannelOffset);
    for (u32 i = 0; i < binCount - 1; i += 2)
    {
      // magnitudes are: [left channel, right channel, left channel + 1, right channel + 1]
      f4 values = complexMagnitude({ data[i], data[i + 1] }, false);
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
    inputVolume = merge(inputVolume, f4(1.0f), f4::equal(inputVolume, 0.0f));
  }
  else
    thisLane->volumeScale.store(1.0f, satomi::memory_order_relaxed);

  // main processing loop
  for (auto *child = thisLane->children; child; child = child->next)
  {
    // this is safe because by design only EffectModules are contained in a lane
    utils::as<EffectModule>(child)->process(laneDataSource, binCount, sampleRate);

    // incrementing where we are currently
    thisLane->currentEffectIndex.fetch_add(1, satomi::memory_order_acq_rel);
  }

  if (isGainMatching)
  {
    f4 outputVolume = getLoudness(laneDataSource, loudnessScale, binCount);
    outputVolume = merge(outputVolume, f4(1.0f), f4::equal(outputVolume, 0.0f));
    f4 scale = inputVolume / outputVolume;

    // some arbitrary limits taken from dtblkfx
    scale = merge(scale, f4{ 1.0f }, f4::greaterThan(scale, 1.0e30f));
    scale = merge(scale, f4{ 0.0f }, f4::lessThan(scale, 1.0e-30f));

    thisLane->volumeScale.store(f4::sqrt(scale), satomi::memory_order_relaxed);
  }

  // unlocking the final output of the lane until we need it again (be it a module or the input itself if the lane is empty)
  laneDataSource.sourceBuffer->dataLock.lock.store(0, satomi::memory_order_seq_cst);

  COMPLEX_ASSERT(interleavedInputBuffer->dataLock.lock.load() >= 0);

  // to let other threads know that the data is in its final state
  thisLane->status.store(EffectsLane::Status::Finished, satomi::memory_order_release);
}

void SoundEngine::sumLanesAndDeinterleaveOutputs(Buffer &out)
{
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
      ->getInternalValue<IndexedData>();

    if (outputOption->id != EffectsLane::OutputOptionsNone)
      outputScaleMultipliers_[index]++;
  }

  // for every lane we add its scaled output to the main sourceBuffer_ at the designated output channels
  for (auto *lane = (EffectsLane *)getChild(children, 0, Processors::EffectsLane); lane;
    lane = (EffectsLane *)getChild(lane, 1, Processors::EffectsLane))
  {
    auto [outputOption, index] = lane->getParameter(EffectsLane::Output)
      ->getInternalValue<IndexedData>();
    if (outputOption->id == EffectsLane::OutputOptionsNone)
      continue;

    ScopedLock g1{ lane->laneDataSource.sourceBuffer->dataLock, false, WaitMechanism::Spin };

    f4 multiplier = f4::max(1.0f, outputScaleMultipliers_[index]);
    applyToThisNoMask<MathOperations::Add>(interleavedOutputBuffer,
      lane->laneDataSource.sourceBuffer, kChannelsPerInOut, binCount, (u32)index * kChannelsPerInOut, 0, 0, 0,
      lane->volumeScale.load(satomi::memory_order_relaxed) / multiplier);
  }

  auto values = utils::array<f4, SimdBuffer::kRelativeSize>{};
  auto valueDestinations = utils::array<utils::ca<float>, decltype(values)::size()>{};

  for (u32 i = 0; i < out.channels; i += (u32)values.size())
  {
    if (!usedOutputChannels_[i])
      continue;

    auto data = interleavedOutputBuffer->get(i / (u32)valueDestinations.size());

    for (u32 k = 0; k < valueDestinations.size(); ++k)
      valueDestinations[k] = out.get((u32)(i + k)).offset(0, 2 * binCount);

    // skipping every n-th complex pair (currently f4 can take 2 pairs)
    for (u32 j = 0; j < binCount - 1; j += (u32)values.size())
    {
      for (u32 k = 0; k < values.size(); ++k)
        values[k] = data[j + k];

      complexTranspose(values);

      // skipping every second sample (complex signal)
      for (u32 k = 0; k < values.size(); k++)
        ::memcpy(&valueDestinations[k][j * 2], &values[k], sizeof(f4));
    }

    auto rest = data[binCount - 1].getArrayOfValues();
    for (u32 k = 0; k < valueDestinations.size(); ++k)
    {
      valueDestinations[k][(binCount - 1) * 2] = rest[2 * k];
      valueDestinations[k][(binCount - 1) * 2 + 1] = rest[2 * k + 1];
    }
  }
}

void *
initialiseTypeStructureEffectModule(void *, PluginStructure &structure)
{
  Component *createUIEffectModule(Processor * processor);

  static constexpr auto moduleChangeCallback = [](Parameter *p, 
    float newValue, ParameterAssignment assignmentType)
  {
    EffectModule *effectModule = (EffectModule *)p->parentProcessor;

    auto newScaled = scaleValue(newValue, p->details);
    auto [option, _] = getOptionFromValue(newScaled, p->details);

    // if we're on the audio thread and the effect isn't present, 
    // we leave it to the main thread to create it
    bool isChanged = changeEffect(effectModule, option, assignmentType);
    if (isChanged)
      return newValue;
    else
      return p->normalisedValue.load(satomi::memory_order_relaxed);
  };

  auto *pool = structure.getNewPool(COMPLEX_KB(2));

  ProcessorMetadata &effectModule = COMPLEX_STRUCTURE_PROCESSOR(EffectModule,
    "Effect Module", Processors::EffectModule, Skin::kNone,
    createEffectModule, createUIEffectModule, .reset = resetEffectModule, .serialise = serialiseEffectModule, 
    .automationMappingChangedCallback = automationMappingChangedEffectModule);

  effectModule.flags |= ProcessorMetadata::NoParameterValidationTag;
  effectModule.parameters =
    (
      COMPLEX_STRUCTURE_PARAMETER("Module Type", EffectModule::ModuleType,
        {
          .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
            Utility::initialiseTypeStructure(structure),
            Filter::initialiseTypeStructure(structure),
            Dynamics::initialiseTypeStructure(structure),
            Phase::initialiseTypeStructure(structure),
            Pitch::initialiseTypeStructure(structure),
            Freeze::initialiseTypeStructure(structure),
            Destroy::initialiseTypeStructure(structure) }}),
          .defaultOptionId = Filter::Types::Normal
        }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::AfterProcess, nullptr, moduleChangeCallback),
      COMPLEX_STRUCTURE_PARAMETER("Module Enabled", EffectModule::ModuleEnabled, { 0.0f, 1.0f, 1.0f },
        ParameterScale::Toggle, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::Realtime, printToggleValues),
      COMPLEX_STRUCTURE_PARAMETER("Module Mix", EffectModule::ModuleMix, { 0.0f, 1.0f, 1.0f }, ParameterScale::Linear, "%",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("Low Bound", EffectModule::LowBound, { 0.0f, 1.0f, 0.0f }, ParameterScale::Frequency, " hz",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("High Bound", EffectModule::HighBound, { 0.0f, 1.0f, 1.0f }, ParameterScale::Frequency, " hz",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo),
      COMPLEX_STRUCTURE_PARAMETER("Shift Bounds", EffectModule::ShiftBounds, { -1.0f, 1.0f, 0.5f }, ParameterScale::Linear, "%",
        ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)
    );

  return &effectModule.computeCounts();
}

void *
initialiseTypeStructureEffectsLane(void *, PluginStructure &structure)
{
  Component *createUIEffectsLane(Processor * processor);

  static constexpr auto laneInputChangeCallback = [](Parameter *p, float newValue, ParameterAssignment)
  {
    float newScaled = scaleValue(newValue, p->details);

    auto *parentProcessor = p->parentProcessor;
    auto [option, _] = getOptionFromValue(newScaled, p->details);

    // disallow feedback until we figure out a good UX for that
    // FIXME: fix lane feedback UX
  outer:
    {
      auto *next = option;
      while (true)
      {
        // have we reached a primary source? (main or sidechain inputs)
        if (next->flags != IndexedData::StateIdFlag)
          break;

        // if we've reached our starting lane change initial option
        if (next->stateId == parentProcessor->stateId)
        {
          // moving back in order to find another option that doesn't create a feedback loop
          // the reason for going back is that eventually we can reach the default option from main input
          --newScaled;

          option = getOptionFromValue(newScaled, p->details).first;
          goto outer; // no labelled break in this language...
        }

        // continue down the chain until we find a primary source or our starting lane
        auto *nextParameter = parentProcessor->state
          ->getProcessorParameter(next->stateId, EffectsLane::Input);
        next = getOptionFromValue(scaleValue(
          nextParameter->normalisedValue.load(satomi::memory_order_relaxed),
          nextParameter->details), nextParameter->details).first;
      }
    }

    newValue = unscaleValue(getValueFromOption(option, p->details), p->details);
    return newValue;
  };

  auto pool = structure.getNewPool(COMPLEX_KB(2));

  ProcessorMetadata &effectsLane = COMPLEX_STRUCTURE_PROCESSOR(EffectsLane, "Effects Lane",
    Processors::EffectsLane, Skin::kEffectsLane, createEffectsLane, createUIEffectsLane, .reset = resetEffectsLane);
  effectsLane.parameters =
  (
    COMPLEX_STRUCTURE_PARAMETER("Lane Enabled", EffectsLane::LaneEnabled, { 0.0f, 1.0f, 1.0f }, ParameterScale::Toggle, {},
      ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess, printToggleValues),
    COMPLEX_STRUCTURE_PARAMETER("Input", EffectsLane::Input,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main Input", .id = EffectsLane::InputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::InputOptionsSidechain,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::inputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Lanes", .id = EffectsLane::InputOptionsLane,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::laneSources) }}),
        .defaultOptionId = EffectsLane::InputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess, nullptr, laneInputChangeCallback),
    COMPLEX_STRUCTURE_PARAMETER("Output", EffectsLane::Output,
      {
        .options = COMPLEX_STRUCTURE_INDEXED_DATA()->addChildren({{
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Main Output", .id = EffectsLane::OutputOptionsMain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "Sidechain", .id = EffectsLane::OutputOptionsSidechain,
            .valueCount = 0, .dynamicUpdateUuid = ParameterChangeReason::outputSidechain),
          COMPLEX_STRUCTURE_INDEXED_DATA(.displayName = "None", .id = EffectsLane::OutputOptionsNone) }}),
        .defaultOptionId = EffectsLane::OutputOptionsMain
      }, ParameterScale::Indexed, {}, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess),
    COMPLEX_STRUCTURE_PARAMETER("Gain Matching", EffectsLane::GainMatching, { 0.0f, 1.0f, 1.0f }, ParameterScale::Toggle, {},
      ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess, printToggleValues)
  );

  effectsLane.children = (ProcessorMetadata *)initialiseTypeStructureEffectModule(&effectsLane, structure);

  return &effectsLane.computeCounts();
}
