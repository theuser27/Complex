/*
	==============================================================================

		plugin_strings.h
		Created: 11 Sep 2022 4:18:10pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "string_utils.h"

namespace Framework
{
	inline constexpr std::array<std::string_view, 2> kOffOnNames = { "Off", "On" };
	inline constexpr auto kEffectModuleNames = magic_enum::enum_names<EffectTypes>();
	inline constexpr auto kWindowNames = magic_enum::enum_names<WindowTypes>();

	inline constexpr std::array<std::string_view, kMaxFFTOrder - kMinFFTOrder + 1> kFFTSizeNames =
		utils::numberSequenceStrings<1 << kMinFFTOrder, kMaxFFTOrder - kMinFFTOrder + 1, 0, 2>();

	// { "Main Input", "Sidechain 1", ... , "Sidechain *kNumSidechains*", "Lane 1", ... , "Lane *kMaxNumLanes*" }
	inline constexpr auto kInputNames = []()
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

	inline constexpr std::array<std::string_view, kNumInputsOutputs> kOutputNames = { "Main Output" };

	// { "Mode 1", "Mode 2", ... "Mode *kMaxEffectModes*" }
	inline constexpr auto kGenericEffectModeNames = []()
	{
		constexpr std::string_view modeString = "Mode ";
		constexpr auto numberStrings = utils::numberSequenceStrings<1, kMaxEffectModes, 1>();

		constexpr auto totalIndices = numberStrings.size();
		constexpr auto totalSize = totalIndices * modeString.size() + utils::getArraysDataSize(numberStrings);

		auto stringArray = utils::appendStringViewsArrays<"", totalSize, totalIndices>(modeString, numberStrings);
		return stringArray;
	}();

	inline constexpr std::array<std::string_view, kMaxEffectModes> kFilterModeNames = 
		utils::toDifferentSizeArray<kMaxEffectModes>(magic_enum::enum_names<FilterModes>(), std::string_view("Empty"));

	inline constexpr std::array<std::string_view, kMaxEffectModes> kDynamicsModeNames = 
		utils::toDifferentSizeArray<kMaxEffectModes>(magic_enum::enum_names<DynamicsModes>(), std::string_view("Empty"));

}