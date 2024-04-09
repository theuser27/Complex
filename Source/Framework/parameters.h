/*
	==============================================================================

		parameters.h
		Created: 11 Jul 2022 5:25:41pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <span>
#include "simd_values.h"
#include "constants.h"
#include "nested_enum.h"

namespace Generation
{
	class SoundEngine;
	class EffectsState;
	class EffectsLane;
	class EffectModule;
	class utilityEffect;
	class filterEffect;
	class dynamicsEffect;
	class phaseEffect;
	class pitchEffect;
	class stretchEffect;
	class warpEffect;
	class destroyEffect;
}

namespace Framework
{
	// used for updating parameters
	enum class UpdateFlag : u32 { NoUpdates = 0, Realtime = 1, BeforeProcess = 2, AfterProcess = 3 };

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
		std::string_view pluginName{};												// internal plugin name
		std::string_view displayName{};                       // name displayed to the user
		float minValue = 0.0f;                                // minimum scaled value
		float maxValue = 1.0f;                                // maximum scaled value
		float defaultValue = 0.0f;                            // default scaled value
		float defaultNormalisedValue = 0.0f;                  // default normalised value
		ParameterScale scale = ParameterScale::Linear;        // value skew factor
		std::string_view displayUnits{};                      // "%", " db", etc.
		std::span<const std::string_view> stringLookup{};     // if scale is Indexed, you can use a text lookup
		bool isStereo = false;                                // if parameter allows stereo modulation
		bool isModulatable = true;                            // if parameter allows modulation at all
		UpdateFlag updateFlag = UpdateFlag::Realtime;         // at which point during processing the parameter is updated
		bool isExtensible = false;                            // if parameter's minimum/maximum/default values can change
		bool isAutomatable = true;                            // if parameter allows host automation
	};
	

	NESTED_ENUM(BaseProcessors, ((SoundEngine, ID_TYPE, "{6B31ED46-FBF0-4219-A645-7B774F903026}", Generation::SoundEngine),
	                             (EffectsState, ID_TYPE, "{39B6A6C9-D33F-4AF0-BBDB-6C1F1960184F}", Generation::EffectsState),
	                             (EffectsLane, ID_TYPE, "{F260616E-CF7D-4099-A880-9C52CED263C1}", Generation::EffectsLane),
	                             (EffectModule, ID_TYPE, "{763F9D86-D535-4D63-B486-F863F88CC259}", Generation::EffectModule),
	                             BaseEffect),
		(DEFER),
		(ENUM, EffectsState),
		(DEFER),
		(DEFER),
		(DEFER)
	)

    NESTED_ENUM_FROM(BaseProcessors, (SoundEngine, u64), ((MasterMix, ID, "MASTER_MIX"), (BlockSize, ID, "BLOCK_SIZE"), (Overlap, ID, "OVERLAP"), 
		                                                      (WindowType, ID, "WINDOW_TYPE"), (WindowAlpha, ID, "WINDOW_ALPHA"), (OutGain, ID, "OUT_GAIN")))
    NESTED_ENUM_FROM(BaseProcessors, (EffectsLane, u64), ((LaneEnabled, ID, "LANE_ENABLED"), (Input, ID, "INPUT"), (Output, ID, "OUTPUT"), (GainMatching, ID, "GAIN_MATCHING")))
    NESTED_ENUM_FROM(BaseProcessors, (EffectModule, u64), ((ModuleEnabled, ID, "MODULE_ENABLED"), (ModuleType, ID, "MODULE_TYPE"), (ModuleMix, ID, "MODULE_MIX")))
    // it is important that the effect types be first rather than the parameters
		// in multiple places indices are used to address these effects and having them not begin at 0 causes a lot of headaches
		NESTED_ENUM_FROM(BaseProcessors, BaseEffect, ((Utility, ID_TYPE, "{A8129602-D351-46D5-B85B-D5764C071575}", Generation::utilityEffect), 
		                                              (Filter, ID_TYPE, "{809BD1B8-AA18-467E-8DD2-E396F70D6253}", Generation::filterEffect),
		                                              (Dynamics, ID_TYPE, "{D5DADD9A-5B0F-45C6-ADF5-B6A6415AF2D7}", Generation::dynamicsEffect), 
		                                              (Phase, ID_TYPE, "{5670932B-8B6F-4475-9926-000F6C36C5AD}", Generation::phaseEffect),
		                                              (Pitch, ID_TYPE, "{71133386-9421-4B23-91F9-C826DFC506B8}", Generation::pitchEffect), 
		                                              (Stretch, ID_TYPE, "{D700C4AA-EC95-4703-9837-7AD5BDF5C810}", Generation::stretchEffect),
		                                              (Warp, ID_TYPE, "{5FC3802A-B916-4D36-A853-78A29A5F5687}", Generation::warpEffect),
		                                              (Destroy, ID_TYPE, "{EA1DD088-A73A-4FD4-BB27-38EC0BF91850}", Generation::destroyEffect),

		                                              (Algorithm, ID, "FX_ALGORITHM"), (LowBound, ID, "FX_LOW_BOUND"), (HighBound, ID, "FX_HIGH_BOUND"), 
		                                              (ShiftBounds, ID, "FX_SHIFT_BOUNDS")),
            (ENUM, (Utility, u64)),
            (DEFER),
            (DEFER),
            (DEFER),
            (ENUM, (Pitch, u64)),
            (ENUM, (Stretch, u64)),
            (ENUM, (Warp, u64)),
            (ENUM, (Destroy, u64))
    )

			// Normal - Lowpass/Highpass/Bandpass/Notch
			// Regular - Harmonic/Bin based filters (like dtblkfx peaks)
      NESTED_ENUM_FROM(BaseProcessors::BaseEffect, (Filter, u64), (Normal, Regular, Phase), (DEFER), (DEFER), (DEFER))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Filter, Normal, ((Gain, ID, "FILTER_NORMAL_FX_GAIN"), (Cutoff, ID, "FILTER_NORMAL_FX_CUTOFF"), (Slope, ID, "FILTER_NORMAL_FX_SLOPE")))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Filter, Regular, ((Gain, ID, "FILTER_REGULAR_FX_GAIN"), (Cutoff, ID, "FILTER_REGULAR_FX_CUTOFF"), 
				                                                               (Phase, ID, "FILTER_REGULAR_FX_PHASE"), (Stretch, ID, "FILTER_REGULAR_FX_STRETCH")))
				NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Filter, Phase, ((Gain, ID, "FILTER_PHASE_FX_GAIN"), (LowPhaseBound, ID, "FILTER_PHASE_FX_LOW_PHASE_BOUND"), 
				                                                             (HighPhaseBound, ID, "FILTER_PHASE_FX_HIGH_PHASE_BOUND")))
            
			// Contrast - dtblkfx contrast
			// Clip - dtblkfx clip
			// Compressor - specops spectral compander/compressor
      NESTED_ENUM_FROM(BaseProcessors::BaseEffect, (Dynamics, u64), (Contrast, Clip, Compressor), (DEFER), (DEFER), (DEFER))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Contrast, ((Depth, ID, "DYNAMICS_CONTRAST_FX_DEPTH")))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Clip, ((Threshold, ID, "DYNAMICS_CLIPPING_FX_THRESHOLD")))
        NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Dynamics, Compressor)

			NESTED_ENUM_FROM(BaseProcessors::BaseEffect, (Phase, u64), (Shift, Transform), (DEFER), (DEFER))
				NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Phase, Shift, ((PhaseShift, ID, "PHASE_SHIFT_FX_PHASE_SHIFT"), (Interval, ID, "PHASE_SHIFT_FX_INTERVAL"), (Offset, ID, "PHASE_SHIFT_FX_OFFSET")))
				NESTED_ENUM_FROM(BaseProcessors::BaseEffect::Phase, Transform)
	

	namespace Parameters
	{
		// 1 - ParameterDetails::id, 2 - enum reflected string
		// the rationalle for using a pair of keys is that it's much more convenient to address parameters through 
		// their corresponding enum values inside the program, but parameter names and semantic meaning are unstable 
		// and change at any time so when it comes to long term save backwards compatibility 
		// it's much more stable to use a specified id
		using key = std::pair<std::string_view, std::string_view>;
		using value = ParameterDetails;

		std::optional<ParameterDetails> getDetailsId(std::string_view id);
		const ParameterDetails &getDetailsEnum(std::string_view enumString);
		
		std::string_view getEnumName(std::string_view id);
		std::string_view getIdString(std::string_view enumString);

		std::span<const std::string_view> getEffectModesStrings(BaseProcessors::BaseEffect::type type);

		std::optional<std::pair<size_t, size_t>> getIndexAndCountForEffect(BaseProcessors::BaseEffect::type type, size_t algorithmIndex);
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