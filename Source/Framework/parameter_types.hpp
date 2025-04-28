/*
  ==============================================================================

    parameter_types.hpp
    Created: 26 Mar 2025 11:20:23am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <string>
#include <vector>

#include "simd_values.hpp"
#include "constants.hpp"
#include "utils.hpp"

#define NESTED_ENUM_ARRAY_TYPE ::utils::array
#define NESTED_ENUM_STRING_VIEW_TYPE ::utils::string_view
#include "nested_enum.hpp"

namespace Framework
{
  class ParameterValue;

  // symmetric types apply the flipped curve to negative values
  // all x values are normalised

  NESTED_ENUM((ParameterScale, u32),
    (
      (Toggle            , ID, "3195faf6-b6ea-4d21-94a3-cb15b6184d84"), // round(x)
      (Indexed           , ID, "f0850d8b-2f46-4860-82f8-5d46d2d27bef"), // round(x * (max - min))
      (IndexedNumeric    , ID, "e593dbd0-1af6-410f-865b-4044646ab9c9"), // round(x * (max - min)), but cannot have values rearranged
      (Linear            , ID, "e4c9e4aa-c9a0-493f-869b-f090460bfbf1"), // x * (max - min) + min
      (Clamp             , ID, "20f78758-8eb1-403a-8207-9a124e4b5683"), // clamp(x, min, max)
      (Quadratic         , ID, "9c3ac761-4c0e-462e-be03-8604c391e085"), // x ^ 2 * (max - min) + min
      (ReverseQuadratic  , ID, "ed4e371b-26d6-42b0-8148-9f0355055d6b"), // (x - 1) ^ 2 * sgn(x - 1) * (max - min) + max
      (SymmetricQuadratic, ID, "69f8a98b-7bd0-494d-b971-3ded188485c4"), // ((x - 0.5) ^ 2 * sgn(x - 0.5) + 0.5) * 2 * (max - min) + min
      (Cubic             , ID, "cf999277-1c98-4b1b-a8ff-4161d2cd9f35"), // x ^ 3
      (ReverseCubic      , ID, "f427599a-c9c7-45be-b824-50fde17f1b3c"), // (x - 1) ^ 3 * (max - min) + max
      (SymmetricCubic    , ID, "1124f08e-6ac1-4ac7-8cc7-81bd662e9fe7"), // (2 * x - 1) ^ 3
      (Loudness          , ID, "b64df42f-b5b7-4b76-af63-167722c26543"), // 20 * log10(x)
      (SymmetricLoudness , ID, "fb7963d1-2ba8-4061-88ab-76f9397fc6ad"), // 20 * log10(abs(x)) * sgn(x)
      (Frequency         , ID, "9ed6e3bc-f91d-46fa-bd6a-2edcb1e178a2"), // (sampleRate / 2 * minFrequency) ^ x
      (SymmetricFrequency, ID, "200df43d-0c2c-4e1d-a780-07b9c024ba1a")  // (sampleRate / 2 * minFrequency) ^ (abs(x)) * sgn(x)
    )
  )

  struct IndexedData
  {
    utils::string_view displayName{};                     // user-readable name for given parameter value
    utils::string_view id{};                              // uuid for parameter value
    u64 count = 1;                                        // how many consecutive values are of this indexed type
                                                          //   can be more than the ones currently available
    utils::string_view dynamicUpdateUuid{};               // this uuid is used to register for in ProcessorTree
                                                          //   these updates will happen only if the parameter is not mapped/modulated
    struct DynamicData
    {
      std::string stringData{};
      std::vector<IndexedData> dataLookup{};
      utils::small_fn<bool(const Framework::IndexedData &, int)> ignoreItemFn{};
    };
  };

  struct ParameterDetails
  {
    enum Flags : u8
    { 
      None        = 0     ,
      Stereo      = 1 << 0,         // if parameter allows stereo modulation
      Modulatable = 1 << 1,         // if parameter allows modulation at all
      Automatable = 1 << 2,         // if parameter allows host automation
      Extensible  = 1 << 3,         // if parameter's minimum/maximum/default values can change
      All = Stereo | Modulatable |  
        Automatable | Extensible,
    };

    utils::string_view id{};												    // internal plugin name
    utils::string_view displayName{};                   // name displayed to the user
    float minValue = 0.0f;                              // minimum scaled value
    float maxValue = 1.0f;                              // maximum scaled value
    float defaultValue = 0.0f;                          // default scaled value
    float defaultNormalisedValue = 0.0f;                // default normalised value
    ParameterScale scale = ParameterScale::Linear;      // value skew factor
    utils::string_view displayUnits{};                  // "%", " db", etc.
    utils::span<const IndexedData> indexedData{};       // extra data for indexed parameters
    u32 flags = Modulatable | Automatable;              // various flags
    UpdateFlag updateFlag = UpdateFlag::Realtime;       // at which point during processing the parameter is updated
    std::string (*generateNumeric)(float value,         // string generator function for IndexedNumeric parameters
      const ParameterDetails &details) = nullptr;
    utils::sp<IndexedData::DynamicData> dynamicData;
  };

  auto getParameterDetails(utils::string_view id) noexcept
    -> std::optional<ParameterDetails>;

  auto getIndexedData(double scaledValue, const ParameterDetails &details) noexcept
    -> utils::pair<const IndexedData *, usize>;

  // with skewOnly == true a normalised value between [0,1] or [-0.5, 0.5] is returned,
  // depending on whether the parameter is bipolar
  double scaleValue(double value, const ParameterDetails &details, float sampleRate = kDefaultSampleRate,
    bool scalePercent = false, bool skewOnly = false) noexcept;

  double unscaleValue(double value, const ParameterDetails &details,
    float sampleRate = kDefaultSampleRate, bool unscalePercent = true) noexcept;

  simd_float scaleValue(simd_float value, const ParameterDetails &details,
    float sampleRate = kDefaultSampleRate) noexcept;

}
