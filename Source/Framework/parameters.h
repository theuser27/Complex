/*
	==============================================================================

		parameters.h
		Created: 11 Jul 2022 5:25:41pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "nested_enum.h"
#include "constexpr_utils.h"

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
		Quadratic,              // x ^ 2
		SymmetricQuadratic,     // x ^ 2 * sgn(x)
		Cubic,                  // x ^ 3
		Loudness,               // 20 * log10(x)
		SymmetricLoudness,      // 20 * log10(abs(x)) * sgn(x)
		Frequency,              // (sampleRate / 2 * minFrequency) ^ x
		SymmetricFrequency,     // (sampleRate / 2 * minFrequency) ^ (abs(x)) * sgn(x)
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
	

	NESTED_ENUM(BaseProcessors, (IDS, SoundEngine, "{6B31ED46-FBF0-4219-A645-7B774F903026}", EffectsState, "{39B6A6C9-D33F-4AF0-BBDB-6C1F1960184F}", 
	EffectsLane, "{F260616E-CF7D-4099-A880-9C52CED263C1}", EffectModule, "{763F9D86-D535-4D63-B486-F863F88CC259}", BaseEffect, {}), 
		(DEFER),
		(),
		(DEFER),
		(DEFER),
		(DEFER)
	)

    NESTED_ENUM_FROM(BaseProcessors, SoundEngine, (IDS, MasterMix, "MASTER_MIX", BlockSize, "BLOCK_SIZE", Overlap, "OVERLAP", WindowType, "WINDOW_TYPE", WindowAlpha, "WINDOW_ALPHA", OutGain, "OUT_GAIN"))
    NESTED_ENUM_FROM(BaseProcessors, EffectsLane, (IDS, LaneEnabled, "LANE_ENABLED", Input, "INPUT", Output, "OUTPUT", GainMatching, "GAIN_MATCHING"))
    NESTED_ENUM_FROM(BaseProcessors, EffectModule, (IDS, ModuleEnabled, "MODULE_ENABLED", ModuleType, "MODULE_TYPE", ModuleMix, "MODULE_MIX"))
    // it is important that the effect types be first rather than the parameters
		// in multiple places indices are used to address these effects and having them not begin at 0 causes a lot of headaches
		NESTED_ENUM_FROM(BaseProcessors, BaseEffect, (IDS,
        Utility, "{A8129602-D351-46D5-B85B-D5764C071575}", Filter, "{809BD1B8-AA18-467E-8DD2-E396F70D6253}",
        Dynamics, "{D5DADD9A-5B0F-45C6-ADF5-B6A6415AF2D7}", Phase, "{5670932B-8B6F-4475-9926-000F6C36C5AD}",
        Pitch, "{71133386-9421-4B23-91F9-C826DFC506B8}", Stretch, "{D700C4AA-EC95-4703-9837-7AD5BDF5C810}",
        Warp, "{5FC3802A-B916-4D36-A853-78A29A5F5687}", Destroy, "{EA1DD088-A73A-4FD4-BB27-38EC0BF91850}",

				Algorithm, "FX_ALGORITHM", LowBound, "FX_LOW_BOUND", HighBound, "FX_HIGH_BOUND", ShiftBounds, "FX_SHIFT_BOUNDS"),
            (ENUM, Utility),
            (DEFER),
            (DEFER),
            (ENUM, Phase),
            (ENUM, Pitch),
            (ENUM, Stretch),
            (ENUM, Warp),
            (ENUM, Destroy)
    )

			// Normal - Lowpass/Highpass/Bandpass/Notch
			// Regular - Harmonic/Bin based filters (like dtblkfx peaks)
      NESTED_ENUM_FROM(BaseProcessors::BaseEffect, Filter, (, Normal, Regular), (DEFER), (DEFER))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Filter, Normal, (IDS, Gain, "FILTER_NORMAL_FX_GAIN", Cutoff, "FILTER_NORMAL_FX_CUTOFF", Slope, "FILTER_NORMAL_FX_SLOPE"))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Filter, Regular, (IDS, Gain, "FILTER_REGULAR_FX_GAIN", Cutoff, "FILTER_REGULAR_FX_CUTOFF", Phase, "FILTER_REGULAR_FX_PHASE", Stretch, "FILTER_REGULAR_FX_STRETCH"))
            
			// Contrast - dtblkfx contrast
			// Clip - dtblkfx clip
			// Compressor - specops spectral compander/compressor
      NESTED_ENUM_FROM(BaseProcessors::BaseEffect, Dynamics, (, Contrast, Clip, Compressor), (DEFER), (DEFER), (DEFER))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Contrast, (IDS, Depth, "DYNAMICS_CONTRAST_FX_DEPTH"))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Clip, (IDS, Threshold, "DYNAMICS_CLIPPING_FX_THRESHOLD"))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Compressor)
	

	namespace Parameters
	{
		// 1 - ParameterDetails::id, 2 - enum reflected string
		// the rationalle for using a pair of keys is that it's much more convenient to address parameters through 
		// their corresponding enum values inside the program, but unfortunately their reflected strings are unstable
		// (they rely on compiler specific trickery to work, see type_name in constexpr_utils.h) and change at any time
		// so when it comes to long term save backwards compatibility it's much more stable to use a specified id
		using key = std::pair<std::string_view, std::string_view>;
		using value = ParameterDetails;

		const ParameterDetails &getDetailsId(std::string_view id);
		const ParameterDetails &getDetailsEnum(std::string_view enumString);
		
		std::string_view getEnumString(std::string_view id);

		std::span<const std::string_view> getEffectModesStrings(BaseProcessors::BaseEffect::type type);

		constexpr std::optional<std::pair<size_t, size_t>> getIndexAndCountForEffect(BaseProcessors::BaseEffect::type type, size_t algorithmIndex)
		{
			auto getIndexAndCount = []<nested_enum::NestedEnum Type>(size_t index) -> std::optional<std::pair<size_t, size_t>>
			{
				constexpr auto typeCounts = utils::applyOne(
					[]<typename T>(T &&)
					{
						using type = typename std::remove_cvref_t<T>::type;
						return type::enum_count(nested_enum::AllNodes);
					},
					Type::template enum_subtypes<nested_enum::AllNodes>());

				if (typeCounts.size() <= index)
					return {};

				size_t totalIndex = std::accumulate(typeCounts.begin(), typeCounts.begin() + index, (size_t)0);
				return { std::pair{ totalIndex, typeCounts[index] } };
			};

			switch (type)
			{
			case BaseProcessors::BaseEffect::Filter: return getIndexAndCount.template operator()<BaseProcessors::BaseEffect::Filter::type>(algorithmIndex);
			case BaseProcessors::BaseEffect::Dynamics: return getIndexAndCount.template operator()<BaseProcessors::BaseEffect::Dynamics::type>(algorithmIndex);
			default: return {};
			}
		}
	}

	// with skewOnly == true a normalised value between [0,1] or [-0.5, 0.5] is returned,
	// depending on whether the parameter is bipolar
	double scaleValue(double value, const ParameterDetails &details, float sampleRate = kDefaultSampleRate,
		bool scalePercent = false, bool skewOnly = false) noexcept;

	double unscaleValue(double value, const ParameterDetails &details,
		float sampleRate = kDefaultSampleRate, bool unscalePercent = true) noexcept;

	simd_float scaleValue(simd_float value, const ParameterDetails &details,
		float sampleRate = kDefaultSampleRate) noexcept;

}