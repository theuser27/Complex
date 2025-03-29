/*
  ==============================================================================

    parameters.hpp
    Created: 11 Jul 2022 5:25:41pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Third Party/gcem/gcem.hpp"
#include "Third Party/constexpr-to-string/to_string.hpp"

#include "parameter_types.hpp"

namespace Generation
{
  class SoundEngine;
  class EffectsState;
  class EffectsLane;
  class EffectModule;
  class UtilityEffect;
  class FilterEffect;
  class DynamicsEffect;
  class PhaseEffect;
  class PitchEffect;
  class StretchEffect;
  class WarpEffect;
  class DestroyEffect;
}

namespace Plugin
{
  class ProcessorTree;
}

namespace Framework
{
  inline constexpr auto kInputSidechainCountChange = utils::string_view{ "39b66ac7-790c-4987-978b-e8679583b94c" };
  inline constexpr auto kOutputSidechainCountChange = utils::string_view{ "2a0534fd-2888-416e-b3aa-e1436f3c88e4" };
  inline constexpr auto kLaneCountChange = utils::string_view{ "ae2ace66-5e7a-41f9-8d45-92f4dc894947" };

  inline constexpr auto kAllChangeIds = utils::array{ kInputSidechainCountChange, kOutputSidechainCountChange, kLaneCountChange };

  template<template<typename, auto> class array_t, typename T, usize N>
  constexpr auto convertNameIdToIndexedData(const array_t<T, N> &array)
  {
    array_t<IndexedData, N> data{};
    for (usize i = 0; i < N; ++i)
      data[i] = IndexedData{ array[i].first, array[i].second };
    return data;
  }

  inline constexpr auto kGetParameterPredicate = []<typename T>() { return requires{ typename T::parameter_tag; }; };
  inline constexpr auto kGetProcessorPredicate = []<typename T>() { return requires{ typename T::processor_tag; }; };
  inline constexpr auto kGetEffectPredicate = []<typename T>() { return requires{ T::effect_tag; }; };
  inline constexpr auto kGetActiveEffectPredicate = []<typename T>() { return requires{ T::effect_tag; requires T::effect_tag; }; };
  inline constexpr auto kGetAlgoPredicate = []<typename T>() { return requires{ T::algo_tag; }; };
  inline constexpr auto kGetActiveAlgoPredicate = []<typename T>() { return requires{ T::algo_tag; requires T::algo_tag; }; };

  inline constexpr auto kOffOnNames = utils::array{ IndexedData{ "Off" }, IndexedData{ "On" } };

#define DECLARE_PARAMETER(...) using parameter_tag = void; static constexpr ParameterDetails details{ id().value(), __VA_ARGS__ };
#define DECLARE_PROCESSOR(type) using processor_tag = void; using linked_type = type;
#define DECLARE_EFFECT(type, isEnabled) DECLARE_PROCESSOR(type) static constexpr bool effect_tag = isEnabled;
#define DECLARE_ALGO(isEnabled) static constexpr bool algo_tag = isEnabled;

#define CREATE_FFT_SIZE_NAMES                                                                                             \
  static constexpr auto kFFTSizeNames = []()                                                                              \
  {                                                                                                                       \
    constexpr auto integers = []<i32 ... Is>(const utils::integer_sequence<i32, Is...> &)                                 \
    {                                                                                                                     \
      return NESTED_ENUM_ARRAY_TYPE{ utils::string_view{ to_string<1 << (kMinFFTOrder + Is)> }... };                      \
    }(utils::make_integer_sequence<i32, kMaxFFTOrder - kMinFFTOrder + 1>{});                                              \
                                                                                                                          \
    NESTED_ENUM_ARRAY_TYPE<IndexedData, integers.size()> fftSizeLookup{};                                                 \
    for (usize i = 0; i < integers.size(); ++i)                                                                           \
      fftSizeLookup[i] = { integers[i] };                                                                                 \
    return fftSizeLookup;                                                                                                 \
  }();

#define CREATE_INPUT_NAMES                                                                                                \
  static constexpr auto kInputNames = NESTED_ENUM_ARRAY_TYPE                                                              \
  {                                                                                                                       \
    IndexedData{ "Main Input", Main::id().value() },                                                                      \
    IndexedData{ "Sidechain", Sidechain::id().value(), 0, kInputSidechainCountChange },                                   \
    IndexedData{ "Lane", Lane::id().value(), 0, kLaneCountChange }                                                        \
  };

#define CREATE_OUTPUT_NAMES                                                                                               \
  static constexpr auto kOutputNames = NESTED_ENUM_ARRAY_TYPE                                                             \
  {                                                                                                                       \
    IndexedData{ "Main Output", Main::id().value() },                                                                     \
    IndexedData{ "Sidechain", Sidechain::id().value(), 0, kOutputSidechainCountChange },                                  \
    IndexedData{ "None", None::id().value() }                                                                             \
  };


  NESTED_ENUM(Processors, 
    (
      (SoundEngine , ID, "6b31ed46-fbf0-4219-a645-7b774f903026"), 
      (EffectsState, ID, "39b6a6c9-d33f-4af0-bbdb-6c1f1960184f"),
      (EffectsLane , ID, "f260616e-cf7d-4099-a880-9c52ced263c1"), 
      (EffectModule, ID, "763f9d86-d535-4d63-b486-f863f88cc259"),
       BaseEffect
    ), 
    (DEFER), (DEFER), (DEFER), (DEFER), (DEFER)
  )

    NESTED_ENUM_FROM(Processors, (SoundEngine, u64, DECLARE_PROCESSOR(Generation::SoundEngine)),
      (
        (MasterMix  , ID_CODE, "b76bc6b8-e33d-4cd8-8a90-6d83d0ba0754", DECLARE_PARAMETER("Mix", 0.0f, 1.0f, 1.0f, 1.0f, 
          ParameterScale::Linear, "%", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess)),
        (BlockSize  , ID_CODE, "7a2f5aca-dd4c-438f-b494-7bf21f7c5c40", CREATE_FFT_SIZE_NAMES DECLARE_PARAMETER("Block Size", kMinFFTOrder, kMaxFFTOrder, 
          kDefaultFFTOrder, (float)(kDefaultFFTOrder - kMinFFTOrder) / (float)(kMaxFFTOrder - kMinFFTOrder), 
          ParameterScale::IndexedNumeric, "", kFFTSizeNames, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess, 
          [](float value, const ParameterDetails &) { return std::to_string(1 << (usize)value); })),
        (Overlap    , ID_CODE, "14525f80-0590-493e-a977-d13661571714", DECLARE_PARAMETER("Overlap", kMinWindowOverlap, kMaxWindowOverlap,
          kDefaultWindowOverlap, kDefaultWindowOverlap, ParameterScale::Clamp, "%", {}, ParameterDetails::Automatable)),
        (WindowType , ID_CODE, "555614bd-c3dd-4dfa-8e2e-563574f2095f", static constexpr auto kWindowNames =  convertNameIdToIndexedData(
          enum_names_and_ids<::nested_enum::All, true>(true)); DECLARE_PARAMETER("Window", 0.0f, (float)enum_count() - 1.0f, 1.0f, 
          1.0f / (float)enum_count(), ParameterScale::Indexed, "", kWindowNames, ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess)),
        (WindowAlpha, ID_CODE, "bd0489de-4251-4591-bbb9-79f3ec787d83", DECLARE_PARAMETER("Alpha", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Linear,
          "%", {}, ParameterDetails::Automatable, UpdateFlag::BeforeProcess)),
        (OutGain    , ID_CODE, "03d3b75e-156e-4d9b-ac6f-4fe294f6f9ab", DECLARE_PARAMETER("Gain", -30.0f, 30.0f, 0.0f, 0.5f, ParameterScale::Linear, 
          " dB", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess))
      ), (), (), (), 
      (ENUM, (WindowType, u64), 
        (
          (Lerp   , ID, "1d7e18d3-2067-49a9-9170-a38d25e975e1"), (Hann     , ID, "2c80a10b-75ae-443f-84d4-23b080c62103"), 
          (Hamming, ID, "6fe48330-7f16-4634-bff8-459ac6ba8b83"), (Triangle , ID, "a03b77d3-6398-493f-851d-37fb751b3750"), 
          (Sine   , ID, "a24409a9-ee38-4ad8-8042-a8b6b583b6ba"), (Rectangle, ID, "d1e38790-c6ec-47d0-96c0-62d331d70acc"), 
          (Exp    , ID, "fc3961f1-176d-41ae-bc9e-23ee42f6a3b0"), (HannExp  , ID, "102f982b-9fc7-496f-b478-af389ad30e22"), 
          (Lanczos, ID, "8223808b-2fbb-4678-b130-72761e6302a6")
        )
      )
    )
    NESTED_ENUM_FROM(Processors, (EffectsState, u64, DECLARE_PROCESSOR(Generation::EffectsState)))
    NESTED_ENUM_FROM(Processors, (EffectsLane, u64, DECLARE_PROCESSOR(Generation::EffectsLane)),
      (
        (LaneEnabled , ID_CODE, "6db94a5f-025e-4270-83fc-61de68167a49", DECLARE_PARAMETER("Lane Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, "", kOffOnNames,
          ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess)),
        (Input       , ID_CODE, "c1f93924-850e-4575-8789-efb5a984619e", CREATE_INPUT_NAMES DECLARE_PARAMETER("Input", 0.0f, 0.0f, 0.0f, 0.0f, 
          ParameterScale::Indexed, "", kInputNames, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess)),
        (Output      , ID_CODE, "b7a717ed-10be-42ea-8025-f093e54ab754", CREATE_OUTPUT_NAMES DECLARE_PARAMETER("Output", 0.0f, 1.0f, 0.0f, 0.0f, 
          ParameterScale::Indexed, "", kOutputNames, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::BeforeProcess)),
        (GainMatching, ID_CODE, "c846df02-4908-4c98-8805-57facbbe7716", DECLARE_PARAMETER("Gain Matching", 0.0f, 1.0f, 1.0f, 1.0f, 
          ParameterScale::Toggle, "", kOffOnNames, ParameterDetails::Modulatable | ParameterDetails::Automatable, UpdateFlag::BeforeProcess))
      ),
      (), 
      (ENUM, Input, 
        (
          (Main, ID, "c9e6aa64-e9f1-4048-a8fd-6fe98dec63b2"), (Sidechain, ID, "8e91a8a8-7241-465f-996e-98b3fab3216d"), 
          (Lane, ID, "c6b45fb5-d1e5-4fdc-99d9-5f3f98c74f79")
        )
      ),
      (ENUM, Output, 
        (
          (Main, ID, "ed61dfd8-54a2-439e-9d19-958e6c81585f"), (Sidechain, ID, "92540efa-8ca1-49ae-826d-78918b448da7"), 
          (None, ID, "88f0181d-5de3-4ce3-89f5-ad97cae97426")
        )
      )
    )
    NESTED_ENUM_FROM(Processors, (EffectModule, u64, DECLARE_PROCESSOR(Generation::EffectModule)),
      ((ModuleEnabled, ID_CODE, "0c47a9ed-3acc-4860-8831-b18eec06cede", DECLARE_PARAMETER("Module Enabled", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Toggle, {}, kOffOnNames)),
       (ModuleType   , ID     , "fb37e6da-b5d5-48ae-8305-c47ebe3a84e1"),
       (ModuleMix    , ID_CODE, "ee2610d1-73dd-4637-ad39-dbfa8ffaa764", DECLARE_PARAMETER("Mix", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Linear, "%", {}, 
         ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))),
      (), (DEFER)
    )
    // it is important that the effect types be first rather than the parameters
    // in multiple places indices are used to address these effects and having them not begin at 0 causes a lot of headaches
    NESTED_ENUM_FROM(Processors, (BaseEffect, u64), 
      (
        (Utility    , ID_CODE, "a8129602-d351-46d5-b85b-d5764c071575", DECLARE_EFFECT(Generation::UtilityEffect, false)),
        (Filter     , ID     , "809bd1b8-aa18-467e-8dd2-e396f70d6253"),
        (Dynamics   , ID     , "d5dadd9a-5b0f-45c6-adf5-b6a6415af2d7"),
        (Phase      , ID     , "5670932b-8b6f-4475-9926-000f6c36c5ad"),
        (Pitch      , ID     , "71133386-9421-4b23-91f9-c826dfc506b8"),
        (Stretch    , ID_CODE, "d700c4aa-ec95-4703-9837-7ad5bdf5c810", DECLARE_EFFECT(Generation::StretchEffect, false)),
        (Warp       , ID_CODE, "5fc3802a-b916-4d36-a853-78a29a5f5687", DECLARE_EFFECT(Generation::WarpEffect, false)),
        (Destroy    , ID     , "ea1dd088-a73a-4fd4-bb27-38ec0bf91850"),
        
        (Algorithm  , ID_CODE, "6cecfa1f-a5dc-4ce3-9ae2-b1bf20a4f3bc", DECLARE_PARAMETER("Algorithm", 0.0f, 0.0f, 0.0f, 0.0f, ParameterScale::Indexed, "", {}, 
          ParameterDetails::Automatable | ParameterDetails::Extensible)),
        (LowBound   , ID_CODE, "af36221b-cee5-4cdb-b514-fd12dbf4034b", DECLARE_PARAMETER("Low Bound", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz", {}, 
          ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
        (HighBound  , ID_CODE, "98db60d3-09bf-40f6-b6c6-0ed90b4c7d9f", DECLARE_PARAMETER("High Bound", 0.0f, 1.0f, 1.0f, 1.0f, ParameterScale::Frequency, " hz", {}, 
          ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
        (ShiftBounds, ID_CODE, "5198be8d-cf98-435a-96b2-7b90d69bf846", DECLARE_PARAMETER("Shift Bounds", -1.0f, 1.0f, 0.0f, 0.5f, ParameterScale::Linear, "%", {}, 
          ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))
      ),
      (), (DEFER), (DEFER), (DEFER), (DEFER), (), (), (DEFER)
    )
    

      // Normal - Lowpass/Highpass/Bandpass/Notch
      // Regular - Harmonic/Bin based filters (like dtblkfx peaks)
      NESTED_ENUM_FROM(Processors::BaseEffect, (Filter, u64, DECLARE_EFFECT(Generation::FilterEffect, true)),
        (
          (Normal , ID, "49786369-b4b6-42c4-af5c-97eb40e16632"),
          (Gate   , ID, "38b204a1-dfac-4cb2-8cd3-d11274e6e227"),
          (Regular, ID, "7dcd7705-b43f-41e9-8503-c7c8c71fe337")
        ), (DEFER), (DEFER), (DEFER))
        NESTED_ENUM_FROM(Processors::BaseEffect::Filter, (Normal, u64, DECLARE_ALGO(true)),
          (
            (Gain  , ID_CODE, "2a8eda72-b3a7-4340-b8a2-87de97e08027", DECLARE_PARAMETER("Gain", kMinusInfDb, kInfDb, 0.0f, 0.5f, 
              ParameterScale::SymmetricLoudness, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Cutoff, ID_CODE, "d8854139-826b-40ad-ba8e-fbb603dd782e", DECLARE_PARAMETER("Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, 
              ParameterScale::Frequency, " hz", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Slope , ID_CODE, "17c7d282-0732-4a79-adaa-d2ed436a6890" , DECLARE_PARAMETER("Slope", -1.0f, 1.0f, 0.25f, 0.75f, 
              ParameterScale::SymmetricQuadratic, "%", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))
          )
        )
        NESTED_ENUM_FROM(Processors::BaseEffect::Filter, (Gate, u64, DECLARE_ALGO(true)),
          (
            (InputGain, ID_CODE, "741ebdbd-6269-485e-b527-f96d206ca74b", DECLARE_PARAMETER("Gain", kMinusInfDb, kInfDb, 0.0f, 0.5f,
              ParameterScale::SymmetricLoudness, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Gain, ID_CODE, "95b7b39f-be9e-4a9e-bada-412bc51d424f", DECLARE_PARAMETER("Gain", kMinusInfDb, kInfDb, 0.0f, 0.5f,
              ParameterScale::SymmetricLoudness, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Threshold, ID_CODE, "2a510c52-d916-458c-bf2f-e5edeed1d5ce", DECLARE_PARAMETER("Threshold", -180.0f, 0.0f, -45.0f, 0.5f,
              ParameterScale::ReverseQuadratic, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Tilt, ID_CODE, "6147073f-709a-4d83-8449-bbb3d94c4a9c", DECLARE_PARAMETER("Tilt", -24.0f, 24.0f, 0.0f, 0.5f,
              ParameterScale::SymmetricQuadratic, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Mode, ID_CODE, "bddbd917-96ee-40e0-a416-ea0b0cb1b3c9",
              (static constexpr auto kModeNames = convertNameIdToIndexedData(enum_names_and_ids<::nested_enum::All, true>(true));
              DECLARE_PARAMETER("Mode", 0.0f, (float)(kModeNames.size() - 1), 0.0f, 0.0f, ParameterScale::Indexed, {}, kModeNames,
              ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)))
          ),
          (), (), (), (),
          (ENUM, Mode, ((Decibels, ID, "c1062f9a-1ee9-4173-aabc-ece944e3edf9"), (Rank, ID, "5c655206-c82a-462e-852d-52321afcc168")))
        )
        NESTED_ENUM_FROM(Processors::BaseEffect::Filter, (Regular, u64, DECLARE_ALGO(false)),
          (
            (Gain   , ID_CODE, "1e28ea1d-a7f2-42a3-a9d1-87a996393af9", DECLARE_PARAMETER("Gain", kMinusInfDb, kInfDb, 0.0f, 0.5f, 
              ParameterScale::SymmetricLoudness, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Cutoff , ID_CODE, "4c22e6b1-dd40-4a71-b3f6-aee3f3684611", DECLARE_PARAMETER("Cutoff", 0.0f, 1.0f, 0.5f, 0.5f, 
              ParameterScale::Linear, "", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Phase  , ID_CODE, "1363f53e-5dc6-45ca-9189-5f71a359de70", DECLARE_PARAMETER("Phase", -1.0f, 1.0f, 0.25f, 0.75f, 
              ParameterScale::SymmetricQuadratic, "", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Stretch, ID_CODE, "4aee1cfd-b8df-4f3d-9780-be8323d3c7c1", DECLARE_PARAMETER("Stretch", 0.0f, 1.0f, 0.5f, 0.5f, 
              ParameterScale::Linear, "", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))
          )
        )
            
      // Contrast - dtblkfx contrast
      // Clip - dtblkfx clip
      // Compressor - specops spectral compander/compressor
      NESTED_ENUM_FROM(Processors::BaseEffect, (Dynamics, u64, DECLARE_EFFECT(Generation::DynamicsEffect, true)),
        (
          (Contrast  , ID, "c90fa192-f410-4924-8c84-0026969193ea"), 
          (Clip      , ID, "6c5df100-8d53-4678-850c-c260e5465666"), 
          (Compressor, ID, "457151a7-c1aa-4065-98cb-c9e7d5895115")
        ), (DEFER), (DEFER), (DEFER), (DEFER))
        NESTED_ENUM_FROM(Processors::BaseEffect::Dynamics, (Contrast, u64, DECLARE_ALGO(true)),
          ((Depth, ID_CODE, "e250f5f2-d0a0-4331-87fe-1b0b808e088d", DECLARE_PARAMETER("Depth", -1.0f, 1.0f, 0.0f, 0.5f, 
            ParameterScale::Linear, "%", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))))
        NESTED_ENUM_FROM(Processors::BaseEffect::Dynamics, (Clip, u64, DECLARE_ALGO(true)),
          ((Threshold, ID_CODE, "097943a6-e5d9-4e95-bc32-c2b961ce1540", DECLARE_PARAMETER("Threshold", 0.0f, 1.0f, 0.0f, 0.0f, 
            ParameterScale::Linear, "%", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))))
        NESTED_ENUM_FROM(Processors::BaseEffect::Dynamics, (Compressor, u64, DECLARE_ALGO(false)))

      NESTED_ENUM_FROM(Processors::BaseEffect, (Phase, u64, DECLARE_EFFECT(Generation::PhaseEffect, true)),
        (
          (Shift    , ID, "38360263-5f6a-4c91-8da1-2c3edf33593e"), 
          (Transform, ID, "3d14ff55-a3ca-4c5f-8ebc-2eff2dc93504")
        ), (DEFER), (DEFER))
        NESTED_ENUM_FROM(Processors::BaseEffect::Phase, (Shift, u64, DECLARE_ALGO(true)),
          (
            (PhaseShift, ID_CODE, "98c64b24-cfec-4306-a38d-bce56c49ea35", DECLARE_PARAMETER("Shift", -180.0f, 180.0f, 0.0f, 0.5f, 
              ParameterScale::Linear, COMPLEX_DEGREE_SIGN_LITERAL, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Interval  , ID_CODE, "38acd2c1-6a20-4003-8964-2f07dd983995", DECLARE_PARAMETER("Interval", 0.0f, 10.0f, 1.0f, gcem::pow(1.0f / 10.0f, 1.0f / 3.0f), 
              ParameterScale::Cubic, " oct", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Offset    , ID_CODE, "1808ef90-46e8-43b8-bcc1-908014abd2f6", DECLARE_PARAMETER("Offset", 0.0f, 1.0f, 0.0f, 0.0f, ParameterScale::Frequency, " hz", {}, 
              ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Slope     , ID_CODE, "c4ee95da-cb8d-4969-9ee4-5ca24019d5ed",
              (static constexpr auto kPhaseShiftSlopeNames = convertNameIdToIndexedData(enum_names_and_ids<::nested_enum::All, true>(true));
              DECLARE_PARAMETER("Slope", 0.0f, (float)(kPhaseShiftSlopeNames.size() - 1), 0.0f, 0.0f, ParameterScale::Indexed, {}, kPhaseShiftSlopeNames,
              ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Extensible)))
          ),
          (), (), (), 
          (ENUM, Slope, ((Constant, ID, "36722e1b-a1f3-4c1f-8599-647c1a637e0b"), (Linear, ID, "f1dc8a19-d0f2-4878-9b52-4ed05706281c"), (Exp, ID, "0a10a4b6-6657-42f4-80de-a5ce14d00d4a"))))
        NESTED_ENUM_FROM(Processors::BaseEffect::Phase, (Transform, u64, DECLARE_ALGO(false)))
  
      NESTED_ENUM_FROM(Processors::BaseEffect, (Pitch, u64, DECLARE_EFFECT(Generation::PitchEffect, true)),
        (
          (Resample, ID, "1860e713-f01a-4183-94a4-61500da98911"),
          (ConstShift, ID, "e7528343-f4d2-42e4-9f66-b87416259844")
        ), (DEFER), (DEFER))
        NESTED_ENUM_FROM(Processors::BaseEffect::Pitch, (Resample, u64, DECLARE_ALGO(true)),
          (
            (Shift, ID_CODE, "38949dbf-6edc-446a-908d-955fe492077c", DECLARE_PARAMETER("Shift", -48.0f, 48.0f, 0.0f, 0.5f, 
              ParameterScale::Linear, " st", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Wrap, ID_CODE, "4ec6f4e7-9bb8-439e-9a5f-50a6f818d17e", DECLARE_PARAMETER("Wrap Around", 0.0f, 1.0f, 0.0f, 0.0f,
              ParameterScale::Toggle, {}, {}, ParameterDetails::Modulatable | ParameterDetails::Automatable))
          )
        )
        NESTED_ENUM_FROM(Processors::BaseEffect::Pitch, (ConstShift, u64, DECLARE_ALGO(true)),
          ((Shift, ID_CODE, "4b78cec4-c746-4a50-a9a8-3cbd010a0f34", DECLARE_PARAMETER("Frequency Shift", -20'000.0f, 20'000.0f, 0.0f, 0.5f, 
            ParameterScale::SymmetricCubic, " hz", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo))))

      NESTED_ENUM_FROM(Processors::BaseEffect, (Destroy, u64, DECLARE_EFFECT(Generation::DestroyEffect, true)),
        (
          (Reinterpret, ID, "eefc7c4c-be13-4cdb-af4f-0c656a1ff4a9")
        ), (DEFER))
        NESTED_ENUM_FROM(Processors::BaseEffect::Destroy, (Reinterpret, u64, DECLARE_ALGO(true)),
          (
            (Attenuation, ID_CODE, "f81e64d3-44c4-4a11-91f1-b2640d9e2839", DECLARE_PARAMETER("Real/Imag Atten", kMinusInfDb, kInfDb, 0.0f, 0.5f,
              ParameterScale::SymmetricLoudness, " db", {}, ParameterDetails::Modulatable | ParameterDetails::Automatable | ParameterDetails::Stereo)),
            (Mapping, ID_CODE, "0206e09a-f472-4cc7-94fd-d3f6f6cd5787",
              (static constexpr auto kMappingNames = utils::array{ IndexedData{ "No Change", NoMapping::id().value() },
                IndexedData{ "Swap Real/Imaginary", SwitchRealImag::id().value() }, IndexedData{ "Cartesian->Polar", CartToPolar::id().value() },
                IndexedData{ "Polar->Cartesian", PolarToCart::id().value() } };
              DECLARE_PARAMETER("Mapping", 0.0f, (float)(kMappingNames.size() - 1), 0.0f, 0.0f, ParameterScale::Indexed,
              {}, kMappingNames, ParameterDetails::Modulatable | ParameterDetails::Automatable)))
          ), (),
          (ENUM, (Mapping, u64),
            (
              (NoMapping     , ID, "681c148f-dd68-4044-b69b-a64585047c22"),
              (SwitchRealImag, ID, "50d9ec45-87c8-4945-b953-116b9f7a01cf"),
              (CartToPolar   , ID, "7eb5abe6-a6af-4c87-82c8-ca177ba069d8"),
              (PolarToCart   , ID, "ee041eac-78fa-47f4-9432-1e6cc294fcbe")
            )
          )
        )

    NESTED_ENUM_FROM(Processors::EffectModule, (ModuleType, u64, static constexpr auto kEffectModuleNames = convertNameIdToIndexedData(
      Processors::BaseEffect::enum_names_and_ids_filter<kGetActiveEffectPredicate, true>(true)); static constexpr float maxValue = 
      (float)Processors::BaseEffect::enum_count_filter(kGetActiveEffectPredicate) - 1.0f;
      DECLARE_PARAMETER("Module Type", 0.0f, maxValue, 1.0f, 1.0f / maxValue, ParameterScale::Indexed, "", kEffectModuleNames,
        ParameterDetails::Automatable | ParameterDetails::Extensible, UpdateFlag::AfterProcess))
    )

#undef DECLARE_PARAMETER
#undef DECLARE_PROCESSOR
#undef DECLARE_EFFECT
#undef DECLARE_ALGO
#undef CREATE_FFT_SIZE_NAMES
#undef CREATE_INPUT_NAMES
#undef CREATE_OUTPUT_NAMES

}