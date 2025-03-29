/*
  ==============================================================================

    parameter_bridge.cpp
    Created: 29 Dec 2022 4:13:18am
    Author:  theuser27

  ==============================================================================
*/

#include "parameter_bridge.hpp"

#include <format>

#include "parameter_value.hpp"
#include "Interface/Components/BaseControl.hpp"
#include "Plugin/PluginProcessor.hpp"

namespace Framework
{
  ParameterBridge::ParameterBridge(Plugin::ComplexPlugin *plugin, u64 parameterIndex, 
    ParameterLink *link) noexcept : parameterIndex_(parameterIndex), plugin_(plugin)
  {
    // should be enough i think
    name_.second.preallocateBytes(64);
    name_.second = juce::String{ parameterIndex + 1 };

    if (link)
    {
      name_.second += " > ";
      parameterLinkPointer_.store(link, std::memory_order_release);
      auto name = link->parameter->getParameterDetails().displayName;
      name_.second += { name.data(), name.size() };
      link->hostControl = this;
      value_ = link->parameter->getNormalisedValue();
    }
  }

  ParameterBridge::~ParameterBridge() noexcept
  {
    if (auto link = parameterLinkPointer_.load(std::memory_order_acquire); link && link->parameter)
      link->parameter->changeBridge(nullptr);
  }

  void ParameterBridge::resetParameterLink(ParameterLink *link, bool getValueFromParameter) noexcept
  {
    auto *oldLink = parameterLinkPointer_.load(std::memory_order_acquire);
    if (link == oldLink)
      return;

    parameterLinkPointer_.store(link, std::memory_order_release);

    {
      utils::ScopedLock guard{ name_.first, utils::WaitMechanism::Spin };

      if (!link)
      {
        if (oldLink)
          oldLink->parameter->changeBridge(nullptr);
        name_.second = juce::String{ parameterIndex_ + 1 };
      }
      else
      {
        link->parameter->changeBridge(this);
        if (getValueFromParameter)
        {
          auto newValue = link->parameter->getNormalisedValue();
          value_.store(newValue, std::memory_order_release);
          sendValueChangedMessageToListeners(newValue);
        }
        else if (link->UIControl)
          link->UIControl->setValueFromHost(value_.load(std::memory_order_relaxed), this);

        auto name = link->parameter->getParameterDetails().displayName;
        juce::String newString{};
        newString.preallocateBytes(64);
        newString += juce::String{ parameterIndex_ + 1 };
        newString += " > ";
        newString += juce::String{ name.data(), name.size() };
        name_.second = COMPLEX_MOVE(newString);
      }
    }

    utils::as<ComplexAudioProcessor>(plugin_)->updateHostDisplay(
      juce::AudioProcessor::ChangeDetails{}.withParameterInfoChanged(true));

    // the unfortunate consequence that listeners might remove themselves in this callback
    std::vector<Listener *> listenersCopy = listeners_;
    for (auto *listener : listenersCopy)
      listener->parameterLinkReset(this, link, oldLink);
  }

  // this function is called on the message thread
  void ParameterBridge::updateUIParameter() noexcept
  {
    // for wasValueSet_ we only require atomicity, therefore memory_order_relaxed suffices
    bool dummy = true;
    if (wasValueSet_.compare_exchange_strong(dummy, false, std::memory_order_relaxed))
    {
      // for parameterLinkPointer_ it's fine to use relaxed because
      // this method is only called from the message thread
      // which is the only one that touches the UI
      auto link = parameterLinkPointer_.load(std::memory_order_relaxed);
      if (!link || !link->UIControl)
        return;

      bool shouldUpdate = link->UIControl->setValueFromHost(
        value_.load(std::memory_order_relaxed), this);

      if (shouldUpdate)
        link->UIControl->valueChanged();
    }
  }

  void ParameterBridge::setCustomName(const juce::String &name)
  {
    utils::ScopedLock guard{ name_.first, utils::WaitMechanism::Spin };
    name_.second = name;
  }

  void ParameterBridge::setValue(float newValue)
  {
    value_.store(newValue, std::memory_order_relaxed);
    wasValueSet_.store(true, std::memory_order_relaxed);
  }

  float ParameterBridge::getDefaultValue() const
  {
    if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
      return std::clamp(pointer->parameter->getParameterDetails().defaultNormalisedValue, 0.0f, 1.0f);
    return kDefaultParameterValue;
  }

  juce::String ParameterBridge::getName() const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<std::atomic<bool>&>(name_.first), utils::WaitMechanism::Spin };

    if (parameterLinkPointer_.load(std::memory_order_acquire))
      return name_.second;

    return name_.second.trim();
  }

  juce::String ParameterBridge::getName(int maximumStringLength) const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<std::atomic<bool>&>(name_.first), utils::WaitMechanism::Spin };

    if (parameterLinkPointer_.load(std::memory_order_acquire))
      return name_.second.substring(0, maximumStringLength);

    auto index = name_.second.indexOfChar(0, ' ');
    return name_.second.substring(0, (index > maximumStringLength) ? maximumStringLength : index);
  }

  juce::String ParameterBridge::getLabel() const
  {
    if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
      return pointer->parameter->getParameterDetails().displayUnits.data();
    return "";
  }

  juce::String ParameterBridge::getText(float value, int maximumStringLength) const
  {
    static constexpr auto kMaxDecimals = 6;
    
    auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
    if (!pointer)
      return { value, std::min(maximumStringLength, kMaxDecimals) };

    auto details = pointer->parameter->getParameterDetails();
    auto sampleRate = plugin_->getSampleRate();
    double internalValue = scaleValue(value, details, sampleRate, true);
    if (!details.indexedData.empty())
    {
      auto [indexedData, index] = getIndexedData(internalValue, details);
      std::string string;
      if (indexedData->count > 1)
        string = std::format("{} {}", indexedData->displayName.data(), index + 1);
      else
        string = std::format("{}", indexedData->displayName.data());

      return string.substr(0, (usize)maximumStringLength);
    }

    return { internalValue, std::min(maximumStringLength, kMaxDecimals) };
  }

  float ParameterBridge::getValueForText(const juce::String &text) const
  {
    if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
      return (float)unscaleValue(text.getFloatValue(), pointer->parameter->getParameterDetails(), true);

    return text.getFloatValue();
  }

  int ParameterBridge::getNumSteps() const
  {
    if (!isDiscrete())
      return AudioProcessorParameter::getNumSteps();

    auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
    return (int)pointer->parameter->getParameterDetails().maxValue - 
      (int)pointer->parameter->getParameterDetails().minValue + 1;
  }

  bool ParameterBridge::isDiscrete() const
  {
    if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Indexed ||
        pointer->parameter->getScale() == ParameterScale::IndexedNumeric;
    return false;
  }

  bool ParameterBridge::isBoolean() const
  {
    if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Toggle;
    return false;
  }
}
