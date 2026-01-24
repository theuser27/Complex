/*
  ==============================================================================

    Complex.hpp
    Created: 23 May 2021 00:20:15
    Author:  theuser27

  ==============================================================================
*/

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
  class WaitingUpdate;

  class UndoableAction
  {
  public:
    u32 type{};
    virtual ~UndoableAction() = default;

    virtual bool perform() = 0;
    virtual bool undo() = 0;

    /// Allows multiple actions to be coalesced into a single action object, to reduce storage space.
    /// The combined action can be the current, next or an entire new action
    virtual UndoableAction *combineActions([[maybe_unused]] UndoableAction *nextAction) { return nullptr; }
  };

  class UndoManager
  {
  public:
    UndoManager(usize transactionsToKeep);

    ~UndoManager();

    void clearUndoHistory();
    void setTransationStorage(usize transactionsToKeep);

    /// Performs an action and adds it to the undo history list.
    bool perform(UndoableAction *action);

    /// Starts a new group of actions that together will be treated as a single transaction.
    void beginNewTransaction();

    // Returns true if there's at least one action in the list to undo.
    bool canUndo() const { return undoActionsCount; }

    /// Tries to roll-back the last transaction, 
    /// returns true if the transaction can be undone, and false if it fails, or
    /// if there aren't any transactions to undo
    /// 
    /// if undoCurrentTransactionOnly == true, then the transaction index won't be changed
    /// when undone and things can immediately be added to the current transaction
    bool undo(bool undoCurrentTransactionOnly = false);

    /// Returns the number of UndoableAction objects that have been performed during the
    /// transaction that is currently open.
    usize getNumActionsInCurrentTransaction() const { return transactions[currentIndex].size(); }

    /// Returns true if there's at least one action in the list to redo.
    bool canRedo() const { return redoActionsCount; }

    /// Tries to redo the last transaction that was undone.
    /// returns true if the transaction can be redone, and false if it fails, or
    /// if there aren't any transactions to redo
    bool redo();

    /// Returns true if the caller code is in the middle of an undo or redo action.
    bool isPerformingUndoRedo() const noexcept { return isInsideUndoRedoCall; }

  private:
    utils::vector<utils::vector<utils::up<UndoableAction>>> transactions{};
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

    float getSampleRate() const noexcept { return sampleRate_.load(satomi::memory_order_acquire); }
    u32 getSamplesPerBlock() const noexcept { return samplesPerBlock_.load(satomi::memory_order_acquire); }

    void pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction = true);
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
    utils::ScopedLock acquireProcessingLock(bool isExclusive)
    {
      while (processingLock_.lock.load(satomi::memory_order_relaxed) != 0)
        utils::millisleep();
      return utils::ScopedLock{ processingLock_, isExclusive, utils::WaitMechanism::Spin };
    }

    // not atomic because these are only set at plugin instantiation
    const u32 inSidechains = 0;
    const u32 outSidechains = 0;
    const usize parameterCount = 0;

    // might be updated on any thread hence atomic
    satomi::atomic<u32> samplesPerBlock_ = 0;
    satomi::atomic<float> sampleRate_ = kDefaultSampleRate;
    // if any updates are supposed to happen to the processing tree/undoManager
    // the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
    mutable utils::ReentrantLock<i32> processingLock_{ 0, {} };
    satomi::atomic<u32> latency{};
    satomi::atomic<bool> hasLatencyChanged{};
    bool wasStateInitialised{};

    Framework::FFT fft{};
    utils::sp<State> state_;
    
    Framework::UndoManager undoManager_;

    CplugHostContext *hostContext;

    // pointer to the main processing engine
    Interface::Renderer *rendererInstance_ = nullptr;
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
      usize reservationTag{};
      satomi::atomic<bool> shouldStop = false;
    };

    Thread &reserveFreeWorker(usize reservationTag);

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
