
// Created: 2022-07-11 15:22:48

#pragma once

#include "utils.hpp"
#include "memory.hpp"
#include "sync_primitives.hpp"
#include "simd_utils.hpp"
#include "parameter_types.hpp"

namespace Plugin
{
  struct State;
}

namespace Interface
{
  class Control;
}

namespace Framework
{
  template<typename T>
  concept ParameterRepresentation = utils::is_same_v<T, float> || utils::is_same_v<T, u32>
    || utils::SimdValue<T> || utils::is_same_v<T, Framework::IndexedData>;

  class ParameterBridge;
  class ParameterValue;

  class ParameterModulator
  {
  public:
    // deltaValue is the difference between the current and previous values
    simd_float getDeltaValue()
    {
      return currentValue.load(satomi::memory_order_relaxed) -
        previousValue.load(satomi::memory_order_relaxed);
    }

    satomi::atomic<simd_float> currentValue{ 0.0f };
    satomi::atomic<simd_float> previousValue{ 0.0f };
  };

  struct ParameterLink
  {
    // the lifetime of the UIControl and parameter are the same, so there's no danger of accessing freed memory
    // as for the hostControl, in the destructor of the UI element, tied to the BaseProcessor,
    // we reset its pointer to the ParameterLink and so it cannot access the parameter/UIControl
    Interface::Control *UIControl = nullptr;
    ParameterBridge *hostControl = nullptr;
    utils::vector<ParameterModulator *> modulators{};
    ParameterValue *parameter = nullptr;
  };

  class ParameterValue
  {
  public:
    ParameterValue(ParameterValue &&) = delete;
    ParameterValue &operator=(const ParameterValue &) = delete;
    ParameterValue &operator=(ParameterValue &&) = delete;

    ParameterValue() = default;

    ParameterValue(ParameterDetails details) noexcept :
      details_(COMPLEX_MOVE(details)) { reset(); }

    ParameterValue(const ParameterValue &other) noexcept : 
      details_(other.details_)
    {
      utils::ScopedLock g{ other.waitLock_, utils::WaitMechanism::Spin };
      reset(&other.normalisedValue_);
    }

    void reset(const float *value = nullptr, float sampleRate = kDefaultSampleRate)
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      if (!value && details_.scale == ParameterScale::Indexed)
      {
        internalValue_ = (float)getValueFromOptionId(details_.defaultOptionId, details_);
        normalisedValue_ = (float)unscaleValue(internalValue_[0], details_, sampleRate);
      }
      else
      {
        normalisedValue_ = (value) ? *value : details_.defaultNormalisedValue;
        internalValue_ = (!value) ? details_.defaultValue : (float)scaleValue(*value, details_, sampleRate);
      }
      modulations_ = 0.0f;
      normalisedInternalValue_ = normalisedValue_;

      isDirty_ = false;
    }
    
    // prefer calling this only once if possible
    template<ParameterRepresentation T>
    auto getInternalValue(float sampleRate = kDefaultSampleRate, bool isNormalised = false) const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      
      [[maybe_unused]] T result;

      if constexpr (utils::is_same_v<T, simd_float>)
      {
        COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle, "Parameter isn't supposed to be a toggle control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed, "Parameter isn't supposed to be a choice control");

        if (isNormalised)
          result = normalisedInternalValue_;
        else
          result = internalValue_;
        return result;
      }
      else if constexpr (utils::is_same_v<T, float>)
      {
        COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle, "Parameter isn't supposed to be a toggle control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed, "Parameter isn't supposed to be a choice control");

        if (details_.flags & ParameterDetails::Stereo)
        {
          simd_float modulations = modulations_;
          simd_float difference = utils::getStereoDifference(modulations);
          if (isNormalised)
            result = (modulations - difference)[0];
          else
            result = scaleValue(modulations - difference, details_, sampleRate)[0];
        }
        else
        {
          if (isNormalised)
            result = normalisedInternalValue_[0];
          else
            result = internalValue_[0];
          return result;
        }

        return result;
      }
      else if constexpr (utils::is_same_v<T, simd_int>)
      {
        return utils::toInt(simd_float::round(internalValue_));
      }
      else if constexpr (utils::is_same_v<T, u32>)
      {
        if (details_.scale == ParameterScale::Toggle)
          result = (u32)(simd_float::round(internalValue_)[0]);
        else if (details_.flags & ParameterDetails::Stereo)
        {
          simd_int difference = utils::getStereoDifference(utils::toInt(modulations_));
          result = (u32)(simd_float::round(scaleValue(modulations_ - utils::toFloat(difference), details_, sampleRate))[0]);
        }
        else
          result = (u32)(simd_float::round(internalValue_)[0]);
        return result;
      }
      else if constexpr (utils::is_same_v<T, Framework::IndexedData>)
      {
        COMPLEX_ASSERT(details_.scale == ParameterScale::Indexed,
          "Parameter must be indexed to support value to string conversion");
        COMPLEX_ASSERT((details_.flags & ParameterDetails::Stereo) == 0,
          "Indexed types that support value to string conversion must not be stereo");

        return getIndexedData(internalValue_[0], details_);
      }
      else
      {
        COMPLEX_HARD_ASSERT_FALSE("Unknown type provided");
        return result;
      }
    }

    Interface::Control *
    changeControl(Interface::Control *control) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto oldControl = parameterLink_.UIControl;
      parameterLink_.UIControl = control;
      return oldControl;
    }

    ParameterBridge *
    changeBridge(ParameterBridge *bridge) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto oldBridge = parameterLink_.hostControl;
      parameterLink_.hostControl = bridge;
      return oldBridge;
    }

    void addModulator(ParameterModulator &modulator, isize index = -1)
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      if (index < 0) parameterLink_.modulators.emplaceBack(&modulator);
      else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, &modulator);

      isDirty_ = true;
    }

    ParameterModulator &
    updateModulator(ParameterModulator &modulator, usize index)
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto &replacedModulator = *parameterLink_.modulators[index];
      parameterLink_.modulators[index] = &modulator;
      
      isDirty_ = true;

      return replacedModulator;
    }

    ParameterModulator &
    deleteModulator(usize index)
    {
      COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto &deletedModulator = *parameterLink_.modulators[index];
      parameterLink_.modulators.erase(parameterLink_.modulators.begin() + (isize)index);

      isDirty_ = true;

      return deletedModulator;
    }

    // this method will only update the values of the parameter, parameter bridge and ui control
    // it will NOT notify the latter about the change, so any GUI changes must be forced
    void updateValue(float sampleRate);
    void updateNormalisedValue(float *value = nullptr) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      if (value)
        normalisedValue_ = *value;
      isDirty_ = true;
    }

    float
    getNormalisedValue() const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return normalisedValue_;
    }
    ParameterDetails
    getParameterDetails() const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_;
    }
    uuid // id can't change, so there's no reason to acquire a lock
    getParameterId() const noexcept { return details_.id; }
    utils::string_view
    getParameterName() const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.displayName;
    }
    Framework::ParameterScale::Value
    getScale() const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.scale;
    }
    UpdateFlag
    getUpdateFlag() const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.updateFlag;
    }
    ParameterLink *
    getParameterLink() noexcept { return &parameterLink_; }

    void setParameterDetails(const ParameterDetails &details, float *value = nullptr) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      details_ = details;
      if (value)
        normalisedValue_ = *value;
      isDirty_ = true;
    }

    void serialiseToJson(void *jsonData) const;
    static utils::dll<ParameterValue> *
    deserialiseFromJson(Generation::BaseProcessor *processor, void *jsonData, 
      ParameterDetails &reference, utils::dll<ParameterValue> *memory = nullptr);

  private:
    // after adding modulations and scaling
    simd_float internalValue_ = 0.0f;
    // after adding modulations
    simd_float normalisedInternalValue_ = 0.0f;
    // value of all internal modulations
    simd_float modulations_ = 0.0f;
    // normalised, received from gui changes or from host when mapped out
    float normalisedValue_ = 0.0f;

    mutable satomi::atomic<bool> waitLock_ = false;
    bool isDirty_ = false;

  public:
    Generation::BaseProcessor *parentProcessor{};
  private:

    ParameterLink parameterLink_{ .parameter = this };

    ParameterDetails details_{};
  };
}
