/*
  ==============================================================================

    load_save.cpp
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#include "load_save.hpp"

#include "Third Party/json/json.hpp"
#include <juce_core/juce_core.h>

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/ProcessorTree.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/EffectsState.hpp"
#include "Generation/EffectModules.hpp"
#include "Interface/LookAndFeel/Miscellaneous.hpp"
#include "update_types.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/PluginProcessor.hpp"

using json = nlohmann::json;

namespace Framework::LoadSave
{
  juce::File getConfigFile();
}

namespace
{
  class LoadingException final : public std::exception
  {
  public:
    LoadingException(juce::String message) : message_(std::move(message)) { }
    ~LoadingException() noexcept override = default;

    char const *what() const override { return message_.toRawUTF8(); }
    LoadingException &append(juce::String string) { message_ += string; return *this; }
    LoadingException &prepend(juce::String string) { message_ = string + message_; return *this; }

  private:
    juce::String message_;
  };

  json getConfigJson()
  {
    juce::File configFile = Framework::LoadSave::getConfigFile();
    if (!configFile.exists())
      return {};

    try
    {
      json parsed = json::parse(configFile.loadFileAsString().toStdString(), nullptr, false);
      if (parsed.is_discarded())
        return {};
      return parsed;
    }
    catch (const json::exception &)
    {
      return {};
    }
  }

  void saveConfigJson(const json &configState)
  {
    juce::File configFile = Framework::LoadSave::getConfigFile();

    if (!configFile.exists())
      configFile.create();
    configFile.replaceWithText(configState.dump(2, ' ', true));
  }

  void upgradeSave([[maybe_unused]] json &save)
  {

  }
}

namespace Framework::LoadSave
{
  juce::File getConfigFile()
  {
  #if defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
    juce::PropertiesFile::Options configOptions;
    configOptions.applicationName = "Complex";
    configOptions.osxLibrarySubFolder = "Application Support";
    configOptions.filenameSuffix = "config";

  #ifdef LINUX
    configOptions.folderName = "." + juce::String{ juce::CharPointer_UTF8{ JucePlugin_Name } }.toLowerCase();
  #else
    configOptions.folderName = juce::String{ juce::CharPointer_UTF8{ JucePlugin_Name } }.toLowerCase();
  #endif

    return configOptions.getDefaultFile();
  #else
    return File();
  #endif
  }



  std::pair<int, int> getWindowSize()
  {
    json data = getConfigJson();

    int width = Interface::kMinWidth;
    int height = Interface::kMinHeight;

    if (data.contains("window_width"))
      width = std::max<int>(Interface::kMinWidth, data["window_width"]);

    if (data.contains("window_height"))
      height = std::max<int>(Interface::kMinHeight, data["window_height"]);
    
    return { width, height };
  }

  double getWindowScale()
  {
    json data = getConfigJson();
    double scale = 1.0;

    if (data.contains("window_scale"))
      scale = data["window_scale"];

    return scale;
  }

  std::tuple<size_t, size_t, size_t> getParameterMappingsAndSidechains()
  {
    json data = getConfigJson();
    size_t parameterMappings = 64;
    size_t in = 0;
    size_t out = 0;

    if (data.contains("parameter_count"))
      parameterMappings = data["parameter_count"];

    if (data.contains("input_sidechains"))
      in = data["input_sidechains"];

    if (data.contains("output_sidechains"))
      out = data["output_sidechains"];

    return { parameterMappings, in, out };
  }

  void saveParameterMappings(size_t parameterMappings)
  {
    json data = getConfigJson();
    data["parameter_count"] = parameterMappings;
    saveConfigJson(data);
  }

  void saveWindowSize(int windowWidth, int windowHeight)
  {
    json data = getConfigJson();
    data["window_width"] = windowWidth;
    data["window_height"] = windowHeight;
    saveConfigJson(data);
  }

  void saveWindowScale(double windowScale)
  {
    json data = getConfigJson();
    data["window_scale"] = windowScale;
    saveConfigJson(data);
  }

  void writeSave(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    juce::File testSave = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
      .getChildFile(juce::File::createLegalFileName("test_save.json"));
    if (!testSave.replaceWithText(data.dump()))
    {
      COMPLEX_ASSERT_FALSE("Couldn't save");
    }
  }
}

namespace Framework
{
  bool PresetUpdate::perform()
  {
    /*if (oldSavedState_.has_value())
    {
      json previousState{};
      processorTree_.serialiseToJson(&previousState);
      oldSavedState_ = std::move(previousState);
    }

    processorTree_.clearState();
    auto &state = std::get()*/

    return true;
  }

  bool PresetUpdate::undo()
  {
    if (newSavedState_.has_value())
    {
      json newState{};
      processorTree_.serialiseToJson(&newState);
      newSavedState_ = std::move(newState);
    }

    processorTree_.clearState();

    return true;
  }
}

namespace Plugin
{
  void ProcessorTree::clearState()
  {

  }

  void ProcessorTree::serialiseToJson(void *jsonData) const
  {
    json &data = *static_cast<json *>(jsonData);

    std::vector<Generation::BaseProcessor *> topLevelProcessors{};
    for (auto &[id, processor] : allProcessors_.data)
      if (processor->getParentProcessorId() == processorTreeId)
        topLevelProcessors.push_back(processor.get());

    COMPLEX_ASSERT(!topLevelProcessors.empty());

    json topLevelProcessorsSerialised{};

    for (auto &topLevelProcessor : topLevelProcessors)
    {
      json &topLevelData = topLevelProcessorsSerialised.emplace_back();
      topLevelProcessor->serialiseToJson(&topLevelData);
    }

    data["version"] = JucePlugin_VersionString;
    data["tree"] = topLevelProcessorsSerialised;
  }

  void ComplexPlugin::deserialiseFromJson(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);

    // TODO: save previous state (if it exists) in case of loading failure or undo
    //auto *currentSave = new Framework::PresetUpdate{ *this };
    try
    {
      upgradeSave(data);

      json &soundEngine = data["tree"][0];
      std::string_view type = soundEngine["id"];
      if (type != Framework::Processors::SoundEngine::id().value())
        throw LoadingException{ "SoundEngine type doesn't match." };

      for (const auto &[_, parameter] : soundEngine["parameters"].items())
      {
        if (parameter["id"].get<std::string_view>() == Framework::Processors::SoundEngine::BlockSize::id().value())
        {
          minFFTOrder_.store(parameter["min_value"].get<u32>(), std::memory_order_release);
          maxFFTOrder_.store(parameter["max_value"].get<u32>(), std::memory_order_release);
          break;
        }
      }
      
      soundEngine_ = new Generation::SoundEngine{ this };
      soundEngine_->setParentProcessorId(processorTreeId);
      soundEngine_->deserialiseFromJson(&soundEngine);

      for (auto &reason : Framework::kAllChangeIds)
        updateDynamicParameters(reason);

      //pushUndo(currentSave, true);
    }
    catch (const LoadingException &e)
    {
      juce::String error = "There was an error opening the preset.\n";
      error += e.what();
      juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon, "Error opening preset", error);

      // TODO: restore old state
    }
    catch (const std::exception &e)
    {
      juce::String error = "An unknown error occured while opening preset.\n";
      error += e.what();
      juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon, "Error opening preset", error);

      // TODO: try to restore old state???? idk what happened
    }
  }

  void ComplexPlugin::loadDefaultPreset()
  {
    soundEngine_ = utils::as<Generation::SoundEngine>(createProcessor(Framework::Processors::SoundEngine::id().value()));
    auto *effectsState = createProcessor(Framework::Processors::EffectsState::id().value());
    auto *effectsLane = createProcessor(Framework::Processors::EffectsLane::id().value());
    auto *effectModule = createProcessor(Framework::Processors::EffectModule::id().value());
    auto *effect = createProcessor(Framework::Processors::BaseEffect::Dynamics::id().value());

    effectModule->insertSubProcessor(0, *effect);
    effectsLane->insertSubProcessor(0, *effectModule);
    effectsState->insertSubProcessor(0, *effectsLane);
    soundEngine_->insertSubProcessor(0, *effectsState);
    soundEngine_->setParentProcessorId(processorTreeId);

    static constexpr auto pluginParameterIds = Framework::Processors::SoundEngine::
      enum_ids_filter<[]<typename T>() { return requires{ T::parameter_tag; }; }, true>();

    auto min = std::min(pluginParameterIds.size(), parameterBridges_.size());
    for (size_t i = 0; i < min; ++i)
    {
      auto *parameter = getProcessorParameter(soundEngine_->getProcessorId(), pluginParameterIds[i]);
      COMPLEX_ASSERT(parameter);
      parameterBridges_[i]->resetParameterLink(parameter->getParameterLink(), true);
    }

    isLoaded_.store(true, std::memory_order_release);
  }

  auto ProcessorTree::createProcessor(std::string_view processorType, void *jsonData)
    -> Generation::BaseProcessor *
  {
    static constexpr auto processorTypes = Framework::Processors::
      enum_subtypes_filter_recursive<Framework::kGetProcessorPredicate>();

    return [&]<typename ... Ts>(const std::tuple<std::type_identity<Ts>...> &)
    {
      Generation::BaseProcessor *processor = nullptr;
      auto create = [&]<std::derived_from<Generation::BaseProcessor> T>()
      {
        T *processor = new T{ this };
        if (jsonData != nullptr)
          processor->deserialiseFromJson(jsonData);
        else
          processor->initialiseParameters();
        return processor;
      };
      std::ignore = ((Ts::id() == processorType && (processor = create.template operator()<typename Ts::linked_type>(), true)) || ...);
      if (processor == nullptr)
      {
        throw LoadingException{ std::format("Processor with id {} does not exist", processorType) };
      }

      return processor;
    }(processorTypes);
  }
}

static void handleIndexedData(Framework::ParameterDetails &details, json *indexedData)
{
  std::vector<std::pair<std::string_view *, size_t>> stringRelocations{};

  size_t relocationIndex = 0;
  auto appendAndAddRelocation = [&](std::string_view &relocate, std::string_view text)
  {
    stringRelocations.emplace_back(&relocate, relocationIndex++);
    details.dynamicData->stringData.append(text);
    details.dynamicData->stringData += '\0';
  };

  auto doStringRelocations = [&]()
  {
    for (size_t i = 0, textIndex = 0; i < stringRelocations.size(); ++i)
    {
      *stringRelocations[i].first = details.dynamicData->stringData.data() + textIndex;
      textIndex += stringRelocations[i].first->size() + 1;
    }
  };

  if (details.scale == Framework::ParameterScale::IndexedNumeric)
  {
    float minValue = details.minValue;
    float maxValue = details.maxValue;
    std::vector<std::string> generatedStrings{};
    details.dynamicData->dataLookup.reserve((size_t)(maxValue - minValue) + 1);
    for (float i = minValue; i <= maxValue; ++i)
    {
      auto string = details.generateNumeric(i, details);
      auto &element = details.dynamicData->dataLookup.emplace_back();
      appendAndAddRelocation(element.displayName, string);
    }

    doStringRelocations();
    
    return;
  }

  std::vector<size_t> accountedElements{};
  accountedElements.reserve(details.indexedData.empty() ? indexedData->size() : details.indexedData.size());
  details.dynamicData->dataLookup.reserve(details.indexedData.empty() ? indexedData->size() : details.indexedData.size());

  for (auto &[_, value] : indexedData->items())
  {
    if (!value.contains("id") || !value.contains("display_name"))
    {
      throw LoadingException{ std::format("Missing indexed attributes in parameter {} ({})",
        details.displayName, details.id) };
    }

    std::string_view id = value["id"];
    std::string_view name = value["display_name"];
    u64 count = value["count"].get<u64>();
    auto dynamicUpdateUuid = (!value.contains("dynamic_update_uuid")) ?
      std::string_view{} : value["dynamic_update_uuid"].get<std::string_view>();

    if (details.indexedData.empty())
    {
      auto &element = details.dynamicData->dataLookup.emplace_back();
      element.count = count;
      appendAndAddRelocation(element.id, id);
      appendAndAddRelocation(element.displayName, name);
      if (!dynamicUpdateUuid.empty())
      {
        if (std::ranges::find(Framework::kAllChangeIds, dynamicUpdateUuid) == Framework::kAllChangeIds.end())
        {
          // TODO: add option to continue loading without this indexed value
          throw LoadingException{ std::format("Unknown dynamic update reason ({}) for indexed element {} ({})",
            dynamicUpdateUuid, name, id) };
        }
        appendAndAddRelocation(element.dynamicUpdateUuid, dynamicUpdateUuid);
      }

      continue;
    }

    auto iter = std::ranges::find_if(details.indexedData, [id](const auto &data) { return data.id == id; });
    if (iter == details.indexedData.end())
    {
      throw LoadingException{ std::format("Unknown indexed element {} ({}) in parameter {} ({})",
        name, id, details.displayName, details.id) };
    }

    accountedElements.emplace_back((size_t)(iter - details.indexedData.begin()));
    auto &element = details.dynamicData->dataLookup.emplace_back(*iter);

    bool isNameSame = element.displayName == name;
    bool isDynamicUpdateUuidSame = dynamicUpdateUuid == element.dynamicUpdateUuid;
    element.count = count;

    if (isNameSame && isDynamicUpdateUuidSame)
      continue;

    if (!isNameSame)
      appendAndAddRelocation(element.displayName, name);

    if (!isDynamicUpdateUuidSame && !dynamicUpdateUuid.empty())
    {
      if (std::ranges::find(Framework::kAllChangeIds, dynamicUpdateUuid) == Framework::kAllChangeIds.end())
      {
        // TODO: add option to continue loading without this indexed value
        throw LoadingException{ std::format("Unknown dynamic update reason ({}) for indexed element {} ({})",
          dynamicUpdateUuid, name, id) };
      }

      appendAndAddRelocation(element.dynamicUpdateUuid, dynamicUpdateUuid);
    }
  }

  // do string_view relocations
  doStringRelocations();

  // append elements that were not present in the save
  for (size_t i = 0; i < details.indexedData.size(); ++i)
  {
    auto erasedCount = std::erase(accountedElements, i);
    if (erasedCount != 0)
      continue;

    details.dynamicData->dataLookup.emplace_back(details.indexedData[i]);
  }

  details.indexedData = details.dynamicData->dataLookup;
}

namespace Generation
{
  void BaseProcessor::serialiseToJson(void *jsonData) const
  {
    COMPLEX_ASSERT(getParentProcessorId() != 0 && "This processor wasn't assigned a parent \
      or the parent forgot to set their id inside the child");

    json &processorInfo = *static_cast<json *>(jsonData);
    processorInfo["id"] = processorType_;

    std::vector<json> subProcessors{};
    subProcessors.reserve(subProcessors_.size());
    for (auto &subProcessor : subProcessors_)
    {
      json &subProcessorData = subProcessors.emplace_back();
      subProcessor->serialiseToJson(&subProcessorData);
    }
    processorInfo["processors"] = std::move(subProcessors);

    std::vector<json> parameters{};
    for (auto &[name, parameter] : processorParameters_.data)
    {
      json &parameterData = parameters.emplace_back();
      parameter->serialiseToJson(&parameterData);
    }
    processorInfo["parameters"] = std::move(parameters);
  }

  void BaseProcessor::deserialiseFromJson(std::span<const std::string_view> parameterIds, 
    BaseProcessor *processor, void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    auto *processorTree = processor->processorTree_;

    auto getNameId = [processor]()
    {
      auto id = processor->getProcessorType();
      auto name = Framework::Processors::enum_name_by_id_recursive(id, false).value();
      return std::pair{ name, id };
    };

    for (auto &[_, value] : data["parameters"].items())
    {
      utils::up<Framework::ParameterValue> parameter;
      try
      {
        parameter = Framework::ParameterValue::deserialiseFromJson(processorTree, &value);
      } 
      catch (LoadingException &e)
      {
        auto [name, id] = getNameId();
        throw e.prepend(std::format("Inside processor {} ({})\n", name, id));
      }
      auto parameterId = parameter->getParameterId();
      if (std::ranges::find(parameterIds, parameterId) == parameterIds.end())
      {
        auto [name, id] = getNameId();
        throw LoadingException{ std::format("Parameter {} ({}) is not part of processor {} ({}).", 
          parameter->getParameterName(), parameterId, name, id) };
      }
      processor->processorParameters_.data.emplace_back(parameterId, std::move(parameter));
    }

    for (auto &[_, value] : data["processors"].items())
    {
      if (!value.contains("id"))
      {
        auto [name, id] = getNameId();
        throw LoadingException{ std::format("Unknown processor without id inside {} ({})", name, id) };
      }

      std::string_view subProcessorsId = value["id"];
      Generation::BaseProcessor *subProcessor;
      try
      {
        subProcessor = processorTree->createProcessor(subProcessorsId, &value);
      }
      catch (LoadingException &e)
      {
        auto [name, id] = getNameId();
        throw e.prepend(std::format("Inside processor {} ({})\n", name, id));
      }
      processor->insertSubProcessor(processor->subProcessors_.size(), *subProcessor);
    }
  }

  void SoundEngine::deserialiseFromJson(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    json &subProcessors = data["processors"];

    if (subProcessors.size() > 1)
      throw LoadingException{ "More than one EffectsState is defined." };

    json &effectsState = subProcessors[0];
    std::string_view type = effectsState["id"];
    if (Framework::Processors::EffectsState::id().value() != type)
      throw LoadingException{ "EffectsState type doesn't match." };

    static constexpr auto kSoundEngineParameters = Framework::Processors::SoundEngine::enum_ids_filter<
      Framework::kGetParameterPredicate, true>();

    BaseProcessor::deserialiseFromJson(kSoundEngineParameters, this, &data);
  }

  void EffectsState::deserialiseFromJson(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    json &subProcessors = data["processors"];

    for (auto &[_, value] : subProcessors.items())
      if (value["id"].get<std::string_view>() != Framework::Processors::EffectsLane::id().value())
        throw LoadingException{ "Non-EffectLane found in EffectsState" };
    
    static constexpr auto kEffectsStateParameters = Framework::Processors::EffectsState::enum_ids_filter<
      Framework::kGetParameterPredicate, true>();

    BaseProcessor::deserialiseFromJson(kEffectsStateParameters, this, &data);
  }

  void EffectsLane::deserialiseFromJson(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    json &subProcessors = data["processors"];

    for (auto &[_, value] : subProcessors.items())
      if (value["id"].get<std::string_view>() != Framework::Processors::EffectModule::id().value())
        throw LoadingException{ "Non-EffectModule found in EffectsLane" };

    static constexpr auto kEffectsLaneParameters = Framework::Processors::EffectsLane::enum_ids_filter<
      Framework::kGetParameterPredicate, true>();

    BaseProcessor::deserialiseFromJson(kEffectsLaneParameters, this, &data);
  }

  void EffectModule::deserialiseFromJson(void *jsonData)
  {
    json &data = *static_cast<json *>(jsonData);
    json &subProcessors = data["processors"];

    static constexpr auto kContainedEffects = Framework::Processors::BaseEffect::enum_ids_filter<
      Framework::kGetProcessorPredicate, true>();

    for (auto &[_, value] : subProcessors.items())
      if (std::ranges::find(kContainedEffects, value["id"].get<std::string_view>()) == kContainedEffects.end())
        throw LoadingException{ "Non-Effect found in EffectModule" };

    static constexpr auto kEffectModuleParameters = Framework::Processors::EffectModule::enum_ids_filter<
      Framework::kGetParameterPredicate, true>();

    BaseProcessor::deserialiseFromJson(kEffectModuleParameters, this, &data);
  }

  void BaseEffect::deserialiseFromJson(void *jsonData)
  {
    BaseProcessor::deserialiseFromJson(parameters_, this, jsonData);
  }
}

namespace Framework
{
  void ParameterValue::serialiseToJson(void *jsonData) const
  {
    json &data = *static_cast<json *>(jsonData);
    data["id"] = details_.id;
    data["display_name"] = details_.displayName;
    data["value"] = normalisedValue_;
    data["min_value"] = details_.minValue;
    data["max_value"] = details_.maxValue;
    data["default_value"] = details_.defaultValue;
    data["default_normalised_value"] = details_.defaultNormalisedValue;
    data["scale"] = ParameterScale::enum_id(details_.scale).value();
    data["is_stereo"] = (details_.flags & ParameterDetails::Stereo) != 0;
    data["is_modulatable"] = (details_.flags & ParameterDetails::Modulatable) != 0;
    data["is_extensible"] = (details_.flags & ParameterDetails::Extensible) != 0;
    if (parameterLink_.hostControl)
      data["automation_slot"] = parameterLink_.hostControl->getIndex();
    if (details_.scale == ParameterScale::Indexed && !details_.indexedData.empty())
    {
      json indexedData{};
      for (const auto &element : details_.indexedData)
      {
        json &elementData = indexedData.emplace_back();
        elementData["id"] = element.id;
        elementData["display_name"] = element.displayName;
        elementData["count"] = element.count;
        if (!element.dynamicUpdateUuid.empty())				
          elementData["dynamic_update_uuid"] = element.dynamicUpdateUuid;
      }
      data["indexed_data"] = std::move(indexedData);
    }
    
    // TODO: add modulators
  }

  

  utils::up<ParameterValue> ParameterValue::deserialiseFromJson(
    Plugin::ProcessorTree *processorTree, void *jsonData)
  {
    auto parameter = utils::up<ParameterValue>{ new ParameterValue{} };

    json &data = *static_cast<json *>(jsonData);

    std::string_view id = data["id"].get<std::string_view>();
    if (auto details = Framework::getParameterDetails(id); details.has_value())
      parameter->details_ = details.value();
    else
      throw LoadingException{ std::format("Nonexistent parameter ({})", id) };

    if (parameter->details_.scale != ParameterScale::enum_value_by_id(data["scale"].get<std::string_view>()))
    {
      COMPLEX_ASSERT_FALSE();
      // TODO: log this
    }

    auto minValue = data["min_value"].get<float>();
    auto maxValue = data["max_value"].get<float>();
    std::optional<u64> automationSlot{};
    if (data.contains("automation_slot"))
      automationSlot = data["automation_slot"].get<u64>();
    parameter->details_.flags |= (data["is_stereo"].get<bool>()) ? ParameterDetails::Stereo : 0;

    // if the save contains an expanded range but the parameter in this version isn't extensible
    // then there's nothing we can do about it
    if ((parameter->details_.minValue > minValue || parameter->details_.maxValue < maxValue) &&
      (parameter->details_.flags & ParameterDetails::Extensible) == 0)
    {
      COMPLEX_ASSERT_FALSE();
      // TODO: log this
    }

    bool changedMinMax = false;
    if (parameter->details_.minValue != minValue || parameter->details_.maxValue != maxValue)
    {
      // the range of the parameter was changed while being automated
      // we mustn't change the range of the parameter otherwise we're going to ruin someone's project
      if (automationSlot.has_value())
      {
        // if we're here then that means the range was changed but it's not larger than the range available
        parameter->details_.minValue = minValue;
        parameter->details_.maxValue = maxValue;
        changedMinMax = true;
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

    // indexed values validation and deserialisation
    if ((parameter->details_.scale == ParameterScale::Indexed && data.contains("indexed_data")) ||
      (parameter->details_.scale == ParameterScale::IndexedNumeric && changedMinMax))
    {
      parameter->details_.dynamicData = utils::sp<IndexedData::DynamicData>::create();
      handleIndexedData(parameter->details_, &data["indexed_data"]);
      processorTree->registerDynamicParameter(parameter.get());
    }

    // TODO: fit modulation here

    [[maybe_unused]] auto value =
      parameter->normalisedValue_ = std::clamp(data["value"].get<float>(), 0.0f, 1.0f);
    parameter->isDirty_ = true;
    parameter->updateValue(processorTree->getSampleRate());

    // paranoid check just in case 
    COMPLEX_ASSERT(parameter->normalisedValue_ == value);

    if (automationSlot.has_value())
    {
      auto slot = automationSlot.value();
      auto parameterBridges = processorTree->getParameterBridges();

      // if we don't have enough parameters then too bad, we only guarantee kMaxParameterMappings generic parameters
      if (slot < parameterBridges.size())
        parameterBridges[slot]->resetParameterLink(&parameter->parameterLink_, true);
    }

    return parameter;
  }
}

void ComplexAudioProcessor::getStateInformation(juce::MemoryBlock &destinationData)
{
  if (!isLoaded_.load(std::memory_order_acquire))
  {
    loadDefaultPreset();
  }

  json data{};
  serialiseToJson(&data);
  juce::String dataString = data.dump(2, ' ', true);
  juce::MemoryOutputStream stream;
  stream.writeString(dataString);
  destinationData.append(stream.getData(), stream.getDataSize());
}

void ComplexAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
  if (!data || sizeInBytes == 0)
  {
    loadDefaultPreset();
    return;
  }

  juce::MemoryInputStream stream{ data, (size_t)sizeInBytes, false };
  juce::String dataString = stream.readEntireStreamAsString();
  
  json jsonData{};
  try
  {
    jsonData = json::parse(dataString.toRawUTF8());
  }
  catch (const std::exception &)
  {
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::NoIcon, 
      "Error opening preset", "Couldn't parse preset json.\n");
  }

  suspendProcessing(true);
  deserialiseFromJson(&jsonData);
  suspendProcessing(false);

  isLoaded_.store(true, std::memory_order_release);

  getRenderer().updateFullGui();
}
