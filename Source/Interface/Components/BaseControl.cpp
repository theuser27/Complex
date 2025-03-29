/*
  ==============================================================================

    BaseControl.cpp
    Created: 31 Jul 2023 7:37:43pm
    Author:  theuser27

  ==============================================================================
*/

#include "BaseControl.hpp"

#include "Framework/update_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "OpenGlImage.hpp"
#include "../Sections/MainInterface.hpp"

namespace Interface
{
  BaseControl::BaseControl() = default;
  BaseControl::~BaseControl() { setParameterLink(nullptr); }

  Framework::ParameterLink *BaseControl::setParameterLink(Framework::ParameterLink *parameterLink) noexcept
  {
    auto replacedLink = parameterLink_;
    if (replacedLink)
      replacedLink->parameter->changeControl(nullptr);

    parameterLink_ = parameterLink;

    if (parameterLink_)
      parameterLink_->parameter->changeControl(this);

    return replacedLink;
  }

  Framework::ParameterValue *BaseControl::changeLinkedParameter(Framework::ParameterValue &parameter, bool getValueFromParameter)
  {
    hasParameter_ = true;

    auto name = parameter.getParameterDetails().id;
    setName({ name.data(), name.size() });
    auto replacedLink = setParameterLink(parameter.getParameterLink());

    Framework::ParameterValue *replacedParameter = nullptr;
    if (replacedLink)
      replacedParameter = replacedLink->parameter;

    setParameterDetails(parameter.getParameterDetails());
    
    if (getValueFromParameter)
      setValueFromParameter();
    else
      setValueToParameter();

    setResetValue(details_.defaultNormalisedValue);

    return replacedParameter;
  }

  bool BaseControl::setValueFromHost(double value, 
    Framework::ParameterBridge *notifyingBridge) noexcept
  {
    if (!parameterLink_ || parameterLink_->hostControl != notifyingBridge)
      return false;

    double currentValue = getValueRaw();
    if (value == currentValue)
      return false;

    setValueRaw(value);

    // checking effective value for discrete parameters
    if (details_.scale == Framework::ParameterScale::Toggle ||
      details_.scale == Framework::ParameterScale::Indexed ||
      details_.scale == Framework::ParameterScale::IndexedNumeric)
    {
      if (Framework::scaleValue(value, details_) ==
        Framework::scaleValue(currentValue, details_))
        return false;
    }

    return true;
  }

  void BaseControl::setValueFromParameter() noexcept
  {
    if (!parameterLink_ || !parameterLink_->parameter)
      return;

    double value = parameterLink_->parameter->getNormalisedValue();
    if (value == getValueRaw())
      return;

    setValueRaw(value);
    valueChanged();
  }

  void BaseControl::setValueToHost() const noexcept
  {
    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->setValueFromUI((float)getValueRaw());
  }

  void BaseControl::setValueToParameter() const noexcept
  {
    if (parameterLink_ && parameterLink_->parameter)
      parameterLink_->parameter->updateNormalisedValue();
  }

  void BaseControl::valueChanged()
  {
    for (auto *listener : controlListeners_)
      listener->controlValueChanged(this);
  }

  void BaseControl::beginChange(double oldValue) noexcept
  {
    valueBeforeChange_ = oldValue;
    hasBegunChange_ = true;
  }

  void BaseControl::endChange()
  {
    hasBegunChange_ = false;
    uiRelated.renderer->getPlugin().pushUndo(
      new Framework::ParameterUpdate(this, valueBeforeChange_, getValueRaw()));
  }

  void BaseControl::resized()
  {
    setColours();
    setComponentsBounds();
    setExtraElementsPositions(drawBounds_.isEmpty() ? getLocalBounds() : drawBounds_);
    repositionExtraElements();
  }

  void BaseControl::moved()
  {
    repositionExtraElements();
  }

  void BaseControl::parentHierarchyChanged()
  {
    BaseComponent::parentHierarchyChanged();
    parent_ = findParentComponentOfClass<BaseSection>();
  }

  void BaseControl::setPosition(Point<int> position)
  {
    int scaledTop = scaleValueRoundInt((float)addedHitbox_.getTop());
    int scaledLeft = scaleValueRoundInt((float)addedHitbox_.getLeft());
    int scaledBottom = scaleValueRoundInt((float)addedHitbox_.getBottom());
    int scaledRight = scaleValueRoundInt((float)addedHitbox_.getRight());

    COMPLEX_ASSERT(!drawBounds_.isEmpty(), "You need to call setSizes with \
      specific dimensions so that size can be calculated and stored");

    // offsetting the drawBounds and subsequently the extra elements bounds if origin position was changed
    drawBounds_.setPosition({ scaledLeft, scaledTop });
    OpenGlContainer::setBounds(position.x - scaledLeft, position.y - scaledTop,
      drawBounds_.getWidth() + scaledLeft + scaledRight,
      drawBounds_.getHeight() + scaledTop + scaledBottom);
  }

  void BaseControl::setBounds(int x, int y, int width, int height)
  {
    // if for some reason we didn't use the setSizes -> setPosition(Point<int>) path 
    // for setting bounds of this particular control, we set drawBounds to the specified size
    // and expand the overall size to accommodate an added hitbox if necessary
    
    int scaledTop = scaleValueRoundInt((float)addedHitbox_.getTop());
    int scaledLeft = scaleValueRoundInt((float)addedHitbox_.getLeft());
    int scaledBottom = scaleValueRoundInt((float)addedHitbox_.getBottom());
    int scaledRight = scaleValueRoundInt((float)addedHitbox_.getRight());
    drawBounds_ = { scaledLeft, scaledTop, width, height };
    
    OpenGlContainer::setBounds(x - scaledLeft, y - scaledTop,
      width + scaledLeft + scaledRight, height + scaledTop + scaledBottom);
  }

  void BaseControl::repositionExtraElements()
  {
    for (auto &extraElement : extraElements_.data)
      extraElement.first->setBounds(extraElement.first->getParentComponent()->getLocalArea(this, extraElement.second));
  }

  void BaseControl::renderOpenGlComponents(OpenGlWrapper &openGl)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::SpinNotify };
    ScopedBoundsEmplace b{ openGl.parentStack, this };

    for (auto &openGlComponent : openGlComponents_)
    {
      if (openGlComponent->isVisibleSafe() && !openGlComponent->isAlwaysOnTopSafe())
      {
        openGlComponent->doWorkOnComponent(openGl);
        COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
      }
    }

    for (auto &openGlComponent : openGlComponents_)
    {
      if (openGlComponent->isVisibleSafe() && openGlComponent->isAlwaysOnTopSafe())
      {
        openGlComponent->doWorkOnComponent(openGl);
        COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
      }
    }
  }

  void BaseControl::setColours()
  {
    if (parameterLink_)
      parameterLink_->parameter->setThemeColour(getThemeColour().getARGB());
  }

  void BaseControl::addLabel()
  {
    if (label_)
      return;
    
    label_ = utils::up<PlainTextComponent>::create("Control Label",
      (hasParameter()) ? String{ details_.displayName.data(), details_.displayName.size() } : getName());
    label_->setFontType(PlainTextComponent::kText);
    label_->setTextHeight(Fonts::kInterVDefaultHeight);
    addOpenGlComponent(label_.get());
    label_->setIgnoreClip(this);
  }

  void BaseControl::removeLabel()
  { 
    if (!label_)
      return;

    removeOpenGlComponent(label_.get());
    label_ = nullptr;
  }

  void BaseControl::resetValue() noexcept
  {
    bool isMapped = parameterLink_ && parameterLink_->hostControl;
    if (isMapped)
      parameterLink_->hostControl->beginChangeGesture();

    if (!hasBegunChange_)
      beginChange(getValueRaw());

    setValue(resetValue_, sendNotificationSync);
    setValueToHost();

    if (isMapped)
      parameterLink_->hostControl->endChangeGesture();
  }

  PopupItems BaseControl::createPopupMenu() const noexcept
  {
    PopupItems options{};
    options.addDelimiter(std::string{ details_.displayName.data(), details_.displayName.size() });
    auto &defaultValue = options.addEntry(kDefaultValue, "Set to D" COMPLEX_UNDERSCORE_LITERAL "efault Value");
    defaultValue.shortcut = 'D';

    //if (hasParameterAssignment_)
    //  options.addItem(kArmMidiLearn, "Learn MIDI Assignment");

    //if (hasParameterAssignment_ && interfaceEngineLink_->getPlugin()->isMidiMapped(getName().toStdString()))
    //  options.addItem(kClearMidiLearn, "Clear MIDI Assignment");

    if (details_.flags & Framework::ParameterDetails::Automatable)
    {
      if (parameterLink_ && parameterLink_->hostControl)
      {
        auto &clearMapping = options.addEntry(kClearMapping, "C" COMPLEX_UNDERSCORE_LITERAL "lear Parameter Mapping");
        clearMapping.shortcut = 'C';
      }
      else
      {
        auto &mapFirstSlot = options.addEntry(kMapFirstSlot, "Make a" COMPLEX_UNDERSCORE_LITERAL "utomatable", "Assign to first free Automation Slot");
        mapFirstSlot.shortcut = 'A';

        PopupItems automationSlots{ PopupItems::AutomationList, kMappingList, "Assign automation slot" };

        options.addItem(COMPLEX_MOVE(automationSlots));
      }
    }

    String value = getScaledValueString(getValueRaw(), false) + " " COMPLEX_MIDDLE_DOT_LITERAL " " + getNormalisedValueString(getValueRaw());
    options.addDelimiter("Value", value.toStdString());
    auto &group = options.addInlineGroup();

    auto &copyNormalised = group.addEntry(kCopyNormalisedValue, {}, "Copy Normalised Value");
    copyNormalised.icon = Paths::copyNormalisedValueIcon();

    auto &copyScaled = group.addEntry(kCopyValue, {}, "Copy Scaled Value");
    copyScaled.icon = Paths::copyScaledValueIcon();

    auto &paste = group.addEntry(kPasteValue, {}, "Paste Value");
    paste.icon = Paths::pasteValueIcon();

    if (canInputValue_)
    {
      auto &entry = group.addEntry(kManualEntry, {}, "Enter Value");
      entry.icon = Paths::enterValueIcon();
    }

    return options;
  }

  juce::Rectangle<int> BaseControl::getUnionOfAllElements() const noexcept
  {
    auto bounds = drawBounds_;
    /*if (label_ && label_->isVisible())
      bounds = bounds.getUnion(label_->getBounds());*/
    for (usize i = 0; i < extraElements_.data.size(); i++)
      if (extraElements_.data[i].first->isVisible())
        bounds = bounds.getUnion(extraElements_[i]);
    return bounds;
  }

  double BaseControl::getValueFromText(const String &text) const
  {
    String trimmed = text.removeCharacters(" ").toLowerCase();
    if (text.endsWithChar('%') && details_.displayUnits != "%")
      return 0.01 * trimmed.removeCharacters("%").getDoubleValue();
    if (!details_.indexedData.empty())
    {
      for (int i = 0; i <= (int)(details_.maxValue - details_.minValue); ++i)
        if (trimmed == String{ details_.indexedData[i].displayName.data(), 
          details_.indexedData[i].displayName.size() }.toLowerCase())
          return Framework::unscaleValue(details_.minValue + (float)i, details_, true);
    }

    auto t = text.trimStart();
    if (!details_.displayUnits.empty() && t.endsWith(details_.displayUnits.data()))
      t = t.substring(0, t.length() - (int)details_.displayUnits.length());

    while (t.startsWithChar('+'))
      t = t.substring(1).trimStart();

    double value = t.initialSectionContainingOnly("0123456789.,-").getDoubleValue();
    return Framework::unscaleValue(value, details_, true);
  }

  void BaseControl::handlePopupResult(int result)
  {
    auto &plugin = uiRelated.renderer->getPlugin();

    if (result == kDefaultValue)
    {
      beginChange(getValueRaw());
      resetValue();
      endChange();
    }
    else if (result == kManualEntry)
      showTextEntry();
    else if (result == kCopyValue || result == kCopyNormalisedValue)
    {
      String string{};
      double value = getValueRaw();

      if (!hasParameter() || result == kCopyNormalisedValue)
        string = { value, 6 };
      else
        string = String{ Framework::scaleValue(value, details_, plugin.getSampleRate(), true) };

      SystemClipboard::copyTextToClipboard(string);
    }
    else if (result == kPasteValue)
    {
      auto string = SystemClipboard::getTextFromClipboard();
      if (!string.isEmpty())
        setValue(getValueFromText(string));
    }
    else if (result == kClearMapping)
    {
      if (!parameterLink_ || !parameterLink_->hostControl)
        return;

      parameterLink_->hostControl->resetParameterLink(nullptr);
      for (auto &listener : controlListeners_)
        listener->automationMappingChanged(this, true);
    }
    else if (result == kMapFirstSlot)
    {
      auto parameters = plugin.getParameterBridges();

      for (auto &parameter : parameters)
      {
        if (!parameter->isMappedToParameter())
        {
          parameter->resetParameterLink(getParameterLink());
          for (auto &listener : controlListeners_)
            listener->automationMappingChanged(this, false);
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
        auto *link = plugin.getParameterBridges()[index]->getParameterLink();
        plugin.getParameterBridges()[index]->resetParameterLink(nullptr);
        if (link && link->UIControl)
          for (auto *listener : link->UIControl->controlListeners_)
            listener->automationMappingChanged(link->UIControl, true);
      }
      else
      {
        if (!parameterLink_)
          return;

        plugin.getParameterBridges()[index]->resetParameterLink(parameterLink_);
        for (auto &listener : controlListeners_)
          listener->automationMappingChanged(this, false);
      }
    }
  }

  void ControlContainer::repositionControls()
  {
    if (isArranging_)
      return;
    isArranging_ = true;

    if (controls_.empty())
      return;

    bool isHorizontal = bounds_.getWidth() >= bounds_.getHeight();
    bool isReverse = false;
    int x, y, hSpacing, vSpacing;
    int *mainPosition, *secondaryPosition, *mainSpacing;
    std::tuple<int, int, int> (*getSecondaryPosition)(juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds);

    auto calculateMainSpacing = [&]()
    {
      std::tuple<int, int, int> total{ 0, 0, 0 };
      for (auto &[control, sizes] : controls_)
      {
        if (!control->isVisible())
          continue;

        COMPLEX_ASSERT(sizes.first || sizes.second);
        auto controlBounds = control->setSizes(sizes.second, sizes.first);
        std::get<0>(total) += controlBounds.getWidth();
        std::get<1>(total) += controlBounds.getHeight();
        std::get<2>(total) += 1;
      }
      return total;
    };

    if (isHorizontal)
    {
      vSpacing = 0;
      if (anchor_ == Placement::centerHorizontal)
      {
        auto [width, _, controlsActive] = calculateMainSpacing();
        hSpacing = (bounds_.getWidth() - width) / (controlsActive - 1);
      }
      else
        hSpacing = scaleValueRoundInt((float)controlSpacing_);
      x = bounds_.getX();
      if (anchor_ == Placement::right)
      {
        x = bounds_.getRight();
        isReverse = true;
      }

      switch (anchor_ & Placement::centerVertical)
      {
      case Placement::above:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getX(), controlBounds.getWidth(), bounds.getY() }; };
        break;
      case Placement::below:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getX(), controlBounds.getWidth(), bounds.getBottom() - controlBounds.getHeight() }; };
        break;
      default:
      case Placement::centerVertical:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getX(), controlBounds.getWidth(), 
          bounds.getY() + utils::centerAxis(controlBounds.getHeight(), bounds.getHeight()) }; };
        break;
      }

      mainPosition = &x;
      secondaryPosition = &y;
      mainSpacing = &hSpacing;
    }
    else
    {
      hSpacing = 0;
      if (anchor_ == Placement::centerHorizontal)
      {
        auto [_, height, controlsActive] = calculateMainSpacing();
        vSpacing = (bounds_.getHeight() - height) / (controlsActive - 1);
      }
      else
        vSpacing = scaleValueRoundInt((float)controlSpacing_);
      y = bounds_.getY();
      if (anchor_ == Placement::below)
      {
        y = bounds_.getBottom();
        isReverse = true;
      }

      switch (anchor_ & Placement::centerHorizontal)
      {
      case Placement::left:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getY(), controlBounds.getHeight(), bounds.getX() }; };
        break;
      case Placement::right:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getY(), controlBounds.getHeight(), bounds.getRight() - controlBounds.getWidth() }; };
        break;
      default:
      case Placement::centerHorizontal:
        getSecondaryPosition = [](juce::Rectangle<int> bounds, juce::Rectangle<int> controlBounds)
        { return std::tuple{ controlBounds.getY(), controlBounds.getHeight(), 
          bounds.getX() + utils::centerAxis(controlBounds.getWidth(), bounds.getWidth()) }; };
        break;
      }

      mainPosition = &y;
      secondaryPosition = &x;
      mainSpacing = &vSpacing;
    }

    for (auto &[control, sizes] : controls_)
    {
      if (!control->isVisible())
        continue;

      COMPLEX_ASSERT(sizes.first || sizes.second);
      auto controlBounds = control->setSizes(sizes.second, sizes.first);
      auto [position, postOffset, secondary] = getSecondaryPosition(bounds_, controlBounds);
      *secondaryPosition = secondary;
      int nextMainPosition;

      if (isReverse)
      {
        nextMainPosition = *mainPosition - *mainSpacing - postOffset;
        *mainPosition -= postOffset + position;
      }
      else
      {
        nextMainPosition = *mainPosition + *mainSpacing + postOffset;
        *mainPosition -= position;
      }

      control->setPosition({ x, y });
      *mainPosition = nextMainPosition;
    }

    isArranging_ = false;
  }
}
