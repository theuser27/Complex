
// Created: 2023-09-04 21:38:38

#include "utils.hpp"
#include "simd_utils.hpp"
#include "parameters.hpp"
#include "Plugin/Complex.hpp"
#include "Generation/Processor.hpp"
#include "Interface/Control.hpp"


utils::string_view
IndexedData::getText() const
{
  auto text = displayName;
  if (text.empty() && flags == StateIdFlag)
  {
    auto *processor = getUiRelated()->plugin.state_->getProcessor(stateId);
    text = processor->name;
  }
  return text;
}

utils::pair<const IndexedData *, usize>
getOptionFromValue(float scaledValue, const ParameterDetails &details)
{
  COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);

  usize i = (usize)scaledValue;
  IndexedData *nextIndexedData = details.options, *indexedData = nullptr;

  COMPLEX_ASSERT(details.options->valueCount > i);

  while (nextIndexedData)
  {
    indexedData = nextIndexedData;
    // move to next option if we've run out
    if (indexedData->valueCount <= i)
    {
      i -= indexedData->valueCount;
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

float
getValueFromOptionText(utils::string_view text, const ParameterDetails &details)
{
  COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);
  u32 value = 0;

  bool success = iterateOverIndexedData(details.options, [&value, text](const IndexedData &option)
    {
      if (option.canBeChosen())
      {
        if (option.displayName.compareCaseInsensitive(text) == 0)
          return true;

        ++value;
      }

      return false;
    });

  return (success) ? value : -1.0f;
}

float
getValueFromOptionId(uuid optionId, const ParameterDetails &details)
{
  COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);
  u32 value = 0;

  bool success = iterateOverIndexedData(details.options, [&value, optionId](const IndexedData &option)
  {
    if (option.canBeChosen())
    {
      if (option.id == optionId)
        return true;

      ++value;
    }

    return false;
  });

  return (success) ? value : -1.0f;
}

float
getValueFromOption(const IndexedData *option, const ParameterDetails &details)
{
  COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);
  u32 value = 0;

  bool success = iterateOverIndexedData(details.options, [&value, option](const IndexedData &other)
  {
    if (other.canBeChosen())
    {
      if (other == *option)
        return true;

      ++value;
    }

    return false;
  });

  return (success) ? value : -1.0f;
}

float
scaleValue(float value, const ParameterDetails &details,
  float sampleRate, bool scalePercent, bool skewOnly)
{
  using namespace utils;

  float result{};
  float sign;
  switch (details.scale)
  {
  case ParameterScale::Toggle:
    result = ::roundf(value);
    break;
  case ParameterScale::Indexed:
    result = (!skewOnly) ? ::roundf(value * (float)(details.options->valueCount - 1)) : value;
    break;
  case ParameterScale::Linear:
    result = (!skewOnly) ? value * (details.maxValue - details.minValue) + details.minValue :
      value + details.minValue / (details.maxValue - details.minValue);
    break;
  case ParameterScale::Clamp:
    result = utils::clamp(value, details.minValue, details.maxValue);
    result = (!skewOnly) ? result : result / (details.maxValue - details.minValue);
    break;
  case ParameterScale::Quadratic:
    result = (!skewOnly) ? value * value * (details.maxValue - details.minValue) + details.minValue : value * value;
    break;
  case ParameterScale::ReverseQuadratic:
    result = value - 1.0f;
    result = (!skewOnly) ? details.maxValue - result * result * (details.maxValue - details.minValue): 1.0f - (result * result);
    break;
  case ParameterScale::SymmetricQuadratic:
    value = value * 2.0f - 1.0f;
    sign = unsignFloat(value);
    value *= value;
    result = (!skewOnly) ? (value * 0.5f * sign + 0.5f) * (details.maxValue - details.minValue) + details.minValue : value * 0.5f * sign + 0.5f;
    break;
  case ParameterScale::Cubic:
    result = (!skewOnly) ? value * value * value * (details.maxValue - details.minValue) + details.minValue : value * value * value;
    break;
  case ParameterScale::ReverseCubic:
    result = value - 1.0f;
    result = (!skewOnly) ? result * result * result * (details.maxValue - details.minValue) + details.maxValue : (result * result * result) + 1.0f;
    break;
  case ParameterScale::SymmetricCubic:
    value = 2.0f * value - 1.0f;
    value *= value * value;
    result = (!skewOnly) ? (value * 0.5f + 0.5f) * (details.maxValue - details.minValue) + details.minValue : value;
    break;
  case ParameterScale::Loudness:
    result = normalisedToDb(value, details.maxValue);
    result = (!skewOnly) ? result : result / details.maxValue;
    break;
  case ParameterScale::SymmetricLoudness:
    value = value * 2.0f - 1.0f;
    if (value < 0.0f)
    {
      result = -normalisedToDb(-value, -details.minValue);
      result = (!skewOnly) ? result : result * 0.5f / details.minValue;
    }
    else
    {
      result = normalisedToDb(value, details.maxValue);
      result = (!skewOnly) ? result : result * 0.5f / details.maxValue;
    }
    break;
  case ParameterScale::Frequency:
    result = normalisedToFrequency(value, sampleRate);
    result = (!skewOnly) ? result : result * 2.0f / sampleRate;
    break;
  case ParameterScale::SymmetricFrequency:
    value = value * 2.0f - 1.0f;
    sign = unsignFloat(value);
    result = (!skewOnly) ? normalisedToFrequency(value, sampleRate) * sign :
      (normalisedToFrequency(value, sampleRate) * sign) / sampleRate;
    break;
  default:
    COMPLEX_ASSERT_FALSE("Unhandled case");
    break;
  }

  if (details.displayUnits == "%" && scalePercent)
    result *= 100.0f;

  if (details.flags & ParameterDetails::RoundToInt)
    result = roundf(result);

  return result;
}

float
unscaleValue(float value, const ParameterDetails &details,
  float sampleRate, bool unscalePercent)
{
  using namespace utils;

  if (details.displayUnits == "%" && unscalePercent)
    value *= 0.01f;

  float result{};
  float sign;
  switch (details.scale)
  {
  case ParameterScale::Toggle:
    result = ::roundf(value);
    break;
  case ParameterScale::Indexed:
    result = (details.options->valueCount <= 1) ? 0.0f :
      value / (details.options->valueCount - 1);
    break;
  case ParameterScale::Clamp:
    result = value;
    break;
  case ParameterScale::Linear:
    result = (value - details.minValue) / (details.maxValue - details.minValue);
    break;
  case ParameterScale::Quadratic:
    result = ::sqrtf((value - details.minValue) / (details.maxValue - details.minValue));
    break;
  case ParameterScale::ReverseQuadratic:
    value = -(value - details.maxValue) / (details.maxValue - details.minValue);
    result = -::sqrtf(value) + 1.0f;
    break;
  case ParameterScale::SymmetricQuadratic:
    value = 2.0f * ((value - details.minValue) / (details.maxValue - details.minValue) - 0.5f);
    sign = unsignFloat(value);
    result = (::sqrtf(value) * sign + 1.0f) * 0.5f;
    break;
  case ParameterScale::Cubic:
    value = (value - details.minValue) / (details.maxValue - details.minValue);
    result = ::powf(value, 1.0f / 3.0f);
    break;
  case ParameterScale::ReverseCubic:
    value = (value - details.maxValue) / (details.maxValue - details.minValue);
    result = ::powf(value, 1.0f / 3.0f) + 1.0f;
    break;
  case ParameterScale::SymmetricCubic:
    value = 2.0f * (value - details.minValue) / (details.maxValue - details.minValue) - 1.0f;
    result = (::powf(value, 1.0f / 3.0f) + 1.0f) * 0.5f;
    break;
  case ParameterScale::Loudness:
    value = dbToNormalised(value, details.maxValue);
    result = (value + 1.0f) * 0.5f;
    break;
  case ParameterScale::SymmetricLoudness:
    if (value < 0.0)
      value = -dbToNormalised(-value, -details.minValue);
    else
      value = dbToNormalised(value, details.maxValue);
    result = (value + 1.0f) * 0.5f;
    break;
  case ParameterScale::Frequency:
    result = frequencyToNormalised(value, sampleRate);
    break;
  case ParameterScale::SymmetricFrequency:
    sign = unsignFloat(value);
    value = frequencyToNormalised(value, sampleRate);
    result = (value * sign + 1.0f) * 0.5f;
    break;
  default:
    COMPLEX_ASSERT_FALSE("Unhandled case");
    break;
  }

  return result;
}

f4
scaleValue(f4 value, const ParameterDetails &details, float sampleRate)
{
  using namespace utils;

  f4 result{};
  i4mask sign{};
  switch (details.scale)
  {
  case ParameterScale::Toggle:
    result = f4::round(value);
    break;
  case ParameterScale::Indexed:
    result = f4::round(value * (float)(details.options->valueCount - 1));
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
    value = f4::mulSub(1.0f, value, 2.0f);
    value *= value * value;
    result = f4::mulAdd(0.5f, value, 0.5f) * (details.maxValue - details.minValue) + details.minValue;
    break;
  case ParameterScale::Loudness:
    result = normalisedToDb(value, details.maxValue);
    break;
  case ParameterScale::SymmetricLoudness:
    value = (value * 2.0f - 1.0f);
    sign = unsignSimd(value);
    if (i4mask::allSame(sign))
    {
      float extremum = (sign[0]) ? -details.minValue : details.maxValue;
      result = normalisedToDb(value, extremum) | sign;
    }
    else
    {
      i4mask mask = i4mask::equal(sign, kSignMask);
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
    result = f4::clamp(value, details.minValue, details.maxValue);
    break;
  default:
    COMPLEX_ASSERT_FALSE("Unhandled case");
    break;
  }

  if (details.flags & ParameterDetails::RoundToInt)
    result = f4::round(result);

  return result;
}

HostParameter::HostParameter(State *state, u64 parameterIndex) :
  parameterIndex{ parameterIndex }, state{ state } { customName = { state->miscStorage }; }

HostParameter::~HostParameter()
{
  resetParameter(nullptr);
}

static void checkGlobalLockAcquired(Parameter *parameter)
{
  if (parameter->parentProcessor && parameter->parentProcessor->parent)
  {
    if (auto &lock = parameter->parentProcessor->state->plugin->processingLock;
      lock.lock.load(satomi::memory_order_relaxed) > 0 ||
      lock.lastLockId.load(satomi::memory_order_relaxed) != utils::thread::getCurrentId())
    {
      COMPLEX_HARD_ASSERT_FALSE(
        "Attempted parameter changes without an acquired lock on %v (%zu)",
        parameter->details.displayName, parameter->details.id);
    }
  }
}

// audio-thread, main-thread
// 
// this is rather tricky function because both audio-thread and main-thread can call it
// for the audio-thread setValueToControl MUST be false otherwise it will enter GUI code
// also it's only allowed to change the linked parameters within the same processor
// 
// for the main-thread a lot of edge-cases arise because of the 2 distinct assignments that must happen
// to parameter_ and parameter->hostParameter. Because those can't happen in a single atomic operation
// what can happen is that the audio-thread can change parameter_, but in the meantime
// the main-thread can call into that same host parameter to perform some changes
void HostParameter::resetParameter(Parameter *newParameter, 
  bool getValueFromParameter, ParameterAssignment assignmentType)
{
  Parameter *old = parameter_.load(satomi::memory_order_relaxed);
  for (; !parameter_.compare_exchange_strong(old, newParameter, satomi::memory_order_acq_rel); ) { }
  if (newParameter == old)
    return;

  if (auto expected = this; old)
  {
    [[maybe_unused]] bool success = old->hostParameter
      .compare_exchange_strong(expected, nullptr, satomi::memory_order_release);
    COMPLEX_ASSERT(success);
  }

  {
    GestureState gestureState = GestureState::Begun;
    (void)gestureState_.compare_exchange_strong(gestureState,
      GestureState::ShouldEnd, satomi::memory_order_relaxed);
  }

  if (newParameter)
  {
    newParameter->hostParameter.store(this, satomi::memory_order_release);
    if (getValueFromParameter)
      value_.store(newParameter->normalisedValue.load(satomi::memory_order_relaxed),
        satomi::memory_order_release);
    else
    {
      // the host parameter deliberately ignores the internal parameter's value
      if (assignmentType != ParameterAssignment::AudioThread && newParameter->control)
        newParameter->control->setValue(value_.load(satomi::memory_order_relaxed), true);
      else
        (void)newParameter->updateNormalisedValue(
          value_.load(satomi::memory_order_relaxed), assignmentType);
    }
  }
}

void State::flushHostParameters()
{
  for (usize i = 0; i < parameterBridges.size(); ++i)
    parameterBridges[i].flushUpdates();
}

// main-thread
// this function is intended to be called regularly on the main-thread 
// to perform tasks which can only be done there
void HostParameter::flushUpdates()
{
  bool dummy = true;
  if (wasValueSet_.compare_exchange_strong(dummy, false, satomi::memory_order_relaxed))
  {
    // for parameter_ it's fine to use relaxed because
    // this method is only called from the main-thread
    // which is the only one that touches the GUI
    auto p = parameter_.load(satomi::memory_order_relaxed);
    if (!p || p->hostParameter.load(satomi::memory_order_relaxed) != this)
      return;

    // loop until we have the most up-to-date value
    auto newValue = value_.load(satomi::memory_order_relaxed);
    do
    {
      // NB: undo/redo doesn't track these changes
      if (p->control)
        p->control->setValue(newValue, true);
      else
        (void)p->updateNormalisedValue(newValue, ParameterAssignment::MainThread);

      // check that we've set the most up-to-date value
      // unfortunately this only guarantees eventual consistency because it might happen that
      // the audio-thread processes values set by the main-thread that are are out-of-date
      // and only then does the main-thread manage to set the most recent value
      //
      // a proper solution here would be to timestamp all values and
      // compare_exchange both the value and timestamp at once,
      // but this is would be extremely annoying to work with and probably unperformant
      // given how often parameter values get updated
      if (auto check = value_.load(satomi::memory_order_relaxed); check == newValue)
      {
        newValue = check;
        continue;
      }

    } while (false);
  }

  // has the parameter mapping been updated during a gesture?
  if (gestureState_.load(satomi::memory_order_relaxed) == GestureState::ShouldEnd)
    endChangeGesture();
}

// main-thread
float 
HostParameter::getScaledValue() const
{
  auto parameter = getParameter();
  float internalValue = getValue();
  if (parameter)
  {
    auto details = parameter->details;
    auto sampleRate = state->plugin->getSampleRate();
    internalValue = scaleValue(internalValue, details, sampleRate, true);
  }

  return internalValue;
}

// main-thread
float
HostParameter::getDefaultScaledValue() const
{
  auto parameter = getParameter();
  float defaultValue = 0.5f;
  if (parameter)
  {
    auto details = parameter->details;
    auto sampleRate = state->plugin->getSampleRate();
    defaultValue = scaleValue(details.defaultNormalisedValue, details, sampleRate, true);
  }

  return defaultValue;
}

// main-thread
float
HostParameter::getDefaultValue() const
{
  if (auto parameter = getParameter())
    return utils::clamp(parameter->details.defaultNormalisedValue, 0.0f, 1.0f);
  return kDefaultParameterValue;
}

// audio-thread, main-thread
void HostParameter::setValue(float newValue)
{
  auto oldValue = value_.exchange(newValue, satomi::memory_order_relaxed);
  if (oldValue != newValue)
  {
    wasValueSet_.store(true, satomi::memory_order_relaxed);
    if (auto *parameter = getParameter())
      (void)parameter->updateNormalisedValue(newValue, ParameterAssignment::AudioThread);
  }
}

// main-thread
void HostParameter::getName(utils::string &outString) const
{
  outString.clear();
  auto index = parameterIndex + 1;
  if (!customName.empty())
  {
    outString.appendFormat("%zu > %v", index, utils::string_view{ customName });
  }
  else if (auto parameter = getParameter())
  {
    outString.appendFormat("%zu > %v", index, parameter->details.displayName);
  }
  else
  {
    outString.appendFormat("%zu", index);
  }
}

// main-thread
void HostParameter::getName(char *buffer, usize maximumStringLength) const
{
  auto index = parameterIndex + 1;
  if (!customName.empty())
  {
    stbsp_snprintf(buffer, (int)maximumStringLength, "%zu > %v", index, utils::string_view{ customName });
  }
  else if (auto parameter = getParameter())
  {
    stbsp_snprintf(buffer, (int)maximumStringLength, "%zu > %v", index, parameter->details.displayName);
  }
  else
  {
    stbsp_snprintf(buffer, (int)maximumStringLength, "%zu", index);
  }
}

// main-thread
void HostParameter::getText(float value, char *buffer, usize maximumStringLength) const
{
  auto parameter = getParameter();
  float internalValue;
  if (!parameter)
    internalValue = value;
  else
  {
    auto details = parameter->details;
    auto sampleRate = state->plugin->getSampleRate();
    internalValue = scaleValue(value, details, sampleRate, true);
    if (details.scale == ParameterScale::Indexed)
    {
      auto [indexedData, index] = getOptionFromValue(internalValue, details);

      if (indexedData->valueCount > 1)
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

// main-thread
float
HostParameter::getValueForText(utils::string_view text) const
{
  if (auto parameter = getParameter())
    return (float)unscaleValue(::strtof(text.data(), nullptr), parameter->details, true);

  return ::strtof(text.data(), nullptr);
}


Parameter::Parameter(ParameterDetails details, Processor *parent) :
  details{ COMPLEX_MOVE(details) }, parentProcessor{ parent },
  modulators{ parent->pool } { }

void Parameter::changeDetails(ParameterDetails newDetails)
{
  // in order to make changes to the parameter details
  // the main processing lock needs to be exclusively acquired
  // violating this will cause data races because
  // nothing is guarding most members in this class
  checkGlobalLockAcquired(this);
  const_cast<ParameterDetails &>(details) = COMPLEX_MOVE(newDetails);
}

float
Parameter::reset(const float *value, float sampleRate)
{
  float newValue;
  if (!value && details.scale == ParameterScale::Indexed)
  {
    newValue = unscaleValue(getValueFromOptionId(details.defaultOptionId,
      details), details, sampleRate);
  }
  else
  {
    newValue = (value) ? *value : details.defaultNormalisedValue;
  }

  newValue = updateNormalisedValue(newValue, ParameterAssignment::MainThread);

  return newValue;
}

void Parameter::addModulator(ParameterModulator &modulator)
{
  checkGlobalLockAcquired(this);
  const_cast<utils::vector<ParameterModulator *> &>(modulators).emplaceBack(&modulator);
}

void Parameter::deleteModulator(ParameterModulator &modulator)
{
  checkGlobalLockAcquired(this);
  const_cast<utils::vector<ParameterModulator *> &>(modulators).erase(&modulator);
}

Parameter *
createParameters(Processor *processor, usize count,
  ParameterMetadata *pararmeterMetadata, Parameter *fromCopy)
{
  if (!count)
    return nullptr;

  auto *memory = arranew(processor->pool, Parameter, count, {});

  for (usize i = 0; i < count; ++i)
  {
    ParameterDetails details;

    if (fromCopy)
      details = fromCopy->details;
    else
      details = pararmeterMetadata->details;

    if (details.scale == ParameterScale::Indexed)
      details.options = IndexedData::deepCopy(processor->pool, details.options);

    (void)new(memory + i) Parameter{ COMPLEX_MOVE(details), processor };
    memory[i].previous = memory + i - 1;
    memory[i].next = memory + i + 1;

    processor->state->registerDynamicParameter(&memory[i]);

    if (fromCopy)
      fromCopy = fromCopy->next;
    else
      pararmeterMetadata = pararmeterMetadata->next;
  }

  memory[0].previous = memory + count - 1;
  memory[count - 1].next = nullptr;

  return memory;
}

void initialiseParameters(Parameter *parameters,
  usize count, Parameter *fromCopy, bool keepValue)
{
  if (!count)
    return;

  auto sampleRate = parameters->parentProcessor->state->plugin->getSampleRate();

  for (usize i = 0; i < count; (++i), (parameters = parameters->next))
  {
    if (fromCopy)
    {
      for (auto *modulator : fromCopy->modulators)
        parameters->addModulator(*modulator);
      float newValue = (keepValue) ? parameters->normalisedValue.load(satomi::memory_order_relaxed) :
        fromCopy->normalisedValue.load(satomi::memory_order_relaxed);
      parameters->reset(&newValue, sampleRate);
      fromCopy = fromCopy->next;
    }
    else
    {
      float newValue = parameters->normalisedValue.load(satomi::memory_order_relaxed);
      parameters->reset((keepValue) ? &newValue : nullptr, sampleRate);
    }

    parameters->updateValue(sampleRate);
  }
}

void Processor::remapParameters(utils::span<HostParameter *> bridges,
  bool bridgeValueFromParameters, bool remapOnlyBridges)
{
  COMPLEX_ASSERT(bridges.empty() || bridges.size() == parameterCount);

  auto *parameter = parameters;
  for (usize i = 0; i < parameterCount; (++i), (parameter = parameter->next))
  {
    auto bridge = parameter->hostParameter.load(satomi::memory_order_acquire);

    if (!bridges.empty())
    {
      bridge->resetParameter(nullptr);
      bridges[i]->resetParameter(parameter, bridgeValueFromParameters);

      continue;
    }

    if (!bridge)
      continue;

    if (remapOnlyBridges)
    {
      if (bridge->getParameter())
      {
        bridge->resetParameter(nullptr);
        // remain with a one-way reference to the bridge we detached from
        parameter->hostParameter.store(bridge, satomi::memory_order_release);
      }
      else
        bridge->resetParameter(parameter, bridgeValueFromParameters);
    }
    else
    {
      bridge->resetParameter(nullptr);
    }
  }

  HostParameter::notifyParameterChange();
}

UndoManager::UndoManager(utils::Pool *parentArena, usize transactionsToKeep)
{
  storage = utils::Pool::createNested(parentArena,
    transactionsToKeep * storagePerTransaction);
  setTransationStorage(transactionsToKeep);
}

UndoManager::~UndoManager()
{
  utils::Pool::destroy(storage);
}

void UndoManager::setTransationStorage(usize transactionsToKeep)
{
  transactionsToKeep = utils::max(transactionsToKeep, (usize)1);
  if (transactions.size() == transactionsToKeep)
    return;

  if (!transactions.size())
  {
    transactions = { (decltype(transactions)::value_type *)nullptr, transactionsToKeep };
    clear();
    return;
  }

  usize copySize = undoActionsCount + redoActionsCount;
  if (copySize > transactionsToKeep)
  {
    // new buffer can't house all currently stored actions
    // use half the size for undo and redo actions respectively
    copySize = transactionsToKeep;
    auto newRedoActionsCount = transactionsToKeep / 2;
    auto newUndoActionsCount = transactionsToKeep - newRedoActionsCount;

    // destroy the to be deleted redos/undos
    // in reverse, from newest to oldest

    for (usize i = redoActionsCount - newRedoActionsCount; i > 0; --i)
      for (auto *action = transactions[currentIndex + newRedoActionsCount + i - 1].second; action; action = action->next)
        if (action->destructor)
          action->destructor(action);

    for (usize i = 0; i < undoActionsCount - newUndoActionsCount; ++i)
      for (auto *action = transactions[currentIndex - newUndoActionsCount - i].second; action; action = action->next)
        if (action->destructor)
          action->destructor(action);

    redoActionsCount = newRedoActionsCount;
    undoActionsCount = newUndoActionsCount;
  }

  usize begin = (currentIndex + 1 - undoActionsCount + transactions.size()) % transactions.size();

  auto *newTransactions = arranew(storage, decltype(transactions)::value_type, transactionsToKeep, {});
  for (usize i = 0; i < copySize; ++i)
    newTransactions[i] = transactions[(begin + i) % transactions.size()];
  transactions = { newTransactions, transactionsToKeep };
  currentIndex = undoActionsCount - 1;
}

void UndoManager::clear()
{
  currentIndex = transactions.size() - 1;
  undoActionsCount = 0;
  redoActionsCount = 0;

  // this will also deallocate the transactions buffer, so we need to allocate it again
  storage->clear(storage);

  auto *newTransactions = arranew(storage, decltype(transactions)::value_type, transactions.size(), {});
  transactions = { newTransactions, transactions.size() };
}

bool 
UndoManager::perform(UndoAction *newAction)
{
  if (newAction == nullptr)
    return false;

  if (isInsideUndoRedoCall)
  {
    COMPLEX_ASSERT_FALSE("Don't call UndoManager methods recursively from redo()/undo() in UndoAction");
    return false;
  }

  isInsideUndoRedoCall = true;
  defer { isInsideUndoRedoCall = false; };

  if (!newAction->redo(newAction))
  {
    if (newAction->destructor)
      newAction->destructor(newAction);
    utils::Pool::remove(newAction);
    return false;
  }

  COMPLEX_ASSERT(redoActionsCount == 0, "You need to call UndoManager::beginNewTransaction() \
    if you want to overwrite undone actions");

  UndoAction *previousAction{};
  auto [pool, action] = transactions[currentIndex];
  if (!action)
    transactions[currentIndex].second = newAction;
  else while (true) // try to coalesce the new action with any one of the current transaction
  {
    if (!action)
    {
      newAction->next = transactions[currentIndex].second;
      transactions[currentIndex].second = newAction;
      break;
    }

    if (action->combineActions)
    {
      if (auto coalescedAction = action->combineActions(pool, action, newAction))
      {
        coalescedAction->next = action->next;
        if (previousAction)
          previousAction->next = coalescedAction;
        else
          transactions[currentIndex].second = coalescedAction;

        // the coalesced action might be newAction, action or a newly allocated action
        // therefore we need to clean up afterwards
        if (coalescedAction != newAction)
          utils::Pool::remove(newAction);
        if (coalescedAction != action)
          utils::Pool::remove(action);

        break;
      }
    }

    previousAction = action;
    action = action->next;
  }

  return true;
}

utils::Pool *
UndoManager::beginNewTransaction()
{
  redoActionsCount = 0;
  // increment index only if the a current transaction exists and it has no actions
  if (undoActionsCount && !getCurrent().second)
    return transactions[currentIndex].first;
  
  undoActionsCount = utils::min(undoActionsCount + 1, transactions.size());
  currentIndex = (currentIndex + 1) % transactions.size();

  // make a new action set/pool if there isn't one already
  if (!transactions[currentIndex].first)
  {
    COMPLEX_ASSERT(!transactions[currentIndex].second);
    transactions[currentIndex].first =
      utils::Pool::createNested(storage, storagePerTransaction);
  }

  // destroy transaction to be replaced, if such exist
  for (auto *action = transactions[currentIndex].second; action; action = action->next)
    if (action->destructor)
      action->destructor(action);

  transactions[currentIndex].second = {};

  //COMPLEX_LOG("begin new - index: %zu, undo count: %zu, redo count: %zu\n\n",
  //  currentIndex, undoActionsCount, redoActionsCount);

  return transactions[currentIndex].first;
}

static void undoRedo(UndoAction *&list, bool (*UndoAction:: *fn)(UndoAction *))
{
  for (UndoAction *action = list, *previous = nullptr; action; action = action->next)
  {
    if (!(action->*fn)(action))
    {
      if (action->destructor)
        action->destructor(action);
      utils::Pool::remove(action);

      continue;
    }

    ((previous) ? previous->next : list) = action;
    previous = action;
  }

  COMPLEX_REVERSE_SLL(list);
}

bool
UndoManager::undo()
{
  if (undoActionsCount == 0)
    return false;

  if (isInsideUndoRedoCall)
  {
    COMPLEX_ASSERT_FALSE("Don't call UndoManager methods recursively from redo()/undo() in UndoAction");
    return false;
  }

  isInsideUndoRedoCall = true;

  undoRedo(transactions[currentIndex].second, &UndoAction::undo);

  currentIndex = (currentIndex - 1 + transactions.size()) % transactions.size();
  --undoActionsCount;
  ++redoActionsCount;

  //COMPLEX_LOG("undo - index: %zu, undo count: %zu, redo count: %zu\n\n",
  //  currentIndex, undoActionsCount, redoActionsCount);

  isInsideUndoRedoCall = false;
  return true;
}

bool
UndoManager::redo()
{
  if (!redoActionsCount)
    return false;

  if (isInsideUndoRedoCall)
  {
    COMPLEX_ASSERT_FALSE("Don't call UndoManager methods recursively from redo()/undo() in UndoAction");
    return false;
  }

  isInsideUndoRedoCall = true;

  currentIndex = (currentIndex + 1) % transactions.size();

  undoRedo(transactions[currentIndex].second, &UndoAction::redo);

  --redoActionsCount;
  ++undoActionsCount;

  //COMPLEX_LOG("redo - index: %zu, undo count: %zu, redo count: %zu\n\n",
  //  currentIndex, undoActionsCount, redoActionsCount);

  isInsideUndoRedoCall = false;
  return true;
}

void State::registerDynamicParameter(Parameter *parameter)
{
  auto details = parameter->details;

  if (details.scale != ParameterScale::Indexed)
    return;

  COMPLEX_ASSERT(details.options != nullptr);

  // adding all possible reasons for update
  // and recalculating the new max value based on current state
  IndexedData::visit(details.options, [&](IndexedData &data)
    {
      if (data.dynamicUpdateUuid != uuid{})
        dynamicParameters.emplaceBack(&data, parameter);

      return false;
    });
}

static void updateDynamicOptionEntries(IndexedData *target,
  IndexedData *reference, utils::Pool *pool, bool allowsBreakingChanges = true)
{
  // we've reached a leaf node
  if (target->canBeChosen())
  {
    auto temp = *target;
    *target = *reference;

    target->parent = temp.parent;
    target->children = temp.children;
    target->next = temp.next;
    target->displayName = temp.displayName;
    target->userFlags = temp.userFlags;

    return;
  }

  // IndexedData *endOfUntrackedOptions{};
  // IndexedData *endOfTrackedOptions{};
  u32 i = 0;

  // do a post-order traversal of this function first
  // and check for erased options
  for (IndexedData *targetChild = target->children, *previous = nullptr, *next = nullptr;
    targetChild; (targetChild = next), (++i))
  {
    bool isPresent = false;
    for (auto referenceChild = reference->children; referenceChild; referenceChild = referenceChild->next)
    {
      isPresent = *targetChild == *referenceChild;
      if (isPresent)
      {
        updateDynamicOptionEntries(targetChild, referenceChild, pool, allowsBreakingChanges);
        break;
      }
    }

    next = targetChild->next;
    previous = (isPresent) ? targetChild : previous;
    // endOfUntrackedOptions = previous;
    // if (i < target->childrenCount)
    //   endOfTrackedOptions = endOfUntrackedOptions;

    if (isPresent)
      continue;

    // removing the option is breaking change but it could crash the program otherwise
    target->removeChild(targetChild);
    utils::Pool::remove(targetChild);
  }

  utils::vector<IndexedData *> childrenToAdd{ getLocalScratch() };

  // check for added options, add them as untracked if breaking changes are not allowed
  for (auto referenceChild = reference->children; referenceChild; referenceChild = referenceChild->next)
  {
    bool isPresent = false;
    for (auto targetChild = target->children; targetChild; targetChild = targetChild->next)
    {
      isPresent = *referenceChild == *targetChild;
      if (isPresent)
        break;
    }

    if (isPresent)
      continue;

    childrenToAdd.emplaceBack(IndexedData::deepCopy(pool, referenceChild));
  }

  if (!childrenToAdd.empty())
    target->addChildren(childrenToAdd, !allowsBreakingChanges);
}

static void updateDynamicParameter(State *state,
  Parameter *parameter, IndexedData *indexedData)
{
  // if the current parameter is mapped out, we shouldn't change any of the values
  // if some of them are not valid anymore, it's up to the consumers of said values to handle things properly
  if (parameter->hostParameter.load(satomi::memory_order_acquire))
    return;

  // getting the most up-to-date counts of the dynamic option
  auto dynamicOption = state->dynamicOptions.find(indexedData->dynamicUpdateUuid)->second;
  u32 newCount = dynamicOption->valueCount;

  // this function assumes no changes are ever missed and
  // therefore counts are always a valid method of checking for changes
  // (no situations where 2 changes happen at the same time to cancel each other out)
  if (indexedData->valueCount == newCount)
    return;

  // changing maxValue and setting normalised value to correspond to the current scaled value
  // for parameter and control if it exists
  auto details = parameter->details;
  auto [oldOption, oldIndex] = getOptionFromValue(scaleValue(
    parameter->normalisedValue.load(satomi::memory_order_relaxed), details), details);
  auto oldId = oldOption->id;

  updateDynamicOptionEntries(indexedData, dynamicOption,
    utils::Pool::fromAllocation(details.options->children));

  auto newNormalised = getValueFromOptionId(oldId, details);
  newNormalised = unscaleValue((newNormalised < 0.0f) ? 0.0f :
    newNormalised + (float)oldIndex, details);

  parameter->changeDetails(COMPLEX_MOVE(details));
  newNormalised = parameter->reset(&newNormalised);
  parameter->updateValue();

  if (parameter->control)
  {
    parameter->control->details = details;
    parameter->control->setValue(newNormalised, false);
  }
}

void State::updateAllDynamicParameters()
{
  utils::ScopedLock guard{};
  if (plugin->state_.get() == this)
    guard = plugin->acquireProcessingLock(true);

  for (auto [indexedData, currentParameter] : dynamicParameters)
    updateDynamicParameter(this, currentParameter, indexedData);
}

void State::updateDynamicParameters(uuid reason)
{
  utils::ScopedLock guard{};
  if (plugin->state_.get() == this)
    guard = plugin->acquireProcessingLock(true);

  for (auto [indexedData, currentParameter] : dynamicParameters)
    if (indexedData->dynamicUpdateUuid == reason)
      updateDynamicParameter(this, currentParameter, indexedData);
}

void State::createDynamicParameters()
{
  dynamicOptions.data = { miscStorage, ParameterChangeReason::values.size() };
  for (auto reason : ParameterChangeReason::values)
  {
    auto dynamicOptionsArena = utils::Pool::createNested(miscStorage, COMPLEX_KB(1));

    auto iter = dynamicOptions.add(reason, anew(dynamicOptionsArena, IndexedData,
      { .dynamicUpdateUuid = reason }));

    switch (reason)
    {
    case ParameterChangeReason::inputSidechain:
      iter->second->valueCount = plugin->inSidechains;
      break;

    case ParameterChangeReason::outputSidechain:
      iter->second->valueCount = plugin->outSidechains;
      break;

    case ParameterChangeReason::laneSources:
    {
      iter->second->valueCount = 0;
      for (auto &pair : allProcessors.data)
      {
        if (pair.second->metadata->id != Processors::EffectsLane)
          continue;

        iter->second->addChildren({{ anew(dynamicOptionsArena, IndexedData,
          { .id = pair.second->metadata->id, .flags = IndexedData::StateIdFlag, .stateId = pair.first }) }});
      }

      break;
    }
    default:
      COMPLEX_ASSERT_FALSE("Missing case");
      break;
    }
  }
}
