
// Created: 2022-07-28 04:21:59

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "memory.hpp"

namespace Plugin
{
  struct State;
}

namespace Framework
{
  class Parameter;

  class HostParameter
  {
  public:
    static constexpr float kDefaultParameterValue = 0.5f;

    HostParameter() = delete;
    HostParameter(const HostParameter &) = delete;
    HostParameter(HostParameter &&) = delete;
    HostParameter &operator=(const HostParameter &) = delete;
    HostParameter &operator=(HostParameter &&) = delete;

    HostParameter(Plugin::State *state, u64 parameterIndex = u64(-1));
    ~HostParameter();

    auto *getParameter() const { return parameter_.load(satomi::memory_order_acquire); }
    void resetParameter(Parameter *parameter, bool getValueFromParameter = true);
    static void notifyParameterChange();

    void beginChangeGesture();
    void setValueFromUI(float newValue);
    void endChangeGesture();

    void updateUIParameter();

    void setCustomName(utils::string name);

    bool isMappedToParameter() const { return parameter_.load(satomi::memory_order_acquire); }

    float getValue() const { return value_.load(satomi::memory_order_relaxed); }
    void setValue(float newValue);
    float getDefaultValue() const;

    void getName(utils::string &outString) const;
    void getName(char *buffer, usize maximumStringLength) const;
    void getText(float value, char *buffer, usize maximumStringLength) const;
    float getValueForText(utils::string_view text) const;

    bool isDiscrete() const;
    bool isBoolean() const;

    const u64 parameterIndex;
    Plugin::State *state = nullptr;

  private:
    utils::pair<satomi::atomic<bool>, utils::string> name_{};
    satomi::atomic<float> value_ = kDefaultParameterValue;
    satomi::atomic<bool> wasValueSet_ = false;
    satomi::atomic<Parameter *> parameter_ = nullptr;
  };
}
