
// Created: 2021-05-23 00:20:15

#pragma once

#include "Framework/fourier_transform.hpp"
#include "Framework/utils.hpp"
#include "Framework/constants.hpp"
#include "Framework/sync_primitives.hpp"
#include "Framework/memory.hpp"
#include "Framework/parameter_types.hpp"

extern "C"
{
  typedef struct CplugHostContext CplugHostContext;
  typedef i64 (*cplug_writeProc)(const void *stateCtx, void *writePos, usize numBytesToWrite);
}

namespace Generation
{
  class BaseProcessor;
  class SoundEngine;
  class EffectsState;
}

namespace Framework
{
  struct SimdBuffer;
  class ParameterValue;
  class ParameterModulator;
  class ParameterBridge;

  struct UndoAction
  {
    // mandatory to implement
    void (*redo)(UndoAction *) = nullptr;
    void (*undo)(UndoAction *) = nullptr;

    // free to implement or not
    void (*destructor)(UndoAction *) = nullptr;
    // Allows multiple actions to be coalesced into a single action object, to reduce storage space.
    // The combined action can be the current, next or an entire new action
    UndoAction *(*combineActions)(utils::bumpArena *transaction,
      UndoAction *currentAction, UndoAction *nextAction) = nullptr;

  private:
    UndoAction *next{};
    friend class UndoManager;
  };

  struct AddProcessorUpdate : public Framework::UndoAction
  {
    AddProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex, Generation::BaseProcessor &newProcessor);

    Plugin::State &state_;
    Generation::BaseProcessor *addedProcessor_;
    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
  };

  struct MoveProcessorUpdate final : public Framework::UndoAction
  {
    MoveProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex, u64 sourceStateId, usize sourceSubProcessorIndex);

    Plugin::State &state_;

    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
    u64 sourceStateId_;
    usize sourceSubProcessorIndex_;
  };
  
  struct DeleteProcessorUpdate final : public UndoAction
  {
    DeleteProcessorUpdate(Plugin::State &state, u64 destinationStateId,
      usize destinationSubProcessorIndex);

    Plugin::State &state_;

    Generation::BaseProcessor *deletedProcessor_ = nullptr;

    u64 destinationStateId_;
    usize destinationSubProcessorIndex_;
  };

  class UndoManager
  {
  public:
    ~UndoManager();

    UndoManager(usize transactionsToKeep);

    void clear();
    void setTransationStorage(usize transactionsToKeep);

    // Performs an action and adds it to the undo history list.
    bool perform(UndoAction *action, bool performOnAdd = true);

    // Starts a new group of actions that together will be treated as a single transaction.
    [[nodiscard]] utils::bumpArena *beginNewTransaction();
    [[nodiscard]] utils::bumpArena *getCurrentTransaction() { return transactions[currentIndex].first; }

    bool canUndo() const { return undoActionsCount; }
    bool canRedo() const { return redoActionsCount; }

    // Tries to roll-back the last transaction, 
    // returns false if there aren't any transactions to undo
    // 
    // if undoCurrentTransactionOnly == true, then the transaction index won't be changed
    // when undone and things can immediately be added to the current transaction
    bool undo(bool undoCurrentTransactionOnly = false);

    // Tries to redo the last transaction that was undone.
    // returns false if there aren't any transactions to redo
    bool redo();

    // Returns true if the caller code is in the middle of an undo or redo action.
    bool isPerformingUndoRedo() const noexcept { return isInsideUndoRedoCall; }

  private:
    utils::bumpArena *storage{};
    utils::span<utils::pair<utils::bumpArena *, UndoAction *>> transactions{};
    usize currentIndex = 0, undoActionsCount = 0, redoActionsCount = 0;
    bool isInsideUndoRedoCall = false;
  };
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
      u32 numInputs, u32 numOutputs) noexcept;
    
    Interface::Renderer &getRenderer();

    void rescanLatency();

    float getSampleRate() const noexcept { return sampleRate.load(satomi::memory_order_acquire); }
    u32 getSamplesPerBlock() const noexcept { return samplesPerBlock.load(satomi::memory_order_acquire); }

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
  // 0 is reserved to mean "uninitialised" and
  // 1 is reserved for the State itself
  static constexpr u64 kStateId = 1;

  struct State
  {
    State(ComplexPlugin *plugin);
    ~State();

    void expandIfNecessary();

    Framework::ProcessorMetadata *findProcessorMetadata(uuid id) const;

    Generation::BaseProcessor *getProcessor(u64 processorId) const noexcept;
    // creates a default processor or loads processor from save if jsonData != nullptr
    Generation::BaseProcessor *createProcessor(uuid processorId, void *jsonData = nullptr);
    Generation::BaseProcessor *copyProcessor(Generation::BaseProcessor *processor);
    void deleteProcessor(Generation::BaseProcessor *processor) noexcept;

    utils::pair<u32, u32> getMinMaxFFTOrder() const noexcept { return { minFFTOrder, maxFFTOrder }; }
    u32 getMaxBinCount() const noexcept { return (1 << (maxFFTOrder - 1)) + 1; }

    Framework::ParameterValue *getProcessorParameter(u64 parentProcessorId, uuid parameterId) const noexcept;
    // see IndexedData::dynamicUpdateUuid
    void registerDynamicParameter(Framework::ParameterValue *parameter);
    void updateDynamicParameters(uuid reason) noexcept;

    Generation::SoundEngine &getSoundEngine() noexcept { return *soundEngine; }
    float getOverlap() noexcept;
    u32 getFFTSize() noexcept;
    u32 getBlockPosition() noexcept;
    u32 getLaneCount() const;
    

    struct Thread
    {
      Thread() = default;
      Thread(Thread &&other) noexcept : thread{ COMPLEX_MOVE(other.thread) },
        shouldStop{ other.shouldStop.load(satomi::memory_order_relaxed) } { }
      ~Thread() noexcept { stop(); }

      bool start(const auto &function)
      {
        if (thread != utils::thread{})
          return false;

        thread = [this, function]() { function(shouldStop); };
        return true;
      }

      bool stop()
      {
        if (thread != utils::thread{})
        {
          shouldStop.store(false, satomi::memory_order_release);
          thread.join();
        }
        return true;
      }

      utils::thread thread{};
      utils::typeInfo reservationTag{};
      satomi::atomic<bool> shouldStop = false;
    };

    Thread &reserveFreeWorker(utils::typeInfo reservationTag);

    ComplexPlugin *plugin;
    Generation::SoundEngine *soundEngine = nullptr;

    // outward facing parameters, which can be mapped to in-plugin parameters
    utils::span<Framework::ParameterBridge> parameterBridges{};

    // used to give out non-repeating ids for all processors
    u64 stateIdCounter = kStateId + 1;

    Framework::FFT *fft{};
    u32 minFFTOrder = kMinFFTOrder;
    u32 maxFFTOrder = kMaxFFTOrder;

    // modulators inside the plugin
    utils::vector<Framework::ParameterModulator *> parameterModulators{};
    // parameters that receive updates upon various plugin changes
    utils::vector<utils::pair<Framework::IndexedData *, Framework::ParameterValue *>> dynamicParameters{};
    // the processor tree is stored in a flattened map
    utils::vector_map<u64, Generation::BaseProcessor *> allProcessors{};

    Framework::PluginStructure pluginStructure{};

    utils::bumpArena *processorStorage{};
    utils::bumpArena *miscStorage{};

    utils::vector<Thread> workers{};
  };
}
