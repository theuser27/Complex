/*
	==============================================================================

		parameters.cpp
		Created: 4 Sep 2023 9:38:38pm
		Author:  theuser27

	==============================================================================
*/

#include "parameters.h"
#include "vector_map.h"
#include "utils.h"
#include "simd_utils.h"

namespace Framework
{
	static constexpr std::array<std::string_view, 2> kOffOnNames = { "Off", "On" };
	static constexpr auto kEffectModuleNames = BaseProcessors::BaseEffect::enum_strings<nested_enum::InnerNodes>(true);
	static constexpr auto kWindowNames = WindowTypes::enum_strings<nested_enum::AllNodes>(true);

	static constexpr std::array<std::string_view, kMaxFFTOrder - kMinFFTOrder + 1> kFFTSizeNames =
		utils::numberSequenceStrings<1 << kMinFFTOrder, kMaxFFTOrder - kMinFFTOrder + 1, 0, 2>();

	// { "Mode 1", "Mode 2", ... "Mode *kMaxEffectModes*" }
	static constexpr auto kGenericEffectModeNames = []()
	{
		constexpr std::string_view modeString = "Mode ";
		constexpr auto numberStrings = utils::numberSequenceStrings<1, kMaxEffectModes, 1>();

		constexpr auto totalIndices = numberStrings.size();
		constexpr auto totalSize = totalIndices * modeString.size() + utils::getArraysDataSize(numberStrings);

		auto stringArray = utils::appendStringViewsArrays<"", totalSize, totalIndices>(modeString, numberStrings);
		return stringArray;
	}();

	// { "Main Input", "Sidechain 1", ... , "Sidechain *kNumSidechains*", "Lane 1", ... , "Lane *kMaxNumLanes*" }
	static constexpr auto kInputNames = []()
	{
		static_assert(kNumInputsOutputs >= 1, "You need at least one main input");

		constexpr std::string_view mainInputString = "Main Input";
		constexpr std::string_view laneString = "Lane ";
		constexpr auto laneNumberStrings = utils::numberSequenceStrings<1, kMaxNumLanes, 1>();

		constexpr auto lanesNumCharacters = laneString.size() * kMaxNumLanes +
			utils::getArrayDataSize<laneNumberStrings.size()>(laneNumberStrings);

		constexpr auto totalSize = mainInputString.size() + lanesNumCharacters;
		constexpr auto totalIndices = kMaxNumLanes + 1;

		auto laneStringsArray = utils::appendStringViewsArrays<"", lanesNumCharacters, kMaxNumLanes>(laneString, laneNumberStrings);
		auto stringArray = utils::combineStringViewArrays<totalSize, totalIndices>(mainInputString, laneStringsArray);
		if constexpr (kNumSidechains == 0)
			return stringArray;
		else
		{
			constexpr std::string_view sidechainString = "Sidechain ";
			constexpr auto sidechainNumberStrings = utils::numberSequenceStrings<1,
				std::max<size_t>(kNumSidechains, 1), 1>();

			constexpr auto sidechainNumCharacters = sidechainString.size() * kNumSidechains +
				utils::getArrayDataSize<sidechainNumberStrings.size()>(sidechainNumberStrings);

			auto sidechainStrings = utils::appendStringViewsArrays<"", sidechainNumCharacters,
				kNumSidechains>(sidechainString, sidechainNumberStrings);
			auto stringArrayWithSidechins = utils::insertStringViewsArray<1, stringArray.size(),
				totalSize + sidechainNumCharacters, totalIndices +
				kNumSidechains>(stringArray, sidechainStrings);
			return stringArrayWithSidechins;
		}
	}();

	static constexpr std::array<std::string_view, kNumInputsOutputs> kOutputNames = { "Main Output" };

	static constexpr std::array<ParameterDetails, BaseProcessors::SoundEngine::enum_count(nested_enum::OuterNodes)> pluginParameterList =
	{
		ParameterDetails
		{ "MASTER_MIX", "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, false, true, UpdateFlag::BeforeProcess },
		{ "BLOCK_SIZE", "Block Size", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder,
											(float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), ParameterScale::Indexed, "",
											kFFTSizeNames, false, true, UpdateFlag::BeforeProcess },
		{ "OVERLAP", "Overlap", 0.0f, kMaxWindowOverlap, kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Clamp, "%" },
		{ "WINDOW_TYPE", "Window", 0.0f, (float)WindowTypes::enum_count() - 1.0f, 1.0f,
											1.0f / (float)WindowTypes::enum_count(), ParameterScale::Indexed, {},
											kWindowNames, false, false, UpdateFlag::BeforeProcess },
		{ "WINDOW_ALPHA", "Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, false, false, UpdateFlag::BeforeProcess },
		{ "OUT_GAIN", "Gain" , -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", {}, false, true, UpdateFlag::BeforeProcess }
	};

	static constexpr std::array<ParameterDetails, BaseProcessors::EffectsLane::enum_count(nested_enum::OuterNodes)> effectLaneParameterList =
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

	static constexpr std::array<ParameterDetails, BaseProcessors::EffectModule::enum_count(nested_enum::OuterNodes)> effectModuleParameterList =
	{
		ParameterDetails
		{ "MODULE_ENABLED", "Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ "MODULE_TYPE", "Module Type", 0.0f, (float)BaseProcessors::BaseEffect::enum_count(nested_enum::InnerNodes) - 1.0f, 1.0f,
											1.0f / (float)BaseProcessors::BaseEffect::enum_count(nested_enum::InnerNodes), ParameterScale::Indexed, "",
											kEffectModuleNames, false, true, UpdateFlag::AfterProcess },
		{ "MODULE_MIX", "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, true },
	};

	static constexpr std::array<ParameterDetails, BaseProcessors::BaseEffect::enum_count(nested_enum::OuterNodes)> baseEffectParameterList =
	{
		ParameterDetails
		{ "FX_ALGORITHM", "Algorithm", 0.0f, kMaxEffectModes - 1, 0.0f, 0.0f, ParameterScale::Indexed, "", kGenericEffectModeNames.getSpan() },
		{ "FX_LOW_BOUND", "Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz", {}, true },
		{ "FX_HIGH_BOUND", "High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, " hz", {}, true },
		{ "FX_SHIFT_BOUNDS", "Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true }
	};

	static constexpr std::array<ParameterDetails, BaseProcessors::BaseEffect::Filter::enum_count_recursive(nested_enum::OuterNodes)> filterEffectParameterList =
	{
		ParameterDetails
		{ "FILTER_NORMAL_FX_GAIN", "Gain", -120.0f, 120.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, " db", {}, true },
		{ "FILTER_NORMAL_FX_CUTOFF", "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Frequency, " hz", {}, true },
		{ "FILTER_NORMAL_FX_SLOPE", "Slope", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "%", {}, true },

		{ "FILTER_REGULAR_FX_GAIN", "Gain", -120.0f, 120.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", {}, true },
		{ "FILTER_REGULAR_FX_CUTOFF", "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },
		{ "FILTER_REGULAR_FX_PHASE", "Phase", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "", {}, true },
		{ "FILTER_REGULAR_FX_STRETCH", "Stretch", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true }
	};

	static constexpr std::array<ParameterDetails, BaseProcessors::BaseEffect::Dynamics::enum_count_recursive(nested_enum::OuterNodes)> dynamicsEffectParameterList =
	{
		ParameterDetails
		{ "DYNAMICS_CONTRAST_FX_DEPTH", "Depth", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true },

		{ "DYNAMICS_CLIPPING_FX_THRESHOLD", "Threshold", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, true }
	};

	namespace
	{
		template<nested_enum::NestedEnum E>
		constexpr size_t addParametersToMap(auto &map, const auto &parameterList, size_t mapIndex)
		{
			constexpr auto enumStrings = E::template enum_strings<nested_enum::OuterNodes>(false);

			for (size_t i = 0; i < enumStrings.size(); i++)
				map.data[i + mapIndex] = std::pair{ std::pair{ parameterList[i].id, enumStrings[i] }, parameterList[i] };

			return mapIndex + enumStrings.size();
		}

		template<nested_enum::NestedEnum E>
		constexpr size_t addMultipleParametersToMap(auto &map, const auto &parameterList, size_t mapIndex)
		{
			constexpr auto enumStrings = utils::tupleOfArraysToArray(E::template enum_strings_recursive<nested_enum::OuterNodes, true, false>());

			for (size_t i = 0; i < enumStrings.size(); i++)
				map.data[i + mapIndex] = std::pair{ std::pair{ parameterList[i].id, enumStrings[i] }, parameterList[i] };

			return mapIndex + enumStrings.size();
		}

		consteval auto getParameterLookup()
		{
			constexpr size_t totalSize = pluginParameterList.size() +
				effectLaneParameterList.size() + effectModuleParameterList.size() +
				baseEffectParameterList.size() + filterEffectParameterList.size() +
				dynamicsEffectParameterList.size();

			ConstexprMap<Parameters::key, Parameters::value, totalSize> map{};
			size_t index = 0;

			index = addParametersToMap<BaseProcessors::SoundEngine::type>(map, pluginParameterList, index);
			index = addParametersToMap<BaseProcessors::EffectsLane::type>(map, effectLaneParameterList, index);
			index = addParametersToMap<BaseProcessors::EffectModule::type>(map, effectModuleParameterList, index);
			index = addParametersToMap<BaseProcessors::BaseEffect::type>(map, baseEffectParameterList, index);

			index = addMultipleParametersToMap<BaseProcessors::BaseEffect::Filter::type>(map, filterEffectParameterList, index);
			index = addMultipleParametersToMap<BaseProcessors::BaseEffect::Dynamics::type>(map, dynamicsEffectParameterList, index);

			return map;
		}
	}

	static constexpr auto lookup = getParameterLookup();

	const ParameterDetails &Parameters::getDetailsId(std::string_view id)
	{
		auto functor = [&](const auto &v) { return v.first.first == id; };
		return lookup.find_if(functor)->second;
	}

	const ParameterDetails &Parameters::getDetailsEnum(std::string_view enumString)
	{
		auto functor = [&](const auto &v) { return v.first.second == enumString; };
		return lookup.find_if(functor)->second;
	}

	std::string_view Parameters::getEnumString(std::string_view id)
	{
		auto functor = [&](const auto &v) { return v.first.first == id; };
		return lookup.find_if(functor)->first.second;
	}

	std::span<const std::string_view> Parameters::getEffectModesStrings(BaseProcessors::BaseEffect::type type)
	{
		static constexpr std::array<std::string_view, kMaxEffectModes> kFilterModeNames =
			utils::toDifferentSizeArray<kMaxEffectModes>(BaseProcessors::BaseEffect::Filter::enum_strings<nested_enum::AllNodes>(true), std::string_view("Empty"));

		static constexpr std::array<std::string_view, kMaxEffectModes> kDynamicsModeNames =
			utils::toDifferentSizeArray<kMaxEffectModes>(BaseProcessors::BaseEffect::Dynamics::enum_strings<nested_enum::AllNodes>(true), std::string_view("Empty"));

		switch (type)
		{
		case BaseProcessors::BaseEffect::Filter:
			return kFilterModeNames;
		case BaseProcessors::BaseEffect::Dynamics:
			return kDynamicsModeNames;
		default:
			return kGenericEffectModeNames.getSpan();
		}
	}

	double scaleValue(double value, const ParameterDetails &details, float sampleRate, bool scalePercent, bool skewOnly) noexcept
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
			result = (!skewOnly) ? normalisedToDb(value, (double)details.maxValue) * sign :
				normalisedToDb(value, 1.0) * 0.5 * sign;
			break;
		case ParameterScale::Loudness:
			result = (!skewOnly) ? normalisedToDb(value, (double)details.maxValue) : normalisedToDb(value, 1.0);
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

	double unscaleValue(double value, const ParameterDetails &details, float sampleRate, bool unscalePercent) noexcept
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

	simd_float scaleValue(simd_float value, const ParameterDetails &details, float sampleRate) noexcept
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
			result = normalisedToDb(value, details.maxValue) | sign;
			break;
		case ParameterScale::Loudness:
			result = normalisedToDb(value, details.maxValue);
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
