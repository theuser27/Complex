
// Created: 2023-09-04 21:38:38

#include "utils.hpp"
#include "simd_utils.hpp"
#include "parameter_value.hpp"
#include "parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "Generation/Processor.hpp"
#include "Interface/Components/Control.hpp"

namespace Framework
{
  utils::string_view 
  IndexedData::getText() const
  {
    auto text = displayName;
    if (text.empty() && flags & StateIdFlag)
    {
      auto *processor = Interface::getPlugin(Interface::uiRelated.renderer).state_->getProcessor(stateId);
      text = processor->name;
    }
    return text;
  }

  utils::pair<const IndexedData *, usize>
  getOptionFromValue(double scaledValue, const ParameterDetails &details)
  {
    COMPLEX_ASSERT(details.scale == ParameterScale::Indexed);

    usize i = (usize)scaledValue;
    IndexedData *nextIndexedData = details.options, *indexedData = nullptr;

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

  double 
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

    return (success) ? value : -1.0;
  }

  double 
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

    return (success) ? value : -1.0;
  }

  double
  scaleValue(double value, const ParameterDetails &details,
    float sampleRate, bool scalePercent, bool skewOnly)
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
      result = (!skewOnly) ? ::round(value * (details.options->valueCount - 1)) : value;
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

    if (details.flags & ParameterDetails::RoundToInt)
      result = round(result);

    return result;
  }

  double
  unscaleValue(double value, const ParameterDetails &details,
    float sampleRate, bool unscalePercent)
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
      result = (details.options->valueCount <= 1) ? 0.0 : 
        value / (details.options->valueCount - 1);
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
  scaleValue(simd_float value, const ParameterDetails &details, float sampleRate)
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
      result = simd_float::round(value * (float)details.options->valueCount);
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

    if (details.flags & ParameterDetails::RoundToInt)
      result = simd_float::round(result);

    return result;
  }

  ParameterBridge::ParameterBridge(Plugin::State *state, u64 parameterIndex,
    ParameterLink *link) : parameterIndex{ parameterIndex }, state{ state }
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

  ParameterBridge::~ParameterBridge()
  {
    if (auto link = parameterLinkPointer_.load(satomi::memory_order_acquire); link && link->parameter)
      link->parameter->changeBridge(nullptr);
  }

  void ParameterBridge::resetParameterLink(ParameterLink *link, bool getValueFromParameter)
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
          link->UIControl->setValue(value_.load(satomi::memory_order_relaxed));

        auto name = link->parameter->getParameterDetails().displayName;
        name_.second = utils::string::create(state->miscStorage, "%zu > %s", parameterIndex + 1, name.data());
      }
    }
  }



  // this function is called on the UI thread
  void ParameterBridge::updateUIParameter()
  {
    // for wasValueSet_ we only require atomicity, therefore memory_order_relaxed suffices
    bool dummy = true;
    if (wasValueSet_.compare_exchange_strong(dummy, false, satomi::memory_order_relaxed))
    {
      // for parameterLinkPointer_ it's fine to use relaxed because
      // this method is only called from the UI thread
      // which is the only one that touches the UI
      auto link = parameterLinkPointer_.load(satomi::memory_order_relaxed);
      if (!link || !link->UIControl || link->hostControl != this)
        return;

      link->UIControl->setValue(value_.load(satomi::memory_order_relaxed));
    }
  }

  void ParameterBridge::setCustomName(utils::string name)
  {
    utils::ScopedLock guard{ name_.first, utils::WaitMechanism::Spin };
    name_.second = COMPLEX_MOVE(name);
  }

  float 
  ParameterBridge::getDefaultValue() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return utils::clamp(pointer->parameter->getParameterDetails().defaultNormalisedValue, 0.0f, 1.0f);
    return kDefaultParameterValue;
  }

  void ParameterBridge::getName(utils::string &outString) const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<satomi::atomic<bool> &>(name_.first), utils::WaitMechanism::Spin };

    if (parameterLinkPointer_.load(satomi::memory_order_acquire))
    {
      outString.copy(name_.second);
    }
    else
    {
      auto index = name_.second.find(" ");
      outString.copy({ name_.second, 0, index });
    }
  }

  void ParameterBridge::getName(char *buffer, usize maximumStringLength) const
  {
    // this is hacky i know but there's no way to declare the atomic mutable inside the pair
    utils::ScopedLock guard{ const_cast<satomi::atomic<bool> &>(name_.first), utils::WaitMechanism::Spin };

    usize size = utils::min(name_.second.size(), maximumStringLength - 1);
    ::memcpy(buffer, name_.second.data(), size);
    buffer[size] = '\0';
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

  float 
  ParameterBridge::getValueForText(utils::string_view text) const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return (float)unscaleValue(::strtod(text.data(), nullptr), pointer->parameter->getParameterDetails(), true);

    return ::strtof(text.data(), nullptr);
  }

  bool 
  ParameterBridge::isDiscrete() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Indexed;
    return false;
  }

  bool 
  ParameterBridge::isBoolean() const
  {
    if (auto pointer = parameterLinkPointer_.load(satomi::memory_order_acquire))
      return pointer->parameter->getScale() == ParameterScale::Toggle;
    return false;
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

  UndoManager::UndoManager(usize transactionsToKeep)
  {
    storage = utils::bumpArena::create(COMPLEX_MB(64), transactionsToKeep * 512);
    setTransationStorage(transactionsToKeep);
  }

  UndoManager::~UndoManager()
  {
    utils::bumpArena::destroy(storage);
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

    usize copySize, begin;
    if ((usize)(undoActionsCount + redoActionsCount) <= transactionsToKeep)
    {
      // new buffer can house all currently stored actions
      copySize = undoActionsCount + redoActionsCount;
      begin = ((usize)currentIndex + 1 - undoActionsCount +
        transactions.size()) % transactions.size();
    }
    else
    {
      // new buffer can't house all currently stored actions
      // use half the size for undo and redo actions respectively
      copySize = transactionsToKeep;
      redoActionsCount = transactionsToKeep / 2;
      undoActionsCount = transactionsToKeep - redoActionsCount;
      begin = ((usize)currentIndex + 1 - undoActionsCount +
        transactions.size()) % transactions.size();
    }

    currentIndex = undoActionsCount - 1;

    auto *newTransactions = arranew(storage, decltype(transactions)::value_type,
      transactionsToKeep, {});
    for (usize i = 0; i < copySize; ++i)
      newTransactions[i] = transactions[(begin + i) % transactions.size()];
    transactions = { newTransactions, transactionsToKeep };
  }

  void UndoManager::clear()
  {
    currentIndex = 0;
    undoActionsCount = 1; // for the currentIndex
    redoActionsCount = 0;

    // this will also deallocate the transactions buffer, so we need to allocate it again
    storage->clear(storage);

    auto *newTransactions = arranew(storage, decltype(transactions)::value_type,
      transactions.size(), {});
    transactions = { newTransactions, transactions.size() };
  }

  bool UndoManager::perform(UndoAction *newAction)
  {
    if (newAction == nullptr)
      return false;

    if (isPerformingUndoRedo())
    {
      COMPLEX_ASSERT_FALSE("Don't call perform() recursively from the UndoAction::perform() \
        or undo() methods, or else these actions will be discarded!");
      return false;
    }

    newAction->redo(newAction);

    COMPLEX_ASSERT(redoActionsCount == 0, "You need to call beginNewTransaction() \
      if you want to overwrite undone actions");

    UndoAction *previousAction{};
    auto [arena, action] = transactions[currentIndex];
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
        if (auto coalescedAction = action->combineActions(arena, action, newAction))
        {
          coalescedAction->next = action->next;
          if (previousAction)
            previousAction->next = coalescedAction;
          else
            transactions[currentIndex].second = coalescedAction;

          // the coalesced action might be newAction, action or a newly allocated action
          // therefore we need to clean up afterwards
          if (coalescedAction != newAction)
            utils::bumpArena::remove(newAction);
          if (coalescedAction != action)
            utils::bumpArena::remove(action);

          break;
        }
      }

      previousAction = action;
      action = action->next;
    }

    return true;
  }

  utils::bumpArena *
  UndoManager::beginNewTransaction()
  {
    bool currentHasArena = transactions[currentIndex].first;

    // skip making a new action set if current one is empty
    if (!currentHasArena || transactions[currentIndex].second)
    {
      if (currentHasArena)
      {
        currentIndex = (currentIndex + 1) % transactions.size();

        // destroy transaction to be replaced
        for (auto *action = transactions[currentIndex].second; action; action = action->next)
          if (action->destructor)
            action->destructor(action);

        undoActionsCount = utils::min(undoActionsCount + 1, transactions.size());
        redoActionsCount = 0;
      }

      transactions[currentIndex].second = {};
      if (!transactions[currentIndex].first)
        transactions[currentIndex].first = utils::bumpArena::createNested(storage, 512);
    }

    return transactions[currentIndex].first;
  }

  bool UndoManager::undo(bool undoCurrentTransactionOnly)
  {
    if (undoActionsCount == 0)
      return false;

    isInsideUndoRedoCall = true;

    COMPLEX_FOREACH_AND_REVERSE_SLL(action, transactions[currentIndex].second)
    {
      action->undo(action);
      transactions[currentIndex].second = action;
    }

    if (!undoCurrentTransactionOnly)
    {
      currentIndex = (currentIndex - 1 + transactions.size()) % transactions.size();
      --undoActionsCount;
      ++redoActionsCount;
    }

    isInsideUndoRedoCall = false;
    return true;
  }

  bool UndoManager::redo()
  {
    if (!redoActionsCount)
      return false;

    isInsideUndoRedoCall = true;

    COMPLEX_FOREACH_AND_REVERSE_SLL(action, transactions[(currentIndex + 1) % transactions.size()].second)
    {
      action->redo(action);
      transactions[currentIndex].second = action;
    }

    currentIndex = (currentIndex + 1) % transactions.size();
    --redoActionsCount;
    ++undoActionsCount;

    isInsideUndoRedoCall = false;
    return true;
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
        if (data.dynamicUpdateUuid != uuid{})
          dynamicParameters.emplaceBack(&data, parameter);

        return false;
      });
  }

  static void updateDynamicOptionEntries(Framework::IndexedData *target,
    Framework::IndexedData *reference, utils::bumpArena *arena, bool allowsBreakingChanges = true)
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

    Framework::IndexedData *endOfUntrackedOptions{}, *endOfTrackedOptions{};
    u32 i = 0;

    // do a post-order traversal of this function first
    // and check for erased options
    for (Framework::IndexedData *targetChild = target->children, *previous = nullptr, *next = nullptr; 
      targetChild; (targetChild = next), (++i))
    {
      bool isPresent = false;
      for (auto referenceChild = reference->children; referenceChild; referenceChild = referenceChild->next)
      {
        isPresent = targetChild->id == referenceChild->id;
        if (isPresent)
        {
          updateDynamicOptionEntries(targetChild, referenceChild, arena, allowsBreakingChanges);
          break;
        }
      }

      next = targetChild->next;
      previous = (isPresent) ? targetChild : previous;
      endOfUntrackedOptions = previous;
      if (i < target->childrenCount)
        endOfTrackedOptions = endOfUntrackedOptions;

      if (isPresent)
        continue;

      // removing the option is breaking change but it could crash the program otherwise
      if (previous)
        previous->next = next;
      utils::bumpArena::remove(targetChild);
    }
    
    utils::vector<Framework::IndexedData *> childrenToAdd{ localScratch };

    // check for added options, add them as untracked if breaking changes are not allowed
    for (auto referenceChild = reference->children; referenceChild; referenceChild = referenceChild->next)
    {
      bool isPresent = false;
      for (auto targetChild = target->children; targetChild; targetChild = targetChild->next)
      {
        isPresent = referenceChild->id == targetChild->id;
        if (isPresent)
          break;
      }
      
      if (isPresent)
        continue;

      childrenToAdd.emplaceBack(Framework::IndexedData::deepCopy(arena, referenceChild));
    }

    if (!childrenToAdd.empty())
      target->addChildren(childrenToAdd, !allowsBreakingChanges);
  }

  static void updateDynamicParameter(State *state, 
    Framework::ParameterValue *parameter, Framework::IndexedData *indexedData)
  {
    using namespace Framework;

    auto *link = parameter->getParameterLink();
    // if the current parameter is mapped out, we shouldn't change any of the values
    // if some of them are not valid anymore, it's up to the consumers of said values to handle things properly
    if (link->hostControl)
      return;

    // this function assumes no changes are ever missed and 
    // therefore counts are always a valid method of checking for changes
    // (no situations where 2 changes happen at the same time to cancel each other out)

    // getting the most up-to-date counts of the dynamic option
    auto dynamicOption = state->dynamicOptions.find(indexedData->dynamicUpdateUuid)->second;
    u32 newCount = dynamicOption->valueCount;

    if (indexedData->valueCount == newCount)
      return;

    // changing maxValue and setting normalised value to correspond to the current scaled value
    // for parameter and UIControl if it exists
    auto details = parameter->getParameterDetails();
    auto [oldOption, oldIndex] = getOptionFromValue(scaleValue((double)parameter->getNormalisedValue(), details), details);
    auto oldId = oldOption->id;

    updateDynamicOptionEntries(indexedData, dynamicOption,
      utils::bumpArena::fromAllocation(details.options->children));

    auto newNormalised = getValueFromOptionId(oldId, details);
    newNormalised = unscaleValue((newNormalised < 0.0) ? 0.0 : 
      newNormalised + (double)oldIndex, details);

    if (link->UIControl)
    {
      link->UIControl->details = details;
      link->UIControl->setValue(newNormalised, true);
    }

    float value = (float)newNormalised;
    parameter->setParameterDetails(details, &value);
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
    using namespace Framework;

    utils::ScopedLock guard{};
    if (plugin->state_.get() == this)
      guard = plugin->acquireProcessingLock(true);

    for (auto [indexedData, currentParameter] : dynamicParameters)
      if (indexedData->dynamicUpdateUuid == reason)
        updateDynamicParameter(this, currentParameter, indexedData);
  }

  void State::createDynamicParameters()
  {
    using namespace Framework;

    dynamicOptions.data = { { miscStorage, false }, ParameterChangeReason::kParameterChangeReasonValues.size() };
    for (auto reason : ParameterChangeReason::kParameterChangeReasonValues)
    {
      auto dynamicOptionsArena = utils::bumpArena::createNested(miscStorage, COMPLEX_KB(1));

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
          if (pair.second->metadata->id != Generation::Processors::EffectsLane)
            continue;

          iter->second->addChildren({{ anew(dynamicOptionsArena, IndexedData,
            {.id = pair.second->metadata->id, .flags = IndexedData::StateIdFlag, .stateId = pair.first }) }});
        }

        break;
      }
      default:
        COMPLEX_ASSERT_FALSE("Missing case");
        break;
      }
    }
  }
}
