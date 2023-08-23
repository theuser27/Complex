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
#include "utils.h"
#include "simd_utils.h"
#include "plugin_strings.h"

namespace Framework
{
	// symmetric types apply the flipped curve to negative values
	// all x values are normalised
	enum class ParameterScale : u32
	{
		Toggle,                 // round(x)
		Indexed,                // round(x * n)
		Linear,                 // x
		Clamp,                  // clamp(x, min, max)
		Quadratic,              // x^2
		SymmetricQuadratic,     // x^2 for x >= 0, and -x^2 for x < 0
		Cubic,                  // x^3
		Loudness,               // standard 20 * log10(x)
		SymmetricLoudness,      // 20 * log10(x) for x >= 0, and -20 * log10(x) for x < 0
		Frequency,              // (sampleRate / 2 * minFrequency) ^ x
		SymmetricFrequency,     // same as above but -(sampleRate / 2 * minFrequency) ^ (-x) for x < 0
	};

	struct ParameterDetails
	{
		std::string_view id{};                                // internal plugin name
		std::string_view displayName{};                       // name displayed to the user
		float minValue = 0.0f;                                // minimum scaled value
		float maxValue = 1.0f;                                // maximum scaled value
		float defaultValue = 0.0f;                            // default scaled value
		float defaultNormalisedValue = 0.0f;                  // default normalised value
		ParameterScale scale = ParameterScale::Linear;        // value skew factor
		std::string_view displayUnits{};                      // "%", "db", etc.
		std::span<const std::string_view> stringLookup{};     // if scale is Indexed, you can use a text lookup
		bool isStereo = false;                                // if parameter allows stereo modulation
		bool isModulatable = true;                            // if parameter allows modulation at all
		UpdateFlag updateFlag = UpdateFlag::Realtime;         // at which point during processing the parameter is updated
		bool isExtensible = false;                            // if parameter's minimum/maximum/default values can change
		bool isAutomatable = true;                            // if parameter allows host automation
	};

	enum class PluginParameters : u32 { MasterMix = 0, BlockSize, Overlap, WindowType, WindowAlpha, OutGain };

	inline constexpr std::array<ParameterDetails, magic_enum::enum_count<PluginParameters>()> pluginParameterList =
	{
		ParameterDetails
		{ "MASTER_MIX", "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, false, true, UpdateFlag::BeforeProcess },
		{ "BLOCK_SIZE", "Block Size", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder, 
											(float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), ParameterScale::Indexed, "",
		                  kFFTSizeNames, false, true, UpdateFlag::BeforeProcess },
		{ "OVERLAP", "Overlap", 0.0f, kMaxWindowOverlap, kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Clamp, "%" },
		{ "WINDOW_TYPE", "Window", 0.0f, (float)magic_enum::enum_count<WindowTypes>() - 1.0f, 1.0f,
											1.0f / (float)magic_enum::enum_count<WindowTypes>(), ParameterScale::Indexed, {},
											kWindowNames, false, false, UpdateFlag::BeforeProcess },
		{ "WINDOW_ALPHA", "Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, false, false, UpdateFlag::BeforeProcess },
		{ "OUT_GAIN", "Gain" , -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", {}, false, true, UpdateFlag::BeforeProcess }
	};
	
	enum class EffectsLaneParameters : u32 { LaneEnabled = 0, Input, Output, GainMatching };

	inline constexpr std::array<ParameterDetails, magic_enum::enum_count<EffectsLaneParameters>()> effectLaneParameterList =
	{
		ParameterDetails
		{ "LANE_ENABLED", "Lane Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ "INPUT", "Input", 0.0f, kNumInputsOutputs + kMaxNumLanes - 1.0f, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kInputNames.getSpan(), false, true, UpdateFlag::BeforeProcess },
		{ "OUTPUT", "Output", 0.0f, kNumInputsOutputs - 1.0f, 0.0f, 0.0f, ParameterScale::Indexed, "",
		                  kOutputNames, false, true, UpdateFlag::BeforeProcess },
		{ "GAIN_MATCHING", "Gain Matching", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", 
		                  kOffOnNames, false, true, UpdateFlag::BeforeProcess }
	};

	enum class EffectModuleParameters : u32 { ModuleEnabled = 0, ModuleType, ModuleMix };

	inline constexpr std::array<ParameterDetails, magic_enum::enum_count<EffectModuleParameters>()> effectModuleParameterList =
	{
		ParameterDetails
		{ "MODULE_ENABLED", "Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ "MODULE_TYPE", "Module Type", 0.0f, (float)magic_enum::enum_count<EffectTypes>() - 1.0f, 1.0f,
											1.0f / (float)magic_enum::enum_count<EffectTypes>(), ParameterScale::Indexed, "",
											kEffectModuleNames, false, true, UpdateFlag::AfterProcess },
		{ "MODULE_MIX", "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, true },
	};

	enum class BaseEffectParameters : u32 { Algorithm = 0, LowBound, HighBound, ShiftBounds };

	inline constexpr std::array<ParameterDetails, magic_enum::enum_count<BaseEffectParameters>()> baseEffectParameterList =
	{
		ParameterDetails
		{ "FX_ALGORITHM", "Algorithm", 0.0f, kMaxEffectModes - 1, 0.0f, 0.0f, ParameterScale::Indexed,
											"", kGenericEffectModeNames.getSpan() },
		{ "FX_LOW_BOUND", "Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz", {}, true },
		{ "FX_HIGH_BOUND", "High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, " hz", {}, true },
		{ "FX_SHIFT_BOUNDS", "Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true }
	};

	namespace FilterEffectParameters
	{
		enum class Normal : u32
		{
			Gain = magic_enum::enum_count<BaseEffectParameters>(),
			Cutoff,
			Slope
		};

		enum class Regular : u32
		{
			Gain = magic_enum::enum_count<BaseEffectParameters>(),
			Cutoff,
			Phase,
			Stretch
		};

		inline constexpr size_t size = std::max(magic_enum::enum_count<Normal>(), magic_enum::enum_count<Regular>());
	}

	inline constexpr std::array<std::array<ParameterDetails,
		(u32)FilterEffectParameters::size>, magic_enum::enum_count<FilterModes>()> filterEffectParameterList =
	{
		std::array<ParameterDetails, (u32)FilterEffectParameters::size>
		{
			ParameterDetails
			{ "FILTER_NORMAL_FX_GAIN", "Gain", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, " db", {}, true },
			{ "FILTER_NORMAL_FX_CUTOFF", "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Frequency, " hz", {}, true },
			{ "FILTER_NORMAL_FX_SLOPE", "Slope", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "%", {}, true }
		},
		{
			ParameterDetails
			{ "FILTER_REGULAR_FX_GAIN", "Gain", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", {}, true },
			{ "FILTER_REGULAR_FX_CUTOFF", "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },
			{ "FILTER_REGULAR_FX_PHASE", "Phase", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "", {}, true },
			{ "FILTER_REGULAR_FX_STRETCH", "Stretch", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true }
		}
	};

	namespace DynamicsEffectParameters
	{
		enum class Contrast : u32
		{
			Depth = magic_enum::enum_count<BaseEffectParameters>()
		};

		enum class Clip : u32
		{
			Threshold = magic_enum::enum_count<BaseEffectParameters>()
		};

		enum class Compressor : u32
		{
			Depth = magic_enum::enum_count<BaseEffectParameters>(),
			Threshold,
			Ratio
		};

		inline constexpr size_t size = std::max({ magic_enum::enum_count<Contrast>(),
			magic_enum::enum_count<Clip>(), magic_enum::enum_count<Compressor>() });
	}

	inline constexpr std::array<std::array<ParameterDetails,
		(u32)DynamicsEffectParameters::size>, magic_enum::enum_count<DynamicsModes>()> dynamicsEffectParameterList =
	{
		std::array<ParameterDetails, (u32)DynamicsEffectParameters::size>
		{
			ParameterDetails
			{ "DYNAMICS_CONTRAST_FX_DEPTH", "Depth", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true }
		},
		{
			ParameterDetails
			{ "DYNAMICS_CLIPPING_FX_THRESHOLD", "Threshold", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, true }
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
					effectLaneParameterList.size() + effectModuleParameterList.size() +
					baseEffectParameterList.size() + getParametersSize(filterEffectParameterList) +
					getParametersSize(dynamicsEffectParameterList);

				ConstexprMap<std::string_view, ParameterDetails, totalSize> map{};
				size_t index = 0;

				index = addParametersToMap(map, pluginParameterList, index);
				index = addParametersToMap(map, effectLaneParameterList, index);
				index = addParametersToMap(map, effectModuleParameterList, index);
				index = addParametersToMap(map, baseEffectParameterList, index);

				index = extractParametersToMap(map, filterEffectParameterList, index);
				index = extractParametersToMap(map, dynamicsEffectParameterList, index);

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
					map.data[index] = { param.id, param };
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
						if (param.id.empty())
							continue;

						map.data[index] = { param.id, param };
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

		static constexpr std::span<const std::string_view> getEffectModesStrings(EffectTypes type)
		{
			switch (type)
			{
			case EffectTypes::Filter:
				return kFilterModeNames;
			case EffectTypes::Dynamics:
				return kDynamicsModeNames;
			default:
				return kGenericEffectModeNames.getSpan();
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

	// with skewOnly == true a normalised value between [0,1] or [-0.5, 0.5] is returned,
	// depending on whether the parameter is bipolar
	inline double scaleValue(double value, const ParameterDetails &details, float sampleRate = kDefaultSampleRate,
		bool scalePercent = false, bool skewOnly = false) noexcept
	{
		using namespace utils;

		double result{};
		double sign;
		double range;
		switch (details.scale)
		{
		case ParameterScale::Toggle:
			result = std::round(value);
			break;
		case ParameterScale::Indexed:
			result = (!skewOnly) ? std::round(value * (details.maxValue - details.minValue) + details.minValue) :
				std::round(value * (details.maxValue - details.minValue) + details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::Linear:
			result = (!skewOnly) ? value * (details.maxValue - details.minValue) + details.minValue : 
				value + details.minValue / (details.maxValue - details.minValue);
			break;
		case ParameterScale::Clamp:
			result = std::clamp(value, (double)details.minValue, (double)details.maxValue);
			result = (!skewOnly) ? result : result / ((double)details.maxValue - (double)details.minValue);
			break;
		case ParameterScale::SymmetricQuadratic:
			value = value * 2.0 - 1.0;
			sign = unsignFloat(value);
			value *= value;
			range = details.maxValue - details.minValue;
			result = (!skewOnly) ? (value * sign * range + range) * 0.5 + details.minValue : value * 0.5 * sign;
			break;
		case ParameterScale::Quadratic:
			result = (!skewOnly) ? value * value * (details.maxValue - details.minValue) + details.minValue : value * value;
			break;
		case ParameterScale::Cubic:
			result = (!skewOnly) ? value * value * value * (details.maxValue - details.minValue) + details.minValue : value * value * value;
			break;
		case ParameterScale::SymmetricLoudness:
			value = value * 2.0 - 1.0;
			sign = unsignFloat(value);
			result = (!skewOnly) ? normalisedToDb(value, (double)kNormalisedToDbMultiplier) * sign :
				normalisedToDb(value, 1.0) * 0.5 * sign;
			break;
		case ParameterScale::Loudness:
			result = (!skewOnly) ? normalisedToDb(value, (double)kNormalisedToDbMultiplier) : normalisedToDb(value, 1.0);
			break;
		case ParameterScale::SymmetricFrequency:
			value = value * 2.0 - 1.0;
			sign = unsignFloat(value);
			result = (!skewOnly) ? normalisedToFrequency(value, (double)sampleRate) * sign :
				(normalisedToFrequency(value, (double)sampleRate) * sign) / sampleRate;
			break;
		case ParameterScale::Frequency:
			result = (!skewOnly) ? normalisedToFrequency(value, (double)sampleRate) : 
				normalisedToFrequency(value, (double)sampleRate) * 2.0 / sampleRate;
			break;
		}

		if (details.displayUnits == "%" && scalePercent)
			result *= 100.0;

		return result;
	}

	inline double unscaleValue(double value, const ParameterDetails &details, 
		float sampleRate = kDefaultSampleRate, bool unscalePercent = true) noexcept
	{
		using namespace utils;

		if (details.displayUnits == "%" && unscalePercent)
			value *= 0.01f;

		double result{};
		double sign;
		switch (details.scale)
		{
		case ParameterScale::Toggle:
			result = std::round(value);
			break;
		case ParameterScale::Indexed:
			result = (value - details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::Clamp:
			result = value;
			break;
		case ParameterScale::Linear:
			result = (value - details.minValue) / (details.maxValue - details.minValue);
			break;
		case ParameterScale::SymmetricQuadratic:
			value = 2.0 * (value - details.minValue) / (details.maxValue - details.minValue) - 1.0;
			sign = unsignFloat(value);
			result = std::sqrt(value) * sign;
			break;
		case ParameterScale::Quadratic:
			result = std::sqrt((value - details.minValue) / (details.maxValue - details.minValue));
			break;
		case ParameterScale::Cubic:
			value = (value - details.minValue) / (details.maxValue - details.minValue);
			result = std::pow(value, 1.0f / 3.0f);
			break;
		case ParameterScale::SymmetricLoudness:
			sign = unsignFloat(value);
			value = dbToAmplitude(value);
			result = (value * sign + 1.0f) * 0.5f;
			break;
		case ParameterScale::Loudness:
			result = dbToAmplitude(value);
			break;
		case ParameterScale::SymmetricFrequency:
			sign = unsignFloat(value);
			value = frequencyToNormalised(value, (double)sampleRate);
			result = (value * sign + 1.0f) * 0.5f;
			break;
		case ParameterScale::Frequency:
			result = frequencyToNormalised(value, (double)sampleRate);
			break;
		}

		return result;
	}

	inline simd_float scaleValue(simd_float value, const ParameterDetails &details, float sampleRate = kDefaultSampleRate) noexcept
	{
		using namespace utils;

		simd_float result{};
		simd_mask sign{};
		simd_float range;
		switch (details.scale)
		{
		case ParameterScale::Toggle:
			result = reinterpretToFloat(simd_float::notEqual(simd_float::round(value), 0.0f));
			break;
		case ParameterScale::Indexed:
			result = simd_float::round(value * (details.maxValue - details.minValue) + details.minValue);
			break;
		case ParameterScale::Linear:
			result = value * (details.maxValue - details.minValue) + details.minValue;
			break;
		case ParameterScale::SymmetricQuadratic:
			value = value * 2.0f - 1.0f;
			sign = unsignSimd(value);
			value *= value;
			range = details.maxValue - details.minValue;
			result = ((value | sign) * range + range) * 0.5f + details.minValue;
			break;
		case ParameterScale::Quadratic:
			result = value * value * (details.maxValue - details.minValue) + details.minValue;
			break;
		case ParameterScale::Cubic:
			result = value * value * value * (details.maxValue - details.minValue) + details.minValue;
			break;
		case ParameterScale::SymmetricLoudness:
			value = (value * 2.0f - 1.0f);
			sign = unsignSimd(value);
			result = normalisedToDb(value, kNormalisedToDbMultiplier) | sign;
			break;
		case ParameterScale::Loudness:
			result = normalisedToDb(value, kNormalisedToDbMultiplier);
			break;
		case ParameterScale::SymmetricFrequency:
			value = value * 2.0f - 1.0f;
			sign = unsignSimd(value);
			result = normalisedToFrequency(value, sampleRate) | sign;
			break;
		case ParameterScale::Frequency:
			result = normalisedToFrequency(value, sampleRate);
			break;
		case ParameterScale::Clamp:
			result = simd_float::clamp(value, details.minValue, details.maxValue);
			break;
		}

		return result;
	}

}