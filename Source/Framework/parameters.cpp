/*
  ==============================================================================

    parameters.cpp
    Created: 4 Sep 2023 9:38:38pm
    Author:  theuser27

  ==============================================================================
*/

#include "parameters.hpp"

#include <array>

#include "utils.hpp"
#include "simd_utils.hpp"
#include "parameter_value.hpp"
#include "Plugin/ProcessorTree.hpp"
#include "Interface/Components/BaseControl.hpp"
#include "Framework/parameter_bridge.hpp"

namespace Framework
{
  static constexpr auto lookup = []<typename ... Ts>(const std::tuple<std::type_identity<Ts>...> &)
  {
    return std::array<ParameterDetails, sizeof...(Ts)>{ Ts::details... };
  }(Processors::enum_subtypes_filter_recursive<[]<typename T>() { return requires{ T::parameter_tag; }; }>());

  auto getParameterDetails(std::string_view id) noexcept -> std::optional<ParameterDetails> 
  {
    auto iter = std::ranges::find_if(lookup, [&](const auto &v) { return v.id == id; });
    if (iter != lookup.end())
      return *iter;

    return {};
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
    case ParameterScale::IndexedNumeric:
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
    case ParameterScale::Quadratic:
      result = (!skewOnly) ? value * value * (details.maxValue - details.minValue) + details.minValue : value * value;
      break;
    case ParameterScale::SymmetricQuadratic:
      value = value * 2.0 - 1.0;
      sign = unsignFloat(value);
      value *= value;
      range = details.maxValue - details.minValue;
      result = (!skewOnly) ? (value * sign * range + range) * 0.5 + details.minValue : value * 0.5 * sign;
      break;
    case ParameterScale::Cubic:
      result = (!skewOnly) ? value * value * value * (details.maxValue - details.minValue) + details.minValue : value * value * value;
      break;
    case ParameterScale::Loudness:
      result = normalisedToDb(value, (double)details.maxValue);
      result = (!skewOnly) ? result : result / (double)details.maxValue;
      break;
    case ParameterScale::SymmetricLoudness:
      value = value * 2.0 - 1.0;
      if (value < 0.0)
      {
        result = -normalisedToDb(-value, -(double)details.minValue);
        result = (!skewOnly) ? result : result * 0.5 / (double)details.minValue;
      }
      else
      {
        result = normalisedToDb(value, (double)details.maxValue);
        result = (!skewOnly) ? result : result * 0.5 / (double)details.maxValue;
      }
      break;
    case ParameterScale::Frequency:
      result = normalisedToFrequency(value, (double)sampleRate);
      result = (!skewOnly) ? result : result * 2.0 / sampleRate;
      break;
    case ParameterScale::SymmetricFrequency:
      value = value * 2.0 - 1.0;
      sign = unsignFloat(value);
      result = (!skewOnly) ? normalisedToFrequency(value, (double)sampleRate) * sign :
        (normalisedToFrequency(value, (double)sampleRate) * sign) / sampleRate;
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
    case ParameterScale::IndexedNumeric:
      result = (value - details.minValue) / (details.maxValue - details.minValue);
      break;
    case ParameterScale::Clamp:
      result = value;
      break;
    case ParameterScale::Linear:
      result = (value - details.minValue) / (details.maxValue - details.minValue);
      break;
    case ParameterScale::Quadratic:
      result = std::sqrt((value - details.minValue) / (details.maxValue - details.minValue));
      break;
    case ParameterScale::SymmetricQuadratic:
      value = 2.0 * (value - details.minValue) / (details.maxValue - details.minValue) - 1.0;
      sign = unsignFloat(value);
      result = std::sqrt(value) * sign;
      break;
    case ParameterScale::Cubic:
      value = (value - details.minValue) / (details.maxValue - details.minValue);
      result = std::pow(value, 1.0f / 3.0f);
      break;
    case ParameterScale::Loudness:
      value = dbToNormalised(value, (double)details.maxValue);
      result = (value + 1.0f) * 0.5f;
      break;
    case ParameterScale::SymmetricLoudness:
      if (value < 0.0)
        value = -dbToNormalised(-value, -(double)details.minValue);
      else
        value = dbToNormalised(value, (double)details.maxValue);
      result = (value + 1.0f) * 0.5f;
      break;
    case ParameterScale::Frequency:
      result = frequencyToNormalised(value, (double)sampleRate);
      break;
    case ParameterScale::SymmetricFrequency:
      sign = unsignFloat(value);
      value = frequencyToNormalised(value, (double)sampleRate);
      result = (value * sign + 1.0f) * 0.5f;
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
    switch (details.scale)
    {
    case ParameterScale::Toggle:
      result = reinterpretToFloat(simd_float::notEqual(simd_float::round(value), 0.0f));
      break;
    case ParameterScale::Indexed:
    case ParameterScale::IndexedNumeric:
      result = simd_float::round(value * (details.maxValue - details.minValue) + details.minValue);
      break;
    case ParameterScale::Linear:
      result = value * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::Quadratic:
      result = value * value * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::SymmetricQuadratic:
      value -= 0.5f;
      sign = getSign(value);
      value *= value;
      result = ((value ^ sign) + 0.25f) * 2.0f * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::Cubic:
      result = value * value * value * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::Loudness:
      result = normalisedToDb(value, details.maxValue);
      break;
    case ParameterScale::SymmetricLoudness:
      value = (value * 2.0f - 1.0f);
      sign = unsignSimd(value);
      if (areAllElementsSame(sign))
      {
        float extremum = (sign[0]) ? -details.minValue : details.maxValue;
        result = normalisedToDb(value, extremum) | sign;
      }
      else
      {
        simd_mask mask = simd_mask::equal(sign, kSignMask);
        result = merge(normalisedToDb(value, details.maxValue), 
          normalisedToDb(value, -details.minValue), mask) | sign;
      }
      break;
    case ParameterScale::Frequency:
      result = normalisedToFrequency(value, sampleRate);
      break;
    case ParameterScale::SymmetricFrequency:
      value = value * 2.0f - 1.0f;
      sign = unsignSimd(value);
      result = normalisedToFrequency(value, sampleRate) | sign;
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

namespace Plugin
{
  void ProcessorTree::registerDynamicParameter(Framework::ParameterValue *parameter)
  {
    auto details = parameter->getParameterDetails();

    if (details.scale != Framework::ParameterScale::Indexed || std::ranges::all_of(details.indexedData,
      [](const auto &data) { return data.dynamicUpdateUuid.empty(); }))
      return;

    COMPLEX_ASSERT(details.minValue == 0.0f);

    if (details.dynamicData->dataLookup.data() != details.indexedData.data())
    {
      details.dynamicData = utils::sp<Framework::IndexedData::DynamicData>::create();
      details.dynamicData->dataLookup.reserve(details.indexedData.size());
      std::ranges::copy(details.indexedData, details.dynamicData->dataLookup.begin());
      details.indexedData = details.dynamicData->dataLookup;

      parameter->setParameterDetails(details);
    }

    COMPLEX_ASSERT(details.dynamicData->dataLookup.size() == details.indexedData.size());

    // adding all possible reasons for update
    // and recalculating the new max value based on current state
    for (auto &data : details.dynamicData->dataLookup)
    {
      if (data.dynamicUpdateUuid.empty())
        continue;

      dynamicParameters_.emplace_back(&data, parameter);
    }
  }

  void ProcessorTree::updateDynamicParameters(std::string_view reason) const noexcept
  {
    using namespace Framework;

    for (auto [indexedData, currentParameter] : dynamicParameters_)
    {
      if (indexedData->dynamicUpdateUuid != reason)
        continue;

      auto *link = currentParameter->getParameterLink();
      // if the current parameter is mapped out, we shouldn't change any of the values
      // if some of them are not valid anymore, it's up to the consumers of said values to handle things properly
      if (link->hostControl)
        continue;

      u64 oldCount = indexedData->count;
      if (indexedData->dynamicUpdateUuid == Framework::kLaneCountChange)
        indexedData->count = getLaneCount();
      else if (indexedData->dynamicUpdateUuid == Framework::kInputSidechainCountChange)
        indexedData->count = getInputSidechains();
      else if (indexedData->dynamicUpdateUuid == Framework::kOutputSidechainCountChange)
        indexedData->count = getOutputSidechains();
      else COMPLEX_ASSERT_FALSE("Missing case");

      if (indexedData->count == oldCount)
        continue;

      // changing maxValue and setting normalised value to correspond to the current scaled value
      // for parameter and UIControl if it exists
      auto details = currentParameter->getParameterDetails();
      auto oldScaled = scaleValue((double)currentParameter->getNormalisedValue(), details);
      details.maxValue += (float)indexedData->count - (float)oldCount;
      auto newNormalised = unscaleValue(oldScaled, details);

      if (link->UIControl)
      {
        // intentionally inhibiting dynamic dispatch because it will try to update/redraw
        // which we intend to do with setValue
        link->UIControl->BaseControl::setParameterDetails(details);
        link->UIControl->setValue(newNormalised, juce::sendNotificationSync);
      }

      currentParameter->setParameterDetails(details, (float)newNormalised);
    }
  }
}
