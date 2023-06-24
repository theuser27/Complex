/*
	==============================================================================

		plugin_strings.h
		Created: 11 Sep 2022 4:18:10pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "utils.h"

namespace Framework
{
	inline constexpr std::array<std::string_view, 2> kOffOnNames = { "Off", "On" };

	enum class temp { one, two, three, four };

	inline constexpr auto kEffectModuleNames = magic_enum::enum_names<EffectTypes>();
	//inline constexpr auto kEffectModuleNamesf = magic_enum::enum_count<temp>();

	inline constexpr std::array<std::string_view, kMaxFFTOrder - kMinFFTOrder + 1> kFFTSizeNames =
	{ "128", "256", "512", "1024", "2048", "4096", "8192", "16384" };

	inline constexpr auto kWindowNames = magic_enum::enum_names<WindowTypes>();

	inline constexpr std::array<std::string_view, kNumInputsOutputs + kMaxNumChains> kInputNames = 
	{ "Main Input", "Chain 1", "Chain 2", "Chain 3", "Chain 4", "Chain 5", "Chain 6", "Chain 7", "Chain 8", "Chain 9", "Chain 10",
		"Chain 11", "Chain 12", "Chain 13", "Chain 14", "Chain 15", "Chain 16" };

	inline constexpr std::array<std::string_view, kNumInputsOutputs> kOutputNames = { "Main Output" };

	inline constexpr std::array<std::string_view, kMaxEffectModes> kGenericEffectModeNames = 
	{ "Mode 1", "Mode 2", "Mode 3", "Mode 4", "Mode 5", "Mode 6", "Mode 7", "Mode 8", "Mode 9", "Mode 10",
		"Mode 11", "Mode 12", "Mode 13", "Mode 14", "Mode 15", "Mode 16" };

	inline constexpr std::array<std::string_view, kMaxEffectModes> kFilterModeNames = 
		utils::toDifferentSizeArray<kMaxEffectModes>(magic_enum::enum_names<FilterModes>());

	inline constexpr std::array<std::string_view, kMaxEffectModes> kDynamicsModeNames = 
		utils::toDifferentSizeArray<kMaxEffectModes>(magic_enum::enum_names<DynamicsModes>());

}