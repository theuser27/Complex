/*
  ==============================================================================

    BaseSlider.cpp
    Created: 14 Dec 2022 6:59:59am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseSlider.hpp"

#include "Third Party/gcem/gcem.hpp"
#include "Framework/update_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "OpenGlQuad.hpp"
#include "OpenGlImage.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "../Sections/BaseSection.hpp"
#include "TextEditor.hpp"
#include "Plugin/Renderer.hpp"

namespace Interface
{
  BaseSlider::BaseSlider(Framework::ParameterValue *parameter)
  {
    quadComponent_.setInterceptsMouseClicks(false, false);

    imageComponent_.paintEntireComponent(false);
    imageComponent_.setInterceptsMouseClicks(false, false);

    // enabled otherwise text entry gets focus and caret appears
    setWantsKeyboardFocus(true);

    if (!parameter)
      return;
    
    hasParameter_ = true;
    
    auto name = parameter->getParameterDetails().id;
    setName({ name.data(), name.size() });
    setParameterLink(parameter->getParameterLink());
    setParameterDetails(parameter->getParameterDetails());
    setValueRaw(parameterLink_->parameter->getNormalisedValue());

    setResetValue(details_.defaultNormalisedValue, resetValueOnDoubleClick_, resetValueModifiers_);
    setValueInterval();

    setRepaintsOnMouseActivity(false);
  }

  BaseSlider::~BaseSlider() = default;

  void BaseSlider::mouseDown(const MouseEvent &e)
  {
    useDragEvents_ = false;
    lastMouseDragPosition_ = e.position;

    if (!isEnabled())
      return;

    if (e.mods.isAltDown() && canInputValue_)
    {
      showTextEntry();
      return;
    }

    if (e.mods.isPopupMenu())
    {
      showPopupSelector(this, e.getPosition(), createPopupMenu(),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);
      return;
    }

    if (!resetValueOnDoubleClick_ && e.mods.withoutMouseButtons() == resetValueModifiers_)
    {
      resetValue();
      showPopup(true);
      return;
    }

    showPopup(true);

    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->beginChangeGesture();

    beginChange(getValueRaw());

    useDragEvents_ = true;
    mouseDrag(e);

    quadComponent_.getAnimator().setIsClicked(true);
    imageComponent_.getAnimator().setIsClicked(true);

    for (auto *listener : controlListeners_)
      listener->mouseDown(this);
  }

  void BaseSlider::mouseDrag(const MouseEvent &e)
  {
    if (!useDragEvents_ || e.mouseWasClicked())
      return;

    auto mouseDiff = (isHorizontal()) ? 
      e.position.x - lastMouseDragPosition_.x : lastMouseDragPosition_.y - e.position.y;
    lastMouseDragPosition_ = e.position;

    double newPos = getValueRaw() + mouseDiff * (1.0 / immediateSensitivity_);
    newPos = (type_ == SliderType::CanLoopAround) ? newPos - std::floor(newPos) : std::clamp(newPos, 0.0, 1.0);

    setValue(snapValue(newPos, DragMode::absoluteDrag), sendNotificationSync);
    setValueToHost();

    if (!e.mods.isPopupMenu())
      showPopup(true);
  }

  void BaseSlider::mouseUp(const MouseEvent &e)
  {
    if (!useDragEvents_ || e.mods.isPopupMenu() || e.mods.isAltDown())
      return;

    endChange();
    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->endChangeGesture();

    quadComponent_.getAnimator().setIsClicked(false);
    imageComponent_.getAnimator().setIsClicked(false);

    for (auto *listener : controlListeners_)
      listener->mouseUp(this);
  }

  void BaseSlider::mouseEnter(const MouseEvent &)
  {
    quadComponent_.getAnimator().setIsHovered(true);
    imageComponent_.getAnimator().setIsHovered(true);

    for (auto *listener : controlListeners_)
      listener->hoverStarted(this);

    if (showPopupOnHover_)
      showPopup(true);

    if (shouldRepaintOnHover_)
      redoImage();
  }

  void BaseSlider::mouseExit(const MouseEvent &)
  {
    quadComponent_.getAnimator().setIsHovered(false);
    imageComponent_.getAnimator().setIsHovered(false);

    for (auto *listener : controlListeners_)
      listener->hoverEnded(this);

    hidePopup(true);
    if (shouldRepaintOnHover_)
      redoImage();
  }

  void BaseSlider::mouseDoubleClick(const MouseEvent &e)
  {
    if (!isEnabled() || !resetValueOnDoubleClick_ || e.mods.isPopupMenu())
      return;
    
    resetValue();

    for (auto *listener : controlListeners_)
      listener->mouseDoubleClick(this);
    
    showPopup(true);
  }

  void BaseSlider::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    if (!isEnabled() || (!canUseScrollWheel_ || needsToRedirectMouse(e)))
    {
      if (!redirectMouse(RedirectMouseWheel, e, &wheel))
        BaseControl::mouseWheelMove(e, wheel);
      return;
    }

    // sometimes duplicate wheel events seem to be sent, so since we're going to
    // bump the value by a minimum of the interval, avoid doing this twice..
    if (e.eventTime == lastMouseWheelTime_)
      return;
    
    lastMouseWheelTime_ = e.eventTime;

    if (e.mods.isAnyMouseButtonDown())
      return;
    
    auto value = getValue();
    auto mouseWheelDelta = (std::abs(wheel.deltaX) > std::abs(wheel.deltaY)) ? -wheel.deltaX : wheel.deltaY;
    auto valueDelta = value + 0.15 * mouseWheelDelta * (wheel.isReversed ? -1.0 : 1.0);
    valueDelta = (type_ & CanLoopAround) ? valueDelta - std::floor(valueDelta) : std::clamp(valueDelta, 0.0, 1.0);
    valueDelta -= value;
    if (valueDelta == 0.0)
      return;
    
    auto newValue = value + std::max(valueInterval_, std::abs(valueDelta)) * (valueDelta < 0.0 ? -1.0 : 1.0);

    bool isMapped = parameterLink_ && parameterLink_->hostControl;
    if (isMapped)
      parameterLink_->hostControl->beginChangeGesture();

    if (!hasBegunChange_)
      beginChange(value);

    setValue(snapValue(newValue, DragMode::notDragging), sendNotificationSync);
    setValueToHost();

    if (isMapped)
      parameterLink_->hostControl->endChangeGesture();

    showPopup(true);
  }

  void BaseSlider::setValue(double newValue, NotificationType notification)
  {
    newValue = std::clamp(newValue, 0.0, 1.0);
    if (newValue == getValueRaw())
      return;
    
    setValueRaw(newValue);

    if (notification != NotificationType::dontSendNotification)
    {
      valueChanged();
    }
  }

  Framework::ParameterValue *BaseSlider::changeLinkedParameter(Framework::ParameterValue &parameter, bool getValueFromParameter)
  {
    auto *replacedParameter = BaseControl::changeLinkedParameter(parameter, getValueFromParameter);

    setResetValue(details_.defaultNormalisedValue, resetValueOnDoubleClick_, resetValueModifiers_);
    setValueInterval();

    return replacedParameter;
  }

  void BaseSlider::updateValueFromTextEntry()
  {
    if (!textEntry_ || textEntry_->getText().isEmpty())
      return;

    auto value = getValueFromText(textEntry_->getText());
    setValue(value, NotificationType::sendNotificationSync);
    if (auto hostControl = parameterLink_->hostControl)
      hostControl->setValueFromUI((float)value);
  }

  String BaseSlider::getScaledValueString(double value, bool addPrefix) const
  {
    if (!hasParameter())
      return getNormalisedValueString(value);

    double scaledValue = Framework::scaleValue(value, details_, getSampleRate(), true);
    if (!details_.indexedData.empty())
    {
      usize index = (usize)(std::clamp(scaledValue, (double)details_.minValue, 
        (double)details_.maxValue) - details_.minValue);
      usize option = 0;
      while ((index || !details_.indexedData[option].count) && 
        details_.indexedData[option].count <= index)
      {
        index -= details_.indexedData[option].count;
        option++;
      }

      String string = details_.indexedData[option].displayName.data();
      if (details_.indexedData[option].count > 1)
      {
        string += ' ';
        string += (index + 1);
      }
      return (addPrefix) ? popupPrefix_ + string : string;
    }
    
    return popupPrefix_ + formatValue(scaledValue);
  }

  double BaseSlider::snapValue(double attemptedValue, DragMode dragMode)
  {
    static constexpr double percent = 0.025;
    if (!shouldSnapToValue_ || sensitiveMode_ || dragMode != DragMode::absoluteDrag)
      return attemptedValue;

    if (attemptedValue - snapValue_ <= percent && attemptedValue - snapValue_ >= -percent)
      return snapValue_;
    return attemptedValue;
  }

  String BaseSlider::formatValue(double value) const noexcept
  {
    if (details_.scale == Framework::ParameterScale::Indexed ||
      details_.scale == Framework::ParameterScale::IndexedNumeric)
      return String(value) + details_.displayUnits.data();

    if (details_.scale == Framework::ParameterScale::Loudness && utils::closeTo(kInfDb, value))
    {
      String format = "inf";
      format = ((shouldUsePlusMinusPrefix_) ? String("+") : String()) + format;
      return format + details_.displayUnits.data();
    }
    if (auto closeToInf = utils::closeTo(kInfDb, value); details_.scale == Framework::ParameterScale::SymmetricLoudness &&
      (utils::closeTo(kMinusInfDb, value) || closeToInf))
    {
      String format = "inf";
      format = ((closeToInf) ? (shouldUsePlusMinusPrefix_) ? String("+") : String() : String("-")) + format;
      return format + details_.displayUnits.data();
    }

    String format;

    int integerCharacters = maxTotalCharacters_;
    if (maxDecimalCharacters_ == 0)
      format = String(std::round(value));
    else
    {
      format = String(value, maxDecimalCharacters_);
      // +1 because of the dot
      integerCharacters -= maxDecimalCharacters_ + 1;
    }

    int numberOfIntegers = format.indexOfChar('.');
    int insertIndex = 0;
    int displayCharacters = maxTotalCharacters_;
    if (format[0] == '-')
    {
      insertIndex++;
      numberOfIntegers--;
      displayCharacters += 1;
    }
    else if (shouldUsePlusMinusPrefix_)
    {
      insertIndex++;
      displayCharacters += 1;
      format = String("+") + format;
    }

    // insert leading zeroes
    int numZeroesToInsert = integerCharacters - numberOfIntegers;
    for (int i = 0; i < numZeroesToInsert; i++)
      format = format.replaceSection(insertIndex, 0, StringRef("0"));

    // truncating string to fit
    format = format.substring(0, displayCharacters);
    if (format.getLastCharacter() == '.')
      format = format.removeCharacters(".");

    // adding suffix
    return format + details_.displayUnits.data();
  }

  float BaseSlider::getNumericTextMaxWidth(const Font &usedFont) const
  {
    int integerPlaces = std::max(maxTotalCharacters_ - maxDecimalCharacters_, 0);
    // for the separating '.' between integer and decimal parts
    if (maxDecimalCharacters_)
      integerPlaces--;

    String maxStringLength;
    if (shouldUsePlusMinusPrefix_ || details_.minValue < 0.0f)
      maxStringLength += '+';

    // figured out that 8s take up the most space in DDin
    for (int i = 0; i < integerPlaces; i++)
      maxStringLength += '8';

    if (maxDecimalCharacters_)
    {
      maxStringLength += '.';

      for (int i = 0; i < maxDecimalCharacters_; i++)
        maxStringLength += '8';
    }

    maxStringLength += details_.displayUnits.data();

    return usedFont.getStringWidthFloat(maxStringLength);
  }

  void BaseSlider::setValueInterval()
  {
    if (!hasParameter())
      return;

    if (details_.scale == Framework::ParameterScale::Indexed ||
      details_.scale == Framework::ParameterScale::IndexedNumeric)
      valueInterval_ = 1.0 / (details_.maxValue - details_.minValue);
    else
      valueInterval_ = 0.0;
  }

  void BaseSlider::addTextEntry()
  {
    if (textEntry_)
      return;

    canInputValue_ = true;

    textEntry_ = utils::up<TextEditor>::create("Slider Text Entry");
    textEntry_->setMultiLine(false);
    textEntry_->setScrollToShowCursor(true);
    textEntry_->addListener(this);
    textEntry_->setSelectAllWhenFocused(true);
    textEntry_->setKeyboardType(TextEditor::numericKeyboard);
    textEntry_->setJustification(Justification::centred);
    textEntry_->setIndents(0, 0);
    textEntry_->setBorder({ 0, 0, 0, 0 });
    textEntry_->setAlwaysOnTop(true);
    textEntry_->setInterceptsMouseClicks(true, false);
    textEntry_->getImageComponent().setRenderFunction(
      [this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        ScopedBoundsEmplace b{ openGl.parentStack, textEntry_.get() };
        target.render(openGl);
      });
    addChildComponent(textEntry_.get());
  }

  void BaseSlider::removeTextEntry()
  {
    canInputValue_ = false;
    if (textEntry_)
    {
      removeChildComponent(textEntry_.get());
      textEntry_ = nullptr;
    }
  }

  void BaseSlider::changeTextEntryFont(Font font)
  {
    if (textEntry_)
      textEntry_->setUsedFont(COMPLEX_MOV(font));
  }

  void BaseSlider::showTextEntry()
  {
    String text = (!hasParameter()) ? getNormalisedValueString(getValueRaw()) :
      String{ Framework::scaleValue(getValueRaw(), details_, getSampleRate(), true), maxDecimalCharacters_ };

    textEntry_->setText(text);
    textEntry_->selectAll();
    if (textEntry_->isShowing())
      textEntry_->grabKeyboardFocus();
    textEntry_->setVisible(true);
  }

  void BaseSlider::textEditorReturnKeyPressed(TextEditor &editor)
  {
    if (&editor != textEntry_.get())
      return;

    updateValueFromTextEntry();
    textEntry_->setVisible(false);
  }

  void BaseSlider::showPopup(bool primary)
  {
    if (shouldShowPopup_)
      showPopupDisplay(this, getScaledValueString(getValue()), popupPlacement_, primary);
  }

  void BaseSlider::hidePopup(bool primary) { hidePopupDisplay(primary); }	

  float BaseSlider::getSampleRate() const noexcept { return uiRelated.renderer->getPlugin().getSampleRate(); }

  //////////////////////////////////////////////////////////////////////////
  //   _____                 _       _ _           _   _                  //
  //  / ____|               (_)     | (_)         | | (_)                 //
  // | (___  _ __   ___  ___ _  __ _| |_ ___  __ _| |_ _  ___  _ __  ___  //
  //  \___ \| '_ \ / _ \/ __| |/ _` | | / __|/ _` | __| |/ _ \| '_ \/ __| //
  //  ____) | |_) |  __/ (__| | (_| | | \__ \ (_| | |_| | (_) | | | \__ \ //
  // |_____/| .__/ \___|\___|_|\__,_|_|_|___/\__,_|\__|_|\___/|_| |_|___/ //
  //        | |                                                           //
  //        |_|                                                           //
  //////////////////////////////////////////////////////////////////////////

  RotarySlider::RotarySlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    addLabel();
    setLabelPlacement(Placement::right);

    addTextEntry();
    changeTextEntryFont(Fonts::instance()->getDDinFont());

    quadComponent_.setMaxArc(kRotaryAngle);
    quadComponent_.setFragmentShader(Shaders::kRotarySliderFragment);
    quadComponent_.getAnimator().setHoverIncrement(0.15f);
    quadComponent_.setRenderFunction(
      [this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        auto &animator = target.getAnimator();
        animator.tick(openGl.animate);
        float thickness = knobArcThickness_;
        utils::as<OpenGlQuad>(&target)->setThickness(thickness +
          thickness * 0.15f * animator.getValue(Animator::Hover));
        target.render(openGl);
      });

    imageComponent_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>) { drawShadow(g); });

    addOpenGlComponent(&imageComponent_);
    addOpenGlComponent(&quadComponent_);
    addOpenGlComponent(&textEntry_->getImageComponent());

    imageComponent_.setIgnoreClip(this);
    textEntry_->getImageComponent().setIgnoreClip(this);

    // yes i know this is dumb but it works for now
    if (details_.minValue == -details_.maxValue)
    {
      setBipolar(true);
      setShouldUsePlusMinusPrefix(true);
    }
  }

  RotarySlider::~RotarySlider() = default;

  void RotarySlider::mouseDrag(const MouseEvent &e)
  {
    float multiply = 1.0f;

    sensitiveMode_ = e.mods.isShiftDown();
    if (sensitiveMode_)
      multiply *= kSlowDragMultiplier;

    setImmediateSensitivity((int)(kDefaultRotaryDragLength / (sensitivity_ * multiply)));

    BaseSlider::mouseDrag(e);
  }

  void RotarySlider::redoImage()
  {
    if (drawBounds_.getWidth() <= 0 || drawBounds_.getHeight() <= 0)
      return;

    knobArcThickness_ = getValue(Skin::kKnobArcThickness);
    float arc = quadComponent_.getMaxArc();
    quadComponent_.setShaderValue(0, std::lerp(-arc, arc, (float)getValue()));
    quadComponent_.setColor(selectedColor_);
    quadComponent_.setAltColor(unselectedColor_);
    quadComponent_.setThumbColor(thumbColor_);
    quadComponent_.setStartPos(isBipolar() ? 0.0f : -kPi);

    imageComponent_.redrawImage();
    if (!modifier_)
    {
      textEntry_->applyColourToAllText(selectedColor_);
      textEntry_->setText(getScaledValueString(getValue()));
      textEntry_->redrawImage();
    }
  }

  void RotarySlider::setComponentsBounds(bool redoImage)
  {
    auto width = (float)drawBounds_.getWidth();
    auto height = (float)drawBounds_.getHeight();

    float thickness = getValue(Skin::kKnobArcThickness);
    float size = getValue(Skin::kKnobArcSize) * getKnobSizeScale() + thickness;
    float radiusX = (size + 0.5f) / width;
    float radiusY = (size + 0.5f) / height;

    quadComponent_.setQuad(0, -radiusX, -radiusY, 2.0f * radiusX, 2.0f * radiusY);
    quadComponent_.setThumbAmount(getValue(Skin::kKnobHandleLength));
    quadComponent_.setBounds(drawBounds_);

    imageComponent_.setBounds(drawBounds_);

    if (redoImage)
      this->redoImage();
  }

  void RotarySlider::drawShadow(Graphics &g) const
  {
    Colour shadowColor = getColour(Skin::kShadow);

    auto width = (float)drawBounds_.getWidth();
    auto height = (float)drawBounds_.getHeight();

    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float strokeWidth = scaleValue(getValue(Skin::kKnobArcThickness));
    float radius = knobSizeScale_ * scaleValue(getValue(Skin::kKnobArcSize)) / 2.0f;
    float shadowWidth = scaleValue(getValue(Skin::kKnobShadowWidth));
    float shadowOffset = scaleValue(getValue(Skin::kKnobShadowOffset));

    Colour body = getColour(Skin::kRotaryBody);
    float bodyRadius = knobSizeScale_ * scaleValue(getValue(Skin::kKnobBodySize)) / 2.0f;
    if (bodyRadius >= 0.0f && bodyRadius < width)
    {
      if (shadowWidth > 0.0f)
      {
        Colour transparentShadow = shadowColor.withAlpha(0.0f);
        float shadowRadius = bodyRadius + shadowWidth;
        ColourGradient shadowGradient(shadowColor, centerX, centerY + shadowOffset,
          transparentShadow, centerX - shadowRadius, centerY + shadowOffset, true);
        float shadowStart = std::max(0.0f, bodyRadius - std::abs(shadowOffset)) / shadowRadius;
        shadowGradient.addColour(shadowStart, shadowColor);
        shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.75f, shadowColor.withMultipliedAlpha(0.5625f));
        shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.5f, shadowColor.withMultipliedAlpha(0.25f));
        shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.25f, shadowColor.withMultipliedAlpha(0.0625f));
        g.setGradientFill(shadowGradient);
        g.fillRect(getLocalBounds());
      }

      g.setColour(body);
      juce::Rectangle ellipse{ centerX - bodyRadius, centerY - bodyRadius, 2.0f * bodyRadius, 2.0f * bodyRadius };
      g.fillEllipse(ellipse);

      ColourGradient borderGradient(getColour(Skin::kRotaryBodyBorder), centerX, 0.0f,
        body, centerX, 0.75f * height, false);

      g.setGradientFill(borderGradient);
      g.drawEllipse(ellipse.reduced(0.5f), 1.0f);
    }

    Path shadowOutline;
    Path shadowPath;

    PathStrokeType shadowStroke(strokeWidth + 1, PathStrokeType::beveled, PathStrokeType::rounded);
    shadowOutline.addCentredArc(centerX, centerY, radius, radius,
      0.0f, -kRotaryAngle, kRotaryAngle, true);
    shadowStroke.createStrokedPath(shadowPath, shadowOutline);
    if ((!getColour(Skin::kRotaryArcUnselected).isTransparent() && isActive()) ||
      (!getColour(Skin::kRotaryArcUnselectedDisabled).isTransparent() && !isActive()))
    {
      g.setColour(shadowColor);
      g.fillPath(shadowPath);
    }
  }

  void RotarySlider::showTextEntry()
  {
    /*textEntry_->setColour(CaretComponent::caretColourId, getColour(Skin::kTextEditorCaret));
    textEntry_->setColour(TextEditor::textColourId, getColour(Skin::kNormalText));
    textEntry_->setColour(TextEditor::highlightedTextColourId, getColour(Skin::kNormalText));
    textEntry_->setColour(TextEditor::highlightColourId, getColour(Skin::kTextEditorSelection));*/

    /*auto width = (float)drawBounds_.getWidth();
    auto height = (float)drawBounds_.getHeight();

    int text_width = width;
    float font_size = findValue(Skin::kTextComponentFontSize);
    int text_height = font_size;

    textEntry_->setBounds((getWidth() - text_width) / 2, (height - text_height + 1) / 2,
      text_width, text_height);*/

    //BaseSlider::showTextEntry();
  }

  void RotarySlider::textEditorReturnKeyPressed(TextEditor &editor)
  {
    updateValueFromTextEntry();

    textEditorEscapeKeyPressed(editor);
  }

  void RotarySlider::textEditorEscapeKeyPressed(TextEditor &)
  {
    textEntry_->giveAwayKeyboardFocus();
    textEntry_->setColour(TextEditor::textColourId, selectedColor_);
    textEntry_->setText(getScaledValueString(getValue()));
    textEntry_->redrawImage();
  }

  void RotarySlider::setExtraElementsPositions(juce::Rectangle<int> anchorBounds)
  {
    static constexpr int kVerticalPadding = 2;

    if (!label_)
      return;

    label_->updateState();
    auto labelTextWidth = label_->getTotalWidth();
    auto labelX = anchorBounds.getX();
    auto vPadding = scaleValueRoundInt(kVerticalPadding);
    switch (labelPlacement_)
    {
    case Placement::left:
      labelX -= scaleValueRoundInt(kLabelOffset) + labelTextWidth;
      label_->setJustification(Justification::centredRight);
      label_->setBounds(labelX, vPadding, labelTextWidth,
        (anchorBounds.getHeight() - 2 * vPadding) / 2);

      textEntry_->setJustification(Justification::centredRight);

      // this may or may not work, look at the other case
      if (modifier_)
        extraElements_.find(modifier_)->second = { labelX, label_->getBottom(),
          modifier_->getDrawBounds().getWidth(), modifier_->getDrawBounds().getHeight() };
      break;
    default:
    case Placement::right:
      labelX += anchorBounds.getWidth() + scaleValueRoundInt(kLabelOffset);
      label_->setJustification(Justification::centredLeft);
      label_->setBounds(labelX, vPadding, labelTextWidth,
        (anchorBounds.getHeight() - 2 * vPadding) / 2);

      textEntry_->setJustification(Justification::centredLeft);

      if (modifier_)
      {
        textEntry_->setVisible(false);
        auto drawBounds = modifier_->setSizes(scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight));
        int leftOffset = scaleValueRoundInt((float)drawBounds.getHeight() * TextSelector::kPaddingHeightRatio);

        extraElements_.find(modifier_)->second = { std::max(0, labelX - leftOffset), label_->getBottom(),
          drawBounds.getWidth(), drawBounds.getHeight() };
      }
      break;
    }
  }

  juce::Rectangle<int> RotarySlider::setSizes(int height, int)
  {
    auto widthHeight = scaleValueRoundInt((float)height);
    drawBounds_ = { widthHeight, widthHeight };

    setExtraElementsPositions(drawBounds_);
    if (modifier_)
      return getUnionOfAllElements();

    auto labelBounds = label_->getBounds();
    auto usedFont = textEntry_->getUsedFont();
    Fonts::instance()->setHeightFromAscent(usedFont, (float)labelBounds.getHeight() * 0.5f);
    
    auto valueBounds = juce::Rectangle{ labelBounds.getX(), labelBounds.getBottom(),
      (int)std::ceil(getNumericTextMaxWidth(usedFont)), labelBounds.getHeight() };

    textEntry_->setUsedFont(std::move(usedFont));
    textEntry_->setBounds(valueBounds);
    textEntry_->setVisible(true);
    
    return drawBounds_.getUnion(labelBounds).getUnion(valueBounds);
  }

  void RotarySlider::setMaxArc(float arc) noexcept { quadComponent_.setMaxArc(arc); }

  void RotarySlider::setModifier(TextSelector *modifier) noexcept
  {
    if (modifier)
    {
      extraElements_.add(modifier, {});
      setShouldShowPopup(true);
    }
    else
    {
      extraElements_.erase(modifier_);
      setShouldShowPopup(false);
    }
    modifier_ = modifier;
  }

  LinearSlider::LinearSlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    quadComponent_.setFragmentShader(Shaders::kHorizontalSliderFragment);

    addOpenGlComponent(&quadComponent_);
  }

  void LinearSlider::redoImage()
  {
    if (drawBounds_.getWidth() <= 0 || drawBounds_.getHeight() <= 0)
      return;

    quadComponent_.setActive(true);
    auto t = (float)Framework::scaleValue(getValue(), details_, getSampleRate(), false, true);
    quadComponent_.setShaderValue(0, t);
    quadComponent_.setColor(selectedColor_);
    quadComponent_.setAltColor(unselectedColor_);
    quadComponent_.setThumbColor(thumbColor_);
    quadComponent_.setStartPos(isBipolar() ? 0.0f : -1.0f);

    int total_width = isHorizontal() ? drawBounds_.getHeight() : drawBounds_.getWidth();
    int extra = total_width % 2;
    auto slider_width = (float)(total_width + extra);

    quadComponent_.setThickness(slider_width);
    quadComponent_.setRounding(slider_width / 2.0f);
  }

  void LinearSlider::setComponentsBounds(bool)
  {
    if (isHorizontal())
    {
      float margin = 2.0f * (getValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBounds_.getWidth();
      quadComponent_.setQuad(0, -1.0f + margin, -1.0f, 2.0f - 2.0f * margin, 2.0f);
    }
    else
    {
      float margin = 2.0f * (getValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBounds_.getHeight();
      quadComponent_.setQuad(0, -1.0f, -1.0f + margin, 2.0f, 2.0f - 2.0f * margin);
    }
  }

  void LinearSlider::showTextEntry()
  {
    /*static constexpr float kTextEntryWidthRatio = 3.0f;

    float font_size = findValue(Skin::kTextComponentFontSize);
    int text_height = (int)(font_size);
    int text_width = (int)(text_height * kTextEntryWidthRatio);

    textEntry_->setBounds((drawBounds_.getWidth() - text_width) / 2, 
      (drawBounds_.getHeight() - text_height) / 2, text_width, text_height);*/

    BaseSlider::showTextEntry();
  }

  ImageSlider::ImageSlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    addOpenGlComponent(&imageComponent_);
  }

  void ImageSlider::redoImage() { imageComponent_.redrawImage(); }

  PinSlider::PinSlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    quadComponent_.setFragmentShader(Shaders::kPinSliderFragment);
    imageComponent_.setAlwaysOnTop(true);
    imageComponent_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>)
      {
        static constexpr float kWidth = 10.0f;
        static constexpr float kHeight = kWidth * 0.9f;
        static constexpr float kRounding = 1.0f;
        static constexpr float kVerticalSideYLength = 4.0f;
        static constexpr float kRotatedSideAngle = kPi * 0.25f;

        static constexpr float controlPoint1YOffset = gcem::tan(kRotatedSideAngle / 2.0f) * kRounding;
        static constexpr float controlPoint2XOffset = controlPoint1YOffset * gcem::cos(kRotatedSideAngle);
        static constexpr float controlPoint2YOffset = controlPoint1YOffset * gcem::sin(kRotatedSideAngle);

        static constexpr float controlPoint3XOffset = controlPoint2XOffset;
        static constexpr float controlPoint3YOffset = controlPoint2YOffset;

        static const Path pinPentagon = []()
        {
          Path shape{};

          // top
          shape.startNewSubPath(kWidth * 0.5f, 0.0f);
          shape.lineTo(kWidth - kRounding, 0.0f);
          shape.quadraticTo(kWidth, 0.0f, kWidth, kRounding);

          // right vertical
          shape.lineTo(kWidth, kVerticalSideYLength - controlPoint1YOffset);
          shape.quadraticTo(kWidth, kVerticalSideYLength,
            kWidth - controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);

          // right sideways
          shape.lineTo(kWidth * 0.5f + controlPoint3XOffset, kHeight - controlPoint3YOffset);
          shape.quadraticTo(kWidth * 0.5f, kHeight,
            kWidth * 0.5f - controlPoint3XOffset, kHeight - controlPoint3YOffset);

          // left sideways
          shape.lineTo(controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);
          shape.quadraticTo(0.0f, kVerticalSideYLength,
            0.0f, kVerticalSideYLength - controlPoint2YOffset);

          // left vertical
          shape.lineTo(0.0f, kRounding);
          shape.quadraticTo(0.0f, 0.0f, kRounding, 0.0f);

          shape.closeSubPath();
          return shape;
        }();

        auto bounds = drawBounds_.withZeroOrigin().toFloat();
        g.setColour(getThumbColor());
        g.fillPath(pinPentagon, pinPentagon.getTransformToScaleToFit(bounds, true, Justification::top));
      });

    addTextEntry();
    setShouldShowPopup(true);

    addOpenGlComponent(&quadComponent_);
    addOpenGlComponent(&imageComponent_);
    addOpenGlComponent(&textEntry_->getImageComponent());
  }

  void PinSlider::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      PopupItems options = createPopupMenu();
      showPopupSelector(this, e.getPosition(), std::move(options),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);
      return;
    }

    auto mouseEvent = e.getEventRelativeTo(parent_);
    lastDragPosition_ = mouseEvent.position.toDouble();
    runningTotal_ = getValue();

    BaseSlider::mouseDown(mouseEvent);
  }

  void PinSlider::mouseDrag(const MouseEvent &e)
  {
    float multiply = 1.0f;

    sensitiveMode_ = e.mods.isShiftDown();
    if (sensitiveMode_)
      multiply *= kSlowDragMultiplier;

    auto mouseEvent = e.getEventRelativeTo(parent_);

    auto normalisedDiff = ((double)mouseEvent.position.x - lastDragPosition_.x) / totalRange_;
    runningTotal_ += multiply * normalisedDiff;
    setValue(utils::clamp(runningTotal_, 0.0, 1.0), NotificationType::sendNotificationSync);
    lastDragPosition_ = mouseEvent.position.toDouble();

    setValueRaw(getValue());
    setValueToHost();

    if (!e.mods.isPopupMenu())
      showPopup(true);
  }

  void PinSlider::redoImage()
  {
    quadComponent_.setColor(selectedColor_);
    quadComponent_.setThumbColor(thumbColor_);

    imageComponent_.redrawImage();
  }

  void PinSlider::setComponentsBounds(bool redoImage)
  {
    quadComponent_.setBounds(drawBounds_);
    imageComponent_.setBounds(drawBounds_);

    if (redoImage)
      this->redoImage();
  }

  juce::Rectangle<int> PinSlider::setSizes(int height, int)
  {
    auto scaledWidth = scaleValueRoundInt(kDefaultPinSliderWidth);
    drawBounds_ = { scaledWidth, height };
    return drawBounds_;
  }

  TextSelector::TextSelector(Framework::ParameterValue *parameter, 
    std::optional<Font> usedFont) : BaseSlider(parameter)
  {
    COMPLEX_ASSERT(details_.scale == Framework::ParameterScale::Indexed || 
      details_.scale == Framework::ParameterScale::IndexedNumeric);

    setLabelPlacement(Placement::left);

    quadComponent_.setFragmentShader(Shaders::kRoundedRectangleFragment);
    quadComponent_.getAnimator().setHoverIncrement(0.2f);
    quadComponent_.setRenderFunction(
      [this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        auto &animator = target.getAnimator();
        animator.tick(openGl.animate);
        utils::as<OpenGlQuad>(&target)->setColor(backgroundColor_.get()
          .withMultipliedAlpha(animator.getValue(Animator::Hover)));
        target.render(openGl);
      });

    imageComponent_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>)
      {
        static const Path arrowPath = []()
        {
          Path path;
          path.startNewSubPath(0.0f, 0.0f);
          path.lineTo(0.5f, 0.5f);
          path.lineTo(1.0f, 0.0f);
          return path;
        }();

        float height = (float)drawBounds_.getHeight();
        float leftOffset = kBetweenElementsMarginHeightRatio * height + (float)drawBounds_.getX() +
          ((extraIcon_ && labelPlacement_ == Placement::left) ?
            (float)extraIcon_->getWidth() + kBetweenElementsMarginHeightRatio * height : 0.0f);

        String text = getScaledValueString(getValue());
        g.setColour(selectedColor_);
        g.setFont(usedFont_);
        g.drawText(text, juce::Rectangle{ leftOffset, 0.0f, (float)textWidth_, height }, Justification::centred, false);

        if (!drawArrow_)
          return;

        float arrowOffsetX = std::round(kBetweenElementsMarginHeightRatio * height);
        float arrowOffsetY = height / 2 - 1;
        float arrowWidth = height * kHeightToArrowWidthRatio;

        juce::Rectangle<float> arrowBounds;
        arrowBounds.setX(leftOffset + (float)textWidth_ + arrowOffsetX);
        arrowBounds.setY(arrowOffsetY);
        arrowBounds.setWidth(std::round(arrowWidth));
        arrowBounds.setHeight(std::round(kArrowWidthHeightRatio * arrowWidth));

        g.setColour(selectedColor_);
        g.strokePath(arrowPath, PathStrokeType{ 1.0f, PathStrokeType::mitered, PathStrokeType::square },
          arrowPath.getTransformToScaleToFit(arrowBounds, true));
      });

    addOpenGlComponent(&quadComponent_);
    addOpenGlComponent(&imageComponent_);

    if (usedFont)
      usedFont_ = std::move(usedFont.value());
    else
      usedFont_ = Fonts::instance()->getInterVFont();
  }

  void TextSelector::mouseDown(const juce::MouseEvent &e)
  {
    if (e.mods.isAltDown())
      return;

    if (e.mods.isPopupMenu())
    {
      PopupItems options = createPopupMenu();
      showPopupSelector(this, e.getPosition(), std::move(options),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);
      return;
    }

    // idk when this would happen but just to be sure
    if (details_.indexedData.empty())
      return;

    if (isDropDownVisible_)
    {
      isDropDownVisible_ = false;
      hidePopupSelector();
      return;
    }

    isDropDownVisible_ = true;
    lastValue_ = getValue();

    PopupItems options{};
    std::string title = (!optionsTitle_.empty()) ? optionsTitle_ : std::string{ details_.displayName };
    options.addDelimiter(std::move(title));
    for (int i = 0, currentOption = 0, currentIndex = 0; i <= (int)details_.maxValue; ++i)
    {
      // move to next option if we've run out 
      if (details_.indexedData[currentOption].count <= currentIndex)
      {
        currentIndex = 0;
        // skip options that are not present
        while (details_.indexedData[++currentOption].count == 0) { }
      }
      
      if (!ignoreItemFunction_ || ignoreItemFunction_(details_.indexedData[currentOption], currentIndex))
      {
        if (details_.indexedData[currentOption].count == 1)
          options.addEntry(i, std::format("{}", details_.indexedData[currentOption].displayName));
        else
          options.addEntry(i, std::format("{} {}", details_.indexedData[currentOption].displayName, 
            currentIndex + 1));
      }

      ++currentIndex;
    }

    showPopupSelector(this, popupPlacement_, std::move(options),
      [this](int value)
      {
        if (parameterLink_ && parameterLink_->hostControl)
          parameterLink_->hostControl->beginChangeGesture();

        beginChange(getValue());
        auto unscaledValue = unscaleValue(value, details_, getSampleRate());
        setValue(unscaledValue, NotificationType::sendNotificationSync);
        setValueToHost();
        endChange();

        if (parameterLink_ && parameterLink_->hostControl)
          parameterLink_->hostControl->endChangeGesture();
      }, 
      [this]() 
      {
        if (!isMouseButtonDown(true))
          isDropDownVisible_ = false;
      }, kMinPopupWidth);
  }

  void TextSelector::mouseUp(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
      BaseSlider::mouseUp(e);
  }

  void TextSelector::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
  {
    lastValue_ = getValue();
    MouseWheelDetails newWheel = wheel;
    newWheel.isReversed = !wheel.isReversed;
    BaseSlider::mouseWheelMove(e, newWheel);
  }

  void TextSelector::valueChanged()
  {
    double newValue = getValue();
    if (lastValue_ == newValue)
      return;

    // checks if new value is being ignored,
    // if we're in/decrementing the value, we try to find the >=/<= scaledValue valid item
    if (ignoreItemFunction_)
    {
      int option = 0, index = 0;
      int i = 0, lastValidItem = 0;
      int scaledValue = (int)Framework::scaleValue(newValue, details_);
      bool direction = (newValue - lastValue_) >= 0.0;
    
      for (; i <= (int)details_.maxValue; ++i)
      {
        if (!ignoreItemFunction_(details_.indexedData[option], index))
        {
          lastValidItem = i;
          if (i >= scaledValue)
            break;
        }

        if (i == scaledValue && !direction)
          break;

        // move to next option if we've run out 
        if (details_.indexedData[option].count <= index)
        {
          index = 0;
          // skip options that are not present
          while (details_.indexedData[++option].count == 0);
        }
        ++index;
      }

      if (lastValidItem != scaledValue)
        setValueRaw(Framework::unscaleValue(lastValidItem, details_));
    }

    resizeForText();
    BaseControl::valueChanged();
  }

  void TextSelector::redoImage()
  {
    quadComponent_.setRounding(getValue(Skin::kWidgetRoundedCorner));
    imageComponent_.redrawImage();
  }

  void TextSelector::setComponentsBounds(bool redoImage)
  {
    quadComponent_.setBounds(drawBounds_);
    imageComponent_.setBounds(drawBounds_);
    
    if (redoImage)
      this->redoImage();
  }

  void TextSelector::setExtraElementsPositions(juce::Rectangle<int> anchorBounds)
  {
    if (!label_ && !extraIcon_ && !extraNumberBox_)
      return;

    int labelStartX;
    switch (labelPlacement_)
    {
    case Placement::right:
      if (extraIcon_)
      {
        auto addedMargin = (int)std::round(kBetweenElementsMarginHeightRatio * (float)drawBounds_.getHeight());
        extraElements_.find(extraIcon_)->second = juce::Rectangle{
          anchorBounds.getRight() - extraIcon_->getWidth() - addedMargin,
          (anchorBounds.getHeight() - extraIcon_->getHeight()) / 2, 
          extraIcon_->getWidth(), extraIcon_->getHeight() };
      }

      labelStartX = anchorBounds.getRight();

      if (extraNumberBox_ && extraNumberBox_->isVisible())
      {
        auto numberBoxBounds = extraNumberBox_->setSizes(NumberBox::kDefaultNumberBoxHeight);
        int offset = (int)std::round((float)anchorBounds.getHeight() * kNumberBoxMarginToHeightRatio);
        int y = anchorBounds.getY() + utils::centerAxis(anchorBounds.getHeight(), numberBoxBounds.getHeight());
        auto drawBounds = extraNumberBox_->getDrawBounds();
        if (extraNumberBoxPlacement_ == Placement::left)
        {
          extraElements_.find(extraNumberBox_)->second = { 
            anchorBounds.getX() - numberBoxBounds.getRight() - offset,
            y, drawBounds.getWidth(), drawBounds.getHeight() };
        }
        else
        {
          extraElements_.find(extraNumberBox_)->second = { 
            anchorBounds.getRight() + offset, y, drawBounds.getWidth(), drawBounds.getHeight() };
          labelStartX = offset + numberBoxBounds.getRight();
        }
      }

      if (label_)
      {
        label_->updateState();
        label_->setJustification(Justification::centredLeft);
        label_->setBounds(labelStartX + scaleValueRoundInt(kLabelOffset),
          anchorBounds.getY(), label_->getTotalWidth(), anchorBounds.getHeight());
      }
      break;
    default:
    case Placement::left:
      if (extraIcon_)
      {
        auto addedMargin = (int)std::round(kBetweenElementsMarginHeightRatio * (float)drawBounds_.getHeight());
        extraElements_.find(extraIcon_)->second = juce::Rectangle{
          anchorBounds.getX() + addedMargin,
          (anchorBounds.getHeight() - extraIcon_->getHeight()) / 2,
          extraIcon_->getWidth(), extraIcon_->getHeight() };
      }

      labelStartX = anchorBounds.getX();

      if (extraNumberBox_ && extraNumberBox_->isVisible())
      {
        auto numberBoxBounds = extraNumberBox_->setSizes(NumberBox::kDefaultNumberBoxHeight);
        int offset = (int)std::round((float)anchorBounds.getHeight() * kNumberBoxMarginToHeightRatio);
        int y = anchorBounds.getY() + utils::centerAxis(anchorBounds.getHeight(), numberBoxBounds.getHeight());
        auto drawBounds = extraNumberBox_->getDrawBounds();
        if (extraNumberBoxPlacement_ == Placement::left)
        {
          extraElements_.find(extraNumberBox_)->second = {
            anchorBounds.getX() - numberBoxBounds.getRight() - offset,
            y, drawBounds.getWidth(), drawBounds.getHeight() };
          labelStartX = anchorBounds.getX() - numberBoxBounds.getRight() - offset;
        }
        else
        {
          extraElements_.find(extraNumberBox_)->second = {
            anchorBounds.getRight() + offset, y, drawBounds.getWidth(), drawBounds.getHeight() };
        }
      }

      if (label_)
      {
        label_->updateState();
        auto labelTextWidth = label_->getTotalWidth();
        label_->setJustification(Justification::centredRight);
        label_->setBounds(labelStartX - scaleValueRoundInt(kLabelOffset) - labelTextWidth,
          anchorBounds.getY(), labelTextWidth, anchorBounds.getHeight());
      }
      break;
    }
  }

  juce::Rectangle<int> TextSelector::setSizes(int height, int)
  {
    if (drawBounds_.getHeight() != height || isDirty_)
    {
      float floatHeight = (float)height;
      Fonts::instance()->setHeightFromAscent(usedFont_, floatHeight * kHeightFontAscentRatio);

      String text = getScaledValueString(getValue());
      textWidth_ = usedFont_.getStringWidth(text);
      float totalDrawWidth = (float)textWidth_;

      if (drawArrow_)
      {
        totalDrawWidth += floatHeight * kBetweenElementsMarginHeightRatio;
        totalDrawWidth += floatHeight * kHeightToArrowWidthRatio;
      }

      if (extraIcon_)
      {
        totalDrawWidth += floatHeight * kBetweenElementsMarginHeightRatio;
        totalDrawWidth += (float)extraIcon_->getWidth();
      }

      // there's always some padding at the beginning and end regardless whether anything is added
      totalDrawWidth += floatHeight * kPaddingHeightRatio * 2.0f;

      drawBounds_ = { (int)std::round(totalDrawWidth), height };
      isDirty_ = false;
    }

    setExtraElementsPositions(drawBounds_);
    auto bounds = drawBounds_;
    if (label_)
      bounds = bounds.getUnion(label_->getBounds());
    if (extraNumberBox_)
      bounds = bounds.getUnion(extraNumberBox_->getBounds());
    return bounds;
  }

  void TextSelector::setExtraIcon(PlainShapeComponent *icon) noexcept
  {
    if (icon)
      extraElements_.add(icon, {});
    else
      extraElements_.erase(extraIcon_);
    extraIcon_ = icon;
  }

  void TextSelector::setExtraNumberBox(NumberBox *numberBox) noexcept
  {
    if (numberBox)
      extraElements_.add(numberBox, {});
    else
      extraElements_.erase(extraNumberBox_);
    extraNumberBox_ = numberBox;
  }

  std::string_view TextSelector::getTextValue(bool fromParameter)
  {
    // this assumes all TextSelector controls have ParameterScale::Indexed
    if (fromParameter)
      return parameterLink_->parameter->getInternalValue<std::string_view>().first->id;
    else
      return details_.indexedData[(usize)Framework::scaleValue(getValue(), details_)].id;
  }

  void TextSelector::resizeForText() noexcept
  {
    String text = getScaledValueString(getValue());
    auto newTextWidth = usedFont_.getStringWidth(text);
    auto widthChange = newTextWidth - textWidth_;
    
    textWidth_ = newTextWidth;
    drawBounds_.setWidth(drawBounds_.getWidth() + widthChange);

    setComponentsBounds();

    // allowing the listener to handle setting the bounds
    if (textSelectorListener_)
      textSelectorListener_->resizeForText(this, widthChange, anchor_);
    else
      setBounds(drawBounds_.withPosition(getX() - widthChange, getY()));
  }

  NumberBox::NumberBox(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    addLabel();
    setLabelPlacement(Placement::left);

    quadComponent_.setActive(false);
    setShouldRepaintOnHover(false);
    quadComponent_.setFragmentShader(Shaders::kRoundedRectangleFragment);
    quadComponent_.getAnimator().setHoverIncrement(0.2f);
    quadComponent_.setRenderFunction(
      [this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        auto &animator = target.getAnimator();
        animator.tick(openGl.animate);
        utils::as<OpenGlQuad>(&target)->setColor(backgroundColor_.get()
          .withMultipliedAlpha(animator.getValue(Animator::Hover)));
        target.render(openGl);
      });

    imageComponent_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>)
      {
        static constexpr float kEdgeRounding = 2.0f;
        static constexpr float kCornerRounding = 3.0f;
        static constexpr float kRotatedSideAngle = kPi * 0.25f;

        static constexpr float controlPoint1XOffset = gcem::tan(kRotatedSideAngle * 0.5f) * kCornerRounding;
        static constexpr float controlPoint2XOffset = controlPoint1XOffset * gcem::cos(kRotatedSideAngle);
        static constexpr float controlPoint2YOffset = controlPoint1XOffset * gcem::sin(kRotatedSideAngle);

        static constexpr float edgeControlPointAbsoluteOffset = gcem::tan(kRotatedSideAngle * 0.5f) * kEdgeRounding;
        static constexpr float edgeControlPointXOffset = edgeControlPointAbsoluteOffset * gcem::cos(kRotatedSideAngle);
        static constexpr float edgeControlPointYOffset = edgeControlPointAbsoluteOffset * gcem::sin(kRotatedSideAngle);

        if (!drawBackground_)
          return;

        auto width = (float)drawBounds_.getWidth();
        auto height = (float)drawBounds_.getHeight();
        auto triangleXLength = height * 0.5f;

        Path box;

        // right
        box.startNewSubPath(width - kCornerRounding, 0.0f);
        box.quadraticTo(width, 0.0f, width, kCornerRounding);
        box.lineTo(width, height - kCornerRounding);

        // bottom
        box.quadraticTo(width, height, width - kCornerRounding, height);
        box.lineTo(triangleXLength + controlPoint1XOffset, height);

        // triangle bottom side
        box.quadraticTo(triangleXLength, height, triangleXLength - controlPoint2XOffset, height - controlPoint2YOffset);
        box.lineTo(edgeControlPointXOffset, height / 2.0f + edgeControlPointYOffset);

        // triangle top side
        box.quadraticTo(0.0f, height * 0.5f, edgeControlPointXOffset, height * 0.5f - edgeControlPointYOffset);
        box.lineTo(triangleXLength - controlPoint2XOffset, controlPoint2YOffset);

        // top
        box.quadraticTo(triangleXLength, 0.0f, triangleXLength + controlPoint2XOffset, 0.0f);
        box.closeSubPath();

        g.setColour(backgroundColor_);
        g.fillPath(box);
      });

    addTextEntry();
    changeTextEntryFont(Fonts::instance()->getDDinFont());
    textEntry_->setInterceptsMouseClicks(false, false);

    addOpenGlComponent(&quadComponent_);
    addOpenGlComponent(&imageComponent_);
    addOpenGlComponent(&textEntry_->getImageComponent());
  }

  void NumberBox::mouseDrag(const MouseEvent& e)
  {
    static constexpr float kNormalDragMultiplier = 0.5f;

    sensitiveMode_ = e.mods.isShiftDown();
    float multiply = kNormalDragMultiplier;
    if (sensitiveMode_)
      multiply *= kSlowDragMultiplier;

    auto sensitivity = (double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply);

    setImmediateSensitivity((int)sensitivity);

    BaseSlider::mouseDrag(e);
  }

  void NumberBox::setVisible(bool shouldBeVisible)
  {
    if (!shouldBeVisible)
    {
      quadComponent_.setActive(false);
      imageComponent_.setActive(false);
      textEntry_->setVisible(false);
    }
    else
    {
      quadComponent_.setActive(!drawBackground_);
      imageComponent_.setActive(drawBackground_);
      textEntry_->setVisible(true);
    }

    BaseSlider::setVisible(shouldBeVisible);
  }

  void NumberBox::textEditorReturnKeyPressed(TextEditor &editor)
  {
    updateValueFromTextEntry();

    textEditorEscapeKeyPressed(editor);
  }

  void NumberBox::textEditorEscapeKeyPressed(TextEditor &)
  {
    isEditing_ = false;
    textEntry_->giveAwayKeyboardFocus();
    textEntry_->setColour(TextEditor::textColourId, selectedColor_);
    textEntry_->setText(getScaledValueString(getValue()));
    textEntry_->redrawImage();
  }

  void NumberBox::redoImage()
  {
    if (!isEditing_)
    {
      textEntry_->applyColourToAllText(selectedColor_);
      textEntry_->setText(getScaledValueString(getValue()));
      textEntry_->redrawImage();
    }

    imageComponent_.redrawImage();
    quadComponent_.setRounding(getValue(Skin::kWidgetRoundedCorner));
  }

  void NumberBox::setComponentsBounds(bool redoImage)
  {
    auto bounds = drawBounds_.toFloat();
    auto xOffset = bounds.getHeight() * kTriangleWidthRatio + 
      bounds.getHeight() * kTriangleToValueMarginRatio;

    // extra offsets are pretty much magic values, don't change
    if (drawBackground_)
      bounds.removeFromLeft(xOffset - 1.0f);
    else
      bounds.removeFromLeft(2.0f);
 
    textEntry_->setBounds(bounds.toNearestInt());
    textEntry_->setVisible(true);

    quadComponent_.setBounds(getLocalBounds());
    imageComponent_.setBounds(getLocalBounds());

    if (redoImage)
      this->redoImage();
  }

  void NumberBox::showTextEntry()
  {
    textEntry_->setColour(CaretComponent::caretColourId, getColour(Skin::kTextEditorCaret));
    textEntry_->setColour(TextEditor::textColourId, getColour(Skin::kNormalText));
    textEntry_->setColour(TextEditor::highlightedTextColourId, getColour(Skin::kNormalText));
    textEntry_->setColour(TextEditor::highlightColourId, getColour(Skin::kTextEditorSelection));

    isEditing_ = true;

    BaseSlider::showTextEntry();
  }

  void NumberBox::setExtraElementsPositions(juce::Rectangle<int> anchorBounds)
  {
    if (!label_)
      return;

    label_->updateState();
    auto labelTextWidth = label_->getTotalWidth();
    auto labelX = anchorBounds.getX();
    switch (labelPlacement_)
    {
    case Placement::right:
      labelX += anchorBounds.getWidth() + scaleValueRoundInt(kLabelOffset);
      label_->setJustification(Justification::centredLeft);
      break;
    default:
    case Placement::above:
    case Placement::below:
    case Placement::left:
      labelX -= scaleValueRoundInt(kLabelOffset) + labelTextWidth;
      label_->setJustification(Justification::centredRight);
      break;
    }

    label_->setBounds({ labelX, anchorBounds.getY(), labelTextWidth, anchorBounds.getHeight() });
  }

  juce::Rectangle<int> NumberBox::setSizes(int height, int)
  {
    if (drawBounds_.getHeight() != height)
    {
      auto floatHeight = (float)height;
      auto usedFont = textEntry_->getUsedFont();
      Fonts::instance()->setHeightFromAscent(usedFont, floatHeight * 0.5f);
      textEntry_->setUsedFont(usedFont);

      float totalDrawWidth = getNumericTextMaxWidth(usedFont);
      if (drawBackground_)
      {
        totalDrawWidth += floatHeight * kTriangleWidthRatio;
        totalDrawWidth += kTriangleToValueMarginRatio * floatHeight;
        totalDrawWidth += kValueToEndMarginRatio * floatHeight;
      }
      else
      {
        // extra space around the value
        totalDrawWidth += floatHeight * 0.5f;
      }
      drawBounds_ = { (int)std::ceil(totalDrawWidth), height };
    }

    setExtraElementsPositions(drawBounds_);
    if (label_)
      return drawBounds_.getUnion(label_->getBounds());
    return drawBounds_;
  }

  void NumberBox::setAlternativeMode(bool isAlternativeMode) noexcept
  {
    quadComponent_.setActive(isAlternativeMode);
    imageComponent_.setActive(!isAlternativeMode);
    drawBackground_ = !isAlternativeMode;
  }

  ModulationSlider::ModulationSlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    quadComponent_.setFragmentShader(Shaders::kModulationKnobFragment);

    addOpenGlComponent(&quadComponent_);
  }

  void ModulationSlider::redoImage()
  {
    if (getWidth() <= 0 || getHeight() <= 0)
      return;

    float t = 2.0f * (float)getValue() - 1.0f;
    quadComponent_.setThumbColor(thumbColor_);

    if (t > 0.0f)
    {
      quadComponent_.setShaderValue(0, std::lerp(kPi, -kPi, t));
      quadComponent_.setColor(unselectedColor_);
      quadComponent_.setAltColor(selectedColor_);
    }
    else
    {
      quadComponent_.setShaderValue(0, std::lerp(-kPi, kPi, -t));
      quadComponent_.setColor(selectedColor_);
      quadComponent_.setAltColor(unselectedColor_);
    }

    if (isMouseOverOrDragging())
      quadComponent_.setThickness(1.8f);
    else
      quadComponent_.setThickness(1.0f);
  }

  void ModulationSlider::setComponentsBounds(bool)
  {
    float radius = 1.0f - 1.0f / (float)drawBounds_.getWidth();
    quadComponent_.setQuad(0, -radius, -radius, 2.0f * radius, 2.0f * radius);
  }

  void ModulationSlider::setDrawWhenNotVisible(bool draw) noexcept { quadComponent_.setDrawWhenNotVisible(draw); }
}
