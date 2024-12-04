/*
  ==============================================================================

    parameter_bridge.hpp
    Created: 28 Jul 2022 4:21:59am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "platform_definitions.hpp"

namespace Plugin
{
  class ComplexPlugin;
}

namespace Framework
{
  struct ParameterLink;

  class ParameterBridge final : public juce::AudioProcessorParameter
  {
  public:
    class Listener
    {
    public:
      virtual ~Listener() { }
      virtual void parameterLinkReset(ParameterBridge *bridge,
        ParameterLink *newLink, ParameterLink *oldLink) = 0;
    };
    
    static constexpr float kDefaultParameterValue = 0.5f;

    ParameterBridge() = delete;
    ParameterBridge(const ParameterBridge &) = delete;
    ParameterBridge(ParameterBridge &&) = delete;
    ParameterBridge &operator=(const ParameterBridge &) = delete;
    ParameterBridge &operator=(ParameterBridge &&) = delete;

    ParameterBridge(Plugin::ComplexPlugin *plugin,
      u64 parameterIndex = UINT64_MAX, ParameterLink *link = nullptr) noexcept;
    ~ParameterBridge() noexcept;

    auto *getParameterLink() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }
    void resetParameterLink(ParameterLink *link, bool getValueFromParameter = true) noexcept;

    void setValueFromUI(float newValue) noexcept
    {
      value_.store(newValue, std::memory_order_release);
      sendValueChangedMessageToListeners(newValue);
    }

    void updateUIParameter() noexcept;

    void setCustomName(const juce::String &name);

    u64 getIndex() const noexcept { return parameterIndex_; }
    bool isMappedToParameter() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }

    // Inherited via AudioProcessorParameter
    float getValue() const override { return value_.load(std::memory_order_relaxed); }
    void setValue(float newValue) override;
    float getDefaultValue() const override;

    juce::String getName() const;
    juce::String getName(int maximumStringLength) const override;
    juce::String getLabel() const override;
    juce::String getText(float value, int maximumStringLength) const override;
    float getValueForText(const juce::String &text) const override;

    bool isAutomatable() const override { return true; }
    int getNumSteps() const override;
    bool isDiscrete() const override;
    bool isBoolean() const override;

    void addListener(Listener *listener) { listeners_.emplace_back(listener); }
    void removeListener(Listener *listener) noexcept { std::erase(listeners_, listener); }

  private:
    u64 parameterIndex_ = UINT64_MAX;
    std::pair<std::atomic<bool>, juce::String> name_{};
    std::atomic<float> value_ = kDefaultParameterValue;
    std::atomic<bool> wasValueSet_ = false;
    std::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;

    Plugin::ComplexPlugin *plugin_ = nullptr;
    std::vector<Listener *> listeners_{};
  };
}