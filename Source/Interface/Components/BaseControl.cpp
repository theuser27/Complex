
// Created: 2023-07-31 19:37:43

#include "BaseControl.hpp"

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

      if (Framework::getIndexedData(oldScaled, details) ==
        Framework::getIndexedData(newScaled, details))
        return false;
    }

    if (notify && valueChangedCallback)
      valueChangedCallback(this, newValue, oldValue);

    return true;
  }

  void Control::beginChange(double oldValue)
  {
    valueBeforeChange = oldValue;
    controlFlags.hasBegunChange = true;
  }

  struct ParameterUpdate final : public Framework::UndoAction
  {
    ParameterUpdate(Interface::Control *control, double oldValue, double newValue) :
      control(control), oldValue(oldValue), newValue(newValue)
    {
      redo = [](UndoAction *a)
      {
        auto *self = (ParameterUpdate *)a;

        self->control->setValue(self->newValue, false);
        self->control->setValueToHost();
      };

      undo = [](UndoAction *a)
      {
        auto *self = (ParameterUpdate *)a;

        self->control->setValue(self->oldValue, false);
        self->control->setValueToHost();
      };

      combineActions = [](utils::bumpArena *,
        UndoAction *currentAction, UndoAction *nextAction) -> UndoAction *
      {
        if (currentAction->redo == nextAction->redo &&
          currentAction->undo == nextAction->undo)
        {
          ((ParameterUpdate *)currentAction)->newValue =
            ((ParameterUpdate *)nextAction)->newValue;

          return currentAction;
        }

        return nullptr;
      };
    }

    Interface::Control *control;
    double oldValue;
    double newValue;
  };

  void Control::endChange()
  {
    controlFlags.hasBegunChange = false;
    auto &plugin = getPlugin(uiRelated.renderer);
    auto *storage = plugin.undoManager.beginNewTransaction();
    plugin.undoManager.perform(anew(storage, ParameterUpdate, 
      { this, valueBeforeChange, getValue() }), false);
  }

  void Control::resetValue()
  {
    bool isMapped = parameterLink && parameterLink->hostControl;
    if (isMapped)
      parameterLink->hostControl->beginChangeGesture();

    if (!controlFlags.hasBegunChange)
      beginChange(getValue());

    setValue(details.defaultNormalisedValue, true);
    setValueToHost();

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
    kValueDisplayText,
    kValueDisplay,
    kControlMenuIdsSize,
    kMappingList = 64,
  };

  struct ControlPopupItem : PopupItem
  {
    bool
    render(OpenGlWrapper &openGl) override
    {
      (void)openGl;

      // TODO:

      return true;
    }
  };

  static Range<i32>
  getControlPopupItemTextMetrics(Component *c, i32 *availableWidth)
  {
    auto *item = (ControlPopupItem *)c;

    utils::string_view text{};
    utils::string savedText{};
    bool canTextWrap = false;
    float height = kPrimaryTextLineHeight;

    switch (item->id)
    {
    case kName:
      text = ((Control *)item->extraData)->details.displayName;
      height = kSecondaryTextLineHeight;
      canTextWrap = true;
      break;

    case kDefaultValue:
      text = "Set to D" COMPLEX_UNDERSCORE_LITERAL "efault Value";
      break;

    case kClearMapping:
      text = "C" COMPLEX_UNDERSCORE_LITERAL "lear Parameter Mapping";
      break;

    case kMapFirstSlot:
      text = "Make a" COMPLEX_UNDERSCORE_LITERAL "utomatable";
      break;

    case kMapFirstSlotSubtitle:
      text = "Assign to first free Automation Slot";
      height = kSecondaryTextLineHeight;
      canTextWrap = true;
      break;

    case kMappingList:
      text = "Assign automation slot";
      break;

    case kValueDisplayText:
      text = "Value";
      height = kSecondaryTextLineHeight;
      break;

    case kValueDisplay:
    {
      auto *control = (Control *)item->extraData;
      savedText.reserve(utils::bumpArena::fromAllocation(item));
      control->getScaledValueString(savedText, control->getValue(), false);

      char unscaledValueStringBuffer[64];
      usize stringSize = utils::floatToString(control->getValue(), 
        unscaledValueStringBuffer, sizeof(unscaledValueStringBuffer) - 1);

      savedText.appendFormat(" " COMPLEX_MIDDLE_DOT_LITERAL " %v",
        utils::string_view{ unscaledValueStringBuffer, stringSize });
      text = savedText;
      height = Control::kPopupSecondaryFontHeight;
      break;
    }
    default:
      COMPLEX_ASSERT_FALSE("Unexpected Item");
      break;
    }
    
    i32 minSize{}, maxSize{};

    uiRelated.cache->setFont(Graphics::InterType, scaleValue(height));

    if (!availableWidth)
    {
      maxSize = (i32)::ceilf(uiRelated.cache->getStringWidthFloat(text));
      minSize = (canTextWrap) ? 0 : maxSize;
    }
    else
    {
      auto area = uiRelated.cache->getStringBoundsMultiline(text, (float)(*availableWidth));
      minSize = maxSize = (i32)area.h;
    }

    return { minSize, maxSize };
  }

  void handleControlPopupResult(Control *control, PopupItem *selectedItem);

  void Control::createPopupMenu(PopupSelector *selector, Point<i32> position)
  {
    auto *itemArena = selector->arena;
  
  #define ITEM(idNumber, ...) (&with_val(*anew(itemArena, ControlPopupItem, {}), \
    .arena = itemArena, .id = idNumber, __VA_OPT__(,) __VA_ARGS__))
  #define TEXT_ITEM(idNumber, ...) ITEM(idNumber, \
    .getDimensions = getControlPopupItemTextMetrics, .sizingFlags = Component::CustomDimensions)

    ControlPopupItem &options = *ITEM(kTopLevel);
    options.componentFlags.isVertical = true;
    options.sublistMinSize = { kPopupMinWidth, 0 };

    options.addChildComponent(TEXT_ITEM(kName, .sizingFlags |= Component::SameAsSiblingsX, .extraData = this));
    options.addChildComponent(TEXT_ITEM(kDefaultValue, .shortcutKeyCode = 'D', .sizingFlags |= Component::SameAsSiblingsX));

    if (details.flags & Framework::ParameterDetails::Automatable)
    {
      if (parameterLink && parameterLink->hostControl)
      {
        options.addChildComponent(TEXT_ITEM(kClearMapping, 
          .shortcutKeyCode = 'C', .sizingFlags |= Component::SameAsSiblingsX));
      }
      else
      {
        ControlPopupItem &mapFirstSlot = *ITEM(kMapFirstSlot, .shortcutKeyCode = 'A',
          .sizingFlags |= Component::SameAsSiblingsX, .componentFlags.isVertical = true);
        options.addChildComponent(&mapFirstSlot);
        mapFirstSlot.addChildComponent(TEXT_ITEM(kMapFirstSlot));
        mapFirstSlot.addChildComponent(TEXT_ITEM(kMapFirstSlotSubtitle,
          .sizingFlags |= Component::SameAsSiblingsX));

        ControlPopupItem &mappingList = *TEXT_ITEM(kMappingList);
        options.addChildComponent(&mappingList);
        utils::span parameterBridges = getPlugin(uiRelated.renderer).state_->parameterBridges;
        for (usize i = 0; i < parameterBridges.size(); ++i)
        {
          ControlPopupItem &entry = *TEXT_ITEM(kMappingList + (i32)i + 1);
          if (parameterBridges[i].isMappedToParameter())
          {
            entry.addChildComponent(TEXT_ITEM(kMappingList + (i32)i + 1));
          }

          mappingList.addChildComponent(&entry);
        }
      }
    }

    ControlPopupItem &valueDisplay = *ITEM(kValueDisplay, .sizingFlags |= Component::SameAsSiblingsX);
    options.addChildComponent(&valueDisplay);
    options.addChildComponent(TEXT_ITEM(kValueDisplayText, .placement = Placement::left, .margin = Rectangle<i16>{ 0, 0, 8, 0 }));
    options.addChildComponent(TEXT_ITEM(kValueDisplay, .placement = Placement::right, .extraData = this));

    ControlPopupItem &inlineGroup = *ITEM(kInlineGroup, .sizingFlags |= Component::SameAsSiblingsX);
    options.addChildComponent(&inlineGroup);

    static constexpr auto iconWidthHeight = 16;
  #define ICON(idNumber) ITEM(idNumber, .sizingFlags |= GrowableX | GrowableY, \
    .desiredSize = (Rectangle<i32>{ iconWidthHeight, iconWidthHeight, iconWidthHeight, iconWidthHeight }))

    inlineGroup.addChildComponent(ICON(kCopyNormalisedValue));
    inlineGroup.addChildComponent(ICON(kCopyValue));
    inlineGroup.addChildComponent(ICON(kPasteValue));
    if (controlFlags.canInputValue)
    {
      inlineGroup.addChildComponent(ICON(kManualEntry));
    }

  #undef ICON
  #undef TEXT_ITEM
  #undef ITEM

    selector->items = &options;
    selector->skinOverride = skinOverride;
    selector->callback = [this](PopupSelector *, PopupItem *selectedItem)
    { handleControlPopupResult(this, selectedItem); };
    selector->cancel = {};
    selector->summon(this, position);
  }


  float 
  Control::getNumericTextMaxWidth(FontId usedFont, float lineHeight) const
  {
    COMPLEX_ASSERT(details.scale != Framework::ParameterScale::Indexed);

    auto sampleRate = getPlugin(uiRelated.renderer).getSampleRate();
    double lowestValue = Framework::scaleValue(details.minValue, details, sampleRate, true);
    double highestValue = Framework::scaleValue(details.maxValue, details, sampleRate, true);

    double probablyLongestValue = (::fabs(lowestValue) < ::fabs(highestValue)) ? highestValue : lowestValue;
    // we have nextafter at home
    probablyLongestValue = utils::bit_cast<double>(utils::bit_cast<u64>(probablyLongestValue) - 1);

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

    auto maxStringLength = utils::string::create(localScratch, "%v%v%v", 
      utils::string_view{ popupPrefix }, utils::string_view{ buffer, size }, details.displayUnits);

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
      outString.copy({ (boolean) ? "On" : "Off", (boolean) ? sizeof("On") : sizeof("Off") });
      return;
    }

    double scaledValue = Framework::scaleValue(valueToConvert, details,
      getPlugin(uiRelated.renderer).getSampleRate(), true);

    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto [option, index] = Framework::getIndexedData(scaledValue, details);

      outString.copy(option->displayName);

      if (index > 1)
        outString.appendFormat(" %zu", (index + 1));
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
        ::round(scaledValue) != scaledValue ? maxDecimalCharacters : 0,
        controlFlags.shouldUsePlusMinusPrefix);
    }

    if (addPrefix)
      outString.prepend(popupPrefix);

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
      return Framework::unscaleValue(Framework::getValueFromOptionText(text, details), details);

    if (!details.displayUnits.empty() && trimmed.rfind(details.displayUnits))
      trimmed.removeSuffix(details.displayUnits.size());

    while (trimmed.front() == '+')
      trimmed.removePrefix(1).trim();

    double parsedValue = ::strtod(trimmed.filterIn("0123456789.,-").data(), nullptr);
    return Framework::unscaleValue(parsedValue, details, true);
  }

  void handleControlPopupResult(Control *control, PopupItem *selectedItem)
  {
    auto &plugin = getPlugin(uiRelated.renderer);

    auto result = selectedItem->id;

    if (selectedItem->id == kDefaultValue)
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
        stringSize = utils::floatToString(currentValue, stringBuffer, sizeof(stringBuffer) - 1, 6);
      else
      {
        stringSize = utils::floatToString(Framework::scaleValue(
          currentValue, control->details, plugin.getSampleRate(), true),
          stringBuffer, sizeof(stringBuffer) - 1);
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

      control->parameterLink->hostControl->resetParameterLink(nullptr);
      Framework::ParameterBridge::notifyParameterChange();
      if (control->automationMappingChangedCallback)
        control->automationMappingChangedCallback(control, true);
    }
    else if (result == kMapFirstSlot)
    {
      for (auto &parameter : plugin.state_->parameterBridges)
      {
        if (!parameter.isMappedToParameter())
        {
          parameter.resetParameterLink(control->parameterLink);
          Framework::ParameterBridge::notifyParameterChange();
          if (control->automationMappingChangedCallback)
            control->automationMappingChangedCallback(control, false);
          break;
        }
      }
    }
    else if (result >= kMappingList)
    {
      bool isUnmapping = result % 2 != kMappingList % 2;
      int index = (result - kMappingList) / 2;

      // if negative we unmap the current parameter there
      if (isUnmapping)
      {
        auto &bridge = plugin.state_->parameterBridges[index];
        auto *link = bridge.getParameterLink();
        bridge.resetParameterLink(nullptr);
        Framework::ParameterBridge::notifyParameterChange();
        if (link && link->UIControl && link->UIControl->automationMappingChangedCallback)
          link->UIControl->automationMappingChangedCallback(link->UIControl, true);
      }
      else
      {
        if (!control->parameterLink)
          return;

        auto &bridge = plugin.state_->parameterBridges[index];
        bridge.resetParameterLink(control->parameterLink);
        Framework::ParameterBridge::notifyParameterChange();
        if (control->automationMappingChangedCallback)
          control->automationMappingChangedCallback(control, false);
      }
    }
  }

  //static void textEditorCallback(Component *component, TextEditor::CallbackFlags flags, TextEditor &editor)
  //{
  //  editor.componentFlags.isVisible = false;

  //  auto *control = (Control *)component;
  //  if (!control || !control->controlFlags.canInputValue)
  //    return;

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

    (void)primary;
    //auto *popupDisplay = getGui(uiRelated.renderer)->getPopupDisplay(primary);
    //popupDisplay->setContentControl(this, popupPlacement_);
    //popupDisplay->componentFlags.isVisible = true;
  }

  void Control::hidePopup(bool primary)
  {
    if (!controlFlags.shouldShowPopup)
      return;

    (void)primary;
    //auto *popupDisplay = getGui(uiRelated.renderer)->getPopupDisplay(primary);
    //popupDisplay->componentFlags.isVisible = false;
  }

}
