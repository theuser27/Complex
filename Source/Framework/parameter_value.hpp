/*
  ==============================================================================

    parameter_value.hpp
    Created: 11 Jul 2022 3:22:48pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <memory>
#include <vector>

#include "utils.hpp"
#include "sync_primitives.hpp"
#include "simd_utils.hpp"
#include "parameters.hpp"

namespace Plugin
{
  class ProcessorTree;
}

namespace Interface
{
  class BaseControl;
}

namespace Framework
{
  template<typename T>
  concept ParameterRepresentation = std::is_same_v<T, float> || std::is_same_v<T, u32>
    || SimdValue<T> || std::is_same_v<T, Framework::IndexedData>;

  struct atomic_simd_float
  {
    atomic_simd_float(simd_float value) : value(value) { }

    simd_float load() const noexcept
    {
      utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
      simd_float currentValue = value;
      return currentValue;
    }

    void store(simd_float newValue) noexcept
    {
      utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
      value = newValue;
    }

    simd_float add(const simd_float &other) noexcept
    {
      utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
      value += other;
      simd_float result = value;
      return result;
    }

  private:
    simd_float value = 0.0f;
    mutable std::atomic<bool> guard = false;
  };

  class ParameterBridge;
  class ParameterValue;

  class ParameterModulator
  {
  public:
    ParameterModulator() = default;
    virtual ~ParameterModulator() = default;

    // deltaValue is the difference between the current and previous values
    virtual simd_float getDeltaValue() { return currentValue_.load() - previousValue_.load(); }

  protected:
    atomic_simd_float currentValue_{ 0.0f };
    atomic_simd_float previousValue_{ 0.0f };
  };

  struct ParameterLink
  {
    // the lifetime of the UIControl and parameter are the same, so there's no danger of accessing freed memory
    // as for the hostControl, in the destructor of the UI element, tied to the BaseProcessor,
    // we reset it's pointer to the ParameterLink and so it cannot access the parameter/UIControl
    Interface::BaseControl *UIControl = nullptr;
    ParameterBridge *hostControl = nullptr;
    std::vector<std::weak_ptr<ParameterModulator>> modulators{};
    ParameterValue *parameter = nullptr;
  };

  class ParameterValue
  {
    ParameterValue() = delete;
  public:
    ParameterValue(ParameterValue &&) = delete;
    ParameterValue &operator=(const ParameterValue &) = delete;
    ParameterValue &operator=(ParameterValue &&) = delete;

    ParameterValue(ParameterDetails details) noexcept :
      details_(COMPLEX_MOVE(details)) { initialise(); }

    ParameterValue(const ParameterValue &other) noexcept : 
      details_(other.details_)
    {
      utils::ScopedLock g{ other.waitLock_, utils::WaitMechanism::Spin };
      normalisedValue_ = other.normalisedValue_;
      modulations_ = other.modulations_;
      normalisedInternalValue_ = other.normalisedInternalValue_;
      internalValue_ = other.internalValue_;
    }

    ~ParameterValue() noexcept;

    void initialise(std::optional<float> value = {})
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      normalisedValue_ = value.value_or(details_.defaultNormalisedValue);
      modulations_ = 0.0f;
      normalisedInternalValue_ = value.value_or(details_.defaultNormalisedValue);
      internalValue_ = (!value.has_value()) ? details_.defaultValue :
        (float)scaleValue(value.value(), details_);
      isDirty_ = false;
    }
    
    // prefer calling this only once if possible
    template<ParameterRepresentation T>
    auto getInternalValue(float sampleRate = kDefaultSampleRate, bool isNormalised = false) const noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      
      [[maybe_unused]] T result;

      if constexpr (std::is_same_v<T, simd_float>)
      {
        COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::IndexedNumeric && "Parameter isn't supposed to be a choice control");

        if (isNormalised)
          result = normalisedInternalValue_;
        else
          result = internalValue_;
        return result;
      }
      else if constexpr (std::is_same_v<T, float>)
      {
        COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");
        COMPLEX_ASSERT(details_.scale != ParameterScale::IndexedNumeric && "Parameter isn't supposed to be a choice control");

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
      else if constexpr (std::is_same_v<T, simd_int>)
      {
        COMPLEX_ASSERT(details_.scale == ParameterScale::Toggle || details_.scale == ParameterScale::Indexed || 
          details_.scale == ParameterScale::IndexedNumeric && 
          "Parameter is supposed to be either a toggle or choice control");

        if (details_.scale == ParameterScale::Toggle)
          result = utils::reinterpretToInt(internalValue_);
        else
          result = utils::toInt(internalValue_);
        return result;
      }
      else if constexpr (std::is_same_v<T, u32>)
      {
        COMPLEX_ASSERT(details_.scale == ParameterScale::Toggle || details_.scale == ParameterScale::Indexed || 
          details_.scale == ParameterScale::IndexedNumeric && 
          "Parameter is supposed to be either a toggle or choice control");

        if (details_.scale == ParameterScale::Toggle)
          result = static_cast<u32>(utils::reinterpretToInt(internalValue_)[0]);
        else if (details_.flags & ParameterDetails::Stereo)
        {
          simd_int difference = utils::getStereoDifference(utils::toInt(modulations_));
          result = static_cast<u32>(scaleValue(modulations_ - utils::toFloat(difference), details_, sampleRate)[0]);
        }
        else
          result = utils::toInt(internalValue_)[0];
        return result;
      }
      else if constexpr (std::is_same_v<T, Framework::IndexedData>)
      {
        COMPLEX_ASSERT(details_.scale == ParameterScale::Indexed &&
          "Parameter must be indexed to support value to string conversion");
        COMPLEX_ASSERT(details_.minValue >= 0.0f && (usize)details_.maxValue <= details_.indexedData.size());
        COMPLEX_ASSERT((details_.flags & ParameterDetails::Stereo) == 0 && 
          "Indexed types that support value to string conversion must not be stereo");

        return getIndexedData(internalValue_[0], details_);
      }
      else
      {
        COMPLEX_ASSERT_FALSE("Unknown type provided");
        return result;
      }
    }

    auto changeControl(Interface::BaseControl *control) noexcept -> Interface::BaseControl *
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto oldControl = parameterLink_.UIControl;
      parameterLink_.UIControl = control;
      return oldControl;
    }

    auto changeBridge(ParameterBridge *bridge) noexcept -> ParameterBridge *
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      auto oldBridge = parameterLink_.hostControl;
      parameterLink_.hostControl = bridge;
      return oldBridge;
    }

    void addModulator(std::weak_ptr<ParameterModulator> modulator, i64 index = -1)
    {
      COMPLEX_ASSERT(!modulator.expired() && "You're trying to add an empty modulator to parameter");

      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      if (index < 0) parameterLink_.modulators.emplace_back(COMPLEX_MOVE(modulator));
      else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, COMPLEX_MOVE(modulator));

      isDirty_ = true;
    }

    auto updateModulator(std::weak_ptr<ParameterModulator> modulator, usize index)
      -> std::weak_ptr<ParameterModulator>
    {
      COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      std::weak_ptr replacedModulator = parameterLink_.modulators[index];
      parameterLink_.modulators[index] = COMPLEX_MOVE(modulator);
      
      isDirty_ = true;

      return replacedModulator;
    }

    auto deleteModulator(usize index) -> std::weak_ptr<ParameterModulator>
    {
      COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

      std::weak_ptr deletedModulator = parameterLink_.modulators[index];
      parameterLink_.modulators.erase(parameterLink_.modulators.begin() + (std::ptrdiff_t)index);

      isDirty_ = true;

      return deletedModulator;
    }

    // this method will only update the values of the parameter, parameter bridge and ui control
    // it will NOT notify the latter about the change, so any GUI changes must be forced
    void updateValue(float sampleRate);
    void updateNormalisedValue(std::optional<float> value = {}) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      if (value.has_value())
        normalisedValue_ = value.value();
      isDirty_ = true;
    }

    auto getNormalisedValue() const noexcept -> float
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return normalisedValue_;
    }
    auto getParameterDetails() const noexcept -> ParameterDetails
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_;
    }
    auto getParameterId() const noexcept -> utils::string_view
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.id;
    }
    auto getParameterName() const noexcept -> utils::string_view
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.displayName;
    }
    auto getScale() const noexcept -> Framework::ParameterScale
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.scale;
    }
    auto getUpdateFlag() const noexcept -> UpdateFlag
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      return details_.updateFlag;
    }
    auto getParameterLink() noexcept -> ParameterLink * { return &parameterLink_; }
    auto getThemeColour() const noexcept -> u32 { return themeColour_; }

    void setParameterDetails(const ParameterDetails &details, std::optional<float> value = {}) noexcept
    {
      utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
      details_ = details;
      if (value.has_value())
        normalisedValue_ = value.value();
      isDirty_ = true;
    }
    void setThemeColour(u32 colour) noexcept { themeColour_ = colour; }

    void serialiseToJson(void *jsonData) const;
    static auto deserialiseFromJson(Plugin::ProcessorTree *processorTree, void *jsonData)
      -> utils::up<ParameterValue>;

  private:
    // after adding modulations and scaling
    simd_float internalValue_ = 0.0f;
    // after adding modulations
    simd_float normalisedInternalValue_ = 0.0f;
    // value of all internal modulations
    simd_float modulations_ = 0.0f;
    // normalised, received from gui changes or from host when mapped out
    float normalisedValue_ = 0.0f;

    ParameterLink parameterLink_{ nullptr, nullptr, {}, this };

    ParameterDetails details_;

    u32 themeColour_ = 0;
    mutable std::atomic<bool> waitLock_ = false;
    bool isDirty_ = false;
  };
}
