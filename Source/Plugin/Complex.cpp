
// Created: 2021-05-23 00:20:15

#include <stdlib.h> // offsetof

#include "Complex.hpp"

#include "Third Party/cplug/cplug.h"

#include "Framework/load_save.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Framework/parameter_value.hpp"
#include "Generation/Effects.hpp"
#include "Generation/SoundEngine.hpp"
#include "Interface/LookAndFeel/Graphics.hpp"
#include "Interface/LookAndFeel/Component.hpp"
#include "Interface/Sections/MainInterface.hpp"

Framework::ExecutableStaticData executableStaticData{};

namespace
{
  void initialisePluginStructure()
  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    executableStaticData.arena = utils::bumpArena::create(COMPLEX_MB(1), COMPLEX_KB(64));
    executableStaticData.structure.arena = utils::bumpArena::create(COMPLEX_MB(1), COMPLEX_KB(64));

    auto pushString = [&](utils::string_view string)
    {
      if (string.empty())
        return string;

      for (auto *element = executableStaticData.strings; element; element = element->next)
        if (element->object == string)
          return element->object;

      auto *node = anew(executableStaticData.arena, utils::sll<utils::string_view>, { string });
      if (executableStaticData.strings)
        node->next = executableStaticData.strings;
      executableStaticData.strings = node;

      return node->object;
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

    {
      char buffer[512];
      int pathSize = xfiles_get_user_directory(buffer, sizeof(buffer), XFILES_USER_DIRECTORY_APPDATA);
      auto string = utils::string::create(getLocalScratch(), "%*s" XFILES_DIR_STR "%s", pathSize, buffer, CPLUG_PLUGIN_NAME);
      executableStaticData.configFolderPath = pushString(string);
    }

    executableStaticData.structure.metadata = (Framework::ProcessorMetadata *)initialiseTypeStructure<
      Generation::SoundEngine>(nullptr, executableStaticData.structure);

    recurseProcessorStrings(recurseProcessorStrings, executableStaticData.structure.metadata);
  }

  void deinitialisePluginStructure()
  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    utils::bumpArena::destroy(executableStaticData.structure.arena);

    executableStaticData.strings = {};
    utils::bumpArena::destroy(executableStaticData.arena);
  }
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

    auto *memory = utils::bumpArena::insert(executableStaticData.arena, sizeof(utils::sll<utils::string_view>) +
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

namespace Framework
{
  void ParameterBridge::notifyParameterChange()
  {
    auto hostContext = Interface::getUiRelated()->plugin.hostContext;
    hostContext->rescan(hostContext,
      CPLUG_FLAG_RESCAN_PARAM_VALUES | CPLUG_FLAG_RESCAN_PARAM_NAMES | CPLUG_FLAG_RESCAN_PARAM_METADATA);
  }

  void ParameterBridge::beginChangeGesture()
  {
    if (!state->plugin->renderer.gui)
      return;

    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_BEGIN, .id = (uint32_t)parameterIndex } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }

  void ParameterBridge::setValueFromUI(float newValue)
  {
    value_.store(newValue, satomi::memory_order_release);

    if (!state->plugin->renderer.gui)
      return;

    auto event = CplugEvent{ .parameter = { .type = CPLUG_EVENT_PARAM_CHANGE_UPDATE,
      .id = (uint32_t)parameterIndex, .value = newValue } };
    state->plugin->hostContext->sendParamEvent(state->plugin->hostContext, &event);
  }

  void ParameterBridge::endChangeGesture()
  {
    if (!state->plugin->renderer.gui)
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
    uiStorage = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(4));

    gui = anew(uiStorage, Interface::MainInterface, {});
    gui->arena = uiStorage;

    allProcessors.data = { { miscStorage, false }, 64 };
    parameterModulators = { { miscStorage, false }, 32 };
    dynamicParameters = { { miscStorage, false }, 32 };
    workers = { { miscStorage, false }, 16 };
    cachedHotreloadSymbols.data = { { miscStorage, false }, 16 };

    createDynamicParameters();

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
    pluginStructure.metadata = executableStaticData.structure.metadata;
  }

  State::~State()
  {
    for (auto &worker : workers)
      worker.stop();

    utils::bumpArena::destroy(uiStorage);
    utils::bumpArena::destroy(processorStorage);
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

  Generation::Processor *
  State::getProcessor(u64 processorId) const
  {
    auto processorIter = allProcessors.find(processorId);
    return (processorIter != allProcessors.data.end()) ?
      processorIter->second : nullptr;
  }

  // check if processor creation causes dynamic parameters to change
  void State::registerProcessorForDynamicParameters(Generation::Processor *processor)
  {
    using namespace Framework;

    auto *arena = miscStorage;

    if (processor->metadata->id == Generation::Processors::EffectsLane)
    {
      auto iter = dynamicOptions.find(Framework::ParameterChangeReason::laneSources);
      auto alreadyExists = utils::findIf(iter->second, [&](IndexedData *option) { return option->stateId == processor->stateId; });
      if (!alreadyExists)
      {
        iter->second->addChildren({{ anew(arena, IndexedData,
          { .id = processor->metadata->id, .flags = IndexedData::StateIdFlag, .stateId = processor->stateId }) }});

        updateDynamicParameters(Framework::ParameterChangeReason::laneSources);
      }
    }
  }

  void State::deregisterProcessorForDynamicParameters(Generation::Processor *processor)
  {
    using namespace Framework;

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

        utils::bumpArena::remove(&item);
        return false;
      });
    }

    if (needsUpdate)
      updateAllDynamicParameters();
  }

  Generation::Processor *
  State::createProcessor(uuid processorId, void *jsonData)
  {
    auto *metadata = findProcessorMetadata(processorId);
    Generation::Processor *processor = metadata->create(this, metadata, nullptr, jsonData);

    allProcessors.add(processor->stateId, processor);

    return processor;
  }

  void State::deleteProcessor(Generation::Processor *processor)
  {
    COMPLEX_ASSERT(processor->state == this);
    COMPLEX_ASSERT(processor->stateId != 0);

    for (auto child = processor->children; child; child = child->next)
      deleteProcessor(child);

    deregisterProcessorForDynamicParameters(processor);

    // TODO: free all registered resources
    // TODO: unlink all parameters from their UIs and detach them from the parameter bridges

    if (processor->component)
    {
      if (processor->component->parent)
        processor->component->parent->removeChildComponent(processor->component);
      Interface::deleteComponent(processor->component);
    }

    allProcessors.data.erase({ processor->stateId, processor });
    utils::bumpArena::remove(processor->arena);
  }

  Generation::Processor *
  State::copyProcessor(Generation::Processor *processor)
  {
    utils::ScopedLock guard{};
    if (this == plugin->state_.get())
      guard = plugin->acquireProcessingLock(true);

    return processor->createCopy();
  }

  utils::pair<u32, u32> State::getMinMaxFFTOrder() const { return soundEngine->getMinMaxFFTOrder(); }
  u32 State::getMaxBinCount() const { return soundEngine->getMaxBinCount(); }

  Framework::ParameterValue *
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
    arena{ utils::bumpArena::create(COMPLEX_MB(16), COMPLEX_MB(2)) }, undoManager{ arena, undoSteps },
    hostContext{ hostContext }, renderer{ *this }
  {
    fft.arena = arena;
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

  void ComplexPlugin::process(float *const *in, float *const *out,
    u32 numSamples, u32 numInputs, u32 numOutputs)
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
}

struct TlsContext
{
  utils::bumpArena *localScratch{};
  utils::bumpArena *localMallocArena{};
  Interface::InterfaceRelated *uiRelated{};
};

void *
utils::createTlsContext()
{
  auto *context = anew(globalArena, TlsContext, {});
  
  context->localScratch = utils::bumpArena::create(COMPLEX_MB(4), COMPLEX_KB(128));

  return context;
}

void utils::destroyTlsContext(void *context)
{
  auto *tls = (TlsContext *)context;
  utils::bumpArena::destroy(tls->localScratch);
  tls->localScratch = nullptr;
}

utils::bumpArena *&
getLocalScratch()
{
  return ((TlsContext *)utils::getTls())->localScratch;
}

utils::bumpArena *&
getLocalMallocArena()
{
  return ((TlsContext *)utils::getTls())->localMallocArena;
}

Interface::InterfaceRelated *&
Interface::getUiRelated()
{
  return ((TlsContext *)utils::getTls())->uiRelated;
}

void initialiseCJSONHooks();

// DLL-wide global memmory pool, deallocated when DLL is unloaded
constinit utils::bumpArena *globalArena = nullptr;

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
  Framework::LoadSave::getStartupParameters(parameterMappings, inSidechains, outSidechains, undoSteps);

  auto *plugin = anew(globalArena, utils::sll<Plugin::ComplexPlugin>, 
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
  auto *plugin = (Plugin::ComplexPlugin *)ptr;

  {
    utils::ScopedLock g{ executableStaticData.readWriteLock, true, utils::WaitMechanism::WaitNotify };

    utils::sll<Plugin::ComplexPlugin> *node = executableStaticData.pluginInstances, *lastNode = nullptr;

    while (&node->object != plugin)
    {
      lastNode = node;
      node = node->next;
    }

    ((lastNode) ? lastNode->next : executableStaticData.pluginInstances) = node->next;
  }

  // warning: this only works because the plugin is the first member
  utils::bumpArena::remove(plugin);
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
      *max = details.options->valueCount - 1;
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
  static constexpr auto kCapacityIncrease = COMPLEX_KB(4);

  usize capacity = kCapacityIncrease;
  usize size{};
  char *buffer = arranew(getLocalScratch(), char, capacity, {});
  while (true)
  {
    usize readBytes = readProc(stateCtx, buffer + size, capacity - size);
    size += readBytes;
    if (!readBytes)
      break;

    if (size >= capacity)
    {
      capacity += kCapacityIncrease;
      buffer = (char *)utils::bumpArena::resize(buffer, capacity, true);
    }
  }

  loadState((Plugin::ComplexPlugin *)userPlugin, { buffer, size });
  utils::bumpArena::remove(buffer);
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
