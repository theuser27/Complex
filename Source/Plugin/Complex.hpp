
// Created: 2021-05-23 00:20:15

#pragma once

#include "Framework/fourier_transform.hpp"
#include "Framework/utils.hpp"
#include "Framework/constants.hpp"
#include "Framework/memory.hpp"
#include "Framework/parameter_types.hpp"

extern "C"
{
  typedef struct CplugHostContext CplugHostContext;
  typedef i64 (*cplug_writeProc)(const void *stateCtx, void *writePos, usize numBytesToWrite);
}

namespace Generation
{
  class Processor;
  class SoundEngine;
}

namespace Framework
{
  struct SimdBuffer;
  class ParameterValue;
  class ParameterModulator;
  class ParameterBridge;
}

namespace Interface
{
  class Renderer;
}

namespace Plugin
{
  struct State;

  struct ComplexPlugin
  {
    ComplexPlugin(usize parameterMappings, u32 inSidechains, u32 outSidechains, 
      usize undoSteps, CplugHostContext *hostContext);
    ~ComplexPlugin();

    void initialise(float sampleRate, u32 samplesPerBlock);
    void process(float *const *in, float *const *out, u32 numSamples,
      u32 numInputs, u32 numOutputs);
    
    Interface::Renderer &getRenderer();

    void rescanLatency();

    float getSampleRate() const { return sampleRate.load(satomi::memory_order_acquire); }
    u32 getSamplesPerBlock() const { return samplesPerBlock.load(satomi::memory_order_acquire); }

    void undo();
    void redo();

    utils::sp<State> loadDefaultPreset();
    utils::sp<State> exchangeStates(utils::sp<State> state);

    Framework::FFT *
    getFFTConverter(u32 minOrder, u32 maxOrder)
    {
      fft.extendFFTOrders(minOrder, maxOrder);
      return &fft;
    }

    // quick and dirty spinlock to ensure things are executed outside of an audio callback
    utils::ScopedLock acquireProcessingLock(bool isExclusive = true)
    {
      while (processingLock.lock.load(satomi::memory_order_relaxed) != 0)
        utils::millisleep();
      return utils::ScopedLock{ processingLock, isExclusive, utils::WaitMechanism::Spin };
    }

    // not atomic because these are only set at plugin instantiation
    const u32 inSidechains = 0;
    const u32 outSidechains = 0;
    const usize parameterCount = 0;

    // might be updated on any thread hence atomic
    satomi::atomic<u32> samplesPerBlock = 0;
    satomi::atomic<float> sampleRate = kDefaultSampleRate;
    // if any updates are supposed to happen to the processing tree/undoManager
    // the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
    mutable utils::ReentrantLock<i32> processingLock{ 0, {} };
    satomi::atomic<u32> latency{};
    satomi::atomic<bool> hasLatencyChanged{};
    bool wasStateInitialised{};

    Framework::FFT fft{};
    utils::sp<State> state_;
    
    Framework::UndoManager undoManager;

    CplugHostContext *hostContext;

    // pointer to the main processing engine
    Interface::Renderer *rendererInstance = nullptr;
  };
}

namespace Plugin
{
  struct State
  {
    State(ComplexPlugin *plugin);
    ~State();

    Framework::ProcessorMetadata *findProcessorMetadata(uuid id) const;

    Generation::Processor *getProcessor(u64 processorId) const;
    // creates a default processor or loads processor from save if jsonData != nullptr
    Generation::Processor *createProcessor(uuid processorId, void *jsonData = nullptr);
    Generation::Processor *copyProcessor(Generation::Processor *processor);
    void deleteProcessor(Generation::Processor *processor);

    utils::pair<u32, u32> getMinMaxFFTOrder() const;
    u32 getMaxBinCount() const;

    Framework::ParameterValue *getProcessorParameter(u64 parentProcessorId, uuid parameterId) const;
    // see IndexedData::dynamicUpdateUuid
    void registerDynamicParameter(Framework::ParameterValue *parameter);
    void updateDynamicParameters(uuid reason);
    void updateAllDynamicParameters();
    void createDynamicParameters();

    Generation::SoundEngine &getSoundEngine() { return *soundEngine; }
    float getOverlap();
    u32 getFFTSize();
    u32 getBlockPosition();
    u32 getLaneCount() const;
    
    struct Thread
    {
      Thread() = default;
      Thread(Thread &&other) noexcept : thread{ COMPLEX_MOVE(other.thread) },
        shouldStop{ other.shouldStop } { }
      ~Thread() { stop(); }

      bool 
      start(const auto &function)
      {
        if (thread != utils::thread{})
          return false;

        shouldStop = anew(globalArena, satomi::atomic<bool>, {});

        thread = [this, function]() { function(*shouldStop); };
        return true;
      }

      bool 
      stop(int *exitCode = nullptr)
      {
        if (thread == utils::thread{})
          return true;

        shouldStop->store(false, satomi::memory_order_release);
        bool success = thread.join(exitCode);
        utils::bumpArena::remove(shouldStop);
        return success;
      }

      utils::thread thread{};
      utils::typeInfo reservationTag{};
      satomi::atomic<bool> *shouldStop{};
    };

    Thread &reserveFreeWorker(utils::typeInfo reservationTag);

    ComplexPlugin *plugin;
    Generation::SoundEngine *soundEngine = nullptr;

    // outward facing parameters, which can be mapped to in-plugin parameters
    utils::span<Framework::ParameterBridge> parameterBridges{};

    // used to give out non-repeating ids for all processors
    // 0 is reserved to mean "uninitialised"
    u64 stateIdCounter{};

    Framework::FFT *fft{};

    // modulators inside the plugin
    utils::vector<Framework::ParameterModulator *> parameterModulators{};
    // parameters that receive updates upon various plugin changes
    utils::vector<utils::pair<Framework::IndexedData *, Framework::ParameterValue *>> dynamicParameters{};
    // the processor tree is stored in a flattened map
    utils::vector_map<u64, Generation::Processor *> allProcessors{};

    utils::vector_map<uuid, Framework::IndexedData *> dynamicOptions{};

    Framework::PluginStructure pluginStructure{};

    utils::bumpArena *processorStorage{};
    utils::bumpArena *miscStorage{};

    utils::vector<Thread> workers{};
  };
}
