/*
  ==============================================================================

    BaseSlider.cpp
    Created: 14 Dec 2022 6:59:59am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseControl.hpp"

#include "Third Party/gcem/gcem.hpp"
#include "Framework/update_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Shaders.hpp"
#include "../LookAndFeel/Skin.hpp"
#include "OpenGlQuad.hpp"
#include "OpenGlImage.hpp"
#include "../Sections/Popups.hpp"
//#include "TextEditor.hpp"
#include "Plugin/Renderer.hpp"

namespace Interface
{
  static float getSampleRate() noexcept { return getPlugin(uiRelated.renderer).getSampleRate(); }

  bool BaseSlider::mouseDown(const MouseEvent &e)
  {
    lastMouseDragPosition_ = { e.x, e.y };

    if (e.mods.isAltDown() && controlFlags.canInputValue)
    {
      showTextEntry();
      return true;
    }

    if (e.mods.isPopupMenu())
    {
      auto *selector = getPopupSelector();
      selector->
      showPopupSelector(this, { e.x, e.y }, createPopupMenu(),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);
      return true;
    }

    if (!controlFlags.resetValueOnDoubleClick && e.mods.withoutMouseButtons() == resetValueModifiers_)
    {
      resetValue();
      showPopup(true);
      return true;
    }

    showPopup(true);

    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->beginChangeGesture();

    beginChange(getValue());

    mouseDrag(e);

    animator_.isClicked = true;

    for (auto *listener : controlListeners_)
      listener->mouseDown(this, e);

    return true;
  }

  bool BaseSlider::mouseDrag(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return false;

    auto mouseDiff = (controlFlags.isHorizotalDraggable) ? 
      e.x - lastMouseDragPosition_.x : lastMouseDragPosition_.y - e.y;
    lastMouseDragPosition_ = { e.x, e.y };

    double newPos = getValue() + mouseDiff * (1.0 / sensitivity_);
    newPos = (controlFlags.canLoopAround) ? newPos - ::floor(newPos) : utils::clamp(newPos, 0.0, 1.0);

    setValue(newPos, true);
    setValueToHost();

    if (!e.mods.isPopupMenu())
      showPopup(true);

    return true;
  }

  bool BaseSlider::mouseUp(const MouseEvent &e)
  {
    if (!componentFlags.isClicked || e.mods.isPopupMenu() || e.mods.isAltDown())
      return false;

    endChange();
    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->endChangeGesture();

    animator_.isClicked = false;

    if (e.numberOfClicks >= 2)
    {
      if (!controlFlags.resetValueOnDoubleClick || e.mods.isPopupMenu())
        return;

      resetValue();

      showPopup(true);
    }

    for (auto *listener : controlListeners_)
      listener->mouseUp(this, e);

    return true;
  }

  bool BaseSlider::mouseEnter(const MouseEvent &e)
  {
    animator_.isHovered = true;

    for (auto *listener : controlListeners_)
      listener->hoverStarted(this, e);

    if (controlFlags.showPopupOnHover)
      showPopup(true);

    return true;
  }

  bool BaseSlider::mouseExit(const MouseEvent &e)
  {
    animator_.isHovered = false;

    for (auto *listener : controlListeners_)
      listener->hoverEnded(this, e);

    hidePopup(true);

    return true;
  }

  bool BaseSlider::mouseWheelMove(const MouseEvent &e)
  {
    if (!controlFlags.canUseScrollWheel)
      return false;

    // TODO:
    // sometimes duplicate wheel events seem to be sent, so since we're going to
    // bump the value by a minimum of the interval, avoid doing this twice..
    //if (e.eventTime == lastMouseWheelTime_)
    //	return true;
    //
    //lastMouseWheelTime_ = e.eventTime;

    if (e.mods.isAnyMouseButtonDown())
      return true;
    
    auto value = getValue();
    auto mouseWheelDelta = (::fabsf(e.wheelDeltaX) > ::fabsf(e.wheelDeltaY)) ? -e.wheelDeltaX : e.wheelDeltaY;
    auto valueDelta = value + 0.15 * mouseWheelDelta * (e.wheelIsReversed ? -1.0 : 1.0);
    valueDelta = (controlFlags.canLoopAround) ? valueDelta - ::floor(valueDelta) : utils::clamp(valueDelta, 0.0, 1.0);
    valueDelta -= value;
    if (valueDelta == 0.0)
      return true;
    
    auto newValue = value + utils::max(valueInterval_, ::fabs(valueDelta)) * (valueDelta < 0.0 ? -1.0 : 1.0);

    bool isMapped = parameterLink_ && parameterLink_->hostControl;
    if (isMapped)
      parameterLink_->hostControl->beginChangeGesture();

    if (!controlFlags.hasBegunChange)
      beginChange(value);

    setValue(newValue, true);
    setValueToHost();

    if (isMapped)
      parameterLink_->hostControl->endChangeGesture();

    showPopup(true);

    return true;
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

  void BaseSlider::showTextEntry()
  {
    std::string text = (!hasParameter()) ? floatToString(getValue()) :
      floatToString(Framework::scaleValue(getValue(), details_, getSampleRate(), true), maxDecimalCharacters_);

    textEntry_->setText(text);
    textEntry_->selectAll();
    if (textEntry_->isVisible())
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

  RotarySlider::RotarySlider(Framework::ParameterValue *parameter) : BaseSlider{ parameter }
  {
    hasLabel = true;
    labelPlacement_ = Placement::right;

    addTextEntry();
    changeTextEntryFont(uiRelated.cache->getDDinFont());

    quadComponent_.setMaxArc(kRotaryAngle);
    quadComponent_.setFragmentShader(Shaders::kRotarySliderFragment);
    animator_.setHoverIncrement(0.15f);

    imageComponent_.setPaintFunction([this](Graphics &g, Rectangle<int>) { drawShadow(g); });

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

  bool RotarySlider::render(OpenGlWrapper &openGl)
  {
    animator_.tick();
    quadComponent_.setThickness(knobArcThickness_ +
      knobArcThickness_ * 0.15f * animator_.getValue(Animator::Hover));

    return true;
  }

  bool RotarySlider::mouseDrag(const MouseEvent &e)
  {
    auto oldSensitivity = sensitivity_;

    controlFlags.isInSensitiveMode = e.mods.isShiftDown();
    if (controlFlags.isInSensitiveMode)
      sensitivity_ = kDefaultRotaryDragLength / (sensitivity_ * kSlowDragMultiplier);

    auto result = BaseSlider::mouseDrag(e);

    sensitivity_ = oldSensitivity;
    return result;
  }

  void RotarySlider::drawShadow(Graphics &g) const
  {
    Colour shadowColor = getColour(Skin::kShadow);

    auto width = (float)drawBounds_.getWidth();
    auto height = (float)drawBounds_.getHeight();

    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float strokeWidth = Interface::getValue(Skin::kKnobArcThickness, true, skinOverride);
    float radius = knobSizeScale_ * Interface::getValue(Skin::kKnobArcSize, true, skinOverride) / 2.0f;
    float shadowWidth = Interface::getValue(Skin::kKnobShadowWidth, true, skinOverride);
    float shadowOffset = Interface::getValue(Skin::kKnobShadowOffset, true, skinOverride);

    Colour body = getColour(Skin::kRotaryBody);
    float bodyRadius = knobSizeScale_ * Interface::getValue(Skin::kKnobBodySize, true, skinOverride) / 2.0f;
    if (bodyRadius >= 0.0f && bodyRadius < width)
    {
      if (shadowWidth > 0.0f)
      {
        Colour transparentShadow = shadowColor.withAlpha(0.0f);
        float shadowRadius = bodyRadius + shadowWidth;
        ColourGradient shadowGradient(shadowColor, centerX, centerY + shadowOffset,
          transparentShadow, centerX - shadowRadius, centerY + shadowOffset, true);
        float shadowStart = utils::max(0.0f, bodyRadius - std::abs(shadowOffset)) / shadowRadius;
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

  bool PinSlider::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      PopupItem options = createPopupMenu();
      showPopupSelector(this, e.getPosition(), COMPLEX_MOVE(options),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);
      return true;
    }

    auto mouseEvent = e.getEventRelativeTo(parent);
    lastMouseDragPosition_ = { mouseEvent.x, mouseEvent.y };
    runningTotal_ = getValue();

    return BaseSlider::mouseDown(mouseEvent);
  }

  void PinSlider::mouseDrag(const MouseEvent &e)
  {
    float multiply = 1.0f;

    flags_.sensitiveMode = e.mods.isShiftDown();
    if (flags_.sensitiveMode)
      multiply *= kSlowDragMultiplier;

    auto mouseEvent = e.getEventRelativeTo(parent_);

    auto normalisedDiff = ((double)mouseEvent.position.x - lastMouseDragPosition_.x) / totalRange_;
    runningTotal_ += multiply * normalisedDiff;
    setValue(utils::clamp(runningTotal_, 0.0, 1.0), true);
    lastMouseDragPosition_ = { mouseEvent.x, mouseEvent.y };

    setValue(getValue(), false);
    setValueToHost();

    if (!e.mods.isPopupMenu())
      showPopup(true);
  }

  TextSelector::TextSelector(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    COMPLEX_ASSERT(details_.scale == Framework::ParameterScale::Indexed || 
      details_.scale == Framework::ParameterScale::IndexedNumeric);

    setLabelPlacement(Placement::left);

    quadComponent_.setFragmentShader(Shaders::kRoundedRectangleFragment);
    animator_.setHoverIncrement(0.2f);

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

        float arrowOffsetX = ::roundf(kBetweenElementsMarginHeightRatio * height);
        float arrowOffsetY = height / 2 - 1;
        float arrowWidth = height * kHeightToArrowWidthRatio;

        juce::Rectangle<float> arrowBounds;
        arrowBounds.setX(leftOffset + (float)textWidth_ + arrowOffsetX);
        arrowBounds.setY(arrowOffsetY);
        arrowBounds.setWidth(::roundf(arrowWidth));
        arrowBounds.setHeight(::roundf(kArrowWidthHeightRatio * arrowWidth));

        g.setColour(selectedColor_);
        g.strokePath(arrowPath, PathStrokeType{ 1.0f, PathStrokeType::mitered, PathStrokeType::square },
          arrowPath.getTransformToScaleToFit(arrowBounds, true));
      });

    addOpenGlComponent(&quadComponent_);
    addOpenGlComponent(&imageComponent_);

    if (usedFont)
      usedFont_ = COMPLEX_MOVE(usedFont.value());
    else
      usedFont_ = Fonts::instance()->getInterVFont();
  }

  bool TextSelector::render(OpenGlWrapper &openGl)
  {
    animator_.tick();

    if (!setViewport(bounds.getPosition(), getLocalBounds(), getLocalBounds(), openGl, nullptr))
      return false;

    nvgSave(openGl.graphics);
    nvgTranslate(openGl.graphics, (float)bounds.x, (float)bounds.y);

    auto backgroundColour = backgroundColor_
      .withMultipliedAlpha(animator_.getValue(Animator::Hover));
    nvgFillColor(openGl.graphics->context, backgroundColour);
    nvgRoundedRect(openGl.graphics, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h, 
      Interface::getValue(Skin::kWidgetRoundedCorner, true));
    quadComponent_.setColor();

    return true;
  }

  void TextSelector::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isAltDown())
      return;

    if (e.mods.isPopupMenu())
    {
      PopupItem options = createPopupMenu();
      showPopupSelector(this, e.getPosition(), COMPLEX_MOVE(options),
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

    PopupItem options{};
    std::string title = (!optionsTitle_.empty()) ? optionsTitle_ : std::string{ 
      details_.displayName.data(), details_.displayName.size() };
    options.addDelimiter(COMPLEX_MOVE(title));
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
          options.addEntry(i, std::format("{}", details_.indexedData[currentOption].displayName.data()));
        else
          options.addEntry(i, std::format("{} {}", details_.indexedData[currentOption].displayName.data(),
            currentIndex + 1));
      }

      ++currentIndex;
    }

    showPopupSelector(this, popupPlacement_, COMPLEX_MOVE(options),
      [this](int value)
      {
        if (parameterLink_ && parameterLink_->hostControl)
          parameterLink_->hostControl->beginChangeGesture();

        beginChange(getValue());
        auto unscaledValue = unscaleValue(value, details_, getSampleRate());
        setValue(unscaledValue, true);
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

  void TextSelector::mouseWheelMove(const MouseEvent &e)
  {
    lastValue_ = getValue();
    MouseWheelDetails newWheel = wheel;
    newWheel.isReversed = !wheel.isReversed;
    BaseSlider::mouseWheelMove(e, newWheel);
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

    int labelStartX = anchorBounds.getX();
    if ((labelPlacement_ & Placement::centerHorizontal) == Placement::right)
      labelStartX = anchorBounds.getRight();
    auto addedMargin = (int)::roundf(kBetweenElementsMarginHeightRatio * (float)drawBounds_.getHeight());

    // position extra icon
    if (extraIcon_)
    {
      int xPosition = anchorBounds.getX() + addedMargin;
      if ((labelPlacement_ & Placement::centerHorizontal) == Placement::right)
        xPosition = anchorBounds.getRight() - extraIcon_->getWidth() - addedMargin;

      extraElements_.find(extraIcon_)->second = juce::Rectangle
      {
        xPosition,
        (anchorBounds.getHeight() - extraIcon_->getHeight()) / 2,
        extraIcon_->getWidth(),
        extraIcon_->getHeight()
      };
    }

    // position extra number box
    if (extraNumberBox_ && extraNumberBox_->isVisible())
    {
      COMPLEX_ASSERT((labelPlacement_ & Placement::centerHorizontal) != Placement::none);

      auto numberBoxBounds = extraNumberBox_->setSizes(NumberBox::kDefaultNumberBoxHeight);
      int offset = (int)::roundf((float)anchorBounds.getHeight() * kNumberBoxMarginToHeightRatio);
      int y = anchorBounds.getY() + utils::centerAxis(anchorBounds.getHeight(), numberBoxBounds.getHeight());
      auto drawBounds = extraNumberBox_->getDrawBounds();
      if (extraNumberBoxPlacement_ == Placement::left)
      {
        extraElements_.find(extraNumberBox_)->second = {
          anchorBounds.getX() - numberBoxBounds.getRight() - offset,
          y, drawBounds.getWidth(), drawBounds.getHeight() };

        if ((labelPlacement_ & Placement::centerHorizontal) == Placement::left)
          labelStartX = anchorBounds.getX() - numberBoxBounds.getRight() - offset;
      }
      else
      {
        extraElements_.find(extraNumberBox_)->second = {
          anchorBounds.getRight() + offset, y, drawBounds.getWidth(), drawBounds.getHeight() };

        if ((labelPlacement_ & Placement::centerHorizontal) == Placement::right)
          labelStartX = offset + numberBoxBounds.getRight();
      }
    }

    // position label
    if (label_)
    {
      label_->updateState();
      int labelTextWidth = label_->getTotalWidth();
      int labelTextHeight = label_->getTotalHeight();

      int xOffset = 0;
      int yOffset = 0;
      if ((labelPlacement_ & Placement::centerVertical) == Placement::none)
      {
        labelStartX -= scaleValueRoundInt(kLabelHMargin);
        if ((labelPlacement_ & Placement::centerHorizontal) == Placement::left)
          xOffset = -labelTextWidth;
      }
      else
      {
        int textInset = (int)::roundf(kPaddingHeightRatio * (float)anchorBounds.getHeight());
        labelStartX = anchorBounds.getX() + textInset;
        if ((labelPlacement_ & Placement::centerHorizontal) == Placement::right)
        {
          labelStartX = anchorBounds.getRight() - textInset;
          xOffset = -labelTextWidth;
        }

        int vLabelMargin = scaleValueRoundInt(kLabelVMargin);
        yOffset = ((labelPlacement_ & Placement::centerVertical) == Placement::above) ? 
          -labelTextHeight - vLabelMargin : anchorBounds.getHeight() + vLabelMargin;
      }

      juce::Justification justification = Justification::centredLeft;
      if ((labelPlacement_ & Placement::centerHorizontal) == Placement::right)
        justification = Justification::centredRight;

      label_->setJustification(justification);
      label_->setBounds(labelStartX + xOffset, anchorBounds.getY() + yOffset,
        labelTextWidth, anchorBounds.getHeight());
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

      drawBounds_ = { (int)::roundf(totalDrawWidth), height };
      isDirty_ = false;
    }

    setExtraElementsPositions(drawBounds_);
    auto bounds = getUnionOfAllElements();
    if (label_ && label_->isVisible())
      bounds = bounds.getUnion(label_->getBoundsSafe());
    return bounds;
  }

  utils::string_view TextSelector::getTextValue(bool fromParameter)
  {
    // this assumes all TextSelector controls have ParameterScale::Indexed
    if (fromParameter)
      return parameterLink_->parameter->getInternalValue<Framework::IndexedData>().first->id;
    else
      return details_.indexedData[(usize)Framework::scaleValue(getValue(), details_)].id;
  }

  NumberBox::NumberBox(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    addLabel();
    setLabelPlacement(Placement::left);

    quadComponent_.setActive(false);
    setShouldRepaintOnHover(false);
    quadComponent_.setFragmentShader(Shaders::kRoundedRectangleFragment);
    animator_.setHoverIncrement(0.2f);

    imageComponent_.setPaintFunction([this](Graphics &g, Rectangle<int>)
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

  bool NumberBox::render(OpenGlWrapper &openGl)
  {
    animator_.tick(openGl.animate);
    quadComponent_.setColor(backgroundColor_
      .withMultipliedAlpha(animator_.getValue(Animator::Hover)));

    return true;
  }

  void NumberBox::mouseDrag(const MouseEvent& e)
  {
    static constexpr float kNormalDragMultiplier = 0.5f;

    flags_.sensitiveMode = e.mods.isShiftDown();
    float multiply = kNormalDragMultiplier;
    if (flags_.sensitiveMode)
      multiply *= kSlowDragMultiplier;

    auto oldSensitivity = sensitivity_;
    sensitivity_ = (double)utils::max(getWidth(), getHeight()) / (sensitivity_ * multiply);

    //setImmediateSensitivity((int)sensitivity);

    BaseSlider::mouseDrag(e);

    sensitivity_ = oldSensitivity;
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
      drawBounds_ = { (int)::ceilf(totalDrawWidth), height };
    }

    setExtraElementsPositions(drawBounds_);
    if (label_)
      return drawBounds_.getUnion(label_->getBoundsSafe());
    return drawBounds_;
  }

  void NumberBox::setAlternativeMode(bool isAlternativeMode) noexcept
  {
    quadComponent_.setActive(isAlternativeMode);
    imageComponent_.setActive(!isAlternativeMode);
  }
}
