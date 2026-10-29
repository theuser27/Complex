
// Created: 2023-07-31 19:37:43

#include "Control.hpp"

#include "Third Party/pugl/pugl.h"

#include "Framework/parameters.hpp"
#include "Plugin/Complex.hpp"
#include "MainInterface.hpp"
#include "Popups.hpp"

Control::Control()
{
  componentFlags.clickable = true;
  componentFlags.clickableChildren = false;
}

Parameter *
Control::setParameter(Parameter *newParameter)
{
  auto replacedParameter = parameter;
  if (replacedParameter)
    replacedParameter->control = nullptr;

  parameter = newParameter;

  if (newParameter)
    newParameter->control = this;

  return replacedParameter;
}

Parameter *
Control::changeLinkedParameter(Parameter &newParameter,
  bool getValueFromParameter)
{
  auto *replacedParameter = setParameter(&newParameter);

  details = newParameter.details;
  {
    controlFlags.bipolar = -details.minValue == details.maxValue ||
      details.scale == ParameterScale::SymmetricQuadratic ||
      details.scale == ParameterScale::SymmetricCubic ||
      details.scale == ParameterScale::SymmetricLoudness ||
      details.scale == ParameterScale::SymmetricFrequency;
    controlFlags.shouldCheckDbInfinities =
      details.scale == ParameterScale::Loudness ||
      details.scale == ParameterScale::SymmetricLoudness;
    controlFlags.shouldUsePlusMinusPrefix |=
      details.scale == ParameterScale::SymmetricLoudness;
  }

  if (getValueFromParameter)
    setValue(newParameter.normalisedValue.load(satomi::memory_order_relaxed), true);
  else
    setValue(getValue(), true);

  return replacedParameter;
}

void Control::setValueToHost() const
{
  if (parameter)
  {
    if (auto hostParameter = parameter->hostParameter.load(satomi::memory_order_acquire))
    {
      hostParameter->beginChangeGesture();
      hostParameter->setValueFromUI(getValue());
      hostParameter->endChangeGesture();
    }
    else
      getUiRelated()->plugin.markPluginDirty();
  }
}

bool
Control::setValue(float newValue, bool setParameter)
{
  newValue = utils::clamp(newValue, 0.0f, 1.0f);

  // set parameter value as well
  if (setParameter && parameter)
    newValue = parameter->updateNormalisedValue(newValue, ParameterAssignment::MainThread);

  auto oldValue = value.exchange(newValue, satomi::memory_order_relaxed);
  if (newValue == oldValue)
    return false;

  if (details.scale == ParameterScale::Toggle)
  {
    if (scaleValue(newValue, details) ==
      scaleValue(oldValue, details))
      return false;
  }
  else if (details.scale == ParameterScale::Indexed)
  {
    auto oldScaled = scaleValue(oldValue, details);
    auto newScaled = scaleValue(newValue, details);

    if (getOptionFromValue(oldScaled, details) ==
      getOptionFromValue(newScaled, details))
      return false;
  }

  if (valueChangedCallback)
    valueChangedCallback(this, newValue, oldValue);

  return true;
}

void Control::resetValue()
{
  auto previousValue = getValue();
  bool isChanged = setValue(details.defaultNormalisedValue, true);

  if (isChanged)
  {
    setValueToHost();
    createParameterUpdate(previousValue, false);
  }
}

enum MenuId
{
  kCancel = 0,
  kTopLevel,
  kMidiLearn,
  kClearMidiLearn,
  kName,
  kDefaultValue,
  kInlineGroup,
  kManualEntry,
  kCopyNormalisedValue,
  kCopyValue,
  kPasteValue,
  kClearModulations,
  kMapFirstSlot,
  kMapFirstSlotSubtitle,
  kClearMapping,
  kValueDisplayGroup,
  kValueDisplayText,
  kValueDisplay,
  kControlMenuIdsSize,
  kMappingList = 64,
};

static bool isUnmappingParameter(i32 id) { return id % 2 != (kMappingList + 1) % 2; }
static usize getParameterIndexFromId(i32 id) { return (id - kMappingList - 1) / 2; }

struct ControlPopupItem;
static utils::string_view getControlPopupItemText(ControlPopupItem *item, utils::string &savedText);


struct ControlPopupItem : PopupItem
{
  float rounding = 0.0f;
  bool canTextWrap = false;
  Skin::ColourId textColourId = Skin::kTextComponentText1;
  Placement textPlacement = Placement::left;
  PopupItem *siblingItem{};

  bool
  render(Graphics &g) override
  {
    if ((componentFlags.isHovered || componentFlags.isClicked) && canBeChosen)
      fillRect(g, getLocalBounds().toFloat(), getHighlightColour(), scaleValue(rounding));

    if (id > kMappingList && isUnmappingParameter(id))
    {
      auto crossBounds = getLocalBounds().toFloat().withTrim(scaleValue(padding.toFloat()));
      crossBounds = crossBounds.withTrim(crossBounds.w * 0.25f, crossBounds.h * 0.25f);

      nvgBeginPath(g);
      nvgMoveTo(g, crossBounds.x, crossBounds.y);
      nvgLineTo(g, crossBounds.getRight(), crossBounds.getBottom());

      nvgMoveTo(g, crossBounds.x, crossBounds.getBottom());
      nvgLineTo(g, crossBounds.getRight(), crossBounds.y);

      nvgStrokeColor(g, getColour(Skin::kTextComponentText1, this));
      nvgStroke(g);
    }

    if (id == kManualEntry || id == kCopyNormalisedValue ||
      id == kCopyValue || id == kPasteValue)
    {
      Colour colours[] = { getColour(Skin::kTextComponentText2, this), getColour(Skin::kWidgetPrimary1, this) };

      switch (id)
      {
      case kManualEntry:
      {

        break;
      }
      case kCopyNormalisedValue:
      {
        auto [fn, iconSizes] = Paths::copyNormalisedValueIcon();

        fn(g, { colours },
          scaleValue(iconSizes.toFloat()).withCentre(getLocalBounds().toFloat().getCentre()),
          scaleValue(1.0f));
        break;
      }
      case kCopyValue:
      {
        auto [fn, iconSizes] = Paths::copyScaledValueIcon();

        fn(g, { colours },
          scaleValue(iconSizes.toFloat()).withCentre(getLocalBounds().toFloat().getCentre()),
          scaleValue(1.0f));
        break;
      }
      case kPasteValue:
      {
        auto [fn, iconSizes] = Paths::pasteValueIcon();

        fn(g, { colours },
          scaleValue(iconSizes.toFloat()).withCentre(getLocalBounds().toFloat().getCentre()),
          scaleValue(1.0f));
        break;
      }
      default:
        break;
      }

      return true;
    }

    utils::string savedText{};
    utils::string_view text = getControlPopupItemText(this, savedText);

    if (!canBeChosen && id != kMapFirstSlotSubtitle && id <= kMappingList)
    {
      fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kPopupSelectorDelimiter, this));
    }

    //strokeRect(g, getLocalBounds().withTrim(scaleValueRoundInt(padding.toInt())).toFloat(),
    //  1.0f, Colour{ 128, 128, 128 });

    if (id == kMappingList)
    {
      float width = scaleValueRound(kPopupSubwindowArrowWidth);
      auto arrowBounds = getLocalBounds().withTrim(scaleValueRoundInt(padding.toInt())).toFloat();

      float yCenter = bounds.h * 0.5f;
      float height = width;

      nvgStrokeWidth(g, scaleValue(1.0f));
      nvgBeginPath(g);
      nvgMoveTo(g, arrowBounds.getRight() - width, yCenter - height);
      nvgLineTo(g, arrowBounds.getRight(), yCenter);
      nvgLineTo(g, arrowBounds.getRight() - width, yCenter + height);
      nvgStrokeColor(g, getColour(Skin::kTextComponentText2, this));
      nvgStroke(g);
    }

    if (!text.empty())
    {
      auto textBounds = getLocalBounds().withTrim(scaleValueRoundInt(padding.toInt())).toFloat();
      textBounds.h = ::roundf(scaleValue((float)desiredSize.h));

      renderText(text, FontId::InterType, textBounds, g,
        getColour(textColourId, this), textPlacement, canTextWrap);
    }

    return true;
  }
};

static utils::string_view
getControlPopupItemText(ControlPopupItem *item, utils::string &savedText)
{
  switch (item->id)
  {
  case kName:
    return ((Control *)item->extraData)->details.displayName;
  case kDefaultValue:
    return "Set to D" COMPLEX_UNDERSCORE_LITERAL "efault Value";
  case kClearMapping:
    return "C" COMPLEX_UNDERSCORE_LITERAL "lear Parameter Mapping";
  case kMapFirstSlot:
    return "Make a" COMPLEX_UNDERSCORE_LITERAL "utomatable";
  case kMapFirstSlotSubtitle:
    return "Assign to first free Automation Slot";
  case kMappingList:
    return "Assign automation slot";
  case kValueDisplayText:
    return "Value";
  case kValueDisplay:
  {
    auto *control = (Control *)item->extraData;
    savedText.reserve(getLocalScratch(), 31);
    control->getScaledValueString(savedText, control->getValue(), false);

    char unscaledValueStringBuffer[64];
    usize stringSize = utils::floatToString(control->getValue(),
      unscaledValueStringBuffer, sizeof(unscaledValueStringBuffer) - 1, 3);

    savedText.appendFormat(" " COMPLEX_MIDDLE_DOT_LITERAL " %v",
      utils::string_view{ unscaledValueStringBuffer, stringSize });
    return savedText;
  }
  }

  if (item->id > kMappingList && !isUnmappingParameter(item->id))
  {
    auto index = getParameterIndexFromId(item->id);
    utils::span parameterBridges = getUiRelated()->plugin.state_->parameterBridges;

    if (parameterBridges[index].getParameter())
      item->textColourId = Skin::kWidgetPrimary1;
    else
    {
      item->canBeChosen = true;
      item->textColourId = Skin::kTextComponentText1;
      if (item->siblingItem)
        item->siblingItem->componentFlags.isVisible = false;
    }

    parameterBridges[index].getName(savedText);
    return savedText;
  }

  // item doesn't have text
  return {};
}

static Range<i32>
getControlPopupItemTextMetrics(Component *c, bool isCalculatingVertical)
{
  auto *item = (ControlPopupItem *)c;

  utils::string savedText{};
  utils::string_view text = getControlPopupItemText(item, savedText);

  if (text.empty())
    return { -1, -1 };

  i32 minSize{}, maxSize{};

  bool canTextWrap = item->canTextWrap;
  float height = ::roundf(scaleValue((float)item->desiredSize.h));
  getUiRelated()->g->setFont(FontId::InterType, height);

  if (!isCalculatingVertical)
  {
    minSize = (canTextWrap) ? 0 : (i32)::ceilf(getUiRelated()->g->getStringWidthFloat(text));
    if (item->id == kMappingList)
    {
      minSize += scaleValueRoundInt((float)(kPopupSubwindowArrowMargin + kPopupSubwindowArrowWidth));
    }

    maxSize = -1;
  }
  else
  {
    auto lineCount = getUiRelated()->g->getStringNumberOfLines(text, (float)item->bounds.w);
    minSize = (i32)height * (i32)lineCount;
    maxSize = minSize;
  }

  return { minSize, maxSize };
}

void handleControlPopupResult(Control *control, PopupItem *selectedItem);

void Control::createPopupMenu(PopupSelector *selector, Point<i32> position)
{
  static constexpr auto kInlineGroupHeight = 32;	// serves also as minimum width for the elements

  selector->resetState();
  auto *itemArena = selector->pool;
  auto primaryDesiredSize = Rectangle{ 0, kPrimaryTextLineHeight, 0, kPrimaryTextLineHeight };
  auto secondaryDesiredSize = Rectangle{ 0, 13, 0, 13 };
  auto primaryPadding = Rectangle<u16>{ 12, 4, 12, 4 };
  auto secondaryPadding = Rectangle<u16>{ 12, 4, 12, 4 };

#define ITEM(idNumber, list, ...) (&with_val(*anew(itemArena, ControlPopupItem, {}), \
  .id = idNumber, .associatedList = &list __VA_OPT__(,) __VA_ARGS__))
#define TEXT_ITEM(idNumber, list, ...) ITEM(idNumber, list, .overrideSize = getControlPopupItemTextMetrics, \
  .sizingFlags = Component::GrowableX, .padding = primaryPadding, .desiredSize = primaryDesiredSize __VA_OPT__(,) __VA_ARGS__)

  PopupList &options = *anew(itemArena, PopupList, { selector });
  options.componentFlags.vertical = true;
  options.padding = { 0, 4, 0, 4 };
  options.desiredSize = { kPopupMinWidth, 0, utils::int_max<i32>, utils::int_max<i32> };
  options.draw = [](Graphics &g, PopupList *self)
  {
    auto topPadding = scaleValue((float)self->padding.y);
    auto bottomPadding = scaleValue((float)self->padding.h);

    fillRect(g, self->getLocalBounds().toFloat(), getColour(Skin::kBody, self),
      topPadding, topPadding, bottomPadding, bottomPadding);

    self->renderChildren(g);

    strokeRect(g, self->getLocalBounds().toFloat(), scaleValue(1.0f), Colour{ 45,45,45 },
      topPadding);

    return false;
  };

  options.addChildComponent(TEXT_ITEM(kName, options, .extraData = this, .canTextWrap = true,
    .padding = secondaryPadding, .desiredSize = secondaryDesiredSize, .canBeChosen = false,
    .textColourId = Skin::kTextComponentText2));
  options.addChildComponent(TEXT_ITEM(kDefaultValue, options, .shortcutKeyCode = 'd'));

  if (details.flags & ParameterDetails::Automatable)
  {
    if (parameter && parameter->hostParameter.load(satomi::memory_order_acquire))
    {
      options.addChildComponent(TEXT_ITEM(kClearMapping, options, .shortcutKeyCode = 'c'));
    }
    else
    {
      PopupItem &mapFirstSlot = with_val(*anew(itemArena, PopupItem, {}), .id = kMapFirstSlot,
        .associatedList = &options, .shortcutKeyCode = 'a', .padding = (Rectangle<u16>{ 12, 4, 12, 4 }),
        .componentFlags.vertical = true, .sizingFlags = Component::GrowableX);
      options.addChildComponent(&mapFirstSlot);
      mapFirstSlot.addChildComponent(TEXT_ITEM(kMapFirstSlot, options, .placement = Placement::left, .padding = {}));
      mapFirstSlot.addChildComponent(TEXT_ITEM(kMapFirstSlotSubtitle, options, .canTextWrap = true, .canBeChosen = false,
        .padding = (Rectangle<u16>{ 0, 2, 0, 0 }), .textColourId = Skin::kTextComponentText2,
        .desiredSize = secondaryDesiredSize, .placement = Placement::left));

      ControlPopupItem &mappingList = *TEXT_ITEM(kMappingList, options);
      mappingList.childList = anew(itemArena, PopupList, { selector });
      mappingList.childList->draw = options.draw;
      mappingList.childList->padding = { 0, 4, 8, 4 };
      options.addChildComponent(&mappingList);
      utils::span parameterBridges = getUiRelated()->plugin.state_->parameterBridges;
      for (usize i = 0; i < parameterBridges.size(); ++i)
      {
        if (!parameterBridges[i].getParameter())
        {
          mappingList.childList->addChildComponent(TEXT_ITEM(kMappingList + 1 + (i32)i * 2, (*mappingList.childList)));

          continue;
        }

        COMPLEX_ASSERT(parameterBridges[i].getParameter()->control);

        Component &wrapper = with_val(*anew(itemArena, Component, {}), .sizingFlags = Component::GrowableX,
          .skinOverride = parameterBridges[i].getParameter()->control->getSkinOverride());
        ControlPopupItem *parameterMap = TEXT_ITEM(kMappingList + 1 + (i32)i * 2, (*mappingList.childList),
          .sizingFlags = Component::GrowableX, .placement = Placement::left,
          .textColourId = Skin::kWidgetPrimary1, .canBeChosen = false);
        ControlPopupItem *parameterUnmap = TEXT_ITEM(kMappingList + 1 + (i32)i * 2 + 1, (*mappingList.childList),
          .sizingFlags = Component::None, .padding = (Rectangle<u16>{ 4, 4, 4, 4 }), .closesPopup = false,
          .desiredSize = (Rectangle{ kPrimaryTextLineHeight, kPrimaryTextLineHeight, kPrimaryTextLineHeight, kPrimaryTextLineHeight }));
        parameterMap->siblingItem = parameterUnmap;
        wrapper.addChildComponent(parameterMap);
        wrapper.addChildComponent(parameterUnmap);
        mappingList.childList->addChildComponent(&wrapper);
      }
    }
  }

  ControlPopupItem &valueDisplay = *ITEM(kValueDisplayGroup, options,
    .canBeChosen = false, .sizingFlags = Component::GrowableX, .padding = primaryPadding);
  options.addChildComponent(&valueDisplay);
  valueDisplay.addChildComponent(TEXT_ITEM(kValueDisplayText, options, .placement = Placement::left,
    .margin = (Rectangle<i16>{ 0, 0, 8, 0 }), .sizingFlags = Component::GrowableX, .canBeChosen = false,
    .desiredSize = secondaryDesiredSize, .padding = {}, .textColourId = Skin::kTextComponentText2));
  valueDisplay.addChildComponent(TEXT_ITEM(kValueDisplay, options, .canBeChosen = false, .textPlacement = Placement::right,
    .desiredSize = secondaryDesiredSize, .placement = Placement::right, .extraData = this,
    .sizingFlags = Component::GrowableX, .padding = {}, .textColourId = Skin::kTextComponentText2));

  Component &inlineGroup = with_val(*anew(itemArena, Component, {}), .pool = itemArena,
    .sizingFlags = Component::GrowableX, .padding = (Rectangle<u16>{ 4, 4, 4, 0 }),
    .desiredSize = (Rectangle{ 0, kInlineGroupHeight, 0, kInlineGroupHeight }));
  options.addChildComponent(&inlineGroup);

  static constexpr auto iconWidthHeight = 16;

  auto iconDesiredSize = Rectangle{ iconWidthHeight, iconWidthHeight, iconWidthHeight, iconWidthHeight };
#define ICON(idNumber, list) ITEM(idNumber, list, .rounding = 2.0f, .desiredSize = iconDesiredSize,\
  .sizingFlags = (Component::SizingFlags)(Component::GrowableY | Component::GrowableX))

  inlineGroup.addChildComponent(ICON(kCopyNormalisedValue, options));
  inlineGroup.addChildComponent(ICON(kCopyValue, options));
  inlineGroup.addChildComponent(ICON(kPasteValue, options));
  if (controlFlags.canInputValue)
  {
    inlineGroup.addChildComponent(ICON(kManualEntry, options));
  }

#undef ICON
#undef TEXT_ITEM
#undef ITEM

  controlFlags.isInModalState = true;
  selector->list = &options;
  selector->skinOverride = getSkinOverride();
  selector->callback = [this](PopupSelector *, PopupItem *selectedItem)
  { handleControlPopupResult(this, selectedItem); if (selectedItem->closesPopup) controlFlags.isInModalState = false; };
  selector->cancel = [this](PopupSelector *) { controlFlags.isInModalState = false; };
  selector->summon(this, Placement::custom, position);
}

void Control::createParameterUpdate(float previousValue, bool forceOverwrite)
{
  auto &undoManager = getUndoManager();
  auto [storage, action] = undoManager.getCurrent();

  auto oldEditTime = lastEditTime;
  auto currentValue = getValue();
  if (!forceOverwrite)
    lastEditTime = getUiRelated()->steadyTime;

  // we either force an overwrite or we check if there was a very recent change to the control
  // we're about to create an event for
  bool overwriteInstead = forceOverwrite || getUiRelated()->steadyTime - oldEditTime <= Control::kUndoTimeout;
  if (overwriteInstead)
  {
    overwriteInstead = false;
    while (action)
    {
      if (ParameterUpdate::isOfType(action) && ((ParameterUpdate *)action)->control == this)
      {
        ((ParameterUpdate *)action)->newValue = currentValue;
        overwriteInstead = true;
        break;
      }

      action = action->next;
    }
  }

  // create a parameter change event if one doesn't already exist
  if (!overwriteInstead)
    undoManager.perform(anew(undoManager.beginNewTransaction(),
      ParameterUpdate, { this, previousValue, currentValue }));
}

static utils::string_view
findClosestInteger(utils::string_view string)
{
  usize i = 0;
  for (; i < string.size(); ++i)
    if (string[i] >= '0' && string[i] <= '9')
      break;

  usize j = i;
  for (; j < string.size(); ++j)
    if (string[j] < '0' || string[j] > '9')
      break;

  return utils::string_view{ string.data() + i, j - i };
}

static float getProbablyLongestParameterValue(const ParameterDetails &details, float sampleRate)
{
  float lowestValue = scaleValue(0.0f, details, sampleRate, true);
  float highestValue = scaleValue(1.0f, details, sampleRate, true);

  float probablyLongestValue = (::fabs(lowestValue) < ::fabs(highestValue)) ? highestValue : lowestValue;
  // we have nextafter at home
  if (probablyLongestValue < 0.0)
    probablyLongestValue = probablyLongestValue + 0.1f;
  else
    probablyLongestValue = probablyLongestValue - 0.1f;
  return probablyLongestValue;
}

float
Control::getNumericTextMaxWidth(FontId usedFont, float lineHeight)
{
  COMPLEX_ASSERT(details.scale != ParameterScale::Indexed);

  float probablyLongestValue = getProbablyLongestParameterValue(details,
    getUiRelated()->plugin.getSampleRate());

  char buffer[64]{};
  usize size{};

  if (details.toString)
  {
    size = details.toString(buffer, sizeof(buffer), probablyLongestValue, details);
  }
  else
  {
    size = utils::floatToString(probablyLongestValue, buffer, sizeof(buffer), maxDecimalCharacters,
      controlFlags.shouldUsePlusMinusPrefix || details.minValue < 0.0f);
  }

  auto maxStringLength = utils::string{ getLocalScratch(), { buffer, size } };
  if (!prefix.empty())
    maxStringLength.prepend(prefix);
  if (!details.displayUnits.empty())
    maxStringLength.append(details.displayUnits);

  getUiRelated()->g->setFont(usedFont, lineHeight);
  return getUiRelated()->g->getStringWidthFloat(maxStringLength);
}

void Control::getScaledValueString(utils::string &outString,
  float valueToConvert, bool addPrefix) const
{
  // if the string doesn't have an allocated capacity,
  // provide it through the control's pool
  if (!outString.capacity())
    outString = { pool };

  if (!parameter)
  {
    utils::floatToString(outString, valueToConvert);
    return;
  }

  if (details.scale == ParameterScale::Toggle)
  {
    bool boolean = ::round(valueToConvert) != 0.0;
    outString.copy((boolean) ? utils::string_view{ "On" } : utils::string_view{ "Off" });
    return;
  }

  float sampleRate = getUiRelated()->plugin.getSampleRate();
  float scaledValue = scaleValue(valueToConvert, details, sampleRate, true);

  if (details.scale == ParameterScale::Indexed)
  {
    auto [option, index] = getOptionFromValue(scaledValue, details);

    outString.copy(option->getText());

    if (option->valueCount > 1 || option->dynamicUpdateUuid)
      outString.appendFormat(" %zu", (index + 1));

    while (option->parent)
    {
      if (!option->parent->displayName.empty())
        outString.prependFormat("%v / ", option->parent->displayName);
      option = option->parent;
    }
  }
  else if (details.toString)
  {
    char buffer[64]{};
    usize size = details.toString(buffer, sizeof(buffer), scaledValue, details);
    outString.copy({ buffer, size });
  }
  else if ((details.scale == ParameterScale::SymmetricLoudness ||
    details.scale == ParameterScale::Loudness || controlFlags.shouldCheckDbInfinities) &&
    (utils::closeTo(kMinusInfDb, scaledValue) || utils::closeTo(kInfDb, scaledValue)))
  {
    outString.copy("+inf");
    if (scaledValue < 0.0f)
      outString[0] = '-';
    else if (!controlFlags.shouldUsePlusMinusPrefix)
      outString.removeFirst(1);
  }
  else
  {
    utils::floatToString(outString, scaledValue,
      maxDecimalCharacters,
      controlFlags.shouldUsePlusMinusPrefix);

    usize maxIntergerCharacters = 0;
    float probablyLongestValue = ::fabsf(getProbablyLongestParameterValue(details, sampleRate));
    if (probablyLongestValue < 1.0f)
      maxIntergerCharacters = 1;
    else
    {
      while (probablyLongestValue >= 1.0f)
      {
        probablyLongestValue *= 0.1f;
        ++maxIntergerCharacters;
      }
    }
    usize necessaryIntegerCharacters = utils::min((usize)preferredIntegerCharacters, maxIntergerCharacters);

    utils::string_view currentInteger = findClosestInteger(outString);
    if (currentInteger.size() > maxIntergerCharacters)
    {
      outString.removeLast(currentInteger.size() - maxIntergerCharacters);
      if (outString.back() == '.')
        outString.removeLast(1);
    }
    else if (currentInteger.size() < necessaryIntegerCharacters)
      outString.insert(outString.find(currentInteger),
        "0", necessaryIntegerCharacters - currentInteger.size());
  }

  if (addPrefix)
    outString.prepend(prefix);

  outString.append(details.displayUnits);
}

float
Control::getValueFromText(utils::string_view text) const
{
  utils::string trimmed{ getLocalScratch(), text };
  trimmed.trim().toLower();

  if (text.back() == '%' && details.displayUnits != "%")
    return 0.01f * ::strtof(text.data(), nullptr);

  if (details.scale == ParameterScale::Indexed)
  {
    auto parsedValue = getValueFromOptionText(text, details);
    if (parsedValue >= 0.0f)
      return unscaleValue(parsedValue, details);
  }

  if (!details.displayUnits.empty() && trimmed.rfind(details.displayUnits))
    trimmed.removeLast(details.displayUnits.size());

  while (trimmed.front() == '+')
    trimmed.removeFirst(1).trim();

  float parsedValue = ::strtof(trimmed.filterIn("0123456789.,-").data(), nullptr);
  return unscaleValue(parsedValue, details, true);
}

struct MappingParameterUpdate : public UndoAction
{
  MappingParameterUpdate(HostParameter *bridge,
    Parameter *parameter = nullptr) : bridge{ bridge }, parameter{ parameter }
  {
    UndoAction::redo = redo;
    UndoAction::undo = redo;
  }

  static bool
  redo(UndoAction *a)
  {
    auto *self = (MappingParameterUpdate *)a;
    auto g = self->bridge->state->plugin->acquireProcessingLock();
    auto oldParameter = self->bridge->getParameter();
    auto newParameter = self->parameter;

    if (oldParameter && oldParameter->parentProcessor && oldParameter->parentProcessor->metadata->automationMappingChangedCallback)
      oldParameter->parentProcessor->metadata->automationMappingChangedCallback(
        oldParameter->parentProcessor, oldParameter, false);

    self->bridge->resetParameter(newParameter);

    if (newParameter && newParameter->parentProcessor && newParameter->parentProcessor->metadata->automationMappingChangedCallback)
      newParameter->parentProcessor->metadata->automationMappingChangedCallback(
        newParameter->parentProcessor, newParameter, true);

    HostParameter::notifyParameterChange();
    self->parameter = oldParameter;

    return true;
  }

  static bool isOfType(UndoAction *a) { return a->redo == redo && a->undo == redo; }

  HostParameter *bridge;
  Parameter *parameter;
};

void handleControlPopupResult(Control *control, PopupItem *selectedItem)
{
  auto &plugin = getUiRelated()->plugin;

  auto createMappingParameterUpdate = [&plugin](HostParameter *bridge, Parameter *parameter = nullptr)
  {
    // combine as many mappings into a single transaction
    auto [pool, lastAction] = plugin.undoManager.getCurrent();
    if (!pool || (lastAction && !MappingParameterUpdate::isOfType(lastAction)))
      pool = plugin.undoManager.beginNewTransaction();

    plugin.undoManager.perform(anew(pool, MappingParameterUpdate, { bridge, parameter }));
  };

  auto result = selectedItem->id;

  if (result == kDefaultValue)
  {
    control->resetValue();
  }
  else if (result == kManualEntry)
  {
    //auto font = getUiRelated()->cache->getInterFont();
    control->showTextEntry();
  }
  else if (result == kCopyValue || result == kCopyNormalisedValue)
  {
    char stringBuffer[128]{};
    usize stringSize{};
    float currentValue = control->getValue();

    if (!control->parameter || result == kCopyNormalisedValue)
      stringSize = utils::floatToString(currentValue, stringBuffer, sizeof(stringBuffer) - 1, 5);
    else
    {
      stringSize = utils::floatToString(scaleValue(
        currentValue, control->details, plugin.getSampleRate(), true),
        stringBuffer, sizeof(stringBuffer) - 1, 5);
    }

    getUiRelated()->renderer->setClipboard({ (byte *)stringBuffer, stringSize });
  }
  else if (result == kPasteValue)
  {
    auto stringData = getUiRelated()->renderer->getClipboard();
    if (!stringData.empty())
      control->setValue(control->getValueFromText({ (char *)stringData.data(), stringData.size() }), true);
  }
  else if (result == kClearMapping)
  {
    if (!control->parameter)
      return;

    auto hostParameter = control->parameter->hostParameter.load(satomi::memory_order_acquire);
    if (!hostParameter)
      return;

    createMappingParameterUpdate(hostParameter);
  }
  else if (result == kMapFirstSlot)
  {
    for (auto &parameter : plugin.state_->parameterBridges)
    {
      if (!parameter.getParameter())
      {
        createMappingParameterUpdate(&parameter, control->parameter);
        break;
      }
    }
  }
  else if (result >= kMappingList)
  {
    bool isUnmapping = isUnmappingParameter(result);
    usize index = getParameterIndexFromId(result);

    // if negative we unmap the current parameter there
    if (isUnmapping)
    {
      createMappingParameterUpdate(&plugin.state_->parameterBridges[index]);
      selectedItem->parent->skinOverride = control->getSkinOverride();
    }
    else
    {
      if (!control->parameter)
        return;

      createMappingParameterUpdate(&plugin.state_->parameterBridges[index], control->parameter);
    }
  }
}

//static void textEditorCallback(Component *component, TextEditor::CallbackFlags flags, TextEditor &editor)
//{
//  editor.componentFlags.isVisible = false;
//
//  auto *control = (Control *)component;
//  if (!control || !control->controlFlags.canInputValue)
//    return;
//
//  if (flags == TextEditor::EnterPressed && !editor.enteredText.empty())
//  {
//    auto value = control->getValueFromText(editor.enteredText);
//    control->setValue(value, true);
//    if (auto hostParameter = control->parameterLink->hostParameter)
//      hostParameter->setValueFromUI((float)value);
//  }
//}

void Control::showTextEntry()
{
  if (!controlFlags.canInputValue)
    return;

  controlFlags.isInModalState = true;

  //std::string text = (!hasParameter()) ? floatToString(getValue()) :
  //  floatToString(scaleValue(getValue(), details_, getSampleRate(), true), maxDecimalCharacters_);

  //textEntry_->setText(text);
  //textEntry_->selectAll();
  //if (textEntry_->isVisible())
  //  textEntry_->grabKeyboardFocus();
  //textEntry_->setVisible(true);
}

void Control::showPopup(bool primary)
{
  if (!controlFlags.shouldShowPopup)
    return;

  auto *popupDisplay = getPopupDisplay(primary);
  popupDisplay->setContentControl(this, popupPlacement, popupOffset.toInt());
}

void Control::hidePopup(bool primary)
{
  if (!controlFlags.shouldShowPopup)
    return;

  auto *popupDisplay = getPopupDisplay(primary);
  popupDisplay->reset();
}
