
// Created: 2023-07-31 19:37:43

#include "Control.hpp"

#include "pugl/pugl.h"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../Sections/MainInterface.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
  Control::Control()
  {
    componentFlags.clickable = true;
    componentFlags.clickableChildren = false;
  }

  Framework::ParameterLink *
  Control::setParameterLink(Framework::ParameterLink *newParameterLink)
  {
    auto replacedLink = parameterLink;
    if (replacedLink)
      replacedLink->parameter->changeControl(nullptr);

    parameterLink = newParameterLink;

    if (parameterLink)
      parameterLink->parameter->changeControl(this);

    return replacedLink;
  }

  Framework::ParameterValue *
  Control::changeLinkedParameter(Framework::ParameterValue &parameter, 
    bool getValueFromParameter)
  {
    controlFlags.hasParameter = true;

    auto replacedLink = setParameterLink(parameter.getParameterLink());

    Framework::ParameterValue *replacedParameter = nullptr;
    if (replacedLink)
      replacedParameter = replacedLink->parameter;

    details = parameter.getParameterDetails();
    {
      controlFlags.isBipolar = -details.minValue == details.maxValue || 
        details.scale == Framework::ParameterScale::SymmetricQuadratic ||
        details.scale == Framework::ParameterScale::SymmetricCubic ||
        details.scale == Framework::ParameterScale::SymmetricLoudness ||
        details.scale == Framework::ParameterScale::SymmetricFrequency;
      controlFlags.shouldCheckDbInfinities =
        details.scale == Framework::ParameterScale::Loudness ||
        details.scale == Framework::ParameterScale::SymmetricLoudness;
      controlFlags.shouldUsePlusMinusPrefix |= 
        details.scale == Framework::ParameterScale::SymmetricLoudness;
    }
    
    if (getValueFromParameter)
      setValue(parameterLink->parameter->getNormalisedValue());
    else
      parameterLink->parameter->updateNormalisedValue();
    
    return replacedParameter;
  }

  void Control::setValueToHost() const
  {
    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->setValueFromUI((float)getValue());
  }

  bool 
  Control::setValue(double newValue, bool notify)
  {
    newValue = utils::clamp(newValue, 0.0, 1.0);
    auto oldValue = value.exchange(newValue, satomi::memory_order_relaxed);
    if (newValue == oldValue)
      return false;

    if (details.scale == Framework::ParameterScale::Toggle)
    {
      if (Framework::scaleValue(newValue, details) ==
        Framework::scaleValue(oldValue, details))
        return false;
    }
    else if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto oldScaled = Framework::scaleValue(oldValue, details);
      auto newScaled = Framework::scaleValue(newValue, details);

      if (Framework::getOptionFromValue(oldScaled, details) ==
        Framework::getOptionFromValue(newScaled, details))
        return false;
    }

    if (notify && valueChangedCallback)
      valueChangedCallback(this, newValue, oldValue);

    return true;
  }

  void Control::beginChange(double oldValue)
  {
    valueBeforeChange = oldValue;
  }

  struct ParameterUpdate final : public Framework::UndoAction
  {
    ParameterUpdate(Interface::Control *control, double oldValue, double newValue) :
      control(control), oldValue(oldValue), newValue(newValue)
    {
      UndoAction::redo = redo;
      UndoAction::undo = undo;
      //UndoAction::combineActions = combineActions;
    }

    static void redo(UndoAction *a)
    {
      auto *self = (ParameterUpdate *)a;

      if (self->control->setValue(self->newValue, true))
        self->control->setValueToHost();
    }

    static void undo(UndoAction *a)
    {
      auto *self = (ParameterUpdate *)a;

      if (self->control->setValue(self->oldValue, false))
        self->control->setValueToHost();
    }

    //static UndoAction *
    //combineActions(utils::bumpArena *, UndoAction *currentAction, UndoAction *nextAction)
    //{
    //  if (currentAction->redo != nextAction->redo ||
    //    currentAction->undo != nextAction->undo)
    //    return nullptr;
    //
    //  auto *current = (ParameterUpdate *)currentAction;
    //  auto *next = (ParameterUpdate *)nextAction;
    //
    //  if (current->control != next->control)
    //    return nullptr;
    //
    //  current->newValue = next->newValue;
    //
    //  return currentAction;
    //}

    static bool isOfType(Framework::UndoAction *a) { return a->redo == redo && a->undo == undo; }

    Interface::Control *control;
    double oldValue;
    double newValue;
  };

  void Control::endChange()
  {
    auto &plugin = getPlugin(uiRelated.renderer);
    auto [storage, action] = plugin.undoManager.getCurrent();

    auto oldEditTime = lastEditTime;
    lastEditTime = uiRelated.steadyTime;

    if (uiRelated.steadyTime - oldEditTime <= kUndoTimeout)
    {
      while (action)
      {
        if (ParameterUpdate::isOfType(action) && ((ParameterUpdate *)action)->control == this)
        {
          ((ParameterUpdate *)action)->newValue = getValue();
          return;
        }

        action = action->next;
      } 
    }

    storage = plugin.undoManager.beginNewTransaction();
    plugin.undoManager.perform(anew(storage, ParameterUpdate,
      { this, valueBeforeChange, getValue() }));
  }

  void Control::resetValue()
  {
    bool isMapped = parameterLink && parameterLink->hostControl;
    if (isMapped)
      parameterLink->hostControl->beginChangeGesture();

    beginChange(getValue());

    setValue(details.defaultNormalisedValue, true);
    setValueToHost();

    endChange();

    if (isMapped)
      parameterLink->hostControl->endChangeGesture();
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
    render(OpenGlWrapper &openGl) override
    {
      if ((componentFlags.isHovered || componentFlags.isClicked) && canBeChosen)
        fillRect(openGl, getLocalBounds().toFloat(), getHighlightColour(), scaleValue(rounding));

      if (id > kMappingList && isUnmappingParameter(id))
      {
        auto crossBounds = getLocalBounds().toFloat().trimmed(scaleValue(padding.toFloat()));
        crossBounds.trim(crossBounds.w * 0.25f, crossBounds.h * 0.25f);

        nvgBeginPath(openGl);
        nvgMoveTo(openGl, crossBounds.x, crossBounds.y);
        nvgLineTo(openGl, crossBounds.getRight(), crossBounds.getBottom());

        nvgMoveTo(openGl, crossBounds.x, crossBounds.getBottom());
        nvgLineTo(openGl, crossBounds.getRight(), crossBounds.y);

        nvgStrokeColor(openGl, getColour(Skin::kTextComponentText1, this));
        nvgStroke(openGl);
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

          fn(*openGl.cache, colours,
            scaleValue(iconSizes.toFloat()).withCentre(getLocalBounds().toFloat().getCentre()), 
            scaleValue(1.0f));
          break;
        }
        case kCopyValue:
        {
          auto [fn, iconSizes] = Paths::copyScaledValueIcon();

          fn(*openGl.cache, colours,
            scaleValue(iconSizes.toFloat()).withCentre(getLocalBounds().toFloat().getCentre()),
            scaleValue(1.0f));
          break;
        }
        case kPasteValue:
        {
          auto [fn, iconSizes] = Paths::pasteValueIcon();

          fn(*openGl.cache, colours,
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
        fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kPopupSelectorDelimiter, this));
      }

      //strokeRect(openGl, getLocalBounds().trimmed(scaleValueRoundInt(padding.toInt())).toFloat(), 
      //  1.0f, Colour{ 128, 128, 128 });

      if (id == kMappingList)
      {
        float width = scaleValueRound(kPopupSubwindowArrowWidth);
        auto arrowBounds = getLocalBounds().trimmed(scaleValueRoundInt(padding.toInt())).toFloat();

        float yCenter = bounds.h * 0.5f;
        float height = width;

        nvgStrokeWidth(openGl, scaleValue(1.0f));
        nvgBeginPath(openGl);
        nvgMoveTo(openGl.g, arrowBounds.getRight() - width, yCenter - height);
        nvgLineTo(openGl, arrowBounds.getRight(), yCenter);
        nvgLineTo(openGl, arrowBounds.getRight() - width, yCenter + height);
        nvgStrokeColor(openGl.g, getColour(Skin::kTextComponentText2, this));
        nvgStroke(openGl);
      }

      if (!text.empty())
      {
        auto textBounds = getLocalBounds().trimmed(scaleValueRoundInt(padding.toInt())).toFloat();
        textBounds.h = ::roundf(scaleValue((float)desiredSize.h));

        renderText(text, FontId::InterType, textBounds, openGl,
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
      savedText.reserve(localScratch, 31);
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
      utils::span parameterBridges = getPlugin(uiRelated.renderer).state_->parameterBridges;
      
      if (parameterBridges[index].isMappedToParameter())
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
    uiRelated.cache->setFont(FontId::InterType, height);

    if (!isCalculatingVertical)
    {
      minSize = (canTextWrap) ? 0 : (i32)::ceilf(uiRelated.cache->getStringWidthFloat(text));
      if (item->id == kMappingList)
      {
        minSize += scaleValueRoundInt((float)(kPopupSubwindowArrowMargin + kPopupSubwindowArrowWidth));
      }

      maxSize = -1;
    }
    else
    {
      auto lineCount = uiRelated.cache->getStringNumberOfLines(text, (float)item->bounds.w);
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
    auto *itemArena = selector->arena;
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
    options.desiredSize = { kPopupMinWidth, 0, utils::max_limit<i32>, utils::max_limit<i32> };
    options.draw = [](OpenGlWrapper &openGl, PopupList *self)
    {
      auto topPadding = scaleValue((float)self->padding.y);
      auto bottomPadding = scaleValue((float)self->padding.h);

      fillRect(openGl, self->getLocalBounds().toFloat(), getColour(Skin::kBody, self),
        topPadding, topPadding, bottomPadding, bottomPadding);

      self->doRenderChildren(openGl);

      strokeRect(openGl, self->getLocalBounds().toFloat(), scaleValue(1.0f), Colour{ 45,45,45 },
        topPadding);

      return false;
    };

    options.addChildComponent(TEXT_ITEM(kName, options, .extraData = this, .canTextWrap = true,
      .padding = secondaryPadding, .desiredSize = secondaryDesiredSize, .canBeChosen = false, 
      .textColourId = Skin::kTextComponentText2));
    options.addChildComponent(TEXT_ITEM(kDefaultValue, options, .shortcutKeyCode = 'D'));

    if (details.flags & Framework::ParameterDetails::Automatable)
    {
      if (parameterLink && parameterLink->hostControl)
      {
        options.addChildComponent(TEXT_ITEM(kClearMapping, options, .shortcutKeyCode = 'C'));
      }
      else
      {
        PopupItem &mapFirstSlot = with_val(*anew(itemArena, PopupItem, {}), .id = kMapFirstSlot,
          .associatedList = &options, .shortcutKeyCode = 'A', .padding = (Rectangle<u16>{ 12, 4, 12, 4 }),
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
        utils::span parameterBridges = getPlugin(uiRelated.renderer).state_->parameterBridges;
        for (usize i = 0; i < parameterBridges.size(); ++i)
        {
          if (!parameterBridges[i].isMappedToParameter())
          {
            mappingList.childList->addChildComponent(TEXT_ITEM(kMappingList + 1 + (i32)i * 2, (*mappingList.childList)));

            continue;
          }

          COMPLEX_ASSERT(parameterBridges[i].getParameterLink()->UIControl);
          auto useOverride = parameterBridges[i].getParameterLink()->UIControl->getSkinOverride();

          Component &wrapper = with_val(*anew(itemArena, Component, {}), .sizingFlags = Component::GrowableX);
          ControlPopupItem *parameterMap = TEXT_ITEM(kMappingList + 1 + (i32)i * 2, (*mappingList.childList),
            .sizingFlags = Component::GrowableX, .placement = Placement::left, .skinOverride = useOverride, 
            .textColourId = Skin::kWidgetPrimary1, .canBeChosen = false);
          ControlPopupItem *parameterUnmap = TEXT_ITEM(kMappingList + 1 + (i32)i * 2 + 1, (*mappingList.childList),
            .sizingFlags = Component::None, .padding = (Rectangle<u16>{ 4, 4, 4, 4 }), .closesPopup = false,
            .desiredSize = (Rectangle{ kPrimaryTextLineHeight, kPrimaryTextLineHeight, kPrimaryTextLineHeight, kPrimaryTextLineHeight }), 
            .skinOverride = useOverride/*, .rounding = padding.x*/);
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

    Component &inlineGroup = with_val(*anew(itemArena, Component, {}), .arena = itemArena,
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

  static double getProbablyLongestParameterValue(const Framework::ParameterDetails &details, float sampleRate)
  {
    double lowestValue = Framework::scaleValue(0.0, details, sampleRate, true);
    double highestValue = Framework::scaleValue(1.0, details, sampleRate, true);

    double probablyLongestValue = (::fabs(lowestValue) < ::fabs(highestValue)) ? highestValue : lowestValue;
    // we have nextafter at home
    if (probablyLongestValue < 0.0)
      probablyLongestValue = probablyLongestValue + 0.1;
    else
      probablyLongestValue = probablyLongestValue - 0.1;
    return probablyLongestValue;
  }

  float 
  Control::getNumericTextMaxWidth(FontId usedFont, float lineHeight)
  {
    COMPLEX_ASSERT(details.scale != Framework::ParameterScale::Indexed);

    double probablyLongestValue = getProbablyLongestParameterValue(details, 
      getPlugin(uiRelated.renderer).getSampleRate());

    char buffer[64]{};
    usize size{};

    if (details.generateNumeric)
    {
      size = details.generateNumeric(buffer, sizeof(buffer), probablyLongestValue, details);
    }
    else
    {
      size = utils::floatToString(probablyLongestValue, buffer, sizeof(buffer), maxDecimalCharacters,
        controlFlags.shouldUsePlusMinusPrefix || details.minValue < 0.0f);
    }

    auto maxStringLength = utils::string{ localScratch, { buffer, size } };
    if (!prefix.empty())
      maxStringLength.prepend(prefix);
    if (!details.displayUnits.empty())
      maxStringLength.append(details.displayUnits);

    uiRelated.cache->setFont(usedFont, lineHeight);
    return uiRelated.cache->getStringWidthFloat(maxStringLength);
  }

  void Control::getScaledValueString(utils::string &outString,
    double valueToConvert, bool addPrefix) const
  {
    // if the string doesn't have an allocated capacity, 
    // provide it through the control's arena
    if (!outString.capacity())
      outString = { arena };
    
    if (!controlFlags.hasParameter)
    {
      utils::floatToString(outString, valueToConvert);
      return;
    }

    if (details.scale == Framework::ParameterScale::Toggle)
    {
      bool boolean = ::round(valueToConvert) != 0.0;
      outString.copy((boolean) ? utils::string_view{ "On" } : utils::string_view{ "Off" });
      return;
    }

    float sampleRate = getPlugin(uiRelated.renderer).getSampleRate();
    double scaledValue = Framework::scaleValue(valueToConvert, details, sampleRate, true);

    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto [option, index] = Framework::getOptionFromValue(scaledValue, details);

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
    else if (details.generateNumeric)
    {
      char buffer[64]{};
      usize size = details.generateNumeric(buffer, sizeof(buffer), scaledValue, details);
      outString.copy({ buffer, size });
    }
    else if ((details.scale == Framework::ParameterScale::SymmetricLoudness ||
      details.scale == Framework::ParameterScale::Loudness || controlFlags.shouldCheckDbInfinities) &&
      (utils::closeTo(kMinusInfDb, scaledValue) || utils::closeTo(kInfDb, scaledValue)))
    {
      outString.copy("+inf");
      if (scaledValue < 0.0)
        outString[0] = '-';
      else if (!controlFlags.shouldUsePlusMinusPrefix)
        outString.removePrefix(1);
    }
    else
    {
      utils::floatToString(outString, scaledValue,
        maxDecimalCharacters,
        controlFlags.shouldUsePlusMinusPrefix);

      usize maxIntergerCharacters = 0;
      double probablyLongestValue = ::fabs(getProbablyLongestParameterValue(details, sampleRate));
      if (probablyLongestValue < 1.0)
        maxIntergerCharacters = 1;
      else
      {
        while (probablyLongestValue >= 1.0)
        {
          probablyLongestValue *= 0.1;
          ++maxIntergerCharacters;
        }
      }
      usize necessaryIntegerCharacters = utils::min((usize)preferredIntegerCharacters, maxIntergerCharacters);

      utils::string_view currentInteger = findClosestInteger(outString);
      if (currentInteger.size() > maxIntergerCharacters)
      {
        outString.removeSuffix(currentInteger.size() - maxIntergerCharacters);
        if (outString.back() == '.')
          outString.removeSuffix(1);
      }
      else if (currentInteger.size() < necessaryIntegerCharacters)
        outString.insert(outString.find(currentInteger),
          "0", necessaryIntegerCharacters - currentInteger.size());
    }

    if (addPrefix)
      outString.prepend(prefix);

    outString.append(details.displayUnits);
  }

  double 
  Control::getValueFromText(utils::string_view text) const
  {
    utils::string trimmed{ localScratch, text };
    trimmed.trim().toLower();

    if (text.back() == '%' && details.displayUnits != "%")
      return 0.01 * ::strtod(text.data(), nullptr);

    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto parsedValue = Framework::getValueFromOptionText(text, details);
      if (parsedValue >= 0.0)
        return Framework::unscaleValue(parsedValue, details);
    }

    if (!details.displayUnits.empty() && trimmed.rfind(details.displayUnits))
      trimmed.removeSuffix(details.displayUnits.size());

    while (trimmed.front() == '+')
      trimmed.removePrefix(1).trim();

    double parsedValue = ::strtod(trimmed.filterIn("0123456789.,-").data(), nullptr);
    return Framework::unscaleValue(parsedValue, details, true);
  }

  struct MappingParameterUpdate : public Framework::UndoAction
  {
    MappingParameterUpdate(Framework::ParameterBridge *bridge, 
      Framework::ParameterLink *link = nullptr) : bridge{ bridge }, link{ link }
    {
      // TODO: if you map and unmap the same parameter this transaction will contain both actions 
      //  instead of self cancelling and freeing the arena

      UndoAction::redo = redo;
      UndoAction::undo = redo;
    }

    static void redo(UndoAction *a)
    {
      auto *self = (MappingParameterUpdate *)a;
      
      if (self->link)
      {
        COMPLEX_ASSERT(!self->bridge->getParameterLink());
        self->bridge->resetParameterLink(self->link);

        Framework::ParameterBridge::notifyParameterChange();
        if (self->link->UIControl && self->link->UIControl->automationMappingChangedCallback)
          self->link->UIControl->automationMappingChangedCallback(self->link->UIControl, false);

        self->link = nullptr;
      }
      else
      {
        self->link = self->bridge->getParameterLink();
        COMPLEX_ASSERT(self->link);

        //Framework::ParameterBridge::notifyParameterChange();
        if (self->link->UIControl && self->link->UIControl->automationMappingChangedCallback)
          self->link->UIControl->automationMappingChangedCallback(self->link->UIControl, true);

        self->bridge->resetParameterLink(nullptr);
      }
    }

    static bool isOfType(UndoAction *a) { return a->redo == redo && a->undo == redo; }
    
    Framework::ParameterBridge *bridge;
    Framework::ParameterLink *link;
  };

  void handleControlPopupResult(Control *control, PopupItem *selectedItem)
  {
    auto &plugin = getPlugin(uiRelated.renderer);

    auto createMappingParameterUpdate = [&plugin](Framework::ParameterBridge *bridge, Framework::ParameterLink *link = nullptr)
    {
      // combine as many mappings into a single transaction
      auto *arena = plugin.undoManager.getCurrentTransaction();
      if (auto lastAction = plugin.undoManager.getLastAction(); !arena || (lastAction && !MappingParameterUpdate::isOfType(lastAction)))
        arena = plugin.undoManager.beginNewTransaction();

      plugin.undoManager.perform(anew(arena, MappingParameterUpdate, { bridge, link }));
    };

    auto result = selectedItem->id;

    if (result == kDefaultValue)
    {
      control->beginChange(control->getValue());
      control->resetValue();
      control->endChange();
    }
    else if (result == kManualEntry)
    {
      //auto font = uiRelated.cache->getInterFont();
      control->showTextEntry();
    }
    else if (result == kCopyValue || result == kCopyNormalisedValue)
    {
      char stringBuffer[128]{};
      usize stringSize{};
      double currentValue = control->getValue();

      if (!control->controlFlags.hasParameter || result == kCopyNormalisedValue)
        stringSize = utils::floatToString(currentValue, stringBuffer, sizeof(stringBuffer) - 1, 5);
      else
      {
        stringSize = utils::floatToString(Framework::scaleValue(
          currentValue, control->details, plugin.getSampleRate(), true),
          stringBuffer, sizeof(stringBuffer) - 1, 5);
      }

      puglSetClipboard(getPuglView(uiRelated.renderer), nullptr, stringBuffer, stringSize);
    }
    else if (result == kPasteValue)
    {
      usize length;
      const char *string = (const char *)puglGetClipboard(getPuglView(uiRelated.renderer), 0, &length);
      if (string && length > 0)
        control->setValue(control->getValueFromText(utils::string_view{ string, length }));
    }
    else if (result == kClearMapping)
    {
      if (!control->parameterLink || !control->parameterLink->hostControl)
        return;

      createMappingParameterUpdate(control->parameterLink->hostControl);
    }
    else if (result == kMapFirstSlot)
    {
      for (auto &parameter : plugin.state_->parameterBridges)
      {
        if (!parameter.isMappedToParameter())
        {
          createMappingParameterUpdate(&parameter, control->parameterLink);
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
      }
      else
      {
        if (!control->parameterLink)
          return;

        createMappingParameterUpdate(&plugin.state_->parameterBridges[index], control->parameterLink);
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
  //    if (auto hostControl = control->parameterLink->hostControl)
  //      hostControl->setValueFromUI((float)value);
  //  }
  //}

  void Control::showTextEntry()
  {
    if (!controlFlags.canInputValue)
      return;

    controlFlags.isInModalState = true;

    //std::string text = (!hasParameter()) ? floatToString(getValue()) :
    //  floatToString(Framework::scaleValue(getValue(), details_, getSampleRate(), true), maxDecimalCharacters_);

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

}
