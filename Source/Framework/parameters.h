/*
	==============================================================================

		parameters.h
		Created: 11 Jul 2022 5:25:41pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <span>

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
		Quadratic,							// x^2
		SymmetricQuadratic,			// x^2 for x >= 0, and -x^2 for x < 0
		Cubic,									// x^3
		Loudness,								// standard 20 * log10(x)
		SymmetricLoudness,			// 20 * log10(x) for x >= 0, and -20 * log10(x) for x < 0
		Frequency,							// (sampleRate / 2 * minFrequency) ^ x
		SymmetricFrequency,			// same as above but -(sampleRate / 2 * minFrequency) ^ (-x) for x < 0
		Overlap,
	};

	struct ParameterDetails
	{
		std::string_view name;
		std::string_view displayName;
		float minValue = 0.0f;
		float maxValue = 1.0f;
		float defaultValue = 0.0f;
		float defaultNormalisedValue = 0.5f;
		ParameterScale scale = ParameterScale::Linear;
		std::string_view displayUnits = "";
		const std::string_view *stringLookup = nullptr;
		bool isStereo = false;
		bool isModulatable = true;
		UpdateFlag updateFlag = UpdateFlag::Realtime;
	};

	inline constexpr std::array globalPluginParameterList =
	{
		ParameterDetails{ "MASTER_MIX", "Master Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", nullptr, false, true, UpdateFlag::BeforeProcess },
		ParameterDetails{ "BLOCK_SIZE", "Block Size", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder, 
											(float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), ParameterScale::Indexed, "",
		                  kFFTSizeNames.data(), false, true, UpdateFlag::BeforeProcess },
		ParameterDetails{ "OVERLAP", "Overlap", 0.0f, kMaxWindowOverlap, kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Overlap, "%" },
		ParameterDetails{ "WINDOW_TYPE", "Window Type", 0.0f, kNumWindowTypes, 1.0f, 1.0f / (float)kNumWindowTypes, 
											ParameterScale::Indexed, "", kWindowNames.data() },
		ParameterDetails{ "WINDOW_ALPHA", "Window Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear },
		ParameterDetails{ "OUT_GAIN", "Out Gain" , -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", nullptr, false, true, UpdateFlag::BeforeProcess }
	};

	inline constexpr std::array effectChainParameterList =
	{
		ParameterDetails{ "CHAIN_ENABLED", "Chain Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames.data() },
		ParameterDetails{ "INPUT", "Input", 0.0f, kNumInputsOutputs + kMaxNumChains, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kInputNames.data(), false, true, UpdateFlag::BeforeProcess },
		ParameterDetails{ "OUTPUT", "Output", 0.0f, kNumInputsOutputs, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kOutputNames.data(), false, true, UpdateFlag::BeforeProcess },
		ParameterDetails{ "GAIN_MATCHING", "Gain Matching", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", 
		                  kOffOnNames.data(), false, true, UpdateFlag::BeforeProcess }
	};

	inline constexpr std::array effectModuleParameterList =
	{
		ParameterDetails{ "MODULE_ENABLED", "Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames.data() },
		ParameterDetails{ "MODULE_TYPE", "Module Type", 0.0f, static_cast<float>(EffectModuleTypes::Size), 0.0f, 0.0f, ParameterScale::Indexed, "", 
		                  kEffectModuleNames.data(), false, true, UpdateFlag::BeforeProcess },
		ParameterDetails{ "MODULE_MIX", "Module Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", nullptr, true },
		ParameterDetails{ "MODULE_GAIN", "Module Gain", -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", nullptr, true }
	};

	inline constexpr std::array baseEffectParameterList =
	{
		ParameterDetails{ "FX_TYPE", "Effect Type", 0.0f, kMaxEffectTypes, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kGenericEffectTypeNames.data() },
		ParameterDetails{ "FX_LOW_BOUND", "Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, "", nullptr, true },
		ParameterDetails{ "FX_HIGH_BOUND", "High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, "", nullptr, true },
		ParameterDetails{ "FX_SHIFT_BOUNDS", "Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "", nullptr, true}
	};

	inline constexpr std::array filterEffectParameterList =
	{
		ParameterDetails{ "FILTER_FX_GAIN", "Filter Gain", -1.0f, 1.0f, 0.0f, 1.0f, ParameterScale::SymmetricLoudness, "", nullptr, true },
		ParameterDetails{ "FILTER_FX_CUTOFF", "Filter Cutoff", 0.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Frequency, "", nullptr, true },
		ParameterDetails{ "FILTER_FX_SLOPE", "Filter Slope", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", nullptr, true }
	};

	inline constexpr std::array contrastEffectParameterList =
	{
		ParameterDetails{ "CONTRAST_FX_DEPTH", "Contrast Depth", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "", nullptr, true }
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
				constexpr size_t totalSize = globalPluginParameterList.size() + 
					effectChainParameterList.size() + effectModuleParameterList.size() +
					baseEffectParameterList.size() + filterEffectParameterList.size() +
					contrastEffectParameterList.size();

				ConstexprMap<std::string_view, ParameterDetails, totalSize> map{};
				size_t index = 0;

				index = addParametersToMap(map, globalPluginParameterList, index);
				index = addParametersToMap(map, effectChainParameterList, index);
				index = addParametersToMap(map, effectModuleParameterList, index);
				index = addParametersToMap(map, baseEffectParameterList, index);
				index = addParametersToMap(map, filterEffectParameterList, index);
				index = addParametersToMap(map, contrastEffectParameterList, index);

				return map;
			}

		private:
			static constexpr size_t addParametersToMap(auto &map, const auto &parameterList, size_t index)
			{
				for (auto &param : parameterList)
				{
					COMPLEX_ASSERT(param.defaultValue <= param.maxValue);
					COMPLEX_ASSERT(param.defaultValue >= param.minValue);
					map.data[index] = { param.name, param };
					index++;
				}
				return index;
			}
		};
	}
	

	class Parameters
	{
		Parameters() = delete;
		Parameters(const Parameters&) = delete;
		void operator=(const Parameters &) = delete;
		Parameters(Parameters&&) = delete;
		void operator=(Parameters &&) = delete;
		~Parameters() = delete;
	
	public:
		static constexpr const ParameterDetails *getDetails(const std::string_view name)
		{ 
			auto iter = lookup_.find(name);
			return &(iter->second);
		}

		static constexpr auto getNumParameters()
		{ return lookup_.data.size(); }

		static constexpr const ParameterDetails *getDetails(size_t index)
		{ return &lookup_.data[index].second; }

		static constexpr std::string_view getDisplayName(const std::string_view name)
		{ return getDetails(name)->displayName; }

		static constexpr float getParameterRange(const std::string_view name)
		{
			auto &details = *getDetails(name);
			return details.maxValue - details.minValue;
		}

		static constexpr bool isParameter(const std::string_view name)
		{
			auto iter = lookup_.find(name);
			return iter != lookup_.data.end();
		}

		static constexpr auto &getAllDetails()
		{ return lookup_; }

	private:
		static constexpr auto lookup_ = ParameterDetailsGeneration::getParameterLookup();
	};
}