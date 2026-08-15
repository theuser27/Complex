
// Created: 2021-05-23 00:20:15

#pragma once

#include "Framework/fourier_transform.hpp"
#include "Framework/utils.hpp"
#include "Framework/constants.hpp"
#include "Framework/memory.hpp"
#include "Framework/parameter_types.hpp"
#include "Interface/LookAndFeel/gui_utils.hpp"

extern "C"
{
  typedef struct CplugHostContext CplugHostContext;
  typedef i64 (*cplug_writeProc)(const void *stateCtx, void *writePos, usize numBytesToWrite);
  typedef struct PuglView PuglView;
  typedef void *xfiles_watch_context_t;
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
  struct MainInterface;

  using PersistentCallback = void(Component *component);
  struct MouseInteractions
  {
    Component *hovered = nullptr;
    Component *clicked = nullptr;
    Component *focused = nullptr;

    MouseEvent mouseState{};
  };

  class Renderer
  {
  public:
    enum TimerTypes { kTimerRefreshRate };

    struct PerfGraph
    {
      float values[128];
      int head;

      float getGraphAverage()
      {
        float avg = 0;
        for (usize i = 0; i < countof(values); i++)
          avg += values[i];
        
        return avg / (float)countof(values);
      }

      void updateGraph(float frameTime)
      {
        head = (head + 1) % countof(values);
        values[head] = frameTime;
      }
    };

    static constexpr double kMultiClickTimeout = 0.500; //ms

    Renderer(Plugin::ComplexPlugin &plugin);
    ~Renderer();

    PerfGraph graph{};

    Area<u32> area{};
    Area<u32> unscaledArea{};
    float pluginScale = 1.0f;
    float monitorScale = 1.0f;
    bool isInitialised = false;
    bool isVisible = false;
    bool isResizing = false;
    bool isHandlingOrphanedMouseEvents = false;
    bool hasEnteredResizeCorner = false;
    float fps =
    #if COMPLEX_WINDOWS
      64.0f; // Windows timers are complete ass
    #else
      60.0f;
    #endif

  #ifdef COMPLEX_HOTRELOAD_DIR
    xfiles_watch_context_t watchFileContext{};
  #endif

    PuglView *view_ = nullptr;

    Plugin::ComplexPlugin &plugin;
    InterfaceRelated generalData;

    MainInterface *gui{};

    Component *mouseHoveredComponent_ = nullptr;
    Component *mouseDownComponent_ = nullptr;
    Component *focusedComponent_ = nullptr;
    Component *dragAndDropComponent_ = nullptr;

    ModifierKeys mouseButtonsDown_{};
    ModifierKeys lastKeyboardMods_{};

    u64 numberOfFrames{};

    u8 numberOfClicks = 0;
    double lastMouseClickTime = 0.0;
    Point<i32> lastMousePosition_{ 0, 0 };
    Point<i32> lastMouseDownPosition_{ 0, 0 };

    utils::bumpArena *arena{};

    utils::vectormap<Component *, PersistentCallback *> callbacks{};

    bool renderDebugFps = true;

    void startUI();
    void stopUI();

    void resetGui(MainInterface *newGui);

    void resizeChange(bool isResizing);
    void moveFocusTo(Component *component);

    utils::span<const byte> getClipboard();
    void setClipboard(utils::span<const byte> data);

    void setMouseCursor(MouseCursorTypes cursorType);
    bool setUISize(u32 width, u32 height);
    void setUIScale(float newPluginScale);
    void setHoveredComponent(Component *component);
    void setClickedComponent(Component *component);
    void setFocusedComponent(Component *component);

    MouseEvent getRelativeEvent(MouseEvent e, Component *component);
    MouseInteractions getMouseInteractions();

    void refreshComponentUnderMouse(MouseEvent e, bool forceMouseMove = true);
    void handleMouseMove(MouseEvent e);
    void handleMouseDown(MouseEvent e);
    void handleMouseUp(MouseEvent e);
    void handleMouseEnter(MouseEvent e);
    void handleMouseLeave(MouseEvent e);
    void handleMouseWheel(MouseEvent e);

    void handleKeyPress(KeyPress k);

    // snapping to 0.25 scales for better rendering
    float getEffectiveScale() const { return ::roundf(pluginScale * monitorScale / kWindowScaleIncrements) * kWindowScaleIncrements; }

    void recalculateScale(bool forceResize = false);
    void checkFocusedComponent();
    void doSizingAndPositioning();
    void renderLoop(PuglView *view);
  };
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

    void rescanLatency();

    float getSampleRate() const { return sampleRate.load(satomi::memory_order_acquire); }
    u32 getSamplesPerBlock() const { return samplesPerBlock.load(satomi::memory_order_acquire); }

    utils::sp<State> loadDefaultPreset();
    utils::sp<State> exchangeStates(utils::sp<State> state);

    Framework::FFT *
    getFFTConverter(u32 minOrder, u32 maxOrder)
    {
      fft.extendFFTOrders(minOrder, maxOrder);
      return &fft;
    }

    // quick and dirty spinlock to ensure things are executed outside of an audio callback
    utils::ScopedLock
    acquireProcessingLock(bool isExclusive = true)
    {
      return utils::ScopedLock{ processingLock, isExclusive, utils::WaitMechanism::Sleep };
    }

    // not atomic because these are only set at plugin instantiation
    const u32 inSidechains = 0;
    const u32 outSidechains = 0;
    const usize parameterCount = 0;

    utils::bumpArena *arena{};

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

    Interface::Renderer renderer;
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

    void registerProcessorForDynamicParameters(Generation::Processor *processor);
    void deregisterProcessorForDynamicParameters(Generation::Processor *processor);

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

        thread = [shouldStop = shouldStop, function]() { function(*shouldStop); };
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

    void *getHotreloadSymbol(utils::string_view decoratedName);

    ComplexPlugin *plugin;
    Generation::SoundEngine *soundEngine = nullptr;

    // outward facing parameters, which can be mapped to in-plugin parameters
    utils::span<Framework::ParameterBridge> parameterBridges{};

    // used to give out non-repeating ids for all processors
    // 0 is reserved to mean "uninitialised"
    u64 stateIdCounter{};

    Framework::FFT *fft{};

    Interface::MainInterface *gui{};

    // modulators inside the plugin
    utils::vectornd<Framework::ParameterModulator *> parameterModulators{};
    // parameters that receive updates upon various plugin changes
    utils::vectornd<utils::pair<Framework::IndexedData *, Framework::ParameterValue *>> dynamicParameters{};
    // the processor tree is stored in a flattened map
    utils::vectormap<u64, Generation::Processor *> allProcessors{};

    utils::vectormap<uuid, Framework::IndexedData *> dynamicOptions{};

    utils::vectormap<utils::string_view, void *> cachedHotreloadSymbols{};
    Framework::PluginStructure pluginStructure{};

    utils::bumpArena *processorStorage{};
    utils::bumpArena *uiStorage{};
    utils::bumpArena *miscStorage{};

    utils::vectornd<Thread> workers{};
  };

  #define COMPLEX_HOTRELOAD_CHECK(state, functionName, ...)         \
    if (auto *symbol = (state)->getHotreloadSymbol(function_symbol);\
      symbol && symbol != ((void *)&functionName))                  \
    { return ((decltype(functionName) *)symbol)(__VA_ARGS__); }
}
