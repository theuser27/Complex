
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
  getIndexedData(double scaledValue, const ParameterDetails &details)
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

  double getValueFromOptionText(utils::string_view text, const ParameterDetails &details)
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

  double getValueFromOptionId(uuid optionId, const ParameterDetails &details)
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

    if (details.flags & ParameterDetails::RoundToInt)
      result = simd_float::round(result);

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

  bool UndoManager::perform(UndoAction *newAction, bool performOnAdd)
  {
    if (newAction == nullptr)
      return false;

    if (isPerformingUndoRedo())
    {
      COMPLEX_ASSERT_FALSE("Don't call perform() recursively from the UndoAction::perform() \
        or undo() methods, or else these actions will be discarded!");
      return false;
    }

    if (performOnAdd)
      newAction->redo(newAction);

    COMPLEX_ASSERT(redoActionsCount == 0, "You need to call beginNewTransaction() \
      if you want to overwrite undone actions");

    auto &transaction = transactions[currentIndex];
    if (!transaction.second)
      transaction.second = newAction;
    else if (!transaction.second->combineActions)
    {
      newAction->next = transaction.second;
      transaction.second = newAction->next;
    }
    else
    {
      auto *lastAction = transaction.second;
      auto coalescedAction = lastAction->combineActions(
        transaction.first, lastAction, newAction);

      if (!coalescedAction)
      {
        newAction->next = lastAction;
        transaction.second = newAction;
      }
      else
      {
        coalescedAction->next = lastAction->next;
        transaction.second = coalescedAction;

        if (coalescedAction != newAction)
          utils::bumpArena::remove(newAction);
        if (coalescedAction != lastAction)
          utils::bumpArena::remove(lastAction);
      }
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
        if (data.dynamicUpdateUuid == uuid{})
          return;

        dynamicParameters.emplaceBack(&data, parameter);
      });
  }

  static void updateDynamicParameter(State *state, Framework::ParameterValue *parameter, Framework::IndexedData *indexedData)
  {
    using namespace Framework;

    auto *link = parameter->getParameterLink();
    // if the current parameter is mapped out, we shouldn't change any of the values
    // if some of them are not valid anymore, it's up to the consumers of said values to handle things properly
    if (link->hostControl)
      return;

    u32 newCount = indexedData->count;
    switch (indexedData->dynamicUpdateUuid)
    {
    case ParameterChangeReason::inputSidechain:
      newCount = state->plugin->inSidechains;
      break;
    case ParameterChangeReason::outputSidechain:
      newCount = state->plugin->outSidechains;
      break;
    case ParameterChangeReason::laneCount:
      newCount = state->getLaneCount();
      break;
    default:
      COMPLEX_ASSERT_FALSE("Missing case");
      break;
    }

    if (indexedData->count == newCount)
      return;

    // changing maxValue and setting normalised value to correspond to the current scaled value
    // for parameter and UIControl if it exists
    auto details = parameter->getParameterDetails();
    auto oldScaled = scaleValue((double)parameter->getNormalisedValue(), details);

    auto difference = indexedData->count - newCount;
    for (auto *data = indexedData; data; data = data->parent)
      data->count -= difference;

    auto newNormalised = unscaleValue(oldScaled, details);

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
}
