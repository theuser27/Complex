
// Created: 2022-07-11 15:22:48

#pragma once

#include "utils.hpp"
#include "memory.hpp"
#include "simd_utils.hpp"
#include "metadata.hpp"
#include "satomi.hpp"

struct State;

class Control;

template<typename T>
concept ParameterRepresentation = utils::is_same_v<T, float> || utils::is_same_v<T, u32>
  || utils::SimdValue<T> || utils::is_same_v<T, IndexedData>;

class HostParameter;

class ParameterModulator
{
public:

  f4 
  getValue()
  {
    auto values = currentValue.load(satomi::memory_order_relaxed);
    f4::array_t expandedValues{};
    for (usize i = 0; i < expandedValues.size(); ++i)
      expandedValues[i] = values[i / 2];
      
    return expandedValues;
  }

  satomi::atomic<utils::array<float, f4::complexSize>> currentValue{};
};

class Parameter
{
public:
  Parameter(Parameter &&) = delete;
  Parameter &operator=(const Parameter &) = delete;
  Parameter &operator=(Parameter &&) = delete;

  Parameter() = default;
  Parameter(ParameterDetails details, Processor *parent);

  float reset(const ParameterDetails *newDetails = nullptr,
    const float *value = nullptr, float sampleRate = kDefaultSampleRate);

  // prefer calling this only once if possible
  // audio threads only
  template<ParameterRepresentation T>
  auto
  getInternalValue([[maybe_unused]] float sampleRate = kDefaultSampleRate, bool isNormalised = false) const
  {
    [[maybe_unused]] T result;

    if constexpr (utils::is_same_v<T, f4>)
    {
      COMPLEX_ASSERT(details.scale != ParameterScale::Toggle, "Parameter isn't supposed to be a toggle control");
      COMPLEX_ASSERT(details.scale != ParameterScale::Indexed, "Parameter isn't supposed to be a choice control");

      if (isNormalised)
        result = normalisedInternalValue;
      else
        result = internalValue;
      return result;
    }
    else if constexpr (utils::is_same_v<T, float>)
    {
      COMPLEX_ASSERT(details.scale != ParameterScale::Toggle, "Parameter isn't supposed to be a toggle control");
      COMPLEX_ASSERT(details.scale != ParameterScale::Indexed, "Parameter isn't supposed to be a choice control");
      COMPLEX_ASSERT((details.flags & ParameterDetails::Stereo) == 0, "Handle values from stereo channels yourself");

      if (isNormalised)
        result = normalisedInternalValue[0];
      else
        result = internalValue[0];

      return result;
    }
    else if constexpr (utils::is_same_v<T, i4>)
    {
      return utils::toInt(f4::round(internalValue));
    }
    else if constexpr (utils::is_same_v<T, u32>)
    {
      COMPLEX_ASSERT((details.flags & ParameterDetails::Stereo) == 0, "Handle values from stereo channels yourself");

      result = (u32)(f4::round(internalValue)[0]);
      return result;
    }
    else if constexpr (utils::is_same_v<T, IndexedData>)
    {
      COMPLEX_ASSERT(details.scale == ParameterScale::Indexed,
        "Parameter must be indexed to support value to string conversion");
      COMPLEX_ASSERT((details.flags & ParameterDetails::Stereo) == 0,
        "Indexed types that support value to string conversion must not be stereo");

      return getOptionFromValue(internalValue[0], details);
    }
    else
    {
      COMPLEX_HARD_ASSERT_FALSE("Unknown type provided");
      return result;
    }
  }

  // this method will sample the current state of normalisedValue and modulations
  // and update the internal values only
  void updateValue(float sampleRate)
  {
    sampledNormalisedValue = normalisedValue.load(satomi::memory_order_relaxed);
    sampledModulations = 0.0f;
    for (auto *mod : modulators)
      sampledModulations += mod->getValue();
    normalisedInternalValue = f4::clamp(sampledModulations + sampledNormalisedValue, 0.0f, 1.0f);
    internalValue = scaleValue(normalisedInternalValue, details, sampleRate);
  }

  void addModulator(ParameterModulator &modulator);
  void deleteModulator(ParameterModulator &modulator);

  [[nodiscard]] float
  updateNormalisedValue(float value)
  {
    COMPLEX_ASSERT(value >= 0.0f && value <= 1.0f);

    if (details.valueChange)
      value = details.valueChange(this, value);

    COMPLEX_ASSERT(value >= 0.0f && value <= 1.0f);
    normalisedValue.store(value, satomi::memory_order_relaxed);

    return value;
  }

  void serialiseToJson(void *jsonData) const;
  static Parameter *deserialiseFromJson(Processor *processor, 
    void *jsonData, ParameterDetails &reference, Parameter *memory = nullptr);

  [[nodiscard]] static Parameter *createParameters(Processor *processor, 
    usize count, ParameterMetadata *metadata, Parameter *fromCopy = nullptr);
  static void initialiseParameters(Parameter *parameters, usize count,
    Parameter *fromCopy = nullptr, bool keepValue = false);
  [[nodiscard]] static Parameter *
  createAndInitialiseParameters(Processor *processor, usize count,
    ParameterMetadata *metadata, Parameter *fromCopy = nullptr)
  {
    auto *createdParameters = createParameters(processor, count, metadata, fromCopy);
    initialiseParameters(createdParameters, count, fromCopy);
    return createdParameters;
  }

private:
  // after adding modulations and scaling
  f4 internalValue = 0.0f;
  // after adding modulations
  f4 normalisedInternalValue = 0.0f;
  // value of all internal modulations, sampled on calling updateValue()
  f4 sampledModulations = 0.0f;
  // initial normalised value, sampled on calling updateValue()
  float sampledNormalisedValue = 0.0f;

public:
  satomi::atomic<float> normalisedValue{};
  const utils::vector<ParameterModulator *> modulators{};

  Processor *const parentProcessor{};
  const ParameterDetails details{};

  Control *control = nullptr;
  HostParameter *hostParameter = nullptr;
  Parameter *previous{};
  Parameter *next{};
};

class HostParameter
{
public:
  static constexpr float kDefaultParameterValue = 0.5f;

  HostParameter() = delete;
  HostParameter(const HostParameter &) = delete;
  HostParameter(HostParameter &&) = delete;
  HostParameter &operator=(const HostParameter &) = delete;
  HostParameter &operator=(HostParameter &&) = delete;

  HostParameter(State *state, u64 parameterIndex = u64(-1));
  ~HostParameter();

  auto *getParameter() const { return parameter_.load(satomi::memory_order_acquire); }
  void resetParameter(Parameter *parameter, bool getValueFromParameter = true);
  static void notifyParameterChange();

  void beginChangeGesture();
  void setValueFromUI(float newValue);
  void endChangeGesture();

  void updateUIParameter();

  void setCustomName(utils::string name);
  void resetName();

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
  State *state = nullptr;

private:
  utils::pair<satomi::atomic<bool>, utils::string> name_{};
  satomi::atomic<float> value_ = kDefaultParameterValue;
  satomi::atomic<bool> wasValueSet_ = false;
  satomi::atomic<Parameter *> parameter_ = nullptr;
};
