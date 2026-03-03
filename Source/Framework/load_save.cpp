
// Created: 2022-12-03 01:46:31

#include "load_save.hpp"

#include "cplug/config.h"

#include "Third Party/xhl/xhl_files.h"
#include "Third Party/cjson/cjson.h"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/EffectsState.hpp"
#include "Generation/EffectModules.hpp"
#include "Interface/LookAndFeel/Graphics.hpp"
#include "Interface/LookAndFeel/ui_constants.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"


// after finishing work with the arena and freeing it, reset this pointer
thread_local utils::bumpArena *jsonArena;

static void *
#ifdef COMPLEX_WINDOWS
__cdecl
#endif
cjsonAllocate(size_t size)
{
  COMPLEX_ASSERT(jsonArena, "Someone forgot to set the arena before using json");
  return utils::bumpArena::insert(jsonArena, size, alignof(void *));
}

// we free the arena at the end, so it's pointless to deallocate
static void
#ifdef COMPLEX_WINDOWS
__cdecl
#endif
cjsonFree(void *) { }

void initialiseCJSONHooks()
{
  cjson_Hooks hooks{ .malloc_fn = cjsonAllocate, .free_fn = cjsonFree };
  cjson_InitHooks(&hooks);
}

utils::string_view
findOrAddPermanentString(utils::string_view string);

namespace Framework::LoadSave
{
  utils::string getConfigFilePath(utils::string_view file)
  {
    static constexpr auto bufferSize = 512;
    static const auto path = []()
    {
      char buffer[bufferSize];
      int pathSize = xfiles_get_user_directory(buffer, sizeof(buffer), XFILES_USER_DIRECTORY_APPDATA);
      auto string = utils::string::create(localScratch, "%*s" XFILES_DIR_STR "%s", pathSize, buffer, CPLUG_PLUGIN_NAME);

      // adding the path to the long term plugin storage
      return findOrAddPermanentString(string);
    }();

    if (!xfiles_exists(path.data()))
      xfiles_create_directory(path.data());

    return utils::string::create(localScratch, "%v" XFILES_DIR_STR "%v", path, file);
  }
}

namespace
{
  thread_local utils::string *errorPath;

  void useConfigJson(const auto &predicate, bool save = false)
  {
    auto filePath = Framework::LoadSave::getConfigFilePath(CPLUG_PLUGIN_NAME ".config");

    if (!xfiles_exists(filePath.data()))
      xfiles_write(filePath.data(), "{}", sizeof("{}"));

    char *string;
    usize stringSize;
    if (!xfiles_read(filePath.data(), (void **)&string, &stringSize))
      return;

    jsonArena = utils::bumpArena::createNested(localScratch, COMPLEX_KB(16));

    cjson *json = cjson_Parse(string, stringSize);
    if (json)
      predicate(json);

    if (save)
    {
      size_t size;
      char *text = cjson_Print(json, &size);
      xfiles_write(filePath.data(), text, size);
    }

    XFILES_FREE(string);
    utils::bumpArena::destroy(jsonArena);
    jsonArena = nullptr;
  }

  void upgradeSave([[maybe_unused]] cjson *save)
  {
    // TODO: change all string ids to numeric ones
    // TODO: low/highBound and shiftBounds now belong to EffectModule
    // TODO: json's "indexed_data" is now "options"
  }
}

namespace Framework::LoadSave
{
  i32
  getModuleWidth()
  {
    i32 moduleWidth = Interface::kEffectModuleWidth;

    useConfigJson([&](cjson *json)
      {
        if (cjson *item = cjson_GetObjectItem(json, "module_width"))
          moduleWidth = utils::max<i32>(Interface::kMinWidth, (i32)item->vint);
      });

    return moduleWidth;
  }
  void getWindowSizeScale(u32 &windowWidth, u32 &windowHeight, float &windowScale)
  {
    windowWidth = Interface::kMinWidth;
    windowHeight = Interface::kMinHeight;
    windowScale = 1.0f;

    useConfigJson([&](cjson *json)
      {
        if (cjson *item = cjson_GetObjectItem(json, "window_width"))
          windowWidth = utils::max<u32>(Interface::kMinWidth, (u32)item->vuint);

        if (cjson *item = cjson_GetObjectItem(json, "window_height"))
          windowHeight = utils::max<u32>(Interface::kMinHeight, (u32)item->vuint);

        if (cjson *item = cjson_GetObjectItem(json, "window_scale"))
          windowScale = (float)item->vdouble;
      });
  }

  void getStartupParameters(usize &parameterMappings, usize &inSidechains, usize &outSidechains, usize &undoSteps)
  {
    parameterMappings = 100;
    inSidechains = 0;
    outSidechains = 0;
    undoSteps = 500;

    useConfigJson([&](cjson *json)
      {
        if (cjson *item = cjson_GetObjectItem(json, "parameter_count"))
          parameterMappings = (usize)item->vuint;

        if (cjson *item = cjson_GetObjectItem(json, "input_sidechains"))
          inSidechains = (usize)item->vuint;

        if (cjson *item = cjson_GetObjectItem(json, "output_sidechains"))
          outSidechains = (usize)item->vuint;

        if (cjson *item = cjson_GetObjectItem(json, "undo_steps"))
          undoSteps = (usize)item->vuint;
      });
  }

#define setJsonItem(data, key, type, value)           \
  {                                                   \
    if (cjson *item = cjson_GetObjectItem(data, key)) \
      cjson_Set(item, type, value);                   \
    else                                              \
    {                                                 \
      item = cjson_Create(cjson_Object);              \
      cjson_AddExistingTo(data, key, item);           \
    }                                                 \
  }

  void saveWindowSizeScale(u32 windowWidth, u32 windowHeight, float windowScale)
  {
    useConfigJson([&](cjson *data)
      {
        setJsonItem(data, "window_width", cjson_Unsigned, windowWidth);
        setJsonItem(data, "window_height", cjson_Unsigned, windowHeight);
        setJsonItem(data, "window_scale", cjson_Float, windowScale);
      }, true);
  }

  void saveParameterMappings(usize parameterMappings)
  {
    useConfigJson([&](cjson *data)
      {
        setJsonItem(data, "parameter_count", cjson_Unsigned, parameterMappings);
      }, true);
  }

  void saveUndoStepCount(usize undoStepCount)
  {
    useConfigJson([&](cjson *data)
      {
        setJsonItem(data, "undo_steps", cjson_Unsigned, undoStepCount);
      }, true);
  }

#undef setJsonItem
}

static void handleIndexedData(utils::bumpArena *arena, [[maybe_unused]] bool isAutomated,
  Framework::ParameterDetails &details, cjson *indexedData)
{
  //bool isExtensible = (details.flags & Framework::ParameterDetails::Extensible) != 0;

  auto processSingle = [&](const auto &self, Framework::IndexedData &option, cjson *data) -> Framework::IndexedData &
  {
    auto *newOption = anew(arena, Framework::IndexedData, { option });

    if (cjson *children = cjson_GetObjectItem(data, "options"))
    {
      Framework::IndexedData dummy{ .parent = newOption };
      for (cjson *value = children->child; value; value = value->next)
      {
        bool isPresent = false;
        auto *child = option.children;
        for (; child; child = child->next)
        {
          if (child->id == cjson_GetObjectItem(value, "id")->vuint)
          {
            isPresent = true;
            break;
          }
        }

        if (isPresent)
        {
          auto &newChildOption = self(self, *child, value);
          // this uses the overloaded comma operator (im sorry)
          dummy, newChildOption;

          char *string = cjson_GetObjectItem(value, "display_name")->vstring;
          utils::string_view dataName{ string, utils::getStringSize(string) };
          if (dataName != newChildOption.displayName)
          {
            dataName = findOrAddPermanentString(dataName);
            newChildOption.displayName = dataName;
          }

          newChildOption.count = (u32)cjson_GetObjectItem(value, "count")->vuint;
          if ([[maybe_unused]] cjson *dataUuid = cjson_GetObjectItem(value, "dynamic_update_uuid"))
          {
            COMPLEX_ASSERT(newChildOption.dynamicUpdateUuid = dataUuid->vuint);
          }
        }
        else
        {
          // TODO:
        }
      }

      // second loop to add new/missing options from the save
      for (auto child = option.children; child; child = child->next)
      {
        bool isPresent = false;
        for (auto newChild = newOption->children; newChild; newChild = newChild->next)
        {
          if (newChild->id == child->id)
          {
            isPresent = true;
            break;
          }
        }

        if (!isPresent)
        {
          auto *missing = Framework::IndexedData::deepCopy(arena, child);
          // this uses the overloaded comma operator (im sorry)
          dummy, *missing;
          newOption->count -= missing->count;
        }
      }

      newOption->children = dummy.next;
    }

    return *newOption;
  };

  auto &newOptions = processSingle(processSingle, *details.options, indexedData);
  details.options = &newOptions;
  details.defaultOptionId = cjson_GetObjectItem(indexedData, "default_option_id")->vuint;
}

namespace Framework
{
  void ParameterValue::serialiseToJson(void *jsonData) const
  {
    cjson *data = (cjson *)jsonData;
    cjson_AddTo(data, "id", cjson_Unsigned, details_.id);
    cjson_AddTo(data, "display_name", cjson_String, details_.displayName.data());
    cjson_AddTo(data, "value", cjson_Float, normalisedValue_);
    cjson_AddTo(data, "scale", cjson_Unsigned, details_.scale);
    cjson_AddTo(data, "is_stereo", cjson_Bool, (details_.flags & ParameterDetails::Stereo) != 0);
    cjson_AddTo(data, "is_modulatable", cjson_Bool, (details_.flags & ParameterDetails::Modulatable) != 0);
    cjson_AddTo(data, "is_extensible", cjson_Bool, (details_.flags & ParameterDetails::Extensible) != 0);
    if (parameterLink_.hostControl)
      cjson_AddTo(data, "automation_slot", cjson_Unsigned, parameterLink_.hostControl->parameterIndex);
    if (details_.scale == ParameterScale::Indexed)
    {
      auto recurseOptions = [&](const auto &self, cjson *optionData, IndexedData *option) -> void
      {
        cjson *children = cjson_Create(cjson_Array);
        for (auto *child = option->children; child; child = child->next)
        {
          cjson *childData = cjson_AddTo(children, nullptr, cjson_Object);
          cjson_AddTo(childData, "id", cjson_Unsigned, child->id);
          cjson_AddTo(childData, "display_name", cjson_String, child->displayName.data());
          cjson_AddTo(childData, "count", cjson_Unsigned, child->count);

          if (child->dynamicUpdateUuid != uuid{})
            cjson_AddTo(childData, "dynamic_update_uuid", cjson_Unsigned, child->dynamicUpdateUuid);

          self(self, childData, child);
        }

        if (option->children)
          cjson_AddExistingTo(optionData, "options", children);
      };

      cjson_AddTo(data, "min_value", cjson_Unsigned, (u64)0);
      cjson_AddTo(data, "max_value", cjson_Unsigned, (u64)details_.options->count);
      cjson_AddTo(data, "default_option_id", cjson_Unsigned, details_.defaultOptionId);
      recurseOptions(recurseOptions, data, details_.options);
    }
    else
    {
      cjson_AddTo(data, "min_value", cjson_Float, (double)details_.minValue);
      cjson_AddTo(data, "max_value", cjson_Float, (double)details_.maxValue);
      cjson_AddTo(data, "default_value", cjson_Float, (double)details_.defaultValue);
      cjson_AddTo(data, "default_normalised_value", cjson_Float, (double)details_.defaultNormalisedValue);
    }

    // TODO: add modulators
  }

  utils::dll<ParameterValue> *
  ParameterValue::deserialiseFromJson(Generation::BaseProcessor *processor, void *jsonData,
    ParameterDetails &reference, utils::dll<ParameterValue> *memory)
  {
    COMPLEX_ASSERT(memory);

    auto *linkedList = new (memory) utils::dll<ParameterValue>{ reference };
    auto *parameter = &linkedList->object;
    cjson *data = (cjson *)jsonData;

    if (parameter->details_.scale != cjson_GetObjectItem(data, "scale")->vuint)
    {
      COMPLEX_ASSERT_FALSE();
      // TODO: log this
    }

    u64 automationSlot = u64(-1);
    if (cjson *slot = cjson_GetObjectItem(data, "automation_slot"))
      automationSlot = slot->vuint;

    COMPLEX_ASSERT(cjson_GetObjectItem(data, "is_stereo")->vbool == ((parameter->details_.flags & ParameterDetails::Stereo) != 0));

    if (parameter->details_.scale == ParameterScale::Indexed)
    {
      if (!cjson_GetObjectItem(data, "options"))
      {
        auto errorString = utils::string::create(localScratch,
          "%v\nOptions parameter %v (%zu) is missing its options, replacing with default ones from the plugin.",
          utils::string_view{ *errorPath }, parameter->details_.displayName, parameter->details_.id);
        Interface::showNativeMessageBox("Error opening preset", errorString.data(), Interface::MessageBoxType::Warning);

        parameter->details_.options = IndexedData::deepCopy(processor->arena, parameter->details_.options);
      }
      else
      {
        // indexed values validation and deserialisation
        handleIndexedData(processor->arena, automationSlot != u64(-1), parameter->details_, data);
        processor->state->registerDynamicParameter(parameter);
      }
    }
    else
    {
      float minValue = (float)cjson_GetObjectItem(data, "min_value")->vdouble;
      float maxValue = (float)cjson_GetObjectItem(data, "max_value")->vdouble;

      float referenceMin = parameter->details_.minValue;
      float referenceMax = parameter->details_.maxValue;

      // if the save contains an expanded range but the parameter in this version isn't extensible
      // then there's nothing we can do about it
      if ((referenceMin > minValue || referenceMax < maxValue) &&
        (parameter->details_.flags & ParameterDetails::Extensible) == 0)
      {
        COMPLEX_ASSERT_FALSE();
        // TODO: log this
      }

      bool changedMinMax = false;
      if (referenceMin != minValue || referenceMax != maxValue)
      {
        // the range of the parameter was changed while being automated
        // we mustn't change the range of the parameter otherwise we're going to ruin someone's project
        if (automationSlot != u64(-1))
        {
          // if we're here then that means the range was changed but it's not larger than the range available
          parameter->details_.minValue = minValue;
          parameter->details_.maxValue = maxValue;
          changedMinMax = true;
        }
        else
        {
          minValue = referenceMin;
          maxValue = referenceMax;
        }

        // always set min/max for dynamic indexed parameters,
        // since it might be possible to check them at this time
        if (parameter->details_.scale == ParameterScale::Indexed &&
          (parameter->details_.flags & ParameterDetails::Extensible) != 0)
        {
          parameter->details_.minValue = minValue;
          parameter->details_.maxValue = maxValue;
          changedMinMax = true;
        }
      }
    }

    // TODO: fit modulation here

    float value = utils::clamp((float)cjson_GetObjectItem(data, "value")->vdouble, 0.0f, 1.0f);
    parameter->normalisedValue_ = value;
    parameter->isDirty_ = true;
    parameter->updateValue(processor->state->plugin->getSampleRate());

    // paranoid check just in case
    COMPLEX_ASSERT(parameter->normalisedValue_ == value);

    if (automationSlot != u64(-1))
    {
      // if we don't have enough parameters then too bad, we only guarantee kMaxParameterMappings generic parameters
      // TODO: report this to user
      if (automationSlot < (u64)processor->state->parameterBridges.size())
        processor->state->parameterBridges[automationSlot].resetParameterLink(parameter->getParameterLink(), true);
    }

    return linkedList;
  }
}

namespace Generation
{
  void BaseProcessor::serialiseToJson(void *jsonData, utils::span<Framework::ParameterValue *> parametersToSerialise) const
  {
    cjson *processorInfo = (cjson *)jsonData;
    cjson_AddTo(processorInfo, "id", cjson_Unsigned, metadata->id);

    cjson *serialisedChildren = cjson_AddTo(processorInfo, "processors", cjson_Array);
    for (auto *child = children; child; child = child->next)
    {
      cjson *subProcessorData = cjson_AddTo(serialisedChildren, nullptr, cjson_Object);
      child->serialiseToJson(subProcessorData);
    }

    cjson *serialisedParameters = cjson_AddTo(processorInfo, "parameters", cjson_Array);
    if (parametersToSerialise.empty())
    {
      for (auto *parameter = parameters; parameter; parameter = parameter->next)
      {
        cjson *parameterData = cjson_AddTo(serialisedParameters, nullptr, cjson_Object);
        parameter->object.serialiseToJson(parameterData);
      }
    }
    else
    {
      for (auto &parameter : parametersToSerialise)
      {
        cjson *parameterData = cjson_AddTo(serialisedParameters, nullptr, cjson_Object);
        parameter->serialiseToJson(parameterData);
      }
    }
  }

  void deserialiseParametersFromJson(void *jsonData, Framework::ProcessorMetadata *metadata,
    utils::dll<Framework::ParameterValue> *&parameters, BaseProcessor *processor, bool validateParameters)
  {
    cjson *data = (cjson *)jsonData;
    cjson *parametersCopy = cjson_Duplicate(cjson_GetObjectItem(data, "parameters"), true);

    auto *memory = arranew(processor->arena, utils::dll<Framework::ParameterValue>, metadata->parametersCount, {});

    auto insertParameter = [&](auto *parameter)
    {
      if (parameters)
      {
        parameters->previous->next = parameter;
        parameter->previous = parameters->previous;
        parameters->previous = parameter;
      }
      else
      {
        parameter->previous = parameter;
        parameters = parameter;
      }
    };

    for (auto *expectedParameter = metadata->parameters;
      expectedParameter; expectedParameter = expectedParameter->next)
    {
      utils::dll<Framework::ParameterValue> *parameter{};
      for (auto child = parametersCopy->child; child; child = child->next)
      {
        uuid id = cjson_GetObjectItem(child, "id")->vuint;
        if (id == expectedParameter->details.id)
        {
          parameter = Framework::ParameterValue::deserialiseFromJson(processor,
            child, expectedParameter->details, memory);
          cjson_Delete(cjson_DetachItemViaPointer(parametersCopy, child));
          break;
        }
      }

      if (!parameter)
      {
        auto errorString = utils::string::create(localScratch,
          "%v\nMissing Parameter %v (%zu), replacing with a default initialised one. \
          This should have been handled by the version upgrade routine but it wasn't. \
          If this is the mainline version of the plugin consider reporting it to the developer.",
          utils::string_view{ *errorPath }, expectedParameter->details.displayName, expectedParameter->details.id);
        Interface::showNativeMessageBox("Error opening preset", errorString.data(), Interface::MessageBoxType::Warning);

        parameter = new (memory) utils::dll<Framework::ParameterValue>{ expectedParameter->details };
      }

      ++memory;
      insertParameter(parameter);
    }

    // handle remaining parameters
    if (validateParameters)
    {
      for (auto child = parametersCopy->child; child; child = child->next)
      {
        bool isPresent = false;
        uuid id = cjson_GetObjectItem(child, "id")->vuint;
        for (auto *parameter = metadata->parameters; parameter; parameter = parameter->next)
        {
          if (id == parameter->details.id)
          {
            isPresent = true;
            break;
          }
        }

        if (!isPresent)
        {
          auto displayName = cjson_GetObjectItem(child, "display_name")->vstring;
          auto errorString = utils::string::create(localScratch,
            "%v\nUnexpected parameter %s (%zu).",
            utils::string_view{ *errorPath }, displayName, id);
          Interface::showNativeMessageBox("Error opening preset", errorString.data(), Interface::MessageBoxType::Warning);
        }
      }
    }
  }

  void BaseProcessor::deserialiseFromJson(void *jsonData)
  {
    auto oldSize = errorPath->size();
    errorPath->appendFormat("Inside processor %v (%zu):\n", metadata->name, metadata->id);
    auto newSize = errorPath->size();

    cjson *data = (cjson *)jsonData;
    parameterCount = (u32)metadata->parametersCount;
    deserialiseParametersFromJson(jsonData, metadata, parameters, this,
      (metadata->flags & Framework::ProcessorMetadata::NoParameterValidationTag) == 0);

    cjson *processors = cjson_GetObjectItem(data, "processors");
    for (auto *processor = processors->child; processor; processor = processor->next)
    {
      uuid subProcessorsId = cjson_GetObjectItem(processor, "id")->vuint;
      Generation::BaseProcessor *subProcessor = state->createProcessor(subProcessorsId, processor);
      insertSubProcessor(childrenCount, *subProcessor);
    }

    errorPath->removeSuffix(newSize - oldSize);
  }
}


namespace Plugin
{
  void serialiseToJson(State *state, void *jsonData)
  {
    utils::vector<Generation::BaseProcessor *> topLevelProcessors{};
    for (auto &[id, processor] : state->allProcessors.data)
      if (!processor->parent)
        topLevelProcessors.emplace_back(processor);

    COMPLEX_ASSERT(!topLevelProcessors.empty());

    cjson *data = (cjson *)jsonData;
    cjson_AddTo(data, "version", cjson_String, CPLUG_PLUGIN_VERSION);
    cjson *topLevelProcessorsSerialised = cjson_AddTo(data, "tree", cjson_Array);

    for (auto &topLevelProcessor : topLevelProcessors)
    {
      cjson *topLevelData = cjson_AddTo(topLevelProcessorsSerialised, nullptr, cjson_Object);
      topLevelProcessor->serialiseToJson(topLevelData);
    }
  }

  utils::sp<State>
  ComplexPlugin::loadDefaultPreset()
  {
    using namespace Generation;

    auto state = utils::sp<State>::create(this);

    state->soundEngine = utils::as<SoundEngine>(state->createProcessor(Processors::SoundEngine));
    auto *effectsState = state->createProcessor(Processors::EffectsState);
    auto *effectsLane = state->createProcessor(Processors::EffectsLane);

    effectsState->insertSubProcessor(0, *effectsLane);
    state->soundEngine->insertSubProcessor(0, *effectsState);

    auto min = utils::min(state->parameterBridges.size(),
      Generation::SoundEngine::kParametersValues.size());
    for (usize i = 0; i < min; ++i)
    {
      auto *parameter = state->getProcessorParameter(state->soundEngine->stateId,
        Generation::SoundEngine::kParametersValues[i]);
      COMPLEX_ASSERT(parameter);
      state->parameterBridges[i].resetParameterLink(parameter->getParameterLink(), true);
    }

    return state;
  }

  Generation::BaseProcessor *
  State::createProcessor(uuid processorId, void *jsonData)
  {
    auto *metadata = findProcessorMetadata(processorId);
    Generation::BaseProcessor *processor = metadata->create(this, metadata, nullptr);
    if (jsonData != nullptr)
      processor->deserialiseFromJson(jsonData);
    else
      processor->initialiseParameters();

    allProcessors.add(processor->stateId, processor);
    expandIfNecessary();

    return processor;
  }

  utils::sp<State>
  deserialiseFromJson(ComplexPlugin *plugin, void *newSave)
  {
    auto state = utils::sp<State>::create(plugin);
    cjson *newData = (cjson *)newSave;

    utils::string errorPath_{};
    errorPath = &errorPath_;

    cjson *soundEngineJson = cjson_GetArrayItem(cjson_GetObjectItem(newData, "tree"), 0);
    uuid type = cjson_GetObjectItem(soundEngineJson, "id")->vuint;
    if (type != Generation::Processors::SoundEngine)
    {
      Interface::showNativeMessageBox("Error opening preset",
        "SoundEngine type doesn't match.", Interface::MessageBoxType::Error);
      return nullptr;
    }

    cjson *parameters = cjson_GetObjectItem(soundEngineJson, "parameters");

    for (auto *parameter = parameters->child; parameter; parameter = parameter->next)
    {
      if (cjson_GetObjectItem(soundEngineJson, "id")->vuint == Generation::SoundEngine::BlockSize)
      {
        state->minFFTOrder = (u32)cjson_GetObjectItem(soundEngineJson, "min_value")->vuint;
        state->maxFFTOrder = (u32)cjson_GetObjectItem(soundEngineJson, "max_value")->vuint;
        break;
      }
    }

    auto soundEngine = state->createProcessor(Generation::Processors::SoundEngine);
    soundEngine->deserialiseFromJson(soundEngineJson);

    for (auto &id : Framework::ParameterChangeReason::kParameterChangeReasonValues)
      state->updateDynamicParameters(id);

    Framework::ParameterBridge::notifyParameterChange();

    return state;
  }

  void saveState(ComplexPlugin *plugin, const void *stateCtx, cplug_writeProc writeProc)
  {
    if (!plugin->state_)
      return;

    jsonArena = utils::bumpArena::createNested(localScratch, COMPLEX_KB(128));

    cjson *data = cjson_Create(cjson_Object);
    serialiseToJson(plugin->state_.get(), data);
    usize size = 0;
    char *dataString = cjson_Print(data, &size);
    writeProc(stateCtx, dataString, size);

    utils::bumpArena::destroy(jsonArena);
    jsonArena = nullptr;
  }

  struct PresetUpdate final : public Framework::UndoAction
  {
    PresetUpdate(Plugin::ComplexPlugin &plugin, utils::sp<Plugin::State> newSavedState) :
      plugin(plugin), state{ COMPLEX_MOVE(newSavedState) }
    {
      redo = [](UndoAction *a)
      {
        auto *self = (PresetUpdate *)a;

        auto g = self->state->plugin->acquireProcessingLock();
        auto oldState = self->plugin.exchangeStates(COMPLEX_MOVE(self->state));
        self->state = COMPLEX_MOVE(oldState);
      };

      undo = [](UndoAction *a)
      {
        auto *self = (PresetUpdate *)a;

        auto g = self->state->plugin->acquireProcessingLock();
        auto newState = self->plugin.exchangeStates(COMPLEX_MOVE(self->state));
        self->state = COMPLEX_MOVE(newState);
      };
    }

    Plugin::ComplexPlugin &plugin;
    utils::sp<Plugin::State> state;
  };

  void loadState(ComplexPlugin *plugin, utils::string_view data)
  {
    bool wasStateInitialised = plugin->wasStateInitialised;
    plugin->wasStateInitialised = true;

    utils::sp<State> state{};

    if (data.size() != 0)
    {
      jsonArena = utils::bumpArena::createNested(localScratch, COMPLEX_KB(128));
      const char *potentialError = nullptr;
      cjson *jsonData = cjson_ParseWithOpts(data.data(), data.size(), &potentialError, false);
      if (!jsonData)
      {
        Interface::showNativeMessageBox("Error opening preset",
          potentialError, Interface::MessageBoxType::Error);
      }
      else
      {
        upgradeSave(jsonData);
        state = deserialiseFromJson(plugin, jsonData);
      }

      utils::bumpArena::destroy(jsonArena);
      jsonArena = nullptr;
    }

    if (!state)
      state = plugin->loadDefaultPreset();

    auto [minOrder, maxOrder] = state->soundEngine->getMinMaxFFTOrder();
    state->fft = plugin->getFFTConverter(minOrder, maxOrder);

    if (wasStateInitialised && plugin->state_.get())
    {
      auto *storage = plugin->undoManager.beginNewTransaction();
      plugin->undoManager.perform(anew(storage, PresetUpdate,
        { *plugin, COMPLEX_MOVE(state) }));
    }
    else
      plugin->state_ = COMPLEX_MOVE(state);

    Interface::uiRelated.state = plugin->state_.get();
  }
}
