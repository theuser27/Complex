/*
	==============================================================================

		load_save.cpp
		Created: 3 Dec 2022 1:46:31am
		Author:  theuser27

	==============================================================================
*/

#include "load_save.h"

#include "Third Party/json/json.hpp"
#include "JuceHeader.h"
#include "Framework/parameter_value.h"
#include "Framework/parameter_bridge.h"
#include "Plugin/ProcessorTree.h"
#include "Generation/SoundEngine.h"
#include "Generation/EffectsState.h"
#include "Interface/LookAndFeel/Miscellaneous.h"
#include "update_types.h"
#include "Plugin/Renderer.h"
#include "Plugin/PluginProcessor.h"

using json = nlohmann::json;

namespace
{
	class LoadingException final : public std::exception
	{
	public:
		LoadingException(String message) : message_(std::move(message)) { }
		~LoadingException() noexcept override = default;

		char const *what() const override { return message_.toRawUTF8(); }

	private:
		String message_;
	};

	json getConfigJson()
	{
		File config_file = Framework::LoadSave::getConfigFile();
		if (!config_file.exists())
			return {};

		try
		{
			json parsed = json::parse(config_file.loadFileAsString().toStdString(), nullptr, false);
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
		File config_file = Framework::LoadSave::getConfigFile();

		if (!config_file.exists())
			config_file.create();
		config_file.replaceWithText(configState.dump());
	}

	enum class ChangeType { NoEffect, Breaking };

	void upgradeSave([[maybe_unused]] json &save)
	{

	}
}

namespace Framework
{
	namespace LoadSave
	{
		File getConfigFile()
		{
		#if defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
			PropertiesFile::Options config_options;
			config_options.applicationName = "Complex";
			config_options.osxLibrarySubFolder = "Application Support";
			config_options.filenameSuffix = "config";

		#ifdef LINUX
			config_options.folderName = "." + String(ProjectInfo::projectName).toLowerCase();
		#else
			config_options.folderName = String(ProjectInfo::projectName).toLowerCase();
		#endif

			return config_options.getDefaultFile();
		#else
			return File();
		#endif
		}

		File getDefaultSkin()
		{
	#if defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
			PropertiesFile::Options config_options;
			config_options.applicationName = "Complex";
			config_options.osxLibrarySubFolder = "Application Support";
			config_options.filenameSuffix = "skin";

	#ifdef LINUX
			config_options.folderName = "." + String(ProjectInfo::projectName).toLowerCase();
	#else
			config_options.folderName = String(ProjectInfo::projectName).toLowerCase();
	#endif

			return config_options.getDefaultFile();
	#else
			return File();
	#endif
		}

		String getVersion()
		{
			json data = getConfigJson();

			if (!data.count("pluginVersion"))
				return "0.0.0";

			std::string version = data["pluginVersion"];
			return version;
		}

		std::pair<int, int> getWindowSize()
		{
			json data = getConfigJson();

			int width;
			int height;

			if (!data.count("windowWidth"))
				width = Interface::kMinWidth;
			else
				width = std::max<int>(Interface::kMinWidth, data["windowWidth"]);

			if (!data.count("windowHeight"))
				height = Interface::kMinHeight;
			else
				height = std::max<int>(Interface::kMinHeight, data["windowHeight"]);
		
			return { width, height };
		}

		double getWindowScale()
		{
			json data = getConfigJson();
			double scale;

			if (!data.count("windowScale"))
				scale = 1.0;
			else
				scale = data["windowScale"];

			return scale;
		}

		void saveWindowSize(int windowWidth, int windowHeight)
		{
			json data = getConfigJson();
			data["windowWidth"] = windowWidth;
			data["windowHeight"] = windowHeight;
			saveConfigJson(data);
		}

		void saveWindowScale(double windowScale)
		{
			json data = getConfigJson();
			data["windowScale"] = windowScale;
			saveConfigJson(data);
		}

		void writeSave(std::any jsonData)
		{
			json data = std::any_cast<json>(std::move(jsonData));
			File testSave = File::getSpecialLocation(File::userDesktopDirectory).getChildFile(File::createLegalFileName("testSave.json"));
			if (!testSave.replaceWithText(data.dump()))
			{
				COMPLEX_ASSERT_FALSE("Couldn't save");
			}
		}
	}
}

namespace Plugin
{
	std::any ProcessorTree::serialiseToJson() const
	{
		std::vector<Generation::BaseProcessor *> topLevelProcessors{};
		for (auto &[id, processor] : allProcessors_.data)
			if (processor->getParentProcessorId() == processorTreeId)
				topLevelProcessors.push_back(processor.get());

		COMPLEX_ASSERT(!topLevelProcessors.empty());

		json topLevelProcessorsSerialised{};

		for (auto &topLevelProcessor : topLevelProcessors)
			topLevelProcessorsSerialised.push_back(std::any_cast<json>(topLevelProcessor->serialiseToJson()));

		json state{};
		state["version"] = ProjectInfo::versionString;
		state["tree"] = topLevelProcessorsSerialised;
		return state;
	}

	void ProcessorTree::deserialiseFromJson(std::any jsonData)
	{
		json data = std::any_cast<json>(std::move(jsonData));

		auto *currentSave = new Framework::PresetUpdate(*this);
		try
		{
			upgradeSave(data);

			json soundEngine = data["tree"][0];
			std::string_view type = soundEngine["type"];
			if (Framework::BaseProcessors::SoundEngine::id().value() != type)
				throw LoadingException{ "SoundEngine type doesn't match." };
			


			pushUndo(currentSave, true);
		}
		catch (const LoadingException &e)
		{
			String error = "There was an error opening the preset.\n";
			error += e.what();
			NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::NoIcon, "Error opening preset", error);

			// TODO: restore old state
		}
		catch (const std::exception &e)
		{
			String error = "An unknown error occured while opening preset.\n";
			error += e.what();
			NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::NoIcon, "Error opening preset", error);

			// TODO: try to restore old state???? idk what happened
		}
	}
}

namespace Generation
{
	std::any BaseProcessor::serialiseToJson() const
	{
		json processorInfo{};
		processorInfo["type"] = processorType_;

		std::vector<json> subProcessors{};
		subProcessors.reserve(subProcessors_.size());
		for (auto &subProcessor : subProcessors_)
			subProcessors.emplace_back(std::any_cast<json>(subProcessor->serialiseToJson()));
		processorInfo["subProcessors"] = subProcessors;

		std::vector<json> parameters{};
		for (auto &[name, parameter] : processorParameters_.data)
			parameters.emplace_back(std::any_cast<json>(parameter->serialiseToJson()));
		processorInfo["parameters"] = parameters;

		return processorInfo;
	}

	void BaseProcessor::deserialiseFromJson(BaseProcessor *processor, std::any jsonData)
	{
		json data = std::any_cast<json>(std::move(jsonData));

		auto type = data["type"].get<std::string_view>();
		json parameters = data["parameters"];
		for (auto &[key, value] : parameters.items())
		{
			auto parameter = Framework::ParameterValue::deserialiseFromJson(processor->processorTree_, value, type);
			auto name = parameter->getParameterDetails().pluginName;
			processor->processorParameters_.data.emplace_back(name, std::move(parameter));
		}
	}

	void SoundEngine::deserialiseFromJson(std::any jsonData)
	{
		json data = std::any_cast<json>(std::move(jsonData));
		json subProcessors = data["subProcessors"];

		if (subProcessors.size() > 1)
			throw LoadingException{ "More than one EffectsState is defined." };

		json effectsState = subProcessors[0];
		std::string_view type = effectsState["type"];
		if (Framework::BaseProcessors::EffectsState::id().value() != type)
			throw LoadingException{ "EffectsState type doesn't match." };

		BaseProcessor::deserialiseFromJson(this, std::move(data));
		effectsState_->deserialiseFromJson(std::move(effectsState));
	}

	void EffectsState::deserialiseFromJson(std::any jsonData)
	{
		json data = std::any_cast<json>(std::move(jsonData));

	}
}

namespace Framework
{
	std::any ParameterValue::serialiseToJson() const
	{
		json parameterInfo{};
		parameterInfo["id"] = Parameters::getIdString(details_.pluginName);
		parameterInfo["value"] = normalisedValue_;
		parameterInfo["minValue"] = details_.minValue;
		parameterInfo["maxValue"] = details_.maxValue;
		parameterInfo["defaultValue"] = details_.defaultValue;
		parameterInfo["defaultNormalisedValue"] = details_.defaultNormalisedValue;
		parameterInfo["isStereo"] = details_.isStereo;
		parameterInfo["isExtensible"] = details_.isExtensible;
		if (parameterLink_.hostControl)
			parameterInfo["automationSlot"] = parameterLink_.hostControl->getIndex();
		// TODO: add modulators 

		return parameterInfo;
	}

	std::unique_ptr<ParameterValue> ParameterValue::deserialiseFromJson(Plugin::ProcessorTree *processorTree,
		std::any jsonData, std::string_view processorId)
	{
		auto parameter = std::unique_ptr<ParameterValue>{ new ParameterValue() };

		json parameterInfo = std::any_cast<json>(std::move(jsonData));

		std::string_view id = parameterInfo["id"].get<std::string_view>();
		if (auto details = Parameters::getDetailsId(id))
			parameter->details_ = details.value();
		else
			throw LoadingException{ std::format("Save contains incompatible parameter with id of {}.", id) };

		// is the found parameter even part of the processor
		if (parameter->details_.pluginName.find(BaseProcessors::enum_name_by_id_recursive(processorId, false).value()) == std::string_view::npos)
			throw LoadingException{ std::format("Parameter with id {} is not part of processor with id {}.", id, processorId) };

		parameter->normalisedValue_ = std::clamp(parameterInfo["value"].get<float>(), 0.0f, 1.0f);

		// parameter is not constrainable, so we don't change anything else
		if (!parameterInfo.contains("minValue") || !parameterInfo.contains("maxValue"))
			return parameter;

		auto minValue = parameterInfo["minValue"].get<float>();
		auto maxValue = parameterInfo["maxValue"].get<float>();
		std::optional<u64> automationSlot{};
		if (parameterInfo.contains("automationSlot"))
			automationSlot = parameterInfo["automationSlot"].get<u64>();

		// the range of the parameter was changed while being automated
		// we mustn't change the range of the parameter otherwise we're going to ruin someone's project
		if ((parameter->details_.minValue != minValue || parameter->details_.maxValue != maxValue) && automationSlot)
		{
			// if the save contains an expanded range but the parameter in this version isn't extensible
			// then there's nothing we can do about it
			if ((parameter->details_.minValue >= minValue || parameter->details_.maxValue <= maxValue) && !parameter->details_.isExtensible)
			{
				// TODO: decide what happens next, ideally this should never even happen
			}

			// if we're here then that means the range was changed but it's not larger than the range available
			parameter->details_.minValue = minValue;
			parameter->details_.maxValue = maxValue;
		}

		if (automationSlot.has_value())
		{
			auto slot = automationSlot.value();
			auto &parameterBridges = processorTree->getParameterBridges();

			// if we don't have enough parameters then too bad, we only guarantee 64 generic parameters so far
			if (slot < parameterBridges.size())
				parameterBridges[BaseProcessors::SoundEngine::enum_count(nested_enum::OuterNodes) + slot]->resetParameterLink(&parameter->parameterLink_);
		}

		parameter->details_.isStereo = parameterInfo["isStereo"].get<bool>();
		parameter->details_.isModulatable = parameterInfo["isModulatable"].get<bool>();

		return parameter;
	}
}

void ComplexAudioProcessor::getStateInformation(MemoryBlock &destinationData)
{
	String dataString = std::any_cast<json>(serialiseToJson()).dump();
	MemoryOutputStream stream;
	stream.writeString(dataString);
	destinationData.append(stream.getData(), stream.getDataSize());
}

void ComplexAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
	MemoryInputStream stream{ data, (size_t)sizeInBytes, false };
	String dataString = stream.readEntireStreamAsString();
	
	json jsonData{};
	try
	{
		jsonData = json::parse(dataString.toStdString());
	}
	catch (const std::exception &)
	{
		NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::NoIcon, "Error opening preset", 
			"Couldn't parse preset json.\n");
	}

	suspendProcessing(true);
	deserialiseFromJson(std::move(jsonData));
	suspendProcessing(false);

	getRenderer().updateFullGui();
}
