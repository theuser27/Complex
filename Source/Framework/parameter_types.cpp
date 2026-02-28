
// Created: 2023-09-04 21:38:38

#include "utils.hpp"
#include "simd_utils.hpp"
#include "parameter_value.hpp"
#include "parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/Components/BaseControl.hpp"

namespace Framework
{
  utils::pair<const IndexedData *, usize>
  getIndexedData(double scaledValue, const ParameterDetails &details) noexcept
  {
    COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);

    usize i = (usize)scaledValue;
    IndexedData *nextIndexedData = details.options, *indexedData = nullptr;

    // if we have an ignore function we need to check all items
    while (nextIndexedData)
    {
      indexedData = nextIndexedData;
      // move to next option if we've run out 
      if (indexedData->count <= i)
      {
        i -= indexedData->count;
        if (indexedData->next)
          nextIndexedData = indexedData->next;
        else if (indexedData->parent)
          nextIndexedData = indexedData->parent->next;
      }
      else
        nextIndexedData = indexedData->children;
    }

    return { indexedData, i };
  }

  double getValueFromOptionText(utils::string_view text, const ParameterDetails &details) noexcept
  {
    bool exitedChild = false;
    auto *option = details.options;
    u32 size = option->count;
    u32 index = 0;
    u32 value = 0;

    while (true)
    {
      // going down
      if (!exitedChild && option->children && option->count)
      {
        option = option->children;
        size = option->count;
        index = 0;
        continue;
      }

      if (option->displayName.compareCaseInsensitive(text) == 0)
        return (double)value;
      
      // going forward
      if (index < size)
      {
        ++value;
        ++index;
        option = option->next;
        continue;
      }

      // going up
      exitedChild = true;
      option = option->parent;
      size = option->parent->count;

      // find the index of the parent again
      index = 0;
      for (auto *child = option; index < size; ++index)
        if (child == option)
          break;
    }

    return (double)(value + 1);
  }

  double getValueFromOptionId(uuid optionId, const ParameterDetails &details) noexcept
  {
    COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);

    double value{};

    IndexedData *current = details.options->children;
    bool visitedChildren = false;
    while (current->id != optionId)
    {
      if (current->children && !visitedChildren)
      {
        current = current->children;
      }
      else if (current->next)
      {
        visitedChildren = false;
        current = current->next;

        if (!current->children)
          ++value;
      }
      else
      {
        visitedChildren = true;
        if (!current->parent)
        {
          COMPLEX_ASSERT_FALSE("Couldn't find option with id: %zu", optionId);
          break;
        }
        current = current->parent;
      }
    }

    return value;
  }

  double
  scaleValue(double value, const ParameterDetails &details, 
    float sampleRate, bool scalePercent, bool skewOnly) noexcept
  {
    using namespace utils;

    double result{};
    double sign;
    switch (details.scale)
    {
    case ParameterScale::Toggle:
      result = ::round(value);
      break;
    case ParameterScale::Indexed:
      result = (!skewOnly) ? ::round(value * (details.options->count - 1)) : value;
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
    case ParameterScale::ReverseQuadratic:
      result = value - 1.0;
      result = (!skewOnly) ? details.maxValue - result * result * (details.maxValue - details.minValue): 1.0 - (result * result);
      break;
    case ParameterScale::SymmetricQuadratic:
      value = value * 2.0 - 1.0;
      sign = unsignFloat(value);
      value *= value;
      result = (!skewOnly) ? (value * 0.5 * sign + 0.5) * (details.maxValue - details.minValue) + details.minValue : value * 0.5 * sign + 0.5;
      break;
    case ParameterScale::Cubic:
      result = (!skewOnly) ? value * value * value * (details.maxValue - details.minValue) + details.minValue : value * value * value;
      break;
    case ParameterScale::ReverseCubic:
      result = value - 1.0;
      result = (!skewOnly) ? result * result * result * (details.maxValue - details.minValue) + details.maxValue : (result * result * result) + 1.0;
      break;
    case ParameterScale::SymmetricCubic:
      value = 2.0 * value - 1.0;
      value *= value * value;
      result = (!skewOnly) ? (value * 0.5 + 0.5) * (details.maxValue - details.minValue) + details.minValue : value;
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
      break;
    }

    if (details.displayUnits == "%" && scalePercent)
      result *= 100.0;

    return result;
  }

  double
  unscaleValue(double value, const ParameterDetails &details, 
    float sampleRate, bool unscalePercent) noexcept
  {
    using namespace utils;

    if (details.displayUnits == "%" && unscalePercent)
      value *= 0.01f;

    double result{};
    double sign;
    switch (details.scale)
    {
    case ParameterScale::Toggle:
      result = ::round(value);
      break;
    case ParameterScale::Indexed:
      result = value / details.options->count;
      break;
    case ParameterScale::Clamp:
      result = value;
      break;
    case ParameterScale::Linear:
      result = (value - details.minValue) / (details.maxValue - details.minValue);
      break;
    case ParameterScale::Quadratic:
      result = ::sqrt((value - details.minValue) / (details.maxValue - details.minValue));
      break;
    case ParameterScale::ReverseQuadratic:
      value = -(value - details.maxValue) / (details.maxValue - details.minValue);
      result = -::sqrt(value) + 1.0;
      break;
    case ParameterScale::SymmetricQuadratic:
      value = 2.0 * ((value - details.minValue) / (details.maxValue - details.minValue) - 0.5);
      sign = unsignFloat(value);
      result = (::sqrt(value) * sign + 1.0) * 0.5;
      break;
    case ParameterScale::Cubic:
      value = (value - details.minValue) / (details.maxValue - details.minValue);
      result = ::pow(value, 1.0 / 3.0);
      break;
    case ParameterScale::ReverseCubic:
      value = (value - details.maxValue) / (details.maxValue - details.minValue);
      result = ::pow(value, 1.0 / 3.0) + 1.0;
      break;
    case ParameterScale::SymmetricCubic:
      value = 2.0 * (value - details.minValue) / (details.maxValue - details.minValue) - 1.0;
      result = (::pow(value, 1.0 / 3.0) + 1.0) * 0.5;
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
      break;
    }

    return result;
  }

  simd_float
  scaleValue(simd_float value, const ParameterDetails &details, float sampleRate) noexcept
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
      result = simd_float::round(value * (float)details.options->count);
      break;
    case ParameterScale::Linear:
      result = value * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::Quadratic:
      result = value * value * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::ReverseQuadratic:
      result = value - 1.0;
      result = details.maxValue - result * result * (details.maxValue - details.minValue);
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
    case ParameterScale::ReverseCubic:
      result = value - 1.0f;
      result = result * result * result * (details.maxValue - details.minValue) + details.maxValue;
      break;
    case ParameterScale::SymmetricCubic:
      value = simd_float::mulSub(1.0f, value, 2.0f);
      value *= value * value;
      result = simd_float::mulAdd(0.5f, value, 0.5f) * (details.maxValue - details.minValue) + details.minValue;
      break;
    case ParameterScale::Loudness:
      result = normalisedToDb(value, details.maxValue);
      break;
    case ParameterScale::SymmetricLoudness:
      value = (value * 2.0f - 1.0f);
      sign = unsignSimd(value);
      if (sign.allSame())
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
      break;
    }

    return result;
  }

  void ParameterValue::updateValue(float sampleRate)
  {
    utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

    bool isDirty = isDirty_;
    float newNormalisedValue = normalisedValue_;

    // if there's a set hostControl set, then we're automating this parameter
    if (parameterLink_.hostControl)
      newNormalisedValue = parameterLink_.hostControl->getValue();
    else if (parameterLink_.UIControl)
      newNormalisedValue = (float)parameterLink_.UIControl->getValue();

    isDirty = isDirty || normalisedValue_ != newNormalisedValue;
    normalisedValue_ = newNormalisedValue;

    simd_float newModulations = modulations_;
    for (auto &modulator : parameterLink_.modulators)
    {
      // only getting the change from the previous used value
      newModulations += modulator->getDeltaValue();
    }

    if (isDirty || modulations_ != newModulations)
    {
      modulations_ = newModulations;
      normalisedInternalValue_ = simd_float::clamp(newModulations + newNormalisedValue, 0.0f, 1.0f);
      internalValue_ = scaleValue(normalisedInternalValue_, details_, sampleRate);
    }

    isDirty_ = false;
  }
}

namespace Plugin
{
  void State::registerDynamicParameter(Framework::ParameterValue *parameter)
  {
    auto details = parameter->getParameterDetails();
    
    if (details.scale != Framework::ParameterScale::Indexed)
      return;
    
    COMPLEX_ASSERT(details.options != nullptr);

    // adding all possible reasons for update
    // and recalculating the new max value based on current state
    Framework::IndexedData::visit(details.options, [&](Framework::IndexedData &data)
      {
        if (data.dynamicUpdateUuid == uuid{})
          return;

        dynamicParameters.emplace_back(&data, parameter);
      });
  }

  void State::updateDynamicParameters(uuid reason) noexcept
  {
    using namespace Framework;

    utils::ScopedLock guard{};
    if (plugin->state_.get() == this)
      guard = plugin->acquireProcessingLock(true);

    for (auto [indexedData, currentParameter] : dynamicParameters)
    {
      if (indexedData->dynamicUpdateUuid != reason)
        continue;

      auto *link = currentParameter->getParameterLink();
      // if the current parameter is mapped out, we shouldn't change any of the values
      // if some of them are not valid anymore, it's up to the consumers of said values to handle things properly
      if (link->hostControl)
        continue;

      u32 newCount = indexedData->count;
      switch (indexedData->dynamicUpdateUuid)
      {
      case ParameterChangeReason::inputSidechain:
        newCount = plugin->inSidechains;
        break;
      case ParameterChangeReason::outputSidechain:
        newCount = plugin->outSidechains;
        break;
      case ParameterChangeReason::laneCount:
        newCount = getLaneCount();
        break;
      default:
        COMPLEX_ASSERT_FALSE("Missing case");
        break;
      }

      if (indexedData->count == newCount)
        continue;

      // changing maxValue and setting normalised value to correspond to the current scaled value
      // for parameter and UIControl if it exists
      auto details = currentParameter->getParameterDetails();
      auto oldScaled = scaleValue((double)currentParameter->getNormalisedValue(), details);
      // TODO: find which indexed element this scaled value belongs to
      //       if it's the same one currently being changed, then clamp
      details.maxValue += (float)newCount - (float)indexedData->count;
      indexedData->count = newCount;
      auto newNormalised = unscaleValue(oldScaled, details);

      if (link->UIControl)
      {
        // intentionally inhibiting dynamic dispatch because it will try to update/redraw
        // which we intend to do with setValue
        link->UIControl->details = details;
        link->UIControl->setValue(newNormalised, true);
      }

      float value = (float)newNormalised;
      currentParameter->setParameterDetails(details, &value);
    }
  }
}
