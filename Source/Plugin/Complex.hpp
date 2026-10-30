
// Created: 2021-05-23 00:20:15

#pragma once

#include "Framework/fourier_transform.hpp"
#include "Framework/utils.hpp"
#include "Framework/constants.hpp"
#include "Framework/metadata.hpp"
#include "Interface/gui_utils.hpp"

extern "C"
{
  typedef struct CplugHostContext CplugHostContext;
  typedef i64 (*cplug_writeProc)(const void *stateCtx, void *writePos, usize numBytesToWrite);
  typedef struct PuglView PuglView;
  typedef void *xfiles_watch_context_t;
}

class Processor;
class SoundEngine;

struct SimdBuffer;
class Parameter;
class ParameterModulator;
class HostParameter;

struct MainInterface;

// return true if the callback needs to be removed
using PersistentCallback = bool(Component *component);
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

  static constexpr double kMultiClickTimeout = 0.500; //ms

  Renderer(ComplexPlugin &plugin);
  ~Renderer();

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

  ComplexPlugin &plugin;
  ThreadLocals generalData;

  MainInterface *gui{};

  Component *mouseHoveredComponent_ = nullptr;
  Component *mouseDownComponent_ = nullptr;
  Component *focusedComponent_ = nullptr;
  Component *dragAndDropComponent_ = nullptr;

  ModifierKeys mouseButtonsDown_{};
  ModifierKeys lastKeyboardMods_{};

  u8 numberOfClicks = 0;
  double lastMouseClickTime = 0.0;
  Point<i32> lastMousePosition_{ 0, 0 };
  Point<i32> lastMouseDownPosition_{ 0, 0 };

  utils::Pool *pool{};

  utils::vectormap<utils::pair<Component *, u64>, utils::smallFn<PersistentCallback>> callbacks{};

  // deferred components that will set their own position
  utils::vector<utils::pair<Component *, utils::smallFn<bool(Component *)>>> customPlacement{};
  utils::vector<Component *> sortedSizesMin{};
  utils::vector<Component *> sortedSizesMax{};

  bool renderDebugFps = true;

  void startUI();
  void stopUI();

  void resetGui(MainInterface *newGui);

  void resizeChange(bool isResizing);

  utils::span<const byte> getClipboard();
  void setClipboard(utils::span<const byte> data);

  void setMouseCursor(MouseCursorTypes cursorType);
  bool setGuiSize(u32 width, u32 height);
  void setGuiScale(float newPluginScale);
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

  bool handleKeyPress(KeyPress k);

  // snapping to 0.25 scales for better rendering
  float getEffectiveScale() const { return ::roundf(pluginScale * monitorScale / kWindowScaleIncrements) * kWindowScaleIncrements; }

  void recalculateScale(bool forceResize = false);
  void checkFocusedComponent();
  void doSizingAndPositioning();
  void renderLoop(PuglView *view);
};

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
  void markPluginDirty();

  float getSampleRate() const { return sampleRate.load(satomi::memory_order_acquire); }
  u32 getSamplesPerBlock() const { return samplesPerBlock.load(satomi::memory_order_acquire); }

  utils::sp<State> loadDefaultPreset();
  utils::sp<State> exchangeStates(utils::sp<State> state);

  FFT *
  getFFTConverter(u32 minOrder, u32 maxOrder)
  {
    auto g = acquireProcessingLock();
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

  utils::Pool *pool{};

  // might be updated on any thread hence atomic
  satomi::atomic<u32> samplesPerBlock = 0;
  satomi::atomic<float> sampleRate = kDefaultSampleRate;
  // if any updates are supposed to happen to the processing tree/undoManager
  // the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
  mutable utils::ReentrantLock<i32> processingLock{ 0, {} };
  satomi::atomic<u32> latency{};
  satomi::atomic<bool> hasLatencyChanged{};
  bool wasStateInitialised{};

  FFT fft{};
  utils::sp<State> state_;

  UndoManager undoManager;

  CplugHostContext *hostContext;

  Renderer renderer;
};

struct State
{
  State(ComplexPlugin *plugin);
  ~State();

  ProcessorMetadata *findProcessorMetadata(uuid id) const;

  Processor *getProcessor(u64 processorId) const;
  // creates a default processor or loads processor from save if jsonData != nullptr
  Processor *createProcessor(uuid processorId, void *jsonData = nullptr, void *extra = nullptr);
  Processor *copyProcessor(Processor *processor);

  utils::pair<u32, u32> getMinMaxFFTOrder() const;
  u32 getMaxBinCount() const;

  Parameter *getProcessorParameter(u64 parentProcessorId, uuid parameterId) const;
  // see IndexedData::dynamicUpdateUuid
  void registerDynamicParameter(Parameter *parameter);
  void updateDynamicParameters(uuid reason);
  void updateAllDynamicParameters();
  void createDynamicParameters();

  void registerProcessorForDynamicParameters(Processor *processor);
  void deregisterProcessorForDynamicParameters(Processor *processor);

  void flushHostParameters();

  SoundEngine &getSoundEngine() { return *soundEngine; }
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

      shouldStop = anew(globalPool, satomi::atomic<bool>, {});

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
      utils::Pool::remove(shouldStop);
      return success;
    }

    utils::thread thread{};
    utils::typeInfo reservationTag{};
    satomi::atomic<bool> *shouldStop{};
  };

  Thread &reserveFreeWorker(utils::typeInfo reservationTag);

  void *getHotreloadSymbol(utils::string_view decoratedName);

  ComplexPlugin *plugin;
  SoundEngine *soundEngine = nullptr;

  // outward facing parameters, which can be mapped to in-plugin parameters
  utils::span<HostParameter> parameterBridges{};

  // used to give out non-repeating ids for all processors
  // 0 is reserved to mean "uninitialised"
  u64 stateIdCounter{};

  FFT *fft{};

  MainInterface *gui{};

  // modulators inside the plugin
  utils::vectornd<ParameterModulator *> parameterModulators{};
  // parameters that receive updates upon various plugin changes
  utils::vectornd<utils::pair<IndexedData *, Parameter *>> dynamicParameters{};
  // the processor tree is stored in a flattened map
  utils::vectormapnd<u64, Processor *> allProcessors{};

  utils::vectormapnd<uuid, IndexedData *> dynamicOptions{};

  utils::vectormapnd<utils::string_view, void *> cachedHotreloadSymbols{};
  PluginStructure pluginStructure{};

  utils::Pool *processorStorage{};
  utils::Pool *guiStorage{};
  utils::Pool *miscStorage{};

  utils::vectornd<Thread> workers{};
};

#define COMPLEX_HOTRELOAD_CHECK(state, functionName, ...)         \
  if (auto *symbol = (state)->getHotreloadSymbol(function_symbol);\
    symbol && symbol != ((void *)&functionName))                  \
  { return ((decltype(functionName) *)symbol)(__VA_ARGS__); }
