
// Created: 2022-12-03 01:46:31

#include "load_save.hpp"

#include "Third Party/cplug/config.h"

#include "Third Party/xhl/xhl_files.h"
#include "Third Party/cjson/cjson.h"

#include "Framework/parameters.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/Effects.hpp"
#include "Interface/Graphics.hpp"
#include "Interface/gui_constants.hpp"
#include "Interface/MainInterface.hpp"
#include "Plugin/Complex.hpp"


static void *
#ifdef COMPLEX_WINDOWS
__cdecl
#endif
cjsonAllocate(size_t size)
{
  auto jsonArena = getJsonContext().pool;
  COMPLEX_ASSERT(jsonArena, "Someone forgot to set the pool before using json");
  return utils::Pool::insert(jsonArena, size, alignof(void *));
}

// we free the pool at the end, so it's pointless to deallocate
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

extern ExecutableMetadata executableStaticData;

utils::string_view
findOrAddPermanentString(utils::string_view string);

utils::string
getConfigFilePath(utils::string_view file)
{
  auto path = executableStaticData.configFolderPath;
  if (!xfiles_exists(path.data()))
    xfiles_create_directory(path.data());

  return utils::string::create(getLocalScratch(), "%v" XFILES_DIR_STR "%v", path, file);
}

static void useConfigJson(const auto &predicate, bool save = false)
{
  auto filePath = getConfigFilePath(CPLUG_PLUGIN_NAME ".config");

  if (!xfiles_exists(filePath.data()))
  {
    auto empty = utils::string_view{ "{}" };
    xfiles_write(filePath.data(), empty.data(), empty.size());
  }

  char *string;
  usize stringSize;
  if (!xfiles_read(filePath.data(), (void **)&string, &stringSize))
    return;
  defer{ xfiles_read_free(string); };

  auto &jsonContext = getJsonContext();
  jsonContext.pool = utils::Pool::createNested(getLocalScratch(), COMPLEX_KB(16));
  defer{ utils::Pool::destroy(jsonContext.pool); jsonContext.pool = nullptr; };

  cjson *json = cjson_Parse(string, stringSize);
  if (json)
    predicate(json);
  else if (save)
  {
    json = cjson_Create(cjson_Object);
    predicate(json);
  }

  if (save)
  {
    size_t size;
    char *text = cjson_Print(json, &size, true);
    xfiles_write(filePath.data(), text, size);
  }
}

static void upgradeSave([[maybe_unused]] cjson *save)
{

}

i32
getModuleWidth()
{
  i32 moduleWidth = kEffectModuleWidth;

  useConfigJson([&](cjson *json)
    {
      if (cjson *item = cjson_GetObjectItem(json, "module_width"))
        moduleWidth = utils::max<i32>(kMinWidth, (i32)item->vint);
    });

  return moduleWidth;
}
void getWindowSizeScale(u32 &windowWidth, u32 &windowHeight, float &windowScale)
{
  windowWidth = kMinWidth;
  windowHeight = kMinHeight;
  windowScale = 1.0f;

  useConfigJson([&](cjson *json)
    {
      if (cjson *item = cjson_GetObjectItem(json, "window_width"))
        windowWidth = utils::max<u32>(kMinWidth, (u32)item->vuint);

      if (cjson *item = cjson_GetObjectItem(json, "window_height"))
        windowHeight = utils::max<u32>(kMinHeight, (u32)item->vuint);

      if (cjson *item = cjson_GetObjectItem(json, "window_scale"))
        windowScale = (float)item->vdouble;
    });
}

void getStartupParameters(usize &parameterMappings, usize &inSidechains, usize &outSidechains, usize &undoSteps)
{
  parameterMappings = 100;
  inSidechains = 0;
  outSidechains = 0;
  undoSteps = 100;

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
    item = cjson_Create(type, value);               \
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

static void handleIndexedData(utils::Pool *pool, bool isAutomated,
  ParameterDetails &details, cjson *indexedData)
{
  auto *dynamicOptionFixups = getJsonContext().dynamicOptionFixups;

  // FIXME: this adds duplicate options to IndexedData
  auto processSingle = [&](const auto &self, IndexedData &option, cjson *data) -> IndexedData *
  {
    auto *newOption = anew(pool, IndexedData, { option });
    // lose the reference to the original parent so that we don't increment its count
    newOption->parent = nullptr;
    // lose the reference to the original children because we'll add copies of them back in
    newOption->children = nullptr;
    // break up child connecitons otherwise we will be duplicating everything after the first entry
    newOption->next = nullptr;

    cjson *children = cjson_GetObjectItem(data, "options");
    if (children)
    {
      for (cjson *value = children->child; value; value = value->next)
      {
        bool isPresent = false;
        auto *child = option.children;
        auto savedId = cjson_GetObjectItem(value, "id")->vuint;
        COMPLEX_ASSERT(savedId, "Option doesn't have an id so there's no way to identify it (this is really bad)");

        for (; child; child = child->next)
          if ((isPresent = (child->id == savedId)))
            break;

        if (isPresent)
        {
          auto *newChildOption = self(self, *child, value);

          if (auto *nameItem = cjson_GetObjectItem(value, "display_name"))
          {
            char *string = nameItem->vstring;
            utils::string_view dataName{ string, utils::getStringSize(string) };
            if (dataName != newChildOption->displayName)
            {
              dataName = findOrAddPermanentString(dataName);
              newChildOption->displayName = dataName;
            }
          }

          newChildOption->valueCount = (u32)cjson_GetObjectItem(value, "value_count")->vuint;
          if ([[maybe_unused]] cjson *dataUuid = cjson_GetObjectItem(value, "dynamic_update_uuid"))
          {
            COMPLEX_ASSERT(newChildOption->dynamicUpdateUuid == dataUuid->vuint);
          }

          newOption->addChildren({{ newChildOption }});

          continue;
        }

        // usually the option would not be present if it points to a processor (IndexedData::StateIdFlag)
        if (auto *stateIdEntry = cjson_GetObjectItem(value, "state_id"))
        {
          // this option points to some processor defined in the save file
          // but because we haven't finished deserialising it might not exist yet
          // for now we just copy the state_id defined in the save file
          // and add this option to a list for a later fixup
          // when the id of the deserialised processor in the current state will be assigned

          auto *newChildOption = anew(pool, IndexedData, {});
          newChildOption->id = savedId;
          newChildOption->stateId = stateIdEntry->vuint;
          newChildOption->flags = IndexedData::StateIdFlag;
          newOption->addChildren({{ newChildOption }});

          dynamicOptionFixups->emplaceBack(newChildOption);

          continue;
        }

        COMPLEX_ASSERT_FALSE("Unhandled child option");
      }
    }

    // second loop to add new/missing options from the save
    for (auto missingChild = option.children; missingChild; missingChild = missingChild->next)
    {
      bool isPresent = false;
      for (auto newChild = newOption->children; newChild; newChild = newChild->next)
        if ((isPresent = (newChild->id == missingChild->id)))
          break;

      if (!isPresent)
      {
        // if the parameter is automated we don't want to mess up the options, so we add them as untracked
        newOption->addChildren({{ IndexedData::deepCopy(pool, missingChild) }}, isAutomated);
      }
    }

    return *newOption;
  };

  details.options = processSingle(processSingle, *details.options, indexedData);
  details.defaultOptionId = cjson_GetObjectItem(indexedData, "default_option_id")->vuint;
}

static void fixDeserialisedProcessorsStateIds(State *state,
  utils::vector<IndexedData *> *dynamicOptionFixups)
{
  state->stateIdCounter = {};
  for (auto &[oldId, processor] : state->allProcessors.data)
  {
    const_cast<u64 &>(processor->stateId) = ++state->stateIdCounter;

    while (true)
    {
      auto iter = utils::findIf(*dynamicOptionFixups, [&oldId](IndexedData *item)
        { return item->flags == IndexedData::StateIdFlag && item->stateId == oldId; });
      if (iter == dynamicOptionFixups->end())
        break;

      (*iter)->stateId = processor->stateId;
      dynamicOptionFixups->eraseAt(iter);
    }

    oldId = processor->stateId;
  }
}

void Parameter::serialiseToJson(void *jsonData) const
{
  cjson *data = (cjson *)jsonData;
  cjson_AddTo(data, "id", cjson_Unsigned, details.id);
  cjson_AddTo(data, "display_name", cjson_String, details.displayName.data());
  cjson_AddTo(data, "value", cjson_Float, normalisedValue.load(satomi::memory_order_relaxed));
  cjson_AddTo(data, "scale", cjson_Unsigned, details.scale);
  cjson_AddTo(data, "is_stereo", cjson_Bool, (details.flags & ParameterDetails::Stereo) != 0);
  cjson_AddTo(data, "is_modulatable", cjson_Bool, (details.flags & ParameterDetails::Modulatable) != 0);
  cjson_AddTo(data, "is_extensible", cjson_Bool, (details.flags & ParameterDetails::Extensible) != 0);
  cjson_AddTo(data, "is_rounding_to_int", cjson_Bool, (details.flags & ParameterDetails::RoundToInt) != 0);
  if (auto hostParameter_ = hostParameter.load(satomi::memory_order_acquire))
    cjson_AddTo(data, "automation_slot", cjson_Unsigned, hostParameter_->parameterIndex);
  if (details.scale == ParameterScale::Indexed)
  {
    auto recurseOptions = [&](const auto &self, cjson *optionData, IndexedData *option) -> void
    {
      // this line is leaking technically at leaf nodes but because
      cjson *children = cjson_Create(cjson_Array);
      for (auto *child = option->children; child; child = child->next)
      {
        cjson *childData = cjson_AddTo(children, nullptr, cjson_Object);
        cjson_AddTo(childData, "id", cjson_Unsigned, child->id);
        cjson_AddTo(childData, "display_name", cjson_String, child->displayName.data());
        cjson_AddTo(childData, "children_count", cjson_Unsigned, child->childrenCount);
        cjson_AddTo(childData, "value_count", cjson_Unsigned, child->valueCount);

        if (child->dynamicUpdateUuid != uuid{})
          cjson_AddTo(childData, "dynamic_update_uuid", cjson_Unsigned, child->dynamicUpdateUuid);

        if (child->userFlags)
          cjson_AddTo(childData, "user_flags", cjson_Unsigned, child->userFlags);

        if (child->flags == IndexedData::StateIdFlag)
          cjson_AddTo(childData, "state_id", cjson_Unsigned, child->stateId);

        if (child->children)
          self(self, childData, child);
      }

      COMPLEX_ASSERT(option->children);
      cjson_AddExistingTo(optionData, "options", children);
    };

    cjson_AddTo(data, "min_value", cjson_Unsigned, (u64)0);
    cjson_AddTo(data, "max_value", cjson_Unsigned, (u64)details.options->valueCount);
    cjson_AddTo(data, "default_option_id", cjson_Unsigned, details.defaultOptionId);
    recurseOptions(recurseOptions, data, details.options);
  }
  else
  {
    cjson_AddTo(data, "min_value", cjson_Float, (double)details.minValue);
    cjson_AddTo(data, "max_value", cjson_Float, (double)details.maxValue);
    cjson_AddTo(data, "default_normalised_value", cjson_Float, (double)details.defaultNormalisedValue);
  }

  // TODO: add modulators
}

Parameter *
Parameter::deserialiseFromJson(Processor *processor, void *jsonData,
  ParameterDetails &reference, Parameter *memory)
{
  COMPLEX_ASSERT(memory);

  auto *parameter = new (memory) Parameter{ reference, processor };
  cjson *data = (cjson *)jsonData;

  if (parameter->details.scale != cjson_GetObjectItem(data, "scale")->vuint)
  {
    COMPLEX_ASSERT_FALSE();
    // FIXME: log this
  }

  u64 automationSlot = u64(-1);
  if (cjson *slot = cjson_GetObjectItem(data, "automation_slot"))
    automationSlot = slot->vuint;

  auto &details = const_cast<ParameterDetails &>(parameter->details);

  details.flags &= ~ParameterDetails::RoundToInt;
  if (cjson *slot = cjson_GetObjectItem(data, "is_rounding_to_int"))
    details.flags |= (slot->vbool) ? ParameterDetails::RoundToInt : 0;

  COMPLEX_ASSERT(cjson_GetObjectItem(data, "is_stereo")->vbool == ((details.flags & ParameterDetails::Stereo) != 0));

  if (details.scale == ParameterScale::Indexed)
  {
    if (!cjson_GetObjectItem(data, "options"))
    {
      auto *errorPath = getJsonContext().errorPath;
      auto errorString = utils::string::create(getLocalScratch(),
        "%v\nOptions parameter %v (%zu) is missing its options, replacing with default ones from the plugin.",
        utils::string_view{ *errorPath }, details.displayName, details.id);
      showNativeMessageBox("Warning", errorString.data(), MessageBoxType::Warning);

      details.options = IndexedData::deepCopy(processor->pool, details.options);
    }
    else
    {
      // indexed values validation and deserialisation
      handleIndexedData(processor->pool, automationSlot != u64(-1), details, data);
      processor->state->registerDynamicParameter(parameter);
    }
  }
  else
  {
    float minValue = (float)cjson_GetObjectItem(data, "min_value")->vdouble;
    float maxValue = (float)cjson_GetObjectItem(data, "max_value")->vdouble;

    float referenceMin = details.minValue;
    float referenceMax = details.maxValue;

    // if the save contains an expanded range but the parameter in this version isn't extensible
    // then there's nothing we can do about it
    if ((referenceMin > minValue || referenceMax < maxValue) &&
      (details.flags & ParameterDetails::Extensible) == 0)
    {
      //COMPLEX_ASSERT_FALSE();
      // FIXME: -inf db gets picked up by this condition which shouldn't happen
      // FIXME: log this
    }

    // bool changedMinMax = false;
    if (referenceMin != minValue || referenceMax != maxValue)
    {
      // the range of the parameter was changed while being automated
      // we mustn't change the range of the parameter otherwise we're going to ruin someone's project
      if (automationSlot != u64(-1))
      {
        // if we're here then that means the range was changed but it's not larger than the range available
        details.minValue = minValue;
        details.maxValue = maxValue;
        // changedMinMax = true;
      }
      else
      {
        minValue = referenceMin;
        maxValue = referenceMax;
      }

      // always set min/max for dynamic indexed parameters,
      // since it might be possible to check them at this time
      if (parameter->details.scale == ParameterScale::Indexed &&
        (parameter->details.flags & ParameterDetails::Extensible) != 0)
      {
        details.minValue = minValue;
        details.maxValue = maxValue;
        // changedMinMax = true;
      }
    }
  }

  // TODO: fit modulation here

  float value = utils::clamp((float)cjson_GetObjectItem(data, "value")->vdouble, 0.0f, 1.0f);
  parameter->normalisedValue.store(value, satomi::memory_order_relaxed);

  // paranoid check just in case
  COMPLEX_ASSERT(parameter->normalisedValue.load(satomi::memory_order_relaxed) == value);

  if (automationSlot != u64(-1))
  {
    if (automationSlot < (u64)processor->state->parameterBridges.size())
      processor->state->parameterBridges[automationSlot].resetParameter(parameter, true);
    else
    {
      // if we don't have enough parameters then too bad, we only guarantee kMaxParameterMappings generic parameters
      // FIXME: report this to user, maybe restart the plugin with more parameter slots
    }
  }

  return parameter;
}


void serialiseParameters(void *jsonData, Parameter *parameters, usize parameterCount)
{
  cjson *processorInfo = (cjson *)jsonData;
  cjson *serialisedParameters = cjson_AddTo(processorInfo, "parameters", cjson_Array);
  for (usize i = 0; parameters && i < parameterCount; parameters = parameters->next)
  {
    cjson *parameterData = cjson_AddTo(serialisedParameters, nullptr, cjson_Object);
    parameters->serialiseToJson(parameterData);
  }
}

void serialiseParameters(void *jsonData, utils::span<Parameter *> parametersToSerialise)
{
  cjson *processorInfo = (cjson *)jsonData;
  cjson *serialisedParameters = cjson_AddTo(processorInfo, "parameters", cjson_Array);
  for (auto &parameter : parametersToSerialise)
  {
    cjson *parameterData = cjson_AddTo(serialisedParameters, nullptr, cjson_Object);
    parameter->serialiseToJson(parameterData);
  }
}

void serialiseProcessorGeneric(const Processor *processor,
  void *jsonData, utils::span<Parameter *> parametersToSerialise)
{
  cjson *processorInfo = (cjson *)jsonData;
  cjson_AddTo(processorInfo, "id", cjson_Unsigned, processor->metadata->id);
  cjson_AddTo(processorInfo, "state_id", cjson_Unsigned, processor->stateId);
  if (!processor->name.empty())
    cjson_AddTo(processorInfo, "name", cjson_String, processor->name.data());

  cjson *serialisedChildren = cjson_AddTo(processorInfo, "processors", cjson_Array);
  for (auto *child = processor->children; child; child = child->next)
  {
    cjson *subProcessorData = cjson_AddTo(serialisedChildren, nullptr, cjson_Object);
    child->serialiseToJson(subProcessorData);
  }

  if (parametersToSerialise.empty())
    serialiseParameters(jsonData, processor->parameters, processor->parameterCount);
  else
    serialiseParameters(jsonData, parametersToSerialise);
}

void deserialiseParametersFromJson(void *jsonData, ProcessorMetadata *metadata,
  Parameter *&parameters, Processor *processor, bool validateParameters)
{
  cjson *data = (cjson *)jsonData;
  cjson *parametersCopy = cjson_Duplicate(cjson_GetObjectItem(data, "parameters"), true);
  COMPLEX_ASSERT(parametersCopy, "Missing parameters for %v (%zu)", metadata->name, metadata->id);

  auto *memory = arranew(processor->pool, Parameter, metadata->parameterCount, {});

  auto insertParameter = [&](Parameter *parameter)
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
    Parameter *parameter{};
    for (auto child = parametersCopy->child; child; child = child->next)
    {
      uuid id = cjson_GetObjectItem(child, "id")->vuint;
      if (id == expectedParameter->details.id)
      {
        parameter = Parameter::deserialiseFromJson(processor,
          child, expectedParameter->details, memory);
        cjson_Delete(cjson_DetachItemViaPointer(parametersCopy, child));
        break;
      }
    }

    if (!parameter)
    {
      auto *errorPath = getJsonContext().errorPath;
      auto errorString = utils::string::create(getLocalScratch(),
        "%v\nMissing Parameter %v (%zu), replacing with a default initialised one. "
        "This should have been handled by the version upgrade routine but it wasn't. "
        "If this is the mainline version of the plugin consider reporting it to the developer.",
        utils::string_view{ *errorPath }, expectedParameter->details.displayName, expectedParameter->details.id);
      showNativeMessageBox("Error opening preset", errorString.data(), MessageBoxType::Warning);

      parameter = new (memory) Parameter{ expectedParameter->details, processor };
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
        auto *errorPath = getJsonContext().errorPath;
        auto displayName = cjson_GetObjectItem(child, "display_name")->vstring;
        auto errorString = utils::string::create(getLocalScratch(),
          "%v\nUnexpected parameter %s (%zu).",
          utils::string_view{ *errorPath }, displayName, id);
        showNativeMessageBox("Error opening preset", errorString.data(), MessageBoxType::Warning);
      }
    }
  }

  // notify processor of all automation mappings that exist
  if (processor->metadata->automationMappingChangedCallback)
    for (auto *parameter = parameters; parameter; parameter = parameter->next)
      if (parameter->hostParameter.load(satomi::memory_order_relaxed))
        processor->metadata->automationMappingChangedCallback(processor, parameter, true);
}

void deserialiseProcessorChildren(void *jsonData, Processor *parent)
{
  auto *errorPath = getJsonContext().errorPath;
  auto oldSize = errorPath->size();
  errorPath->appendFormat("Inside processor %v (%zu):\n", parent->metadata->name, parent->metadata->id);
  auto newSize = errorPath->size();
  defer { errorPath->removeLast(errorPath->size() - oldSize); };

  cjson *data = (cjson *)jsonData;
  cjson *processors = cjson_GetObjectItem(data, "processors");
  for (auto *processorData = processors->child; processorData; processorData = processorData->next)
  {
    uuid subProcessorsId = cjson_GetObjectItem(processorData, "id")->vuint;
    Processor *subProcessor = parent->state->createProcessor(subProcessorsId, processorData);
    parent->addChildProcessor(*subProcessor);
    parent->state->registerProcessorForDynamicParameters(subProcessor);
    deserialiseProcessorChildren(processorData, subProcessor);
  }

  errorPath->removeLast(newSize - oldSize);
}

void Processor::deserialiseFromJson(void *jsonData)
{
  auto *errorPath = getJsonContext().errorPath;
  auto oldSize = errorPath->size();
  errorPath->appendFormat("Inside processor %v (%zu):\n", metadata->name, metadata->id);
  defer { errorPath->removeLast(errorPath->size() - oldSize); };

  cjson *data = (cjson *)jsonData;
  // id fixup will happen later when deserialisation has finished
  const_cast<u64 &>(stateId) = cjson_GetObjectItem(data, "state_id")->vuint;
  if (auto *nameItem = cjson_GetObjectItem(data, "name"))
    name = { pool, nameItem->vstring, utils::getStringSize(nameItem->vstring) };

  parameterCount = (u32)metadata->parameterCount;
  deserialiseParametersFromJson(jsonData, metadata, parameters, this,
    (metadata->flags & ProcessorMetadata::NoParameterValidationTag) == 0);
}


void serialiseToJson(State *state, void *jsonData)
{
  utils::vector<Processor *> topLevelProcessors{ getLocalScratch(), 16 };
  for (auto &[id, processor] : state->allProcessors.data)
    if (!processor->parent)
      topLevelProcessors.emplaceBack(processor);

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
  auto state = utils::sp<State>::create(pool, this);

  state->soundEngine = utils::as<SoundEngine>(state->createProcessor(Processors::SoundEngine));
  state->registerProcessorForDynamicParameters(state->soundEngine);
  auto effectsLane = state->createProcessor(Processors::EffectsLane);
  state->registerProcessorForDynamicParameters(effectsLane);
  state->soundEngine->addChildProcessor(*effectsLane);

  auto min = utils::min(state->parameterBridges.size(), SoundEngine::valuesParameters.size());
  for (usize i = 0; i < min; ++i)
  {
    auto *parameter = state->getProcessorParameter(state->soundEngine->stateId,
      SoundEngine::valuesParameters[i]);
    COMPLEX_ASSERT(parameter);
    state->parameterBridges[i].resetParameter(parameter, true);
  }

  return state;
}

utils::sp<State>
deserialiseFromJson(ComplexPlugin *plugin, void *newSave)
{
  auto state = utils::sp<State>::create(plugin->pool, plugin);
  cjson *newData = (cjson *)newSave;

  auto &jsonContext = getJsonContext();

  utils::string errorPath{ jsonContext.pool, 64 };
  jsonContext.errorPath = &errorPath;

  utils::vector<IndexedData *> dynamicOptionFixups{ jsonContext.pool, 64 };
  jsonContext.dynamicOptionFixups = &dynamicOptionFixups;

  defer { jsonContext.dynamicOptionFixups = nullptr; jsonContext.errorPath = nullptr; };

  cjson *soundEngineJson = cjson_GetArrayItem(cjson_GetObjectItem(newData, "tree"), 0);
  uuid type = cjson_GetObjectItem(soundEngineJson, "id")->vuint;
  if (type != Processors::SoundEngine)
  {
    showNativeMessageBox("Error opening preset",
      "SoundEngine type doesn't match.", MessageBoxType::Error);
    return nullptr;
  }

  state->soundEngine = (SoundEngine *)state->createProcessor(
    Processors::SoundEngine, soundEngineJson);
  state->registerProcessorForDynamicParameters(state->soundEngine);
  deserialiseProcessorChildren(soundEngineJson, state->soundEngine);

  fixDeserialisedProcessorsStateIds(state.get(), &dynamicOptionFixups);
  for (auto &id : ParameterChangeReason::values)
    state->updateDynamicParameters(id);

  HostParameter::notifyParameterChange();

  return state;
}

void saveState(ComplexPlugin *plugin, const void *stateCtx, cplug_writeProc writeProc)
{
  if (!plugin->state_)
    return;

  getThreadLocals() = &plugin->renderer.generalData;
  defer{ getThreadLocals() = nullptr; };

  // flush all changes that might still exist, otherwise the save might be in an invalid state
  plugin->state_->flushHostParameters();

  auto &jsonContext = getJsonContext();
  jsonContext.pool = utils::Pool::createNested(getLocalScratch(), COMPLEX_KB(128));
  defer { utils::Pool::destroy(jsonContext.pool); jsonContext.pool = nullptr; };

  cjson *data = cjson_Create(cjson_Object);
  serialiseToJson(plugin->state_.get(), data);
  usize size = 0;
  char *dataString = cjson_Print(data, &size, true);
  writeProc(stateCtx, dataString, size);
}

struct PresetUpdate final : public UndoAction
{
  PresetUpdate(ComplexPlugin &plugin, utils::sp<State> newSavedState) :
    plugin(plugin), state{ COMPLEX_MOVE(newSavedState) }
  {
    redo = [](UndoAction *a)
    {
      auto *self = (PresetUpdate *)a;

      auto g = self->state->plugin->acquireProcessingLock();
      auto oldState = self->plugin.exchangeStates(COMPLEX_MOVE(self->state));
      self->state = COMPLEX_MOVE(oldState);

      return true;
    };

    undo = [](UndoAction *a)
    {
      auto *self = (PresetUpdate *)a;

      auto g = self->state->plugin->acquireProcessingLock();
      auto newState = self->plugin.exchangeStates(COMPLEX_MOVE(self->state));
      self->state = COMPLEX_MOVE(newState);

      return true;
    };
  }

  ComplexPlugin &plugin;
  utils::sp<State> state;
};

void loadState(ComplexPlugin *plugin, utils::string_view data)
{
  bool wasStateInitialised = plugin->wasStateInitialised;
  plugin->wasStateInitialised = true;

  utils::sp<State> state{};

  getThreadLocals() = &plugin->renderer.generalData;
  defer{ getThreadLocals() = nullptr; };

  if (data.size() != 0)
  {
    auto &jsonContext = getJsonContext();

    jsonContext.pool = utils::Pool::createNested(getLocalScratch(), COMPLEX_KB(128));
    defer { utils::Pool::destroy(jsonContext.pool); jsonContext.pool = nullptr; };

    const char *potentialError = nullptr;
    cjson *jsonData = cjson_ParseWithOpts(data.data(), data.size(), &potentialError, false);
    if (!jsonData)
    {
      showNativeMessageBox("Error opening preset",
        potentialError, MessageBoxType::Error);
    }
    else
    {
      upgradeSave(jsonData);
      state = deserialiseFromJson(plugin, jsonData);
    }
  }

  if (!state)
    state = plugin->loadDefaultPreset();

  auto [minOrder, maxOrder] = state->soundEngine->getMinMaxFFTOrder();
  state->fft = plugin->getFFTConverter(minOrder, maxOrder);

  if (wasStateInitialised && plugin->state_.get())
  {
    auto *storage = plugin->undoManager.beginNewTransaction();
    plugin->undoManager.perform(anew(storage, PresetUpdate, { *plugin, COMPLEX_MOVE(state) }));
  }
  else
    plugin->state_ = COMPLEX_MOVE(state);

  MainInterface::restartUI(plugin->state_.get());
}
