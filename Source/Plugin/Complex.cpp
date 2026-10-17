
// Created: 2021-05-23 00:20:15

#include <stddef.h> // offsetof

#include "Complex.hpp"

#include "Third Party/cplug/config.h"
#include "Third Party/cplug/cplug.h"
#include "Third Party/xhl/xhl_files.h"

#include "Framework/load_save.hpp"
#include "Framework/parameters.hpp"
#include "Generation/Effects.hpp"
#include "Generation/SoundEngine.hpp"
#include "Interface/LookAndFeel/Graphics.hpp"
#include "Interface/LookAndFeel/Component.hpp"
#include "Interface/Sections/MainInterface.hpp"

ExecutableMetadata executableStaticData{};

static void initialisePluginStructure()
{
  utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

  executableStaticData.pool = utils::Pool::create(COMPLEX_MB(1), COMPLEX_KB(64));
  executableStaticData.structure.pool = utils::Pool::create(COMPLEX_MB(1), COMPLEX_KB(64));

  auto pushString = [&](utils::string_view string)
  {
    if (string.empty())
      return string;

    for (auto *element = executableStaticData.strings; element; element = element->next)
      if (element->object == string)
        return element->object;

    auto *node = anew(executableStaticData.pool, utils::sll<utils::string_view>, { string });
    if (executableStaticData.strings)
      node->next = executableStaticData.strings;
    executableStaticData.strings = node;

    return node->object;
  };

  auto recurseParameterStrings = [&](const auto &recurseParameters, const auto &recurseProcessors,
    ParameterMetadata *parameterMetadata) -> void
  {
    pushString(parameterMetadata->details.displayName);
    pushString(parameterMetadata->details.displayUnits);

    if (parameterMetadata->details.scale == ParameterScale::Indexed)
    {
      auto *option = parameterMetadata->details.options;

      bool visitedChildren = false;
      while (true)
      {
        pushString(option->displayName);

        if (option->flags == IndexedData::ProcessorFlag)
          recurseProcessors(recurseProcessors, option->processorMetadata);
        else if (option->flags == IndexedData::ParameterFlag)
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

  auto recurseProcessorStrings = [&](const auto &recurseProcessors, ProcessorMetadata *processorMetadata) -> void
  {
    pushString(processorMetadata->name);

    auto *parameters = processorMetadata->parameters;
    for (u32 i = 0; i < processorMetadata->parameterCount; ++i)
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

  {
    char buffer[512];
    int pathSize = xfiles_get_user_directory(buffer, sizeof(buffer), XFILES_USER_DIRECTORY_APPDATA);
    auto string = utils::string::create(getLocalScratch(), "%*s" XFILES_DIR_STR "%s", pathSize, buffer, CPLUG_PLUGIN_NAME);
    executableStaticData.configFolderPath = pushString(string);
  }

  executableStaticData.structure.metadata = (ProcessorMetadata *)
    initialiseTypeStructureSoundEngine(nullptr, executableStaticData.structure);

  recurseProcessorStrings(recurseProcessorStrings, executableStaticData.structure.metadata);
}

static void deinitialisePluginStructure()
{
  utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

  utils::Pool::destroy(executableStaticData.structure.pool);

  executableStaticData.strings = {};
  utils::Pool::destroy(executableStaticData.pool);
}

utils::string_view
findOrAddPermanentString(utils::string_view string)
{
  auto *element = executableStaticData.strings;
  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, false, utils::WaitMechanism::WaitNotify };

    auto *previousElement = element;
    for (; element; (previousElement = element), (element = element->next))
      if (element->object == string)
        return element->object;
    element = previousElement;
  }

  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    auto *memory = utils::Pool::insert(executableStaticData.pool, sizeof(utils::sll<utils::string_view>) +
      1 + string.size(), alignof(utils::sll<utils::string_view>));
    auto stringStart = memory + sizeof(utils::sll<utils::string_view>);
    ::memcpy(stringStart, string.data(), string.size());
    stringStart[string.size()] = byte('\0');

    auto *node = new(memory) utils::sll<utils::string_view>{ { (char *)stringStart, string.size() } };
    if (!executableStaticData.strings)
      executableStaticData.strings = node;
    else
      element->next = node;

    return node->object;
  }
}

void HostParameter::notifyParameterChange()
{
  auto hostContext = getUiRelated()->plugin.hostContext;
  hostContext->rescan(hostContext,
    CPLUG_FLAG_RESCAN_PARAM_VALUES | CPLUG_FLAG_RESCAN_PARAM_NAMES | CPLUG_FLAG_RESCAN_PARAM_METADATA);
}

void HostParameter::beginChangeGesture()
{
  // weird edge-case
  // if a control has begun change but their host parameter changes underneath them before they can end the gesture
  if (auto expected = GestureState::None;
    gestureState_.compare_exchange_strong(expected, GestureState::Begun,satomi::memory_order_relaxed))
  {
    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_BEGIN, .id = (uint32_t)parameterIndex } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }
  else
  {
    // weirder edge-case
    // if a control was signalled to end its gesture but another one begins before the main-thread can end the previous
    expected = GestureState::ShouldEnd;
    (void)gestureState_.compare_exchange_strong(expected, GestureState::Begun, satomi::memory_order_relaxed);
  }
}

void HostParameter::setValueFromUI(float newValue)
{
  if (gestureState_.load(satomi::memory_order_relaxed) != GestureState::None)
  {
    value_.store(newValue, satomi::memory_order_release);

    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_UPDATE,
      .id = (uint32_t)parameterIndex, .value = newValue } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }
}

void HostParameter::endChangeGesture()
{
  for (auto expected = GestureState::Begun;
    !gestureState_.compare_exchange_strong(expected, GestureState::None, satomi::memory_order_relaxed); )
    if (expected == GestureState::None)
      return;

  auto event = CplugEvent{ .parameter = {.type = CPLUG_EVENT_PARAM_CHANGE_END, .id = (uint32_t)parameterIndex } };
  state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
}

void saveState(ComplexPlugin *plugin, const void *stateCtx, cplug_writeProc writeProc);
void loadState(ComplexPlugin *plugin, utils::string_view data);

State::State(ComplexPlugin *plugin) : plugin{ plugin }
{
  processorStorage = utils::Pool::create(COMPLEX_MB(256), COMPLEX_MB(2));
  miscStorage = utils::Pool::createNested(processorStorage, COMPLEX_KB(128));
  guiStorage = utils::Pool::create(COMPLEX_MB(256), COMPLEX_MB(4));

  allProcessors.data = { miscStorage, 64 };
  parameterModulators = { miscStorage, 32 };
  dynamicParameters = { miscStorage, 32 };
  workers = { miscStorage, 16 };
  cachedHotreloadSymbols.data = { miscStorage, 16 };

  createDynamicParameters();

  auto count = plugin->parameterCount;
  parameterBridges = utils::span{ (HostParameter *)
    utils::Pool::insert(miscStorage, sizealignof(HostParameter, count)), count };

  for (usize i = 0; i < count; ++i)
    (void)new(parameterBridges.data() + i) HostParameter{ this, (u64)i };

  // TODO: copy over metadata tree
  // TODO: decide how runtime changes are going to be handled (this is going to be hard)
  //pluginStructure.pool = utils::Pool::create();
  //utils::ScopedLock g{ structure.readWriteLock, true, utils::WaitMechanism::WaitNotify };

  //pluginStructure.versionNumber = structure.structure.versionNumber;
  //pluginStructure.loadedDynamicLibs = { pluginStructure.pool, structure.structure.loadedDynamicLibs };
  pluginStructure.metadata = executableStaticData.structure.metadata;
}

State::~State()
{
  for (auto &worker : workers)
    worker.stop();

  utils::Pool::destroy(guiStorage);
  utils::Pool::destroy(processorStorage);
}

ProcessorMetadata *
State::findProcessorMetadata(uuid id) const
{
  COMPLEX_ASSERT(pluginStructure.metadata);

  ProcessorMetadata *current = pluginStructure.metadata;
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

Processor *
State::getProcessor(u64 processorId) const
{
  auto processorIter = allProcessors.find(processorId);
  return (processorIter != allProcessors.data.end()) ?
    processorIter->second : nullptr;
}

// check if processor creation causes dynamic parameters to change
void State::registerProcessorForDynamicParameters(Processor *processor)
{
  auto *pool = miscStorage;

  if (processor->metadata->id == Processors::EffectsLane)
  {
    auto iter = dynamicOptions.find(ParameterChangeReason::laneSources);
    auto alreadyExists = utils::findIf(iter->second, [&](IndexedData *option) { return option->stateId == processor->stateId; });
    if (!alreadyExists)
    {
      auto g = plugin->acquireProcessingLock();
      iter->second->addChildren({{ anew(pool, IndexedData,
        { .id = processor->metadata->id, .flags = IndexedData::StateIdFlag, .stateId = processor->stateId }) }});

      updateDynamicParameters(ParameterChangeReason::laneSources);
    }
  }
}

void State::deregisterProcessorForDynamicParameters(Processor *processor)
{
  bool needsUpdate = false;
  // delete all references pointing to this processor
  for (auto &pair : dynamicOptions.data)
  {
    IndexedData::visit(pair.second, [&needsUpdate, processor](IndexedData &item, IndexedData *previous)
    {
      if (item.id != processor->metadata->id || item.stateId != processor->stateId)
        return false;

      needsUpdate = true;
      --item.parent->childrenCount;
      for (auto p = item.parent; p; p = p->parent)
        p->valueCount -= item.valueCount;

      if (!previous)
        item.parent->children = item.next;
      else
        previous->next = item.next;

      utils::Pool::remove(&item);
      return false;
    });
  }

  if (needsUpdate)
    updateAllDynamicParameters();
}

Processor *
State::createProcessor(uuid processorId, void *jsonData, void *extra)
{
  auto *metadata = findProcessorMetadata(processorId);
  Processor *processor = metadata->create(this, metadata, nullptr, jsonData, extra);

  allProcessors.add(processor->stateId, processor);

  return processor;
}

Processor *
State::copyProcessor(Processor *processor)
{
  utils::ScopedLock guard{};
  if (this == plugin->state_.get())
    guard = plugin->acquireProcessingLock(true);

  return processor->createCopy();
}

utils::pair<u32, u32> State::getMinMaxFFTOrder() const { return soundEngine->getMinMaxFFTOrder(); }
u32 State::getMaxBinCount() const { return soundEngine->getMaxBinCount(); }

Parameter *
State::getProcessorParameter(u64 parentProcessorId, uuid parameterId) const
{
  auto *processorPointer = getProcessor(parentProcessorId);
  if (!processorPointer)
    return {};

  return processorPointer->getParameter(parameterId);
}

float
State::getOverlap() { return soundEngine->getOverlap(); }
u32
State::getFFTSize() { return soundEngine->getFFTSize(); }
u32
State::getBlockPosition() { return soundEngine->getBlockPosition(); }
u32
State::getLaneCount() const { return soundEngine->childrenCount; }


State::Thread &
State::reserveFreeWorker(utils::typeInfo reservationTag)
{
  for (auto &worker : workers)
    if (worker.thread == utils::thread{})
      return worker;

  auto &worker = workers.emplaceBack();
  worker.reservationTag = reservationTag;
  return worker;
}

void *
State::getHotreloadSymbol(utils::string_view decoratedName)
{
  if (auto iter = cachedHotreloadSymbols.get_last_of(decoratedName);
    iter != cachedHotreloadSymbols.data.end())
    return iter->second;

  if (!pluginStructure.loadedDynamicLibs.empty())
  {
    void *symbol = pluginStructure.loadedDynamicLibs.back()->getSymbol(decoratedName);
    if (symbol)
      cachedHotreloadSymbols.add_ordered(decoratedName, symbol);
    return symbol;
  }

  return nullptr;
}

ComplexPlugin::ComplexPlugin(usize parameterMappings, u32 inSidechains,
  u32 outSidechains, usize undoSteps, CplugHostContext *hostContext) :
  inSidechains{ inSidechains }, outSidechains{ outSidechains }, parameterCount{ parameterMappings },
  pool{ utils::Pool::create(COMPLEX_MB(16), COMPLEX_MB(2)) }, undoManager{ pool, undoSteps },
  hostContext{ hostContext }, renderer{ *this }
{
  fft.pool = pool;
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

void ComplexPlugin::rescanLatency()
{
  if (hasLatencyChanged.exchange(false, satomi::memory_order_relaxed))
    hostContext->rescan(hostContext, CPLUG_FLAG_RESCAN_LATENCY);
}

void ComplexPlugin::markPluginDirty()
{
  hostContext->rescan(hostContext, CPLUG_FLAG_RESCAN_MARK_DIRTY);
}

void ComplexPlugin::process(float *const *in, float *const *out,
  u32 numSamples, u32 numInputs, u32 numOutputs)
{
  float currentSampleRate = getSampleRate();
  auto state = state_;
  state->soundEngine->updateParameters(UpdateFlag::BeforeProcess, currentSampleRate, true);

  if (auto latency_ = state->soundEngine->getProcessingDelay();
    latency_ != latency.load(satomi::memory_order_relaxed))
  {
    latency.store(latency_, satomi::memory_order_relaxed);
    hasLatencyChanged.store(true, satomi::memory_order_relaxed);
  }

  state->soundEngine->process(in, out, numSamples,
    currentSampleRate, numInputs, numOutputs, *state->fft);

  state->soundEngine->updateParameters(UpdateFlag::AfterProcess, currentSampleRate, true);
}

struct TlsContext
{
  utils::Pool *localScratch{};
  utils::Pool *localMallocArena{};
  InterfaceRelated *uiRelated{};
  JsonTlsContext jsonContext{};
};

void *
utils::createTlsContext()
{
  auto *context = anew(globalPool, TlsContext, {});

  context->localScratch = utils::Pool::create(COMPLEX_MB(4), COMPLEX_KB(128));

  return context;
}

void utils::destroyTlsContext(void *context)
{
  auto *tls = (TlsContext *)context;
  utils::Pool::destroy(tls->localScratch);
  tls->localScratch = nullptr;
}

utils::Pool *&
getLocalScratch()
{
  return ((TlsContext *)utils::getTls())->localScratch;
}

utils::Pool *&
getSettableMallocPool()
{
  return ((TlsContext *)utils::getTls())->localMallocArena;
}

InterfaceRelated *&
getUiRelated()
{
  return ((TlsContext *)utils::getTls())->uiRelated;
}

JsonTlsContext &
getJsonContext()
{
  return ((TlsContext *)utils::getTls())->jsonContext;
}

void initialiseCJSONHooks();

// DLL-wide global memmory pool, deallocated when DLL is unloaded
constinit utils::Pool *globalPool = nullptr;

void cplug_libraryLoad()
{
  utils::atLoad();

  initialisePluginStructure();
  initialiseCJSONHooks();
}
void cplug_libraryUnload()
{
  deinitialisePluginStructure();

  utils::atUnload();
}

void *cplug_createPlugin(CplugHostContext *ctx)
{
  COMPLEX_ASSERT(getLocalScratch());

  usize parameterMappings = 64, inSidechains = 0, outSidechains = 0, undoSteps = 100;
  getStartupParameters(parameterMappings, inSidechains, outSidechains, undoSteps);

  auto *plugin = anew(globalPool, utils::sll<ComplexPlugin>,
    { { parameterMappings, (u32)inSidechains, (u32)outSidechains, undoSteps, ctx } });
  utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

  if (auto *lastNode = executableStaticData.pluginInstances)
  {
    while (lastNode->next)
      lastNode = lastNode->next;
    lastNode->next = plugin;
  }
  else
    executableStaticData.pluginInstances = plugin;

  return &plugin->object;
}

void cplug_destroyPlugin(void *ptr)
{
  auto *plugin = (ComplexPlugin *)ptr;

  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    utils::sll<ComplexPlugin> *node = executableStaticData.pluginInstances, *lastNode = nullptr;

    while (&node->object != plugin)
    {
      lastNode = node;
      node = node->next;
    }

    ((lastNode) ? lastNode->next : executableStaticData.pluginInstances) = node->next;
  }

  // warning: this only works because the plugin is the first member
  utils::Pool::remove(plugin);
}

/* --------------------------------------------------------------------------------------------------------
 * Busses */

uint32_t
cplug_getNumInputBusses(void *ptr)
{
  return ((ComplexPlugin *)ptr)->inSidechains + 1;
}

uint32_t
cplug_getNumOutputBusses(void *ptr)
{
  return ((ComplexPlugin *)ptr)->outSidechains + 1;
}

uint32_t
cplug_getInputBusChannelCount([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t idx)
{
  return 2; // always stereo
}

uint32_t
cplug_getOutputBusChannelCount([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t idx)
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

// audio-thread, main-thread
uint32_t
cplug_getNumParameters(void *ptr)
{
  return (uint32_t)((ComplexPlugin *)ptr)->parameterCount;
}

// main-thread
uint32_t
cplug_getParameterID([[maybe_unused]] void *ptr, uint32_t paramIndex)
{
  // TODO: consider taking advantage of the index -> id mapping in the future
  // return the exact same parameter index to use as ID
  return paramIndex;
}

// main-thread
void cplug_getParameterName(void *ptr, uint32_t paramId, char *buf, size_t buflen)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].getName(buf, buflen);
}

// main-thread
double
cplug_getParameterValue(void *ptr, uint32_t paramId)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
#ifdef COMPLEX_CLAP
  return state->parameterBridges[paramId].getScaledValue();
#else
  return state->parameterBridges[paramId].getValue();
#endif
}

// main-thread
double 
cplug_getDefaultParameterValue(void *ptr, uint32_t paramId)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
#ifdef COMPLEX_CLAP
  return state->parameterBridges[paramId].getDefaultScaledValue();
#else
  return state->parameterBridges[paramId].getDefaultValue();
#endif
}

// audio-thread, main-thread 💀
void cplug_setParameterValue(void *ptr, uint32_t paramId, double value)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(true);
  auto state = ((ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].setValue((float)value);
}

double
cplug_denormaliseParameterValue([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t paramId, double normalised)
{
  return normalised;
}

double
cplug_normaliseParameterValue([[maybe_unused]] void *ptr, [[maybe_unused]] uint32_t paramId, double denormalised)
{
  return denormalised;
}

// main-thread
double
cplug_parameterStringToValue(void *ptr, uint32_t paramId, const char *str)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
  return state->parameterBridges[paramId].getValueForText({ str, utils::getStringSize(str) });
}

// main-thread
void cplug_parameterValueToString(void *ptr, uint32_t paramId, char *buf, size_t bufsize, double value)
{
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
  state->parameterBridges[paramId].getText((float)value, buf, bufsize);
}

// main-thread
void cplug_getParameterRange(void *ptr, uint32_t paramId, double *min, double *max)
{
  *min = 0.0;
  *max = 1.0;

  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
  if (auto parameter = state->parameterBridges[paramId].getParameter())
  {
    auto details = parameter->details;
    if (details.scale == ParameterScale::Indexed)
    {
      *min = 0.0;
      *max = details.options->valueCount - 1;
    }
    else
    {
      *min = details.minValue;
      *max = details.maxValue;
    }
  }
}

// main-thread
uint32_t
cplug_getParameterFlags(void *ptr, uint32_t paramId)
{
  uint32_t flags = CPLUG_FLAG_PARAMETER_IS_AUTOMATABLE;
  auto lock = ((ComplexPlugin *)ptr)->acquireProcessingLock(false);
  auto state = ((ComplexPlugin *)ptr)->state_;
  auto &bridge = state->parameterBridges[paramId];

  if (auto parameter = bridge.getParameter())
  {
    auto details = parameter->details;
    flags |= details.scale == ParameterScale::Toggle ? CPLUG_FLAG_PARAMETER_IS_BOOL : 0;
    flags |= details.scale == ParameterScale::Indexed ?
      CPLUG_FLAG_PARAMETER_IS_INTEGER : 0;
  }

  return flags;
}

// main-thread
uint32_t
cplug_getLatencyInSamples(void *ptr)
{
  return ((ComplexPlugin *)ptr)->latency.load(satomi::memory_order_relaxed);
}
// audio-thread, main-thread
uint32_t cplug_getTailInSamples([[maybe_unused]] void *ptr) { return 0; }

/* --------------------------------------------------------------------------------------------------------
 * State */

// main-thread
void cplug_saveState(void *userPlugin, const void *stateCtx, cplug_writeProc writeProc)
{
  saveState((ComplexPlugin *)userPlugin, stateCtx, writeProc);
}

// main-thread
void cplug_loadState(void *userPlugin, const void *stateCtx, cplug_readProc readProc)
{
  static constexpr auto kCapacityIncrease = COMPLEX_KB(4);

  usize capacity = kCapacityIncrease;
  usize size{};
  char *buffer = arranew(getLocalScratch(), char, capacity, {});
  defer { utils::Pool::remove(buffer); };

  while (true)
  {
    usize readBytes = readProc(stateCtx, buffer + size, capacity - size);
    size += readBytes;
    if (!readBytes)
      break;

    if (size >= capacity)
    {
      capacity += kCapacityIncrease;
      buffer = (char *)utils::Pool::resize(buffer, capacity, true);
    }
  }

  loadState((ComplexPlugin *)userPlugin, { buffer, size });
}


// main-thread
void cplug_setSampleRateAndBlockSize(void *ptr, double sampleRate, uint32_t maxBlockSize)
{
  ((ComplexPlugin *)ptr)->initialise((float)sampleRate, maxBlockSize);
}

// audio-thread
void cplug_process(void *ptr, CplugProcessContext *ctx)
{
  utils::ScopedNoDenormals noDenormals{};

  auto *plugin = (ComplexPlugin *)ptr;
  auto g = plugin->acquireProcessingLock();

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
      auto state = ((ComplexPlugin *)ptr)->state_;
      state->parameterBridges[event.parameter.id].setValue((float)event.parameter.value);
      break;
    }
    case CPLUG_EVENT_MIDI:
      break;
    case CPLUG_EVENT_PROCESS_AUDIO:
    {
    #ifndef COMPLEX_STANDALONE
      float **in = ctx->getAudioInput(ctx, 0);
      float **out = ctx->getAudioOutput(ctx, 0);
      COMPLEX_ASSERT(in != nullptr);
      COMPLEX_ASSERT(in[0] != nullptr);
      COMPLEX_ASSERT(in[1] != nullptr);
      COMPLEX_ASSERT(out != nullptr);
      COMPLEX_ASSERT(out[0] != nullptr);
      COMPLEX_ASSERT(out[1] != nullptr);

      plugin->process(in, out, event.processAudio.endFrame - frame, 2, 2);
    #endif
      // If your plugin does not require sample accurate processing, use this line below to break the loop
      frame = event.processAudio.endFrame;
    } break;
    default:
      break;
    }
  }
}
