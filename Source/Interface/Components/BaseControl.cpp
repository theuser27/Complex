
// Created: 2023-07-31 19:37:43

#include "BaseControl.hpp"

#include "pugl/pugl.h"

#include "Framework/update_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
//#include "../LookAndFeel/Graphics.hpp"
//#include "TextEditor.hpp"
#include "../Sections/MainInterface.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
  BaseControl::BaseControl(Framework::ParameterValue *parameter)
  {
    if (!parameter)
      return;

    changeLinkedParameter(*parameter, true);
  }

  Framework::ParameterLink *
  BaseControl::setParameterLink(Framework::ParameterLink *newParameterLink)
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
  BaseControl::changeLinkedParameter(Framework::ParameterValue &parameter, 
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

    if (details.scale == Framework::ParameterScale::Indexed)
      valueInterval = 1.0 / (details.options->count - 1);
    else
      valueInterval = 0.0;

    return replacedParameter;
  }

  bool 
  BaseControl::setValueFromHost(double newValue, 
    Framework::ParameterBridge *notifyingBridge)
  {
    if (!parameterLink || parameterLink->hostControl != notifyingBridge)
      return false;

    double currentValue = getValue();
    if (newValue == currentValue)
      return false;

    value.store(newValue, satomi::memory_order_release);

    // checking effective value for discrete parameters
    if (details.scale == Framework::ParameterScale::Toggle ||
      details.scale == Framework::ParameterScale::Indexed)
    {
      if (Framework::scaleValue(newValue, details) ==
        Framework::scaleValue(currentValue, details))
        return false;
    }

    return true;
  }

  void BaseControl::setValueToHost() const
  {
    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->setValueFromUI((float)getValue());
  }

  bool 
  BaseControl::setValue(double newValue, bool notify)
  {
    newValue = utils::clamp(newValue, 0.0, 1.0);
    auto oldValue = value.exchange(newValue, satomi::memory_order_relaxed);
    if (newValue == oldValue)
      return false;

    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto oldScaled = Framework::scaleValue(oldValue, details);
      auto newScaled = Framework::scaleValue(newValue, details);

      if (Framework::getIndexedData(oldScaled, details) ==
        Framework::getIndexedData(newScaled, details))
        return false;
    }

    if (notify)
      valueChanged();

    return true;
  }

  void BaseControl::valueChanged()
  {
    for (auto *listener : controlListeners)
      listener->controlValueChanged(this);
  }

  void BaseControl::beginChange(double oldValue)
  {
    valueBeforeChange = oldValue;
    controlFlags.hasBegunChange = true;
  }

  void BaseControl::endChange()
  {
    controlFlags.hasBegunChange = false;
    getPlugin(uiRelated.renderer).pushUndo(
      new Framework::ParameterUpdate(this, valueBeforeChange, getValue()));
  }

  void BaseControl::resetValue()
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
    bool render(OpenGlWrapper &openGl) override
    {
      (void)openGl;

      // TODO:

      return true;
    }
  };

  static Range<i32>
  getItemTextMetrics(Component *c, i32 *availableWidth)
  {
    auto *item = (ControlPopupItem *)c;

    utils::string_view text{};
    utils::string savedText{};
    bool canTextWrap = false;
    float height = BaseControl::kPopupPrimaryFontHeight;

    switch (item->id)
    {
    case kName:
      text = ((BaseControl *)item->extraData)->details.displayName;
      height = BaseControl::kPopupSecondaryFontHeight;
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
      height = BaseControl::kPopupSecondaryFontHeight;
      canTextWrap = true;
      break;

    case kMappingList:
      text = "Assign automation slot";
      break;

    case kValueDisplayText:
      text = "Value";
      height = BaseControl::kPopupSecondaryFontHeight;
      break;

    case kValueDisplay:
    {
      auto *control = (BaseControl *)item->extraData;
      savedText = control->getScaledValueString(
        utils::bumpArena::fromAllocation(item), control->getValue(), false);

      char unscaledValueStringBuffer[64];
      usize stringSize = utils::floatToString(control->getValue(), 
        unscaledValueStringBuffer, sizeof(unscaledValueStringBuffer) - 1);

      savedText.appendFormat(" " COMPLEX_MIDDLE_DOT_LITERAL " %v",
        utils::string_view{ unscaledValueStringBuffer, stringSize });
      text = savedText;
      height = BaseControl::kPopupSecondaryFontHeight;
      break;
    }
    default:
      COMPLEX_ASSERT_FALSE("Unexpected Item");
      break;
    }
    
    i32 minSize{}, maxSize{};

    uiRelated.cache->setFont(uiRelated.cache->getFont(
      uiRelated.cache->InterFontId, scaleValue(height)));

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

  void handlePopupResult(BaseControl *control, PopupItem *selectedItem);

  void BaseControl::createPopupMenu(PopupSelector *selector, Point<i32> position)
  {
    auto *itemArena = selector->arena;
  
  #define ITEM(idNumber, ...) (&with_val(*anew(itemArena, ControlPopupItem, {}), .id = idNumber, __VA_OPT__(,) __VA_ARGS__))
  #define TEXT_ITEM(idNumber, ...) ITEM(idNumber, \
    .desiredSize.getTextDimensions = getItemTextMetrics, .sizingFlags = HasText)

    ControlPopupItem &options = *ITEM(kTopLevel);
    options.sizingFlags = Component::IsVertical;
    options.sublistMinSize = { kPopupMinWidth, 0 };

    options.addChildComponent(TEXT_ITEM(kName, .sizingFlags |= Component::SameAsSiblingsX, .extraData = this));
    options.addChildComponent(TEXT_ITEM(kDefaultValue, .shortcutKeyCode = 'D', .sizingFlags |= Component::SameAsSiblingsX));

    //if (hasParameterAssignment_)
    //  options.addItem(kArmMidiLearn, "Learn MIDI Assignment");

    //if (hasParameterAssignment_ && interfaceEngineLink_->getPlugin()->isMidiMapped(getName().toStdString()))
    //  options.addItem(kClearMidiLearn, "Clear MIDI Assignment");

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
          .sizingFlags |= Component::SameAsSiblingsX | Component::IsVertical);
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
    .desiredSize.minMax = (Rectangle<i32>{ iconWidthHeight, iconWidthHeight, iconWidthHeight, iconWidthHeight }))

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
    { handlePopupResult(this, selectedItem); };
    selector->cancel = {};
    selector->summon(this, position);
  }


  float 
  BaseControl::getNumericTextMaxWidth(const Font &usedFont) const
  {
    usize integerPlaces = utils::max(maxTotalCharacters - maxDecimalCharacters, 0U);
    // for the separating '.' between integer and decimal parts
    if (maxDecimalCharacters)
      integerPlaces--;

    utils::string maxStringLength;
    if (controlFlags.shouldUsePlusMinusPrefix || details.minValue < 0.0f)
      maxStringLength.append("+");

    // figured out that 8s take up the most space in DDin
    for (usize i = 0; i < integerPlaces; i++)
      maxStringLength.append("8");

    if (maxDecimalCharacters)
    {
      maxStringLength.append(".");

      for (usize i = 0; i < maxDecimalCharacters; i++)
        maxStringLength.append("8");
    }

    maxStringLength.append(details.displayUnits);

    uiRelated.cache->setFont(usedFont);
    return uiRelated.cache->getStringWidthFloat(maxStringLength);
  }

  utils::string 
  BaseControl::getScaledValueString(utils::Allocator allocator,
    double valueToConvert, bool addPrefix) const
  {
    if (!controlFlags.hasParameter)
      return utils::floatToString(allocator, valueToConvert);

    if (details.scale == Framework::ParameterScale::Toggle)
      return utils::floatToString(allocator, ::round(valueToConvert));

    double scaledValue = Framework::scaleValue(valueToConvert, details,
      getPlugin(uiRelated.renderer).getSampleRate(), true);

    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto [option, index] = Framework::getIndexedData(scaledValue, details);

      utils::string string{ allocator, option->displayName };
      if (option->count > 1)
        string.appendFormat(" %zu", (index + 1));

      if (addPrefix)
        string.prepend(popupPrefix);

      return string;
    }
    
    auto formatValue = [&]()
    {
      if ((details.scale == Framework::ParameterScale::SymmetricLoudness || 
          details.scale == Framework::ParameterScale::Loudness || controlFlags.shouldCheckDbInfinities) &&
        (utils::closeTo(kMinusInfDb, scaledValue) || utils::closeTo(kInfDb, scaledValue)))
      {
        utils::string format = "+inf";
        if (scaledValue < 0.0)
          format[0] = '-';
        else if (!controlFlags.shouldUsePlusMinusPrefix)
          format.removePrefix(1);

        return format;
      }

      utils::string format;

      usize integerCharacters = maxTotalCharacters;
      if (maxDecimalCharacters == 0)
        format = utils::floatToString(allocator, ::round(scaledValue));
      else
      {
        format = utils::floatToString(allocator, scaledValue, maxDecimalCharacters,
          controlFlags.shouldUsePlusMinusPrefix);
        // +1 because of the dot
        integerCharacters -= maxDecimalCharacters + 1;
      }

      usize numberOfIntegers = format.find(".");
      usize insertIndex = 0;
      usize displayCharacters = maxTotalCharacters;
      if (format[0] == '+' || format[0] == '-')
      {
        insertIndex++;
        numberOfIntegers--;
        displayCharacters += 1;
      }

      // insert leading zeroes
      if (integerCharacters > numberOfIntegers)
        format.insert(insertIndex, "0", integerCharacters - numberOfIntegers);

      // truncating string to fit
      format = format.clone(0, displayCharacters);
      if (format.back() == '.')
        format.removeSuffix(1);

      return format;
    };

    auto format = formatValue();
    format.prepend(popupPrefix).append(details.displayUnits);
    return format;
  }

  double 
  BaseControl::getValueFromText(utils::string_view text) const
  {
    utils::string trimmed = text;
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

  void handlePopupResult(BaseControl *control, PopupItem *selectedItem)
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
      auto font = uiRelated.cache->getInterFont();
      control->showTextEntry(font);
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
      for (auto &listener : control->controlListeners)
        listener->automationMappingChanged(control, true);
    }
    else if (result == kMapFirstSlot)
    {
      for (auto &parameter : plugin.state_->parameterBridges)
      {
        if (!parameter.isMappedToParameter())
        {
          parameter.resetParameterLink(control->parameterLink);
          Framework::ParameterBridge::notifyParameterChange();
          for (auto &listener : control->controlListeners)
            listener->automationMappingChanged(control, false);
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
        if (link && link->UIControl)
          for (auto *listener : link->UIControl->controlListeners)
            listener->automationMappingChanged(link->UIControl, true);
      }
      else
      {
        if (!control->parameterLink)
          return;

        auto &bridge = plugin.state_->parameterBridges[index];
        bridge.resetParameterLink(control->parameterLink);
        Framework::ParameterBridge::notifyParameterChange();
        for (auto &listener : control->controlListeners)
          listener->automationMappingChanged(control, false);
      }
    }
  }

  //static void textEditorCallback(Component *component, TextEditor::CallbackFlags flags, TextEditor &editor)
  //{
  //  editor.componentFlags.isVisible = false;

  //  auto *control = (BaseControl *)component;
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

  void BaseControl::showTextEntry(Font font)
  {
    (void)font;
  }

  void BaseControl::showPopup(bool primary)
  {
    if (!controlFlags.shouldShowPopup)
      return;

    (void)primary;
    //auto *popupDisplay = getGui(uiRelated.renderer)->getPopupDisplay(primary);
    //popupDisplay->setContentControl(this, popupPlacement_);
    //popupDisplay->componentFlags.isVisible = true;
  }

  void BaseControl::hidePopup(bool primary)
  {
    if (!controlFlags.shouldShowPopup)
      return;

    (void)primary;
    //auto *popupDisplay = getGui(uiRelated.renderer)->getPopupDisplay(primary);
    //popupDisplay->componentFlags.isVisible = false;
  }

  bool 
  Label::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
    //nvgSave(openGl.g);

    //openGl.cache->setFont(openGl.cache->getInterFont());
    //nvgText(openGl.g, )

    //nvgRestore(openGl.g);

    return true;
  }
}
