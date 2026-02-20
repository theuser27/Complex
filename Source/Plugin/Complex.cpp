
// Created: 2021-05-23 00:20:15

#include "Complex.hpp"

#include "cplug/cplug.h"

#include "Framework/load_save.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Framework/parameter_value.hpp"
#include "Generation/EffectsState.hpp"
#include "Generation/SoundEngine.hpp"
#include "Renderer.hpp"
#include "Interface/LookAndFeel/Miscellaneous.hpp"

namespace
{
  struct
  {
    utils::LockBlame<i32> readWriteLock{};
    Framework::PluginStructure structure{};
    utils::bumpArena *stringArena{};
    utils::sll<utils::string_view> *strings{};
    utils::sll<Plugin::ComplexPlugin> *pluginInstances{};
  } structure{};

  void initialisePluginStructure()
  {
    utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    structure.stringArena = utils::bumpArena::create(COMPLEX_MB(1), COMPLEX_KB(64));
    structure.structure.arena = utils::bumpArena::create(COMPLEX_MB(1), COMPLEX_KB(64));
    structure.structure.metadata = (Framework::ProcessorMetadata *)initialiseTypeStructure<
      Generation::SoundEngine>(nullptr, structure.structure);

    auto pushString = [&](utils::string_view string)
    {
      if (string.empty())
        return;

      for (auto *element = structure.strings; element; element = element->next)
        if (element->object == string)
          return;

      auto *node = anew(structure.stringArena, utils::sll<utils::string_view>, { string });
      if (structure.strings)
        node->next = structure.strings;
      structure.strings = node;
    };

    auto recurseParameterStrings = [&](const auto &recurseParameters, const auto &recurseProcessors,
      Framework::ParameterMetadata *parameterMetadata) -> void
    {
      pushString(parameterMetadata->details.displayName);
      pushString(parameterMetadata->details.displayUnits);

      if (parameterMetadata->details.scale == Framework::ParameterScale::Indexed)
      {
        auto *option = parameterMetadata->details.options;

        bool visitedChildren = false;
        while (true)
        {
          pushString(option->displayName);

          if (option->flags == Framework::IndexedData::ProcessorFlag)
            recurseProcessors(recurseProcessors, option->processorMetadata);
          else if (option->flags == Framework::IndexedData::ParameterFlag)
            recurseParameters(recurseParameters, recurseProcessors, option->parameterMetadata);

          if (option->children && !visitedChildren)
          {
            option = option->children;
          }
          else if (option->next)
          {
            visitedChildren = false;
            option = option->next;
          }
          else
          {
            visitedChildren = true;
            if (!option->parent)
              break;

            option = option->parent;
          }
        }
      }
    };

    auto recurseProcessorStrings = [&](const auto &recurseProcessors, Framework::ProcessorMetadata *processorMetadata) -> void
    {
      pushString(processorMetadata->name);

      auto *parameters = processorMetadata->parameters;
      for (u32 i = 0; i < processorMetadata->parametersCount; ++i)
      {
        recurseParameterStrings(recurseParameterStrings, recurseProcessors, parameters);
        parameters = parameters->next;
      }

      auto *children = processorMetadata->children;
      for (u32 i = 0; i < processorMetadata->childrenCount; ++i)
      {
        recurseProcessors(recurseProcessors, children);
        children = children->next;
      }
    };

    recurseProcessorStrings(recurseProcessorStrings, structure.structure.metadata);
  }

  void deinitialisePluginStructure()
  {
    utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    utils::bumpArena::destroy(structure.structure.arena);

    structure.strings = {};
    utils::bumpArena::destroy(structure.stringArena);
  }
}

utils::string_view 
findOrAddPermanentString(utils::string_view string)
{
  auto *element = structure.strings;
  {
    utils::ScopedLock g{ structure.readWriteLock, false, utils::WaitMechanism::WaitNotify };

    auto *previousElement = element;
    for (; element; (previousElement = element), (element = element->next))
      if (element->object == string)
        return element->object;
    element = previousElement;
  }

  {
    utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    auto *memory = utils::bumpArena::insert(structure.stringArena, sizeof(utils::sll<utils::string_view>) +
      1 + string.size(), alignof(utils::sll<utils::string_view>));
    auto stringStart = memory + sizeof(utils::sll<utils::string_view>);
    ::memcpy(stringStart, string.data(), string.size());
    memory[sizeof(utils::sll<utils::string_view>) + string.size()] = byte('\0');

    auto *node = new(memory) utils::sll<utils::string_view>{ { (char *)(memory + sizeof(*structure.strings)), string.size() } };
    if (!structure.strings)
      structure.strings = node;
    else
      element->next = node;
    
    return node->object;
  }
}

namespace Framework
{
  void ParameterBridge::notifyParameterChange() noexcept
  {
    auto hostContext = Interface::getPlugin(Interface::uiRelated.renderer).hostContext;
    hostContext->rescan(hostContext,
      CPLUG_FLAG_RESCAN_PARAM_VALUES | CPLUG_FLAG_RESCAN_PARAM_NAMES | CPLUG_FLAG_RESCAN_PARAM_METADATA);
  }

  void ParameterBridge::beginChangeGesture()
  {
    if (!state->plugin->rendererInstance)
      return;

    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_BEGIN, .id = (uint32_t)parameterIndex } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }

  void ParameterBridge::setValueFromUI(float newValue) noexcept
  {
    value_.store(newValue, satomi::memory_order_release);

    if (!state->plugin->rendererInstance)
      return;

    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_UPDATE,
      .id = (uint32_t)parameterIndex, .value = newValue } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }

  void ParameterBridge::endChangeGesture()
  {
    if (!state->plugin->rendererInstance)
      return;

    auto event = CplugEvent{ .parameter = {.type = CPLUG_EVENT_PARAM_CHANGE_END, .id = (uint32_t)parameterIndex } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }
}

namespace Plugin
{
  void saveState(ComplexPlugin *plugin, const void *stateCtx, cplug_writeProc writeProc);
  void loadState(ComplexPlugin *plugin, utils::string_view data);

  State::State(ComplexPlugin *plugin) : plugin{ plugin }
  {
    processorStorage = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(2));
    miscStorage = utils::bumpArena::createNested(processorStorage, COMPLEX_KB(128));
    allProcessors.data.reserve({ miscStorage, false }, 64);

    auto count = plugin->parameterCount;
    parameterBridges = utils::span{ (Framework::ParameterBridge *)
      utils::bumpArena::insert(miscStorage, sizealignof(Framework::ParameterBridge, count)), count };

    for (usize i = 0; i < count; ++i)
      (void)new(parameterBridges.data() + i) Framework::ParameterBridge{ this, (u64)i };

    // TODO: copy over metadata tree
    // TODO: decide how runtime changes are going to be handled (this is going to be hard)
    //pluginStructure.arena = utils::bumpArena::create();
    //utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    //pluginStructure.versionNumber = structure.structure.versionNumber;
    //pluginStructure.loadedDynamicLibs = { pluginStructure.arena, structure.structure.loadedDynamicLibs };
    pluginStructure.metadata = structure.structure.metadata;
  }

  State::~State()
  {
    utils::bumpArena::destroy(processorStorage);
  }

  void State::expandIfNecessary()
  {
    static constexpr usize expandAmount = 2;
    static constexpr float expandThreshold = 0.75f;

    if ((float)allProcessors.data.size() / (float)allProcessors.data.capacity() < expandThreshold)
      return;
    
    auto newProcessors = utils::vector_map<u64, Generation::BaseProcessor *>{};
    newProcessors.data.reserve(allProcessors.data.size() * expandAmount);

    for (auto pair : allProcessors.data)
      new(newProcessors.data.push_back()) decltype(pair){ COMPLEX_MOVE(pair) };

    if (this != plugin->state_.get())
    {
      allProcessors.data.swap(newProcessors.data);
      return;
    }

    auto guard = plugin->acquireProcessingLock(true);
    allProcessors.data.swap(newProcessors.data);
  }

  Framework::ProcessorMetadata *
  State::findProcessorMetadata(uuid id) const
  {
    COMPLEX_ASSERT(pluginStructure.metadata);

    Framework::ProcessorMetadata *current = pluginStructure.metadata;
    bool visitedChildren = false;
    while (current->id != id)
    {
      if (current->children && !visitedChildren)
      {
        current = current->children;
      }
      else if (current->next)
      {
        visitedChildren = false;
        current = current->next;
      }
      else
      {
        visitedChildren = true;
        if (!current->parent)
        {
          COMPLEX_ASSERT_FALSE("Couldn't find processor with id: %zu", id);
          return nullptr;
        }
        current = current->parent;
      }
    }

    return current;
  }
  
  Generation::BaseProcessor *
  State::getProcessor(u64 processorId) const noexcept
  {
    auto processorIter = allProcessors.find(processorId);
    return (processorIter != allProcessors.data.end()) ?
      processorIter->second : nullptr;
  }

  void State::deleteProcessor(Generation::BaseProcessor *processor) noexcept
  {
    auto iter = allProcessors.find(processor->stateId);

    // TODO: free all registered resources
    // TODO: unlink all parameters from their UIs and detach them from the parameter bridges

    allProcessors.data.erase(iter);
    utils::bumpArena::remove(processor->arena);
  }

  Generation::BaseProcessor *
  State::copyProcessor(Generation::BaseProcessor *processor)
  {
    utils::ScopedLock guard{};
    if (this == plugin->state_.get())
      guard = plugin->acquireProcessingLock(true);

    return processor->createCopy();
  }

  Framework::ParameterValue *
  State::getProcessorParameter(u64 parentProcessorId, uuid parameterId) const noexcept
  {
    auto *processorPointer = getProcessor(parentProcessorId);
    if (!processorPointer)
      return {};

    return processorPointer->getParameter(parameterId);
  }

  float
  State::getOverlap() noexcept { return soundEngine->getOverlap(); }
  u32
  State::getFFTSize() noexcept { return soundEngine->getFFTSize(); }
  u32
  State::getBlockPosition() noexcept { return soundEngine->getBlockPosition(); }
  u32
  State::getLaneCount() const { return soundEngine->getEffectsState().childrenCount; }


  State::Thread &
  State::reserveFreeWorker(utils::typeInfo reservationTag)
  {
    for (auto &worker : workers)
      if (worker.thread == utils::thread{})
        return worker;

    auto &worker = workers.emplace_back();
    worker.reservationTag = reservationTag;
    return worker;
  }

  ComplexPlugin::ComplexPlugin(usize parameterMappings, u32 inSidechains, 
    u32 outSidechains, usize undoSteps, CplugHostContext *hostContext) : 
    inSidechains{ inSidechains }, outSidechains{ outSidechains },
    parameterCount{ parameterMappings }, undoManager{ undoSteps },
    hostContext{ hostContext }
  {
    loadState(this, {});
    // plugin formats will later call loadState 
    // but in between that other functions get called that will require *some* state
    wasStateInitialised = hostContext->type == CPLUG_PLUGIN_IS_STANDALONE;
  }

  ComplexPlugin::~ComplexPlugin()
  {
  }

  void ComplexPlugin::initialise(float newSampleRate, u32 newSamplesPerBlock)
  {
    if (newSampleRate != getSampleRate())
      sampleRate.store(newSampleRate, satomi::memory_order_release);

    if (newSamplesPerBlock != getSamplesPerBlock())
    {
      samplesPerBlock.store(newSamplesPerBlock, satomi::memory_order_release);

      auto lock = acquireProcessingLock(true);

      auto state = state_;
      if (!state)
        return;

      state->soundEngine->resetBuffers();
    }
  }

  utils::sp<State> 
  ComplexPlugin::exchangeStates(utils::sp<State> state)
  {
    {
      auto guard = acquireProcessingLock(true);
      state_.swap(state);
    }

    // refresh all parameters as soon as the states are exchanged
    hostContext->rescan(hostContext, 
      CPLUG_FLAG_RESCAN_PARAM_METADATA | CPLUG_FLAG_RESCAN_PARAM_NAMES | CPLUG_FLAG_RESCAN_PARAM_VALUES);

    return state;
  }

  Interface::Renderer &
  ComplexPlugin::getRenderer()
  {
    if (!rendererInstance)
    {
      rendererInstance = Interface::createRenderer(*this);
      return *rendererInstance;
    }

    // setting these once more because i don't know if the message thread 
    // might have been shut down and started up again
    Interface::uiRelated.renderer = rendererInstance;
    Interface::uiRelated.skin = Interface::getSkin(rendererInstance);
    
    return *rendererInstance;
  }

  void ComplexPlugin::rescanLatency()
  {
    if (hasLatencyChanged.exchange(false, satomi::memory_order_relaxed))
      hostContext->rescan(hostContext, CPLUG_FLAG_RESCAN_LATENCY);
  }

  void ComplexPlugin::process(float *const *in, float *const *out, 
    u32 numSamples, u32 numInputs, u32 numOutputs) noexcept
  {
    float currentSampleRate = getSampleRate();

    utils::ScopedLock g{ processingLock, false, utils::WaitMechanism::Spin };

    auto state = state_;
    state->soundEngine->updateParameters(UpdateFlag::BeforeProcess,
      currentSampleRate, true);
    
    if (auto latency_ = state->soundEngine->getProcessingDelay();
      latency_ != latency.load(satomi::memory_order_relaxed))
    {
      latency.store(latency_, satomi::memory_order_relaxed);
      hasLatencyChanged.store(true, satomi::memory_order_relaxed);
    }

    state->soundEngine->process(in, out, numSamples, 
      currentSampleRate, numInputs, numOutputs, *state->fft);

    state->soundEngine->updateParameters(UpdateFlag::AfterProcess,
      currentSampleRate, true);
  }

  void ComplexPlugin::undo() { undoManager.undo(); }
  void ComplexPlugin::redo() { undoManager.redo(); }
}

namespace utils { void atLoad(); }
void initialiseCJSONHooks();
constinit thread_local utils::bumpArena *localScratch = nullptr;
constinit utils::bumpArena *globalArena = nullptr;

void cplug_libraryLoad()
{
  utils::atLoad();

  globalArena = utils::bumpArena::create(COMPLEX_MB(128), COMPLEX_MB(1));

  initialisePluginStructure();
  initialiseCJSONHooks();
}
void cplug_libraryUnload()
{
  deinitialisePluginStructure();

  utils::bumpArena::destroy(globalArena);
}

void *cplug_createPlugin(CplugHostContext *ctx)
{
  localScratch = utils::bumpArena::create(COMPLEX_MB(4), COMPLEX_KB(128));

  usize parameterMappings, inSidechains, outSidechains, undoSteps;
  Framework::LoadSave::getStartupParameters(parameterMappings, inSidechains, outSidechains, undoSteps);

  auto *plugin = new utils::sll<Plugin::ComplexPlugin>{ { parameterMappings, (u32)inSidechains, (u32)outSidechains, undoSteps, ctx } };
  utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };
  
  if (auto *lastNode = structure.pluginInstances)
  {
    while (lastNode->next)
      lastNode = lastNode->next;
    lastNode->next = plugin;
  }
  else
    structure.pluginInstances = plugin;

  return &plugin->object;
}

void cplug_destroyPlugin(void *ptr)
{
  auto *plugin = (Plugin::ComplexPlugin *)ptr;

  {
    utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    utils::sll<Plugin::ComplexPlugin> *node = structure.pluginInstances, *lastNode = nullptr;

    while (&node->object != plugin)
    {
      lastNode = node;
      node = node->next;
    }

    ((lastNode) ? lastNode->next : structure.pluginInstances) = node->next;
  }
  
  delete plugin;

  utils::bumpArena::destroy(localScratch);
}

/* --------------------------------------------------------------------------------------------------------
 * Busses */

uint32_t cplug_getNumInputBusses(void *ptr)
{
  return ((Plugin::ComplexPlugin *)ptr)->inSidechains + 1;
}

uint32_t cplug_getNumOutputBusses(void *ptr)
{
  return ((Plugin::ComplexPlugin *)ptr)->outSidechains + 1;
}

uint32_t cplug_getInputBusChannelCount([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t idx)
{
  return 2; // always stereo
}

uint32_t cplug_getOutputBusChannelCount([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t idx)
{
  return 2; // always stereo
}

void cplug_getInputBusName([[maybe_unused]] void *ptr, uint32_t idx, char *buf, size_t buflen)
{
  if (idx == 0)
  {
    ::stbsp_snprintf(buf, (int)buflen, "%s", "Main In");
    return;
  }

  ::stbsp_snprintf(buf, (int)buflen, "%s %d", "Sidechain In", (int)idx);
}

void cplug_getOutputBusName([[maybe_unused]] void *ptr, uint32_t idx, char *buf, size_t buflen)
{
  if (idx == 0)
  {
    ::stbsp_snprintf(buf, (int)buflen, "%s", "Main Out");
    return;
  }

  ::stbsp_snprintf(buf, (int)buflen, "%s %d", "Sidechain Out", (int)idx);
}

/* --------------------------------------------------------------------------------------------------------
 * Parameters */

uint32_t cplug_getNumParameters(void *ptr)
{
  return (uint32_t)((Plugin::ComplexPlugin *)ptr)->parameterCount;
}

uint32_t cplug_getParameterID([[maybe_unused]] void *ptr, uint32_t paramIndex)
{
  // TODO: consider taking advantage of the index -> id mapping in the future
  // return the exact same parameter index to use as ID
  return paramIndex;
}

void cplug_getParameterName(void *ptr, uint32_t paramId, char *buf, size_t buflen)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].getName(buf, buflen);
}

double cplug_getParameterValue(void *ptr, uint32_t paramId)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  return state->parameterBridges[paramId].getValue();
}

double cplug_getDefaultParameterValue(void *ptr, uint32_t paramId)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  return state->parameterBridges[paramId].getDefaultValue();
}

void cplug_setParameterValue(void *ptr, uint32_t paramId, double value)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].setValue((float)value);
}

double cplug_denormaliseParameterValue([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t paramId, double normalised)
{
  return normalised;
}

double cplug_normaliseParameterValue([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t paramId, double denormalised)
{
  return denormalised;
}

double cplug_parameterStringToValue(void *ptr, uint32_t paramId, const char *str)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  return state->parameterBridges[paramId].getValueForText({ str, utils::getStringSize(str) });
}

void cplug_parameterValueToString(void *ptr, uint32_t paramId, char *buf, size_t bufsize, double value)
{
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].getText((float)value, buf, bufsize);
}

void cplug_getParameterRange(void *ptr, uint32_t paramId, double *min, double *max)
{
  *min = 0.0;
  *max = 1.0;

  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  if (auto link = state->parameterBridges[paramId].getParameterLink())
  {
    auto details = link->parameter->getParameterDetails();
    if (details.scale == Framework::ParameterScale::Indexed)
    {
      *min = 0.0;
      *max = details.options->count - 1;
    }
    else
    {
      *min = details.minValue;
      *max = details.maxValue;
    }
  }
}

uint32_t cplug_getParameterFlags(void *ptr, uint32_t paramId)
{
  uint32_t flags = CPLUG_FLAG_PARAMETER_IS_AUTOMATABLE;
  auto lock = ((Plugin::ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((Plugin::ComplexPlugin *)ptr)->state_;
  auto &bridge = state->parameterBridges[paramId];

  if (auto link = bridge.getParameterLink())
  {
    auto details = link->parameter->getParameterDetails();
    flags |= details.scale == Framework::ParameterScale::Toggle ? CPLUG_FLAG_PARAMETER_IS_BOOL : 0;
    flags |= details.scale == Framework::ParameterScale::Indexed ? 
      CPLUG_FLAG_PARAMETER_IS_INTEGER : 0;
  }

  return flags;
}

uint32_t cplug_getLatencyInSamples(void *ptr)
{
  return ((Plugin::ComplexPlugin *)ptr)->latency.load(satomi::memory_order_relaxed);
}
uint32_t cplug_getTailInSamples([[maybe_unused]] void *ptr) { return 0; }

/* --------------------------------------------------------------------------------------------------------
 * State */

void cplug_saveState(void *userPlugin, const void *stateCtx, cplug_writeProc writeProc)
{
  saveState((Plugin::ComplexPlugin *)userPlugin, stateCtx, writeProc);
}

void cplug_loadState(void *userPlugin, const void *stateCtx, cplug_readProc readProc)
{
  usize capacity = COMPLEX_KB(4);
  usize size{};
  char *buffer = (char *)utils::allocate(capacity);
  while (readProc(stateCtx, buffer + size, COMPLEX_KB(4)) != 0)
  {
    usize newCapacity = capacity + COMPLEX_KB(4);
    char *newBuffer = (char *)utils::allocate(newCapacity);

    ::memcpy(newBuffer, buffer, size * sizeof(char));
    ::zeroset(newBuffer + size, COMPLEX_KB(4));
    utils::deallocate(buffer);

    buffer = newBuffer;
    capacity = newCapacity;
    size += COMPLEX_KB(4);
  }

  usize i = 0;
  for (; i < COMPLEX_KB(4); ++i)
    if (buffer[size - COMPLEX_KB(4) + i] != '\0')
      break;
  size = size - COMPLEX_KB(4) + i;

  loadState((Plugin::ComplexPlugin *)userPlugin, { buffer, size });
  utils::deallocate(buffer);
}



void cplug_setSampleRateAndBlockSize(void *ptr, double sampleRate, uint32_t maxBlockSize)
{
  ((Plugin::ComplexPlugin *)ptr)->initialise((float)sampleRate, maxBlockSize);
}

void cplug_process(void *ptr, CplugProcessContext *ctx)
{
  utils::ScopedNoDenormals noDenormals{};

  auto *plugin = (Plugin::ComplexPlugin *)ptr;

  // "Sample accurate" process loop
  CplugEvent event;
  uint32_t   frame = 0;
  while (ctx->dequeueEvent(ctx, &event, frame))
  {
    switch (event.type)
    {
    case CPLUG_EVENT_UNHANDLED_EVENT:
      break;
    case CPLUG_EVENT_PARAM_CHANGE_UPDATE:
    {
      cplug_setParameterValue(plugin, event.parameter.id, event.parameter.value);
      break;
    }
    case CPLUG_EVENT_MIDI:
      break;
    case CPLUG_EVENT_PROCESS_AUDIO:
    {
      [[maybe_unused]] float **in = ctx->getAudioInput(ctx, 0);
      [[maybe_unused]] float **out = ctx->getAudioOutput(ctx, 0);
      //COMPLEX_ASSERT(in != nullptr);
      //COMPLEX_ASSERT(in[0] != nullptr);
      //COMPLEX_ASSERT(in[1] != nullptr);
      //COMPLEX_ASSERT(out != nullptr);
      //COMPLEX_ASSERT(out[0] != nullptr);
      //COMPLEX_ASSERT(out[1] != nullptr);

      //plugin->process(in, out, event.processAudio.endFrame - frame, 2, 2);

      // If your plugin does not require sample accurate processing, use this line below to break the loop
      frame = event.processAudio.endFrame;
    } break;
    default:
      break;
    }
  }
}


