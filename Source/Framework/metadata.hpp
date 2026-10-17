
// Created: 2025-03-26 11:20:23

#pragma once

#include "constants.hpp"
#include "utils.hpp"

extern "C" typedef struct NSVGimage NSVGimage;

struct State;
struct ComplexPlugin;

class Parameter;

namespace utils
{
  struct i4;
  struct f4;
}

class Processor;
void resetProcessorGeneric(Processor *processor);
void serialiseProcessorGeneric(const Processor *processor,
  void *jsonData, utils::span<Parameter *> parametersToSerialise);

class Component;

// used for updating parameters
enum class UpdateFlag : u8 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };

enum class ParameterAssignment : u8 { AudioThread, MainThread, Skip };

// symmetric types apply the flipped curve to negative values
// all x values are normalised

COMPLEX_ENUM(ParameterScale,
  (            Toggle, 1757693064462),    // round(x)
  (           Indexed, 1757693075999),    // round(x * (max - min))
  (            Linear, 1757693088052),    // x * (max - min) + min
  (             Clamp, 1757693157266),    // clamp(x, min, max)
  (         Quadratic, 1757693166022),    // x ^ 2 * (max - min) + min
  (  ReverseQuadratic, 1757693171422),    // (x - 1) ^ 2 * sgn(x - 1) * (max - min) + max
  (SymmetricQuadratic, 1757693229769),    // ((x - 0.5) ^ 2 * sgn(x - 0.5) + 0.5) * 2 * (max - min) + min
  (             Cubic, 1757693243184),    // x ^ 3
  (      ReverseCubic, 1757693263245),    // (x - 1) ^ 3 * (max - min) + max
  (    SymmetricCubic, 1757693277746),    // (2 * x - 1) ^ 3
  (          Loudness, 1757693294934),    // 20 * log10(x)
  ( SymmetricLoudness, 1757693297826),    // 20 * log10(abs(x)) * sgn(x)
  (         Frequency, 1757693375693),    // (sampleRate / 2 * minFrequency) ^ x
  (SymmetricFrequency, 1757693385953),    // (sampleRate / 2 * minFrequency) ^ (abs(x)) * sgn(x)
);

COMPLEX_ENUM(ParameterChangeReason,
  ( inputSidechain, 1757682036260),
  (outputSidechain, 1757682033430),
  (    laneSources, 1757682031418),
);

struct ProcessorMetadata;
struct ParameterMetadata;

struct IndexedData
{
  enum class Flags { NoneFlag, ProcessorFlag, ParameterFlag, StateIdFlag, SVGData };
  using enum Flags;

  utils::string_view displayName{};       // user-readable name for given parameter value
  uuid id{};                              // uuid for parameter value

  IndexedData *parent{};
  IndexedData *children{};
  IndexedData *next{};
  u32 childrenCount{};

  u32 valueCount = 1;                     // how many choosable values are there inside children (or itself if no children)
  Flags flags{};
  u64 userFlags{};
  uuid dynamicUpdateUuid{};               // this uuid is used to register for in the State
                                          //   these updates will happen only if the parameter is not mapped/modulated
  union
  {
    ProcessorMetadata *processorMetadata;
    ParameterMetadata *parameterMetadata;
    u64 stateId;
    NSVGimage *svgData;
  };

  bool
  operator==(const IndexedData &other) const
  {
    return id == other.id && flags == other.flags && userFlags == other.userFlags &&
      dynamicUpdateUuid == other.dynamicUpdateUuid && stateId == other.stateId;
  }
  operator IndexedData *() { return this; }

  bool canBeChosen() const { return id && valueCount && !children && !childrenCount; }
  utils::string_view getText() const;

  IndexedData &
  addChildren(utils::span<IndexedData *const> childrenToAdd, bool addAsUntracked = false)
  {
    if (!children)
    {
      valueCount = 0;
      childrenCount = 0;
    }

    // count all entries from the children to be inserted
    u32 addedValues = childrenToAdd[0]->valueCount;
    childrenToAdd[0]->parent = this;
    for (usize i = 1; i < childrenToAdd.size(); ++i)
    {
      COMPLEX_ASSERT(childrenToAdd[i]->id, "Option must have an id");
      addedValues += childrenToAdd[i]->valueCount;
      childrenToAdd[i]->parent = this;
      childrenToAdd[i - 1]->next = childrenToAdd[i];
    }

    auto *child = children;
    if (!addAsUntracked)
    {
      for (usize i = 0; childrenCount && i < childrenCount - 1; (++i), (child = child->next)) { }

      for (auto *p = this; p; p = p->parent)
        p->valueCount += addedValues;
      childrenCount += (u32)childrenToAdd.size();
    }
    else
      for (; child && child->next; child = child->next) { }

    if (!child)
      children = childrenToAdd[0];
    else
    {
      childrenToAdd[childrenToAdd.size() - 1]->next = child->next;
      child->next = childrenToAdd[0];
    }

    return *this;
  }

  void removeChild(IndexedData *option)
  {
    COMPLEX_ASSERT(option->parent == this);

    --childrenCount;
    for (auto *p = this; p; p = p->parent)
      p->valueCount -= option->valueCount;

    if (children == option)
      children = option->next;
    else
    {
      auto *child = children;
      for (; child->next != option; child = child->next) { }
      child->next = option->next;
    }
  }

  static IndexedData *
  deepCopy(auto *pool, IndexedData *options)
  {
    auto *newOption = anew(pool, IndexedData, { *options });
    if (newOption->children)
    {
      newOption->children = deepCopy(pool, newOption->children);
      for (auto child = newOption->children; child->next; child = child->next)
        child->next = deepCopy(pool, child->next);
    }

    return newOption;
  }

  static bool
  visit(IndexedData *data, const auto &predicate)
  {
    for (IndexedData *child = data->children, *previous = nullptr; child;
      (previous = child), (child = child->next))
    {
      if constexpr (requires { predicate(*child, previous); })
      {
        if (predicate(*child, previous))
          return true;
      }
      else
      {
        if (predicate(*child))
          return true;
      }

      if (visit(child, predicate))
        return true;
    }

    return false;
  }
};

struct ParameterDetails
{
  enum Flags : u8
  {
           None = 0,
         Stereo = 1 << 0,                                 // if parameter allows stereo modulation
    Modulatable = 1 << 1,                                 // if parameter allows modulation at all
    Automatable = 1 << 2,                                 // if parameter allows host automation
     Extensible = 1 << 3,                                 // if parameter's minimum/maximum/default values can change
     RoundToInt = 1 << 4,                                 // if parameter value must be rounded after scaling
  };

  utils::string_view displayName{};                       // name displayed to the user
  uuid id{};												                      // parameter id
  union
  {
    struct
    {
      float minValue;                                     // minimum scaled value
      float maxValue;                                     // maximum scaled value
      float defaultNormalisedValue;                       // default normalised value
    };
    struct
    {
      IndexedData *options;                               // (nested) list of options
      uuid defaultOptionId;                               // id of the default option
    };
  };

  ParameterScale::Value scale = ParameterScale::Linear;   // value skew factor
  utils::string_view displayUnits{};                      // "%", " db", etc.
  u32 flags = Modulatable | Automatable;                  // various flags
  UpdateFlag updateFlag = UpdateFlag::Realtime;           // at which point during processing the parameter is updated
  usize (*toString)(char *string,                         // string generator function for IndexedNumeric parameters
    usize stringSize, float value,
    const ParameterDetails &details) = nullptr;
  float (*valueChange)(Parameter *parameter,              // calculates new value based on desired and previous ones
    float newValue, ParameterAssignment type) = nullptr;
};


utils::pair<const IndexedData *, usize> getOptionFromValue(float scaledValue, const ParameterDetails &details);
float getValueFromOptionText(utils::string_view text, const ParameterDetails &details);
float getValueFromOptionId(uuid optionId, const ParameterDetails &details);
float getValueFromOption(const IndexedData *option, const ParameterDetails &details);

bool
iterateOverIndexedData(IndexedData *options, const auto &predicate)
{
  auto *option = options->children;
  u32 size = options->childrenCount;
  u32 index = 0;

  while (true)
  {
    if (predicate(*option))
      return true;

    // going down
    if (option->children && option->childrenCount)
      if (iterateOverIndexedData(option, predicate))
        return true;

    // going forward
    if (++index < size)
    {
      option = option->next;
      continue;
    }

    break;
  }

  return false;
}

// with skewOnly == true a normalised value between [0,1] or [-0.5, 0.5] is returned,
// depending on whether the parameter is bipolar
float scaleValue(float value, const ParameterDetails &details,
  float sampleRate = kDefaultSampleRate, bool scalePercent = false, bool skewOnly = false);

float unscaleValue(float value, const ParameterDetails &details,
  float sampleRate = kDefaultSampleRate, bool unscalePercent = true);

utils::f4 scaleValue(utils::f4 value,
  const ParameterDetails &details, float sampleRate = kDefaultSampleRate);


struct ParameterMetadata
{
  ParameterDetails details{};
  ParameterMetadata *next{};

  ParameterMetadata &
  operator,(ParameterMetadata &other)
  {
    if (next)
      *next, other;
    else
      next = &other;
    return *this;
  }
  operator ParameterMetadata *() { return this; }
};

struct ProcessorMetadata
{
  using CreateProcessorFn = Processor *(State *state,
    ProcessorMetadata *metadata, const void *toCopy, void *serialisedSave, void *extra);
  using CreateUIFn = Component *(Processor *processor);
  using ResetFn = void(Processor *processor);
  using SerialiseToJsonFn = void(const Processor *processor,
    void *jsonData, utils::span<Parameter *> parametersToSerialise);
  using AutomationMappingChangedFn = void(Processor *processor, 
    Parameter *parameter, bool isMapping);

  enum : u32
  {
    NoTag = 0U,
    ProcessorTag = 1U << 0,
    NoParameterValidationTag = 1U << 1,

    InactiveTag = 1U << 30,
    RuntimeAddedTag = 1U << 31,
  };

  u32 flags{};
  u32 userFlags{};
  uuid id{};
  utils::string_view name{};
  CreateProcessorFn *create{};
  CreateUIFn *createUI{};
  ResetFn *reset = resetProcessorGeneric;
  SerialiseToJsonFn *serialise = serialiseProcessorGeneric;
  AutomationMappingChangedFn *automationMappingChangedCallback{};
  ProcessorMetadata *parent{};
  ProcessorMetadata *next{};
  ProcessorMetadata *children{};
  ParameterMetadata *parameters{};
  u32 childrenCount{};
  u32 parameterCount{};
  const void *vtable{};

  operator ProcessorMetadata *() { return this; }

  static bool
  visit(ProcessorMetadata *data, const auto &predicate)
  {
    for (auto *child = data->children; child; child = child->next)
    {
      if (predicate(*child))
        return true;

      if (visit(child, predicate))
        return true;
    }

    return false;
  }

  bool
  acceptsChild(ProcessorMetadata *potentialChild)
  {
    auto *child = children;
    for (; child; child = child->next)
      if (potentialChild->id == child->id)
        break;

    return child;
  }

  ProcessorMetadata &
  computeCounts()
  {
    for (auto child = children; child; child = child->next)
    {
      ++childrenCount;
      child->parent = this;
    }

    for (auto parameter = parameters; parameter; parameter = parameter->next)
      ++parameterCount;

    return *this;
  }

  ProcessorMetadata &
  computeParameterCounts(const auto &lambda)
  {
    for (auto child = children; child; child = child->next)
    {
      ++childrenCount;
      child->parent = this;
    }

    lambda(*this);

    for (auto parameter = parameters; parameter; parameter = parameter->next)
      ++parameterCount;

    return *this;
  }
};

struct PluginStructure
{
  u64 versionNumber{};

  utils::Pool *pool{};

  ProcessorMetadata *metadata{};
  utils::vectornd<utils::sp<utils::Dylib>> loadedDynamicLibs{};

  utils::Pool *getNewPool(usize size)
  {
    return utils::Pool::createNested(pool, size);
  }
};

struct ExecutableMetadata
{
  utils::LockBlame<i32> readWriteLock{};
  PluginStructure structure{};
  utils::Pool *pool{};
  utils::string_view configFolderPath{};
  utils::sll<utils::string_view> *strings{};
  utils::sll<ComplexPlugin> *pluginInstances{};
};

inline usize
printToggleValues(char *string, usize size, float value, const ParameterDetails &)
{
  static constexpr utils::string_view toggleStrings[] = { "Off", "On" };
  size = utils::min(toggleStrings[(usize)value].size(), size - 1);
  ::memcpy(string, toggleStrings[(usize)value].data(), size);
  string[size] = '\0';
  return size;
}

extern ExecutableMetadata executableStaticData;

#define COMPLEX_STRUCTURE_PARAMETER(...) (*anew(pool, ParameterMetadata, { .details = { __VA_ARGS__ } }))
#define COMPLEX_STRUCTURE_PARAMETER_CUSTOM(...) (__VA_ARGS__(*anew(pool, ParameterMetadata, {})))

#define COMPLEX_STRUCTURE_PROCESSOR(T, nameString, idNumber, skinOverride, createFn, createUIFn, ...) (*anew(pool, ProcessorMetadata, \
  { .flags = ProcessorMetadata::ProcessorTag, .userFlags = skinOverride, .id = idNumber, .name = nameString, \
    .create = createFn, .createUI = createUIFn __VA_OPT__(,) __VA_ARGS__ }))

#define COMPLEX_STRUCTURE_INDEXED_DATA(...) anew(pool, IndexedData, { __VA_ARGS__ })

struct UndoAction
{
  // mandatory to implement
  void (*redo)(UndoAction *){};
  void (*undo)(UndoAction *){};

  // free to implement or not
  void (*destructor)(UndoAction *){};
  // Allows multiple actions to be coalesced into a single action object, to reduce storage space.
  // The combined action can be the current, next or an entire new action
  // Whichever gets returned, that will be stored
  // If no action gets returned, both will remain
  UndoAction *(*combineActions)(utils::Pool *transaction,
    UndoAction *currentAction, UndoAction *nextAction){};

  UndoAction *next{};
};

class UndoManager
{
public:
  static constexpr auto storagePerTransaction = 512;

  ~UndoManager();

  UndoManager(utils::Pool *parentArena, usize transactionsToKeep);

  void clear();
  void setTransationStorage(usize transactionsToKeep);

  // Performs an action and adds it to the undo history list.
  bool perform(UndoAction *action);

  // Starts a new group of actions that together will be treated as a single transaction.
  [[nodiscard]] utils::Pool *beginNewTransaction();
  [[nodiscard]] utils::pair<utils::Pool *, UndoAction *> getCurrent() { return transactions[currentIndex]; }

  bool canUndo() const { return undoActionsCount; }
  bool canRedo() const { return redoActionsCount; }

  // Tries to roll-back the last transaction,
  // returns false if there aren't any transactions to undo
  bool undo();

  // Tries to redo the last transaction that was undone.
  // returns false if there aren't any transactions to redo
  bool redo();

  utils::Pool *storage{};
  utils::span<utils::pair<utils::Pool *, UndoAction *>> transactions{};
  // once sized, currentIndex starts at the last index of the transactions array
  // this is so that it moves to 0 when beginNewTransaction() is called
  usize currentIndex = 0, undoActionsCount = 0, redoActionsCount = 0;
  // prevents reentrant calls because those would derail the system
  bool isInsideUndoRedoCall = false;
};
