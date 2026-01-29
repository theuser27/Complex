/*
  ==============================================================================

    BaseSlider.cpp
    Created: 14 Dec 2022 6:59:59am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseControl.hpp"

#include "Framework/update_types.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Shaders.hpp"
#include "../LookAndFeel/Skin.hpp"
#include "Plugin/Renderer.hpp"

namespace Interface
{
  static float getSampleRate() noexcept { return getPlugin(uiRelated.renderer).getSampleRate(); }

  bool 
  BaseSlider::mouseDown(const MouseEvent &e)
  {
    lastMouseDragPosition = { e.x, e.y };

    if (e.mods.test(ModifierKeys::altModifier) && controlFlags.canInputValue)
    {
      componentFlags.isClicked = false;
      mouseExit(e);

      if (controlFlags.hasParameter)
        showTextEntry();

      return true;
    }

    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
    {
      componentFlags.isClicked = false;
      mouseExit(e);

      if (controlFlags.hasParameter)
        createPopupMenu(getPopupSelector(), { e.x, e.y });

      return true;
    }

    if (!controlFlags.resetValueOnDoubleClick && 
      resetValueModifiers != ModifierKeys::noModifiers &&
      e.mods.withoutFlags(ModifierKeys::allMouseButtonModifiers) == resetValueModifiers)
    {
      resetValue();
      showPopup();
      return true;
    }

    showPopup();

    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->beginChangeGesture();

    beginChange(getValue());

    mouseDrag(e);

    for (auto *listener : controlListeners)
      listener->mouseDown(this, e);

    return true;
  }

  bool 
  BaseSlider::mouseDrag(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return false;

    auto mouseDiff = (controlFlags.isHorizotalDraggable) ? 
      e.x - lastMouseDragPosition.x : lastMouseDragPosition.y - e.y;
    lastMouseDragPosition = { e.x, e.y };

    double newPos = getValue() + mouseDiff * (1.0 / sensitivity);
    newPos = (controlFlags.canLoopAround) ? newPos - ::floor(newPos) : utils::clamp(newPos, 0.0, 1.0);

    setValue(newPos, true);
    setValueToHost();

    return true;
  }

  bool 
  BaseSlider::mouseUp(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return false;

    endChange();
    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->endChangeGesture();

    if (e.numberOfClicks == 2)
    {
      if (!controlFlags.resetValueOnDoubleClick || 
        e.mods.test(ModifierKeys::popupMenuClickModifier))
        return true;

      resetValue();
      showPopup();

      return true;
    }

    for (auto *listener : controlListeners)
      listener->mouseUp(this, e);

    return true;
  }

  bool 
  BaseSlider::mouseEnter(const MouseEvent &e)
  {
    for (auto *listener : controlListeners)
      listener->hoverStarted(this, e);

    if (controlFlags.showPopupOnHover)
      showPopup();

    return true;
  }

  bool 
  BaseSlider::mouseExit(const MouseEvent &e)
  {
    for (auto *listener : controlListeners)
      listener->hoverEnded(this, e);

    hidePopup();

    return true;
  }

  bool 
  BaseSlider::mouseWheelMove(const MouseEvent &e)
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

    if (e.mods.test(ModifierKeys::allMouseButtonModifiers))
      return true;
    
    auto value = getValue();
    auto mouseWheelDelta = (::fabsf(e.wheelDeltaX) > ::fabsf(e.wheelDeltaY)) ? -e.wheelDeltaX : e.wheelDeltaY;
    auto valueDelta = value + 0.15 * mouseWheelDelta * (e.wheelIsReversed ? -1.0 : 1.0);
    valueDelta = (controlFlags.canLoopAround) ? valueDelta - ::floor(valueDelta) : utils::clamp(valueDelta, 0.0, 1.0);
    valueDelta -= value;
    if (valueDelta == 0.0)
      return true;
    
    auto newValue = value + utils::max(valueInterval, ::fabs(valueDelta)) * (valueDelta < 0.0 ? -1.0 : 1.0);

    bool isMapped = parameterLink && parameterLink->hostControl;
    if (isMapped)
      parameterLink->hostControl->beginChangeGesture();

    if (!controlFlags.hasBegunChange)
      beginChange(value);

    setValue(newValue, true);
    setValueToHost();

    if (isMapped)
      parameterLink->hostControl->endChangeGesture();

    showPopup();

    return true;
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
    // yes i know this is dumb but it works for now
    if (details.minValue == -details.maxValue)
    {
      controlFlags.isBipolar = true;
      controlFlags.shouldUsePlusMinusPrefix = true;
    }
  }

  bool 
  RotarySlider::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.15f;

    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);
    float thickness = knobArcThickness * (1.0f + 0.15f * animator.getValue(Animator::Hover));
    
    // TODO:

    return true;
  }

  bool 
  RotarySlider::mouseDrag(const MouseEvent &e)
  {
    auto oldSensitivity = sensitivity;

    controlFlags.isInSensitiveMode = e.mods.test(ModifierKeys::shiftModifier);
    if (controlFlags.isInSensitiveMode)
      sensitivity = kDefaultRotaryDragLength / (sensitivity * kSlowDragMultiplier);

    auto result = BaseSlider::mouseDrag(e);

    sensitivity = oldSensitivity;
    return result;
  }

  static void drawRotaryShadow(RotarySlider *rotary, Graphics &g)
  {
    //Colour shadowColor = getColour(Skin::kShadow);

    //auto width = (float)drawBounds_.getWidth();
    //auto height = (float)drawBounds_.getHeight();

    //float centerX = width / 2.0f;
    //float centerY = height / 2.0f;
    //float strokeWidth = Interface::getValue(Skin::kKnobArcThickness, true, skinOverride);
    //float radius = knobSizeScale_ * Interface::getValue(Skin::kKnobArcSize, true, skinOverride) / 2.0f;
    //float shadowWidth = Interface::getValue(Skin::kKnobShadowWidth, true, skinOverride);
    //float shadowOffset = Interface::getValue(Skin::kKnobShadowOffset, true, skinOverride);

    //Colour body = getColour(Skin::kRotaryBody);
    //float bodyRadius = knobSizeScale_ * Interface::getValue(Skin::kKnobBodySize, true, skinOverride) / 2.0f;
    //if (bodyRadius >= 0.0f && bodyRadius < width)
    //{
    //  if (shadowWidth > 0.0f)
    //  {
    //    Colour transparentShadow = shadowColor.withAlpha(0.0f);
    //    float shadowRadius = bodyRadius + shadowWidth;
    //    ColourGradient shadowGradient(shadowColor, centerX, centerY + shadowOffset,
    //      transparentShadow, centerX - shadowRadius, centerY + shadowOffset, true);
    //    float shadowStart = utils::max(0.0f, bodyRadius - std::abs(shadowOffset)) / shadowRadius;
    //    shadowGradient.addColour(shadowStart, shadowColor);
    //    shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.75f, shadowColor.withMultipliedAlpha(0.5625f));
    //    shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.5f, shadowColor.withMultipliedAlpha(0.25f));
    //    shadowGradient.addColour(1.0f - (1.0f - shadowStart) * 0.25f, shadowColor.withMultipliedAlpha(0.0625f));
    //    g.setGradientFill(shadowGradient);
    //    g.fillRect(getLocalBounds());
    //  }

    //  g.setColour(body);
    //  juce::Rectangle ellipse{ centerX - bodyRadius, centerY - bodyRadius, 2.0f * bodyRadius, 2.0f * bodyRadius };
    //  g.fillEllipse(ellipse);

    //  ColourGradient borderGradient(getColour(Skin::kRotaryBodyBorder), centerX, 0.0f,
    //    body, centerX, 0.75f * height, false);

    //  g.setGradientFill(borderGradient);
    //  g.drawEllipse(ellipse.reduced(0.5f), 1.0f);
    //}

    //Path shadowOutline;
    //Path shadowPath;

    //PathStrokeType shadowStroke(strokeWidth + 1, PathStrokeType::beveled, PathStrokeType::rounded);
    //shadowOutline.addCentredArc(centerX, centerY, radius, radius,
    //  0.0f, -kRotaryAngle, kRotaryAngle, true);
    //shadowStroke.createStrokedPath(shadowPath, shadowOutline);
    //if ((!getColour(Skin::kRotaryArcUnselected).isTransparent() && isActive()) ||
    //  (!getColour(Skin::kRotaryArcUnselectedDisabled).isTransparent() && !isActive()))
    //{
    //  g.setColour(shadowColor);
    //  g.fillPath(shadowPath);
    //}
  }

  PinSlider::PinSlider(Framework::ParameterValue *parameter) : BaseSlider(parameter)
  {
    controlFlags.shouldShowPopup = true;
  }

  bool PinSlider::render(OpenGlWrapper &openGl)
  {
    //static constexpr float kWidth = 10.0f;
    //static constexpr float kHeight = kWidth * 0.9f;
    //static constexpr float kRounding = 1.0f;
    //static constexpr float kVerticalSideYLength = 4.0f;
    //static constexpr float kRotatedSideAngle = kPi * 0.25f;

    //static constexpr float controlPoint1YOffset = gcem::tan(kRotatedSideAngle / 2.0f) * kRounding;
    //static constexpr float controlPoint2XOffset = controlPoint1YOffset * gcem::cos(kRotatedSideAngle);
    //static constexpr float controlPoint2YOffset = controlPoint1YOffset * gcem::sin(kRotatedSideAngle);

    //static constexpr float controlPoint3XOffset = controlPoint2XOffset;
    //static constexpr float controlPoint3YOffset = controlPoint2YOffset;

    //static const Path pinPentagon = []()
    //{
    //  Path shape{};

    //  // top
    //  shape.startNewSubPath(kWidth * 0.5f, 0.0f);
    //  shape.lineTo(kWidth - kRounding, 0.0f);
    //  shape.quadraticTo(kWidth, 0.0f, kWidth, kRounding);

    //  // right vertical
    //  shape.lineTo(kWidth, kVerticalSideYLength - controlPoint1YOffset);
    //  shape.quadraticTo(kWidth, kVerticalSideYLength,
    //    kWidth - controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);

    //  // right sideways
    //  shape.lineTo(kWidth * 0.5f + controlPoint3XOffset, kHeight - controlPoint3YOffset);
    //  shape.quadraticTo(kWidth * 0.5f, kHeight,
    //    kWidth * 0.5f - controlPoint3XOffset, kHeight - controlPoint3YOffset);

    //  // left sideways
    //  shape.lineTo(controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);
    //  shape.quadraticTo(0.0f, kVerticalSideYLength,
    //    0.0f, kVerticalSideYLength - controlPoint2YOffset);

    //  // left vertical
    //  shape.lineTo(0.0f, kRounding);
    //  shape.quadraticTo(0.0f, 0.0f, kRounding, 0.0f);

    //  shape.closeSubPath();
    //  return shape;
    //}();

    //auto bounds = drawBounds_.withZeroOrigin().toFloat();
    //g.setColour(getThumbColor());
    //g.fillPath(pinPentagon, pinPentagon.getTransformToScaleToFit(bounds, true, Justification::top));

    return false;
  }

  bool PinSlider::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return BaseSlider::mouseDown(e);

    auto mouseEvent = e.getEventRelativeTo(parent);
    lastMouseDragPosition = { mouseEvent.x, mouseEvent.y };
    runningTotal = getValue();

    return BaseSlider::mouseDown(mouseEvent);
  }

  bool PinSlider::mouseDrag(const MouseEvent &e)
  {
    float multiply = 1.0f;

    controlFlags.isInSensitiveMode = e.mods.test(ModifierKeys::shiftModifier);
    if (controlFlags.isInSensitiveMode)
      multiply *= kSlowDragMultiplier;

    auto mouseEvent = e.getEventRelativeTo(parent);

    auto normalisedDiff = ((double)mouseEvent.x - lastMouseDragPosition.x) / totalRange;
    runningTotal += multiply * normalisedDiff;
    setValue(utils::clamp(runningTotal, 0.0, 1.0), true);
    lastMouseDragPosition = { mouseEvent.x, mouseEvent.y };

    setValue(getValue(), false);
    setValueToHost();
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

    controlFlags.isInModalState = true;

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
