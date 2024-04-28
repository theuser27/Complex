/*
	==============================================================================

		parameters.cpp
		Created: 4 Sep 2023 9:38:38pm
		Author:  theuser27

	==============================================================================
*/

#include <array>
#include "Third Party/gcem/gcem.hpp"

#include "constexpr_utils.h"
#include "parameters.h"
#include "vector_map.h"
#include "utils.h"
#include "simd_utils.h"
#include "windows.h"

namespace Framework
{
	static constexpr std::array<std::string_view, 2> kOffOnNames = { "Off", "On" };
	static constexpr auto kEffectModuleNames = BaseProcessors::BaseEffect::enum_names<nested_enum::InnerNodes>(true);
	static constexpr auto kWindowNames = WindowTypes::enum_names<nested_enum::AllNodes>(true);

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
	static constexpr auto kPhaseShiftSlopeNames = std::to_array<std::string_view>({ "Constant", "Linear" });

	using SoundEngine = BaseProcessors::SoundEngine::type;
	using EffectsLane = BaseProcessors::EffectsLane::type;
	using EffectModule = BaseProcessors::EffectModule::type;
	using BaseEffect = BaseProcessors::BaseEffect::type;
	using Filter = BaseProcessors::BaseEffect::Filter::type;
	using Dynamics = BaseProcessors::BaseEffect::Dynamics::type;
	using Phase = BaseProcessors::BaseEffect::Phase::type;

	#define PROCESSOR_TYPES SoundEngine, EffectsLane, EffectModule, BaseEffect
	#define EFFECT_TYPES Filter, Dynamics, Phase
	#define ALL_PROCESSOR_TYPES PROCESSOR_TYPES, EFFECT_TYPES
	#define DECLARE_PARAMETER_LIST(type) static constexpr std::array<ParameterDetails, type::enum_count(nested_enum::OuterNodes)> type##ParameterList
	#define DECLARE_EFFECT_PARAMETER_LIST(type) static constexpr std::array<ParameterDetails, type::enum_count_recursive(nested_enum::OuterNodes)> type##ParameterList

	DECLARE_PARAMETER_LIST(SoundEngine) =
	{
		ParameterDetails
		{ SoundEngine::MasterMix::name(), "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, false, true, UpdateFlag::BeforeProcess },
		{ SoundEngine::BlockSize::name(), "Block Size", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder,
											(float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), ParameterScale::Indexed, "",
											kFFTSizeNames, false, true, UpdateFlag::BeforeProcess },
		{ SoundEngine::Overlap::name(), "Overlap", 0.0f, kMaxWindowOverlap, kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Clamp, "%" },
		{ SoundEngine::WindowType::name(), "Window", 0.0f, (float)WindowTypes::enum_count() - 1.0f, 1.0f,
											1.0f / (float)WindowTypes::enum_count(), ParameterScale::Indexed, {},
											kWindowNames, false, false, UpdateFlag::BeforeProcess },
		{ SoundEngine::WindowAlpha::name(), "Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, false, false, UpdateFlag::BeforeProcess },
		{ SoundEngine::OutGain::name(), "Gain" , -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, " dB", {}, false, true, UpdateFlag::BeforeProcess }
	};

	DECLARE_PARAMETER_LIST(EffectsLane) =
	{
		ParameterDetails
		{ EffectsLane::LaneEnabled::name(), "Lane Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames },
		{ EffectsLane::Input::name(), "Input", 0.0f, kNumInputsOutputs + kMaxNumLanes - 1.0f, 0.0f, 0.0f, ParameterScale::Indexed, "",
											kInputNames.getSpan(), false, true, UpdateFlag::BeforeProcess },
		{ EffectsLane::Output::name(), "Output", 0.0f, kNumInputsOutputs - 1.0f, 0.0f, 0.0f, ParameterScale::Indexed, "",
											kOutputNames, false, true, UpdateFlag::BeforeProcess },
		{ EffectsLane::GainMatching::name(), "Gain Matching", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "",
											kOffOnNames, false, true, UpdateFlag::BeforeProcess }
	};

	DECLARE_PARAMETER_LIST(EffectModule) =
	{
		ParameterDetails
		{ EffectModule::ModuleEnabled::name(), "Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, {}, kOffOnNames },
		{ EffectModule::ModuleType::name(), "Module Type", 0.0f, (float)BaseProcessors::BaseEffect::enum_count(nested_enum::InnerNodes) - 1.0f, 1.0f,
											1.0f / (float)BaseProcessors::BaseEffect::enum_count(nested_enum::InnerNodes), ParameterScale::Indexed, {},
											kEffectModuleNames, false, true, UpdateFlag::AfterProcess },
		{ EffectModule::ModuleMix::name(), "Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, true },
	};

	DECLARE_PARAMETER_LIST(BaseEffect) =
	{
		ParameterDetails
		{ BaseEffect::Algorithm::name(), "Algorithm", 0.0f, kMaxEffectModes - 1, 0.0f, 0.0f, ParameterScale::Indexed, {}, kGenericEffectModeNames.getSpan() },
		{ BaseEffect::LowBound::name(), "Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz", {}, true },
		{ BaseEffect::HighBound::name(), "High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, " hz", {}, true },
		{ BaseEffect::ShiftBounds::name(), "Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true }
	};

	DECLARE_EFFECT_PARAMETER_LIST(Filter) =
	{
		ParameterDetails
		{ Filter::Normal::Gain::name(), "Gain", -120.0f, 120.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, " db", {}, true },
		{ Filter::Normal::Cutoff::name(), "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Frequency, " hz", {}, true },
		{ Filter::Normal::Slope::name(), "Slope", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "%", {}, true },

		{ Filter::Regular::Gain::name(), "Gain", -120.0f, 120.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, "", {}, true },
		{ Filter::Regular::Cutoff::name(), "Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },
		{ Filter::Regular::Phase::name(), "Phase", -1.0f, 1.0f, 0.25f, 0.75f, ParameterScale::SymmetricQuadratic, "", {}, true },
		{ Filter::Regular::Stretch::name(), "Stretch", 0.0f, 1.0f, 0.5f, 0.5f, ParameterScale::Linear, "", {}, true },

		{ Filter::Phase::Gain::name(), "Gain", -120.0f, 120.0f, 0.0f, 0.5f, ParameterScale::SymmetricLoudness, " db", {}, true },
		{ Filter::Phase::LowPhaseBound::name(), "Low Phase Bound", -180.0f, 180.0f, 0.0f, 0.5f, ParameterScale::Linear, "", {}, true },
		{ Filter::Phase::HighPhaseBound::name(), "High Phase Bound", -180.0f, 180.0f, 0.0f, 0.5f, ParameterScale::Linear, "", {}, true }
	};

	DECLARE_EFFECT_PARAMETER_LIST(Dynamics) =
	{
		ParameterDetails
		{ Dynamics::Contrast::Depth::name(), "Depth", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, true },

		{ Dynamics::Clip::Threshold::name(), "Threshold", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear, "%", {}, true }
	};

	DECLARE_EFFECT_PARAMETER_LIST(Phase) =
	{
		ParameterDetails
		{ Phase::Shift::PhaseShift::name(), "Shift", -180.0f, 180.0f, 0.0f, 0.5f, ParameterScale::Linear, "", {}, true },
		{ Phase::Shift::Slope::name(), "Slope", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Indexed, "", kPhaseShiftSlopeNames },
		{ Phase::Shift::Interval::name(), "Interval", 0.0f, 10.0f, 1.0f, gcem::pow(1.0f / 10.0f, 1.0f / 3.0f), ParameterScale::Cubic, "", {}, true },
		{ Phase::Shift::Offset::name(), "Offset", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, "", {}, true }
	};

	namespace
	{
		#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
		#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
		#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
		#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
		#define EXPAND1(...) __VA_ARGS__

		#define IDENTITY(...) __VA_ARGS__
		#define PARENS ()
		#define FOR_EACH(macro, separator, ...) __VA_OPT__(EXPAND(GET_ONE(macro, separator, __VA_ARGS__)))

		#define GET_ONE(macro, separator, a1, ...) macro (a1) __VA_OPT__(IDENTITY separator) __VA_OPT__(GET_ONE_AGAIN PARENS (macro, separator, __VA_ARGS__))
		#define GET_ONE_AGAIN() GET_ONE

		consteval auto getParameterLookup()
		{
			constexpr auto enumStringsAndIds = BaseProcessors::enum_names_and_ids_recursive<nested_enum::OuterNodes, true, false>();

		#define GET_PARAMETER_LIST_SIZE(type) type##ParameterList.size()
			static_assert(enumStringsAndIds.size() == (FOR_EACH(GET_PARAMETER_LIST_SIZE, (+), ALL_PROCESSOR_TYPES)));
		#undef GET_PARAMETER_LIST_SIZE

			ConstexprMap<Parameters::key, Parameters::value, enumStringsAndIds.size()> map{};

			auto recurseParameters = [&](const auto &self, auto &map, size_t mapIndex, const auto &parameterList, const auto & ... rest)
			{
				for (size_t i = 0; i < parameterList.size(); i++)
				{
					if (auto &[name, id] = enumStringsAndIds[mapIndex + i]; name == parameterList[i].pluginName)
						map.data[mapIndex + i] = std::pair{ std::pair{ id.value(), name }, parameterList[i] };
					else
						COMPLEX_ASSERT_FALSE("Reordered or mislabeled member in parameterList");
				}

				if constexpr (sizeof...(rest) != 0)
					self(self, map, mapIndex + parameterList.size(), rest...);
			};

		#define GET_PARAMETER_LIST(type) type##ParameterList
			recurseParameters(recurseParameters, map, 0, FOR_EACH(GET_PARAMETER_LIST, (,), ALL_PROCESSOR_TYPES));
		#undef GET_PARAMETER_LIST

			return map;
		}
	}

	static constexpr auto lookup = getParameterLookup();

	namespace Parameters
	{
		std::optional<ParameterDetails> getDetailsId(std::string_view id)
		{
			auto functor = [&](const auto &v) { return v.first.first == id; };
			auto iter = std::ranges::find_if(lookup.data, functor);
			if (iter != lookup.data.end())
				return iter->second;
			else
				return {};
		}

		const ParameterDetails &getDetailsEnum(std::string_view enumString)
		{
			auto functor = [&](const auto &v) { return v.first.second == enumString; };
			return lookup.find_if(functor)->second;
		}

		std::string_view getEnumName(std::string_view id)
		{
			auto functor = [&](const auto &v) { return v.first.first == id; };
			return lookup.find_if(functor)->first.second;
		}

		std::string_view getIdString(std::string_view enumString)
		{
			auto functor = [&](const auto &v) { return v.first.second == enumString; };
			return lookup.find_if(functor)->first.first;
		}

		std::span<const std::string_view> getEffectModesStrings(BaseEffect type)
		{
			static constexpr std::array<std::string_view, kMaxEffectModes> kFilterModeNames =
				utils::toDifferentSizeArray<kMaxEffectModes>(Filter::enum_names<nested_enum::AllNodes>(true), std::string_view("Empty"));

			static constexpr std::array<std::string_view, kMaxEffectModes> kDynamicsModeNames =
				utils::toDifferentSizeArray<kMaxEffectModes>(Dynamics::enum_names<nested_enum::AllNodes>(true), std::string_view("Empty"));

			static constexpr std::array<std::string_view, kMaxEffectModes> kPhaseModeNames =
				utils::toDifferentSizeArray<kMaxEffectModes>(Phase::enum_names<nested_enum::AllNodes>(true), std::string_view("Empty"));

			switch (type)
			{
			case BaseEffect::Filter: return kFilterModeNames;
			case BaseEffect::Dynamics: return kDynamicsModeNames;
			case BaseEffect::Phase: return kPhaseModeNames;
			default: return kGenericEffectModeNames.getSpan();
			}
		}

		std::optional<std::pair<size_t, size_t>> getIndexAndCountForEffect(BaseEffect type, size_t algorithmIndex)
		{
			auto getIndexAndCount = []<nested_enum::NestedEnum Type>(size_t index) -> std::optional<std::pair<size_t, size_t>>
			{
				constexpr auto typeCounts = utils::applyToEach([]<typename T>(T &&)
				{
					using type = typename std::remove_cvref_t<T>::type;
					return type::enum_count(nested_enum::AllNodes);
				}, Type::template enum_subtypes<nested_enum::AllNodes>());

				if (typeCounts.size() <= index)
					return {};

				size_t totalIndex = std::accumulate(typeCounts.begin(), typeCounts.begin() + index, (size_t)0);
				return { std::pair{ totalIndex, typeCounts[index] } };
			};

			switch (type)
			{
			case BaseEffect::Filter: return getIndexAndCount.template operator()<Filter>(algorithmIndex);
			case BaseEffect::Dynamics: return getIndexAndCount.template operator()<Dynamics>(algorithmIndex);
			case BaseEffect::Phase: return getIndexAndCount.template operator()<Phase>(algorithmIndex);
			default: return {};
			}
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
			result = clamp(value, (double)details.minValue, (double)details.maxValue);
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
		default:
			COMPLEX_ASSERT_FALSE("Unhandled case");
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
		default:
			COMPLEX_ASSERT_FALSE("Unhandled case");
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
		default:
			COMPLEX_ASSERT_FALSE("Unhandled case");
		}

		return result;
	}

}
