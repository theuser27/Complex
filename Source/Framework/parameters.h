/*
	==============================================================================

		parameters.h
		Created: 11 Jul 2022 5:25:41pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "vector_map.h"
#include "plugin_strings.h"

namespace Framework
{
	// symmetric types apply the flipped curve to negative values
	// all x values are normalised
	enum class ParameterScale : u32
	{
		Toggle,									// round(x)
		Indexed,								// round(x * n)
		Linear,									// x
		Clamp,									// clamp(x, min, max)
		Quadratic,							// x^2
		SymmetricQuadratic,			// x^2 for x >= 0, and -x^2 for x < 0
		Cubic,									// x^3
		Loudness,								// standard 20 * log10(x)
		SymmetricLoudness,			// 20 * log10(x) for x >= 0, and -20 * log10(x) for x < 0
		Frequency,							// (sampleRate / 2 * minFrequency) ^ x
		SymmetricFrequency,			// same as above but -(sampleRate / 2 * minFrequency) ^ (-x) for x < 0
	};

	struct ParameterDetails
	{
		std::string_view name;
		std::string_view displayName;
		float minValue = 0.0f;
		float maxValue = 1.0f;
		float defaultValue = 0.0f;
		float defaultNormalisedValue = 0.0f;
		ParameterScale scale = ParameterScale::Linear;
		std::string_view displayUnits = "";
		std::span<const std::string_view> stringLookup{};
		bool isStereo = false;
		bool isModulatable = true;
		UpdateFlag updateFlag = UpdateFlag::Realtime;
	};

	enum class PluginParameters : u32 { MasterMix = 0, BlockSize, Overlap, WindowType, WindowAlpha, OutGain, Size };

	inline constexpr std::array<ParameterDetails, (u32)PluginParameters::Size> pluginParameterList =
	{
		ParameterDetails
		{ "MASTER_MIX", "Master Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, false, true, UpdateFlag::BeforeProcess },
		{ "BLOCK_SIZE", "Block Size", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder, 
											(float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), ParameterScale::Indexed, "",
		                  kFFTSizeNames, false, true, UpdateFlag::BeforeProcess },
		{ "OVERLAP", "Overlap", 0.0f, kMaxWindowOverlap, kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Clamp, "%" },
		{ "WINDOW_TYPE", "Window Type", 0.0f, static_cast<u32>(WindowTypes::Size) - 1, 1.0f, 1.0f / static_cast<u32>(WindowTypes::Size),
											ParameterScale::Indexed, "", kWindowNames },
		{ "WINDOW_ALPHA", "Window Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear },
		{ "OUT_GAIN", "Out Gain" , -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", {}, false, true, UpdateFlag::BeforeProcess }
	};
	
	enum class EffectsChainParameters : u32 { ChainEnabled = 0, Input, Output, GainMatching, Size };

	inline constexpr std::array<ParameterDetails, (u32)EffectsChainParameters::Size> effectChainParameterList =
	{
		ParameterDetails
		{ "CHAIN_ENABLED", "Chain Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ "INPUT", "Input", 0.0f, kNumInputsOutputs + kMaxNumChains, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kInputNames, false, true, UpdateFlag::BeforeProcess },
		{ "OUTPUT", "Output", 0.0f, kNumInputsOutputs, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kOutputNames, false, true, UpdateFlag::BeforeProcess },
		{ "GAIN_MATCHING", "Gain Matching", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", 
		                  kOffOnNames, false, true, UpdateFlag::BeforeProcess }
	};

	enum class EffectModuleParameters : u32 { ModuleEnabled = 0, ModuleType, ModuleMix, ModuleGain, Size };

	inline constexpr std::array<ParameterDetails, (u32)EffectModuleParameters::Size> effectModuleParameterList =
	{
		ParameterDetails
		{ "MODULE_ENABLED", "Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ "MODULE_TYPE", "Module Type", 0.0f, static_cast<float>(EffectModuleTypes::Size) - 1, 1.0f, 
		                  1.0f / static_cast<float>(EffectModuleTypes::Size), ParameterScale::Indexed, "",
		                  kEffectModuleNames, false, true, UpdateFlag::AfterProcess },
		{ "MODULE_MIX", "Module Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, true },
		{ "MODULE_GAIN", "Module Gain", -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", {}, true }
	};

	enum class BaseEffectParameters : u32 { Type = 0, LowBound, HighBound, ShiftBounds, Size };

	inline constexpr std::array<ParameterDetails, (u32)BaseEffectParameters::Size> baseEffectParameterList =
	{
		ParameterDetails
		{ "FX_TYPE", "Type", 0.0f, kMaxEffectTypes - 1, 0.0f, 0.0f, ParameterScale::Indexed, "", kGenericEffectTypeNames },
		{ "FX_LOW_BOUND", "Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, "", {}, true },
		{ "FX_HIGH_BOUND", "High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, "", {}, true },
		{ "FX_SHIFT_BOUNDS", "Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "", {}, true}
	};

	enum class FilterParameters : u32
	{
		NormalGain = (u32)BaseEffectParameters::Size,
		RegularGain = NormalGain,

		NormalCutoff,
		RegularCutoff = NormalCutoff,

		NormalSlope,
		RegularPhase = NormalSlope,

		RegularStretch,

		SizeOverall = RegularStretch - (u32)BaseEffectParameters::Size + 1,
		SizeNormal = NormalSlope - (u32)BaseEffectParameters::Size + 1,
		SizeRegular = RegularStretch - (u32)BaseEffectParameters::Size + 1
	};

	inline constexpr std::array<std::array<ParameterDetails,
		(u32)FilterParameters::SizeOverall>, (u32)FilterTypes::Size> filterEffectParameterList =
	{
		std::array<ParameterDetails, (u32)FilterParameters::SizeOverall>
		{
			ParameterDetails
			{ "FILTER_NORMAL_FX_GAIN", "Filter Gain", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", {}, true },
			{ "FILTER_NORMAL_FX_CUTOFF", "Filter Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },
			{ "FILTER_NORMAL_FX_SLOPE", "Filter Slope", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "", {}, true }
		},
		{
			ParameterDetails
			{ "FILTER_REGULAR_FX_GAIN", "Filter Gain", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", {}, true },
			{ "FILTER_REGULAR_FX_CUTOFF", "Filter Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },
			{ "FILTER_REGULAR_FX_PHASE", "Filter Phase", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "", {}, true },
			{ "FILTER_REGULAR_FX_STRETCH", "Filter Stretch", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true }
		}
	};

	enum class ContrastParameters : u32
	{
		ClassicContrastDepth = (u32)BaseEffectParameters::Size,

		SizeOverall = ClassicContrastDepth - (u32)BaseEffectParameters::Size + 1,
		SizeClassic = ClassicContrastDepth - (u32)BaseEffectParameters::Size + 1
	};

	inline constexpr std::array<std::array<ParameterDetails,
		(u32)ContrastParameters::SizeOverall>, (u32)ContrastTypes::Size> contrastEffectParameterList =
	{
		std::array<ParameterDetails, (u32)ContrastParameters::SizeOverall>
		{
			ParameterDetails
			{ "CONTRAST_FX_DEPTH", "Contrast Depth", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "", {}, true }
		}
	};

	namespace
	{
		class ParameterDetailsGeneration
		{
		public:
			ParameterDetailsGeneration() = delete;
			ParameterDetailsGeneration(const ParameterDetailsGeneration &) = delete;
			void operator=(const ParameterDetailsGeneration &) = delete;
			ParameterDetailsGeneration(ParameterDetailsGeneration &&) = delete;
			void operator=(ParameterDetailsGeneration &&) = delete;
			~ParameterDetailsGeneration() = delete;

			static consteval auto getParameterLookup()
			{
				constexpr size_t totalSize = pluginParameterList.size() + 
					effectChainParameterList.size() + effectModuleParameterList.size() +
					baseEffectParameterList.size() + getParametersSize(filterEffectParameterList) +
					getParametersSize(contrastEffectParameterList);

				constexpr size_t asdf = getParametersSize(filterEffectParameterList);

				ConstexprMap<std::string_view, ParameterDetails, totalSize> map{};
				size_t index = 0;

				index = addParametersToMap(map, pluginParameterList, index);
				index = addParametersToMap(map, effectChainParameterList, index);
				index = addParametersToMap(map, effectModuleParameterList, index);
				index = addParametersToMap(map, baseEffectParameterList, index);

				index = extractParametersToMap(map, filterEffectParameterList, index);
				index = extractParametersToMap(map, contrastEffectParameterList, index);

				return map;
			}

		private:
			static constexpr size_t getParametersSize(const auto &parameterLists)
			{
				size_t size = 0;
				for (auto &parameterList : parameterLists)
					size += parameterList.size();

				return size;
			}

			static constexpr size_t addParametersToMap(auto &map, const auto &parameterList, size_t index)
			{
				for (auto &param : parameterList)
				{
					map.data[index] = { param.name, param };
					index++;
				}
				return index;
			}

			static constexpr size_t extractParametersToMap(auto &map, const auto &parameterLists, size_t index)
			{
				for (auto &parameterList : parameterLists)
				{
					for (auto &param : parameterList)
					{
						// in order to not include placeholder parameters
						if (param.name.empty())
							continue;

						map.data[index] = { param.name, param };
						index++;
					}
				}
				return index;
			}
		};
	}
	

	class Parameters
	{
	public:
		Parameters() = delete;
		Parameters(const Parameters &) = delete;
		void operator=(const Parameters &) = delete;
		Parameters(Parameters &&) = delete;
		void operator=(Parameters &&) = delete;
		~Parameters() = delete;

		static constexpr ParameterDetails getDetails(const std::string_view name)
		{ 
			auto iter = lookup_.find(name);
			return iter->second;
		}

		static constexpr auto getNumParameters()
		{ return lookup_.data.size(); }

		static constexpr const ParameterDetails *getDetails(size_t index)
		{ return &lookup_.data[index].second; }

		static constexpr std::string_view getDisplayName(const std::string_view name)
		{ return getDetails(name).displayName; }

		static constexpr float getParameterRange(const std::string_view name)
		{
			auto details = getDetails(name);
			return details.maxValue - details.minValue;
		}

		static constexpr bool isParameter(const std::string_view name)
		{
			auto iter = lookup_.find(name);
			return iter != lookup_.data.end();
		}

		static constexpr void setEffectType(ParameterDetails &details, EffectModuleTypes type)
		{
			switch (type)
			{
			case EffectModuleTypes::Filter:
				details.stringLookup = kFilterTypeNames;
				break;
			case EffectModuleTypes::Contrast:
				details.stringLookup = kContrastTypeNames;
				break;
			default:
				break;
			}
		}

		static constexpr auto &getAllDetails()
		{ return lookup_; }

	private: // TODO: replace this function call with immediately invoked lambda
		static constexpr auto lookup_ = ParameterDetailsGeneration::getParameterLookup();
		/*static constexpr auto lookup2_ = []() consteval
		{
			auto getParametersSize = [](const auto& parameterLists) constexpr
			{
				size_t size = 0;
				for (auto &parameterList : parameterLists)
					size += parameterList.size();

				return size;
			};

			constexpr size_t totalSize = pluginParameterList.size() +
				effectChainParameterList.size() + effectModuleParameterList.size() +
				baseEffectParameterList.size() + getParametersSize(filterEffectParameterList) + 
				getParametersSize(contrastEffectParameterList);

			constexpr size_t asdf = getParametersSize(filterEffectParameterList);

			ConstexprMap<std::string_view, ParameterDetails, totalSize> map{};
			size_t index = 0;

			auto addParametersToMap = [](auto &map, const auto &parameterList, size_t index) constexpr -> size_t
			{
				for (auto &param : parameterList)
				{
					static_assert(param.defaultValue <= param.maxValue);
					static_assert(param.defaultValue >= param.minValue);
					map.data[index] = { param.name, param };
					index++;
				}
				return index;
			};

			auto extractParametersToMap = [](auto &map, const auto &parameterLists, size_t index) constexpr -> size_t
			{
				for (auto &parameterList : parameterLists)
				{
					for (auto &param : parameterList)
					{
						// in order to not include placeholder parameters
						if (param.name.empty())
							continue;

						map.data[index] = { param.name, param };
						index++;
					}
				}
				return index;
			};

			index = addParametersToMap(map, pluginParameterList, index);
			index = addParametersToMap(map, effectChainParameterList, index);
			index = addParametersToMap(map, effectModuleParameterList, index);
			index = addParametersToMap(map, baseEffectParameterList, index);

			index = extractParametersToMap(map, filterEffectParameterList, index);
			index = extractParametersToMap(map, contrastEffectParameterList, index);

			return map;
		}();*/
	};
}