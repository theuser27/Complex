/*
	==============================================================================

		plugin_strings.h
		Created: 11 Sep 2022 4:18:10pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"

namespace Framework
{
	inline constexpr std::array<std::string_view, 2> kOffOnNames = { "Off", "On" };

	inline constexpr std::array <std::string_view, static_cast<size_t>(EffectModuleTypes::Size)> kEffectModuleNames = 
	{ "Utility", "Filter", "Contrast", "Dynamics", "Phase", "Pitch", "Stretch", "Warp", "Destroy" };

	inline constexpr std::array <std::string_view, kMaxFFTOrder - kMinFFTOrder + 1> kFFTSizeNames =
	{ "128", "256", "512", "1024", "2048", "4096", "8192", "16384" };

	inline constexpr std::array <std::string_view, static_cast<size_t>(WindowTypes::Size)> kWindowNames =
	{ "Clean", "Hann", "Hamming", "Triangle", "Sine", "Rectangle", "Exponential", "Hann-Exponential", "Lanczos" };

	inline constexpr std::array <std::string_view, kNumInputsOutputs + kMaxNumChains> kInputNames =
	{ "Main Input", "Chain 1", "Chain 2", "Chain 3", "Chain 4", "Chain 5", "Chain 6", "Chain 7", "Chain 8", "Chain 9", "Chain 10",
		"Chain 11", "Chain 12", "Chain 13", "Chain 14", "Chain 15", "Chain 16" };

	inline constexpr std::array <std::string_view, kNumInputsOutputs> kOutputNames = { "Main Output" };

	inline constexpr std::array <std::string_view, kMaxEffectTypes> kGenericEffectTypeNames = 
	{ "Type 1", "Type 2", "Type 3", "Type 4", "Type 5", "Type 6", "Type 7", "Type 8", "Type 9", "Type 10",
		"Type 11", "Type 12", "Type 13", "Type 14", "Type 15", "Type 16" };

	inline constexpr std::array <std::string_view, kMaxEffectTypes> kFilterTypeNames = 
	{ "Normal", "Regular" };

	inline constexpr std::array <std::string_view, kMaxEffectTypes> kContrastTypeNames = 
	{ "Contrast" };

}