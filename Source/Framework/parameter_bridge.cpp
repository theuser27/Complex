/*
  ==============================================================================

    parameter_bridge.cpp
    Created: 29 Dec 2022 04:13:18
    Author:  theuser27

  ==============================================================================
*/

#include "parameter_bridge.hpp"

#include "parameter_value.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/Components/BaseControl.hpp"
#include "Plugin/Renderer.hpp"

namespace Framework
{
  ParameterBridge::ParameterBridge(Plugin::State *state, u64 parameterIndex,
    ParameterLink *link) noexcept : parameterIndex{ parameterIndex }, state{ state }
  {
    if (!link)
    {
      name_.second = utils::string::create(state->miscStorage, "%zu", parameterIndex + 1);
      return;
    }

    parameterLinkPointer_.store(link, satomi::memory_order_release);
    auto name = link->parameter->getParameterDetails().displayName;
    name_.second = utils::string::create(state->miscStorage, "%zu > %s", parameterIndex + 1, name.data());
    link->hostControl = this;
    value_.store(link->parameter->getNormalisedValue());
  }

  ParameterBridge::~ParameterBridge() noexcept
  {
    if (auto link = parameterLinkPointer_.load(satomi::memory_order_acquire); link && link->parameter)
      link->parameter->changeBridge(nullptr);
  }

  void ParameterBridge::resetParameterLink(ParameterLink *link, bool getValueFromParameter) noexcept
  {
    auto *oldLink = parameterLinkPointer_.load(satomi::memory_order_acquire);
    if (link == oldLink)
      return;

    parameterLinkPointer_.store(link, satomi::memory_order_release);

    {
      utils::ScopedLock guard{ name_.first, utils::WaitMechanism::Spin };

      if (!link)
      {
        if (oldLink)
          oldLink->parameter->changeBridge(nullptr);
        name_.second = utils::string::create(state->miscStorage, "%zu", parameterIndex + 1);
      }
      else
      {
        link->parameter->changeBridge(this);
        if (getValueFromParameter)
        {
          auto newValue = link->parameter->getNormalisedValue();
          value_.store(newValue, satomi::memory_order_release);
        }
        else if (link->UIControl)
          link->UIControl->setValueFromHost(value_.load(satomi::memory_order_relaxed), this);

        auto name = link->parameter->getParameterDetails().displayName;
        name_.second = utils::string::create(state->miscStorage, "%zu > %s", parameterIndex + 1, name.data());
      }
    }

    // the unfortunate consequence that listeners might remove themselves in this callback
    utils::vector<Listener *> listenersCopy = listeners_.clone();
    for (auto *listener : listenersCopy)
      listener->parameterLinkReset(this, link, oldLink);
  }



  // this function is called on the UI thread
  void ParameterBridge::updateUIParameter() noexcept
  {
    // for wasValueSet_ we only require atomicity, therefore memory_order_relaxed suffices
    bool dummy = true;
    if (wasValueSet_.compare_exchange_strong(dummy, false, satomi::memory_order_relaxed))
    {
      // for parameterLinkPointer_ it's fine to use relaxed because
      // this method is only called from the UI thread
      // which is the only one that touches the UI
      auto link = parameterLinkPointer_.load(satomi::memory_order_relaxed);
      if (!link || !link->UIControl)
        return;

      bool shouldUpdate = link->UIControl->setValueFromHost(
        value_.load(satomi::memory_order_relaxed), this);

      if (shouldUpdate)
        link->UIControl->valueChanged();
    }
  }

  void ParameterBridge::setCustomName(utils::string name)
  {
    utils::ScopedLock guard{ name_.first, utils::WaitMechanism::Spin };
    name_.second = COMPLEX_MOVE(name);
  }

  float ParameterBridge::getDefaultValue() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return utils::clamp(pointer->parameter->getParameterDetails().defaultNormalisedValue, 0.0f, 1.0f);
    return kDefaultParameterValue;
  }

  utils::string ParameterBridge::getName() const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<satomi::atomic<bool> &>(name_.first), utils::WaitMechanism::Spin };

    if (parameterLinkPointer_.load(satomi::memory_order_acquire))
      return name_.second.clone();

    auto index = name_.second.find(" ");
    return name_.second.clone(0, index);
  }

  utils::string ParameterBridge::getName(int maximumStringLength) const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<satomi::atomic<bool> &>(name_.first), utils::WaitMechanism::Spin };

    if (parameterLinkPointer_.load(satomi::memory_order_acquire))
      return name_.second.clone(0, maximumStringLength);

    auto index = name_.second.find(" ");
    return name_.second.clone(0, (index > maximumStringLength) ? maximumStringLength : index);
  }

  void ParameterBridge::getName(char *buffer, usize maximumStringLength) const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<satomi::atomic<bool> &>(name_.first), utils::WaitMechanism::Spin };

    usize size = utils::min(name_.second.size(), maximumStringLength - 1);
    ::memcpy(buffer, name_.second.data(), size);
    buffer[size] = '\0';
  }

  utils::string ParameterBridge::getLabel() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return utils::string{ state->miscStorage, pointer->parameter->getParameterDetails().displayUnits };
    return {};
  }

  void ParameterBridge::getText(float value, char *buffer, usize maximumStringLength) const
  {
    static constexpr auto kMaxDecimals = 6;
    
    auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire);
    double internalValue;
    if (!pointer)
      internalValue = value;
    else
    {
      auto details = pointer->parameter->getParameterDetails();
      auto sampleRate = state->plugin->getSampleRate();
      internalValue = scaleValue(value, details, sampleRate, true);
      if (details.scale == ParameterScale::Indexed)
      {
        auto [indexedData, index] = getIndexedData(internalValue, details);

        if (indexedData->count > 1)
        {
          ::stbsp_snprintf(buffer, (int)maximumStringLength, "%.*s %zu",
            (int)indexedData->displayName.size(), indexedData->displayName.data(), index + 1);
        }
        else
        {
          ::stbsp_snprintf(buffer, (int)maximumStringLength, "%.*s",
            (int)indexedData->displayName.size(), indexedData->displayName.data());
        }

        return;
      }
    }

    utils::floatToString(internalValue, buffer, maximumStringLength);
  }

  float ParameterBridge::getValueForText(utils::string_view text) const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return (float)unscaleValue(::strtod(text.data(), nullptr), pointer->parameter->getParameterDetails(), true);

    return ::strtof(text.data(), nullptr);
  }

  int ParameterBridge::getNumSteps() const
  {
    auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire);
    auto details = pointer->parameter->getParameterDetails();
    if (details.scale == ParameterScale::Indexed)
      return (int)details.options->count;

    return 0x7fffffff;
  }

  bool ParameterBridge::isDiscrete() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Indexed;
    return false;
  }

  bool ParameterBridge::isBoolean() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Toggle;
    return false;
  }
}
