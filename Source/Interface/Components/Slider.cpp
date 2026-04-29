
// Created: 2022-12-14 06:59:59

#include "Control.hpp"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Generation/Processor.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/ui_constants.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Skin.hpp"
#include "../Sections/Popups.hpp"

namespace Interface
{
  bool 
  Slider::mouseDown(const MouseEvent &e)
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

    mouseDrag(e);

    return true;
  }

  bool 
  Slider::mouseDrag(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return false;

    auto mouseDiff = (controlFlags.isHorizotalDraggable) ? 
      e.x - lastMouseDragPosition.x : lastMouseDragPosition.y - e.y;
    lastMouseDragPosition = { e.x, e.y };

    if (mouseDiff)
    {
      double newPos = getValue() + mouseDiff * (1.0 / sensitivity);
      newPos = (controlFlags.canLoopAround) ? newPos - ::floor(newPos) : utils::clamp(newPos, 0.0, 1.0);

      beginChange(getValue());

      setValue(newPos, true);
      setValueToHost();

      endChange();
    }

    return true;
  }

  bool 
  Slider::mouseUp(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return false;

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

    return true;
  }

  bool 
  Slider::mouseEnter(const MouseEvent &)
  {
    if (controlFlags.showPopupOnHover)
      showPopup();

    return true;
  }

  bool 
  Slider::mouseExit(const MouseEvent &)
  {
    hidePopup();

    return true;
  }

  bool 
  Slider::mouseWheelMove(const MouseEvent &e)
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
    
    auto currentValue = getValue();
    auto mouseWheelDelta = (::fabs(e.wheelDeltaX) > ::fabs(e.wheelDeltaY)) ? -e.wheelDeltaX : e.wheelDeltaY;
    auto valueDelta = currentValue + 0.05 * mouseWheelDelta * (e.wheelIsReversed ? -1.0 : 1.0);
    valueDelta = (controlFlags.canLoopAround) ? valueDelta - ::floor(valueDelta) : utils::clamp(valueDelta, 0.0, 1.0);
    valueDelta -= currentValue;
    if (valueDelta == 0.0)
      return true;
    
    double newValue = 0.0;
    if (details.scale == Framework::ParameterScale::Indexed)
    {
      auto valueInterval = 1.0 / (details.options->valueCount - 1);
      newValue = currentValue + valueInterval * (valueDelta < 0.0 ? -1.0 : 1.0);
    }
    else if (details.flags & Framework::ParameterDetails::RoundToInt)
    {
      auto valueInterval = 1.0 / (details.maxValue - details.minValue);
      newValue = currentValue + valueInterval * (valueDelta < 0.0 ? -1.0 : 1.0);
    }
    else
      newValue = currentValue + ::fabs(valueDelta) * (valueDelta < 0.0 ? -1.0 : 1.0);

    bool isMapped = parameterLink && parameterLink->hostControl;
    if (isMapped)
      parameterLink->hostControl->beginChangeGesture();

    beginChange(currentValue);

    setValue(newValue, true);
    setValueToHost();

    endChange();

    if (isMapped)
      parameterLink->hostControl->endChangeGesture();

    showPopup();

    return true;
  }



  RotarySlider::RotarySlider()
  {
    controlFlags.canUseScrollWheel = true;
    desiredSize = { kDefaultWidthHeight, kDefaultWidthHeight,
      kDefaultWidthHeight, kDefaultWidthHeight };
  }
  
  bool 
  RotarySlider::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.15f;
    static constexpr float kHalfPi = kPi * 0.5f;
    static constexpr float kThumbRadiusOffset = 2.0f;

    tickAnimation(animationValues, {{ componentFlags.isHovered || componentFlags.isClicked }}, {{ kHoverIncrement }});
    float thickness = Interface::getValue(Skin::kKnobArcThickness, true, this) * (1.0f + 0.15f * animationValues[0]);

    auto localBounds = getLocalBounds().toFloat().trimmed(scaleValue(padding.toFloat()));
    float centreX = localBounds.getCentreX();
    float centreY = localBounds.getCentreY();
    float fullRadius = (utils::min(localBounds.w, localBounds.h) - thickness) * 0.5f;
    float bodyRadius = Interface::getValue(Skin::kKnobBodySize, true, this) * 0.5f;
    auto backgroundColour = getColour(Skin::kRotaryArcUnselected, this);
    float currentValue = (float)getValue();

    nvgLineCap(openGl, NVG_ROUND);

    // TODO: rotary body shadow

    // body
    nvgBeginPath(openGl);
    nvgCircle(openGl, centreX, centreY, bodyRadius);
    nvgFillColor(openGl, backgroundColour);
    nvgFill(openGl);

    float outlineWidth = scaleValue(1.0f);
    float outlineRadius = bodyRadius - outlineWidth;
    nvgStrokeWidth(openGl, outlineWidth);
    // outline
    nvgBeginPath(openGl);
    nvgCircle(openGl, centreX, centreY - outlineWidth * 0.5f, outlineRadius);
    auto outlineGradient = nvgLinearGradient(openGl, 
      0.0f, centreY - bodyRadius + outlineWidth, 0.0f, centreY + bodyRadius - outlineWidth,
      Colour{ 255, 255, 255, 0.1f }, Colour{ 255, 255, 255, 0.0f });
    nvgStrokePaint(openGl, outlineGradient);
    nvgStroke(openGl);

    // thumb
    float phi = 2 * maxArc * currentValue - maxArc;
    float thumbX = utils::cos(phi - kHalfPi);
    float thumbY = utils::sin(phi + kHalfPi);
    float thumbRadiusOffset = scaleValue(kThumbRadiusOffset);
    float thumbLength = Interface::getValue(Skin::kKnobHandleLength, true, this);

    nvgBeginPath(openGl);
    nvgStrokeWidth(openGl, thickness);
    nvgMoveTo(openGl, centreX + thumbX * (outlineRadius - thumbRadiusOffset - thumbLength),
      centreY - thumbY * (outlineRadius - thumbRadiusOffset - thumbLength));
    nvgLineTo(openGl, centreX + thumbX * (outlineRadius - thumbRadiusOffset),
      centreY - thumbY * (outlineRadius - thumbRadiusOffset));
    nvgStrokeColor(openGl, getColour(Skin::kRotaryHand, this));
    nvgStroke(openGl);

    // radial background fill
    nvgStrokeWidth(openGl, thickness);

    nvgBeginPath(openGl);
    nvgStrokeColor(openGl, backgroundColour);
    nvgArc(openGl, centreX, centreY, fullRadius, -maxArc - kHalfPi, maxArc - kHalfPi, NVG_CW);
    nvgStroke(openGl);

    // radial fill
    float startingPoint = -maxArc - kHalfPi;
    float length = 2 * maxArc * utils::max(currentValue, 0.01f);
    if (controlFlags.isBipolar)
    {
      length = 2 * maxArc * (currentValue - 0.5f);
      startingPoint += utils::min(maxArc + length, maxArc);
      length = utils::abs(length);
    }

    nvgBeginPath(openGl);
    nvgStrokeColor(openGl, getColour(Skin::kWidgetPrimary1, this));
    nvgArc(openGl, centreX, centreY, fullRadius,
      startingPoint, startingPoint + length, NVG_CW);
    nvgStroke(openGl);

    return true;
  }

  bool 
  RotarySlider::mouseDrag(const MouseEvent &e)
  {
    auto oldSensitivity = sensitivity;
    sensitivity *= kDefaultRotaryDragLength;

    controlFlags.isInSensitiveMode = e.mods.test(ModifierKeys::shiftModifier);
    if (controlFlags.isInSensitiveMode)
      sensitivity /= kSlowDragMultiplier;

    auto result = Slider::mouseDrag(e);

    sensitivity = oldSensitivity;
    return result;
  }

  void drawRotaryShadow(RotarySlider *rotary, Graphics &g)
  {
    (void)rotary;
    (void)g;
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

  PinSlider::PinSlider()
  {
    controlFlags.shouldShowPopup = true;
    placement = Placement::custom;
    overridePosition = [](Component *c)
    {
      auto *self = (PinSlider *)c;
      auto x = (i32)::round(self->getValue() * (double)self->parent->bounds.w);
      self->bounds.x = x - self->bounds.w / 2;
      self->bounds.y = 0;

      return true;
    };
  }

  bool 
  PinSlider::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return Slider::mouseDown(e);

    if (e.mods.test(ModifierKeys::ctrlModifier))
      return false;

    auto mouseEvent = e.getEventRelativeTo(parent);
    lastMouseDragPosition = { mouseEvent.x, mouseEvent.y };
    runningTotal = getValue();

    return Slider::mouseDown(mouseEvent);
  }

  bool 
  PinSlider::mouseDrag(const MouseEvent &e)
  {
    float multiply = sensitivity;

    controlFlags.isInSensitiveMode = e.mods.test(ModifierKeys::shiftModifier);
    if (controlFlags.isInSensitiveMode)
      multiply *= kSlowDragMultiplier;

    auto mouseEvent = e.getEventRelativeTo(parent);

    auto normalisedDiff = ((double)mouseEvent.x - lastMouseDragPosition.x) / (double)parent->bounds.w;
    runningTotal += multiply * normalisedDiff;
    setValue(utils::clamp(runningTotal, 0.0, 1.0), true);
    lastMouseDragPosition = { mouseEvent.x, mouseEvent.y };

    setValue(getValue(), false);
    setValueToHost();

    return true;
  }

  bool PinSlider::render(OpenGlWrapper &openGl)
  {
    static constexpr float kWidth = 10.0f;
    static constexpr float kHeight = kWidth * 0.9f;
    static constexpr float kRounding = 1.0f;
    static constexpr float kVerticalSideYLength = 4.0f;
    static constexpr float kRotatedSideAngle = kPi * 0.25f;

    static constexpr float kControlPoint1OffsetY = const_math::tan(kRotatedSideAngle / 2.0f) * kRounding;
    static constexpr float kControlPoint2OffsetX = kControlPoint1OffsetY * const_math::cos(kRotatedSideAngle);
    static constexpr float kControlPoint2OffsetY = kControlPoint1OffsetY * const_math::sin(kRotatedSideAngle);

    static constexpr float kControlPoint3OffsetX = kControlPoint2OffsetX;
    static constexpr float kControlPoint3OffsetY = kControlPoint2OffsetY;

    auto colour = getColour(Skin::kWidgetPrimary1, this);
    auto [x, y, w, h] = getLocalBounds().toFloat().trimmed(scaleValue(padding.toFloat()));
    float rounding = scaleValue(kRounding);
    float verticalSideLength = scaleValue(kVerticalSideYLength);
    float c1OffsetY = scaleValue(kControlPoint1OffsetY);
    float c2OffsetX = scaleValue(kControlPoint2OffsetX);
    float c2OffsetY = scaleValue(kControlPoint2OffsetY);
    float c3OffsetX = scaleValue(kControlPoint3OffsetX);
    float c3OffsetY = scaleValue(kControlPoint3OffsetY);
    float pinHeight = w * 0.9f;

    nvgTranslate(openGl, x, y);
    nvgBeginPath(openGl);

    // right side
    nvgMoveTo(openGl, w - rounding, 0.0f);
    nvgQuadTo(openGl, w, 0.0f, w, rounding);
    nvgLineTo(openGl, w, verticalSideLength - c1OffsetY);
    nvgQuadTo(openGl, w, verticalSideLength, w - c2OffsetX, 
      verticalSideLength + c2OffsetY);
    nvgLineTo(openGl, w * 0.5f + c3OffsetX, pinHeight - c3OffsetY);

    // bottom
    nvgQuadTo(openGl, w * 0.5f, pinHeight, w * 0.5f - c3OffsetX, pinHeight - c3OffsetY);

    // left side
    nvgLineTo(openGl, c2OffsetX, verticalSideLength + c2OffsetY);
    nvgQuadTo(openGl, 0.0f, verticalSideLength, 0.0f, verticalSideLength - c2OffsetY);
    nvgLineTo(openGl, 0.0f, rounding);
    nvgQuadTo(openGl, 0.0f, 0.0f, rounding, 0.0f);

    nvgClosePath(openGl);
    nvgFillColor(openGl, colour);
    nvgFill(openGl);

    // line
    auto gradient = nvgLinearGradient(openGl, 0.0f, pinHeight, 0.0f, h, colour, colour.withAlpha(0.2f));
    float lineWidth = scaleValue(1.0f);
    nvgBeginPath(openGl);
    nvgRect(openGl, (w - lineWidth) * 0.5f, pinHeight - c3OffsetY, lineWidth, h - (pinHeight - c3OffsetY));
    nvgFillPaint(openGl, gradient);
    nvgFill(openGl);

    return false;
  }

  TextSelector::TextSelector()
  {
    controlFlags.canUseScrollWheel = true;
    overrideSize = [](Component *c, bool isCalculatingVertical)
    {
      auto *self = (TextSelector *)c;

      if (!isCalculatingVertical)
      {
        float height = (float)self->text.desiredSize.y;
        self->padding.x = self->padding.w = (u16)::ceilf(height * 0.25f);
      }

      return Range<i32>{ -1, -1 };
    };

    text.control = this;
    text.textColour = Skin::kWidgetPrimary1;
    addChildComponent(&text);

    downArrow.margin = { 4, 0, 0, 0 };
    downArrow.desiredSize = { 5, 0, 5, 0 };
    downArrow.sizingFlags |= (Component::SizingFlags)(Component::FixedX | Component::SameAsSiblingsY);
    downArrow.reference = this;
    downArrow.draw = [](OpenGlWrapper &openGl, Component *c, Component *self, Point<i32>)
    {
      auto bounds = self->bounds.toFloat();
      float yCenter = bounds.h * 0.5f;
      float height = bounds.w * 0.25f;

      nvgStrokeWidth(openGl, scaleValue(1.0f));
      nvgBeginPath(openGl.g);
      nvgMoveTo(openGl.g, 0.0f, yCenter - height);
      nvgLineTo(openGl, bounds.w * 0.5f, yCenter + height);
      nvgLineTo(openGl, bounds.w, yCenter - height);
      nvgStrokeColor(openGl.g, getColour(Skin::kWidgetPrimary1, c));
      nvgStroke(openGl);

      return true;
    };
    addChildComponent(&downArrow);
  }

  bool 
  TextSelector::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.2f;

    tickAnimation(animationValues, {{ componentFlags.isHovered }}, {{ kHoverIncrement }});

    if (auto alpha = animationValues[0]; alpha != 0.0f)
    {
      nvgBeginPath(openGl);
      nvgRoundedRect(openGl, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h,
        Interface::getValue(Skin::kWidgetRoundedCorner, true, this));
      nvgFillColor(openGl, getColour(Skin::kWidgetBackground2, this).withAlpha(alpha));
      nvgFill(openGl);
    }

    return true;
  }

  bool 
  TextSelector::mouseDown(const MouseEvent &e)
  {
    // probably unnecessary but just in case
    COMPLEX_ASSERT(details.scale == Framework::ParameterScale::Indexed);

    if (e.mods.test(ModifierKeys::altModifier))
      return true;

    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return Slider::mouseDown(e);

    // idk when this would happen but just to be sure
    if (!details.options)
    {
      COMPLEX_ASSERT_FALSE();
      return true;
    }

    auto *selector = getPopupSelector();

    if (isDropdownOpen)
    {
      selector->resetState();
      isDropdownOpen = false;
      return true;
    }

    selector->resetState();
    lastValue = getValue();
    isDropdownOpen = true;

    PopupList *list = anew(selector->arena, PopupList, { selector });
    list->parentSelector = selector;

    if (auto title = (!dropdownTitle.empty()) ?
      utils::string_view{ dropdownTitle } : details.displayName; 
      !title.empty())
    {
      list->addChildComponent(OptionPopupItem::createTitle(list, title));
    }

    Framework::iterateOverIndexedData(details.options,
      [list](Framework::IndexedData &option)
      {
        if (option.childrenCount || option.canBeChosen())
          list->addChildComponent(OptionPopupItem::createOption(list, option));

        return false;
      });

    selector->list = list;
    selector->skinOverride = getSkinOverride();
    selector->callback = [this](PopupSelector *, PopupItem *item)
    {
      if (parameterLink && parameterLink->hostControl)
        parameterLink->hostControl->beginChangeGesture();

      auto *option = (Framework::IndexedData *)item->extraData;

      beginChange(getValue());
      auto unscaledValue = unscaleValue(Framework::getValueFromOptionId(option->id, details), 
        details, getPlugin(uiRelated.renderer).getSampleRate());
      setValue(unscaledValue, true);
      setValueToHost();
      endChange();

      isDropdownOpen = false;

      if (parameterLink && parameterLink->hostControl)
        parameterLink->hostControl->endChangeGesture();
    };
    selector->cancel = [this](PopupSelector *) { isDropdownOpen = false; };
    selector->summon(this, dropdownPlacement, dropdownOffset.toInt());

    componentFlags.isClicked = false;

    return true;
  }

  bool
  TextSelector::mouseWheelMove(const MouseEvent &e)
  {
    lastValue = getValue();
    auto newEvent = e;
    newEvent.wheelIsReversed = !e.wheelIsReversed;

    return Slider::mouseWheelMove(newEvent);
  }

  void TextSelector::setExtraIcon(Paths::DrawingFn *drawFn)
  {
    (void)drawFn;
    // TODO:
  }

  Numberbox::Numberbox()
  {
    controlFlags.canUseScrollWheel = true;
    overrideSize = [](Component *c, bool isCalculatingVertical)
    {
      auto *self = (Numberbox *)c;

      if (!isCalculatingVertical)
      {
        float height = (float)self->editor.desiredSize.y;
        self->padding = {};

        if (self->drawBackgroundArrow)
        {
          self->padding.x = (u16)::ceilf(height * (kTriangleWidthRatio + kTriangleToValueMarginRatio));
          self->padding.w = (u16)::ceilf(kValueToEndMarginRatio * height);
        }
        else
          self->padding.x = self->padding.w = (u16)::ceilf(height * 0.25f);
      }

      return Range<i32>{ -1, -1 };
    };

    editor.control = this;
    editor.textColour = Skin::kWidgetPrimary1;
    addChildComponent(&editor);
  }

  bool 
  Numberbox::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.2f;

    // if we've clicked we still want the hover animator to run
    tickAnimation(animationValues, 
      {{ componentFlags.isHovered || componentFlags.isClicked }}, 
      {{ kHoverIncrement }});

    if (drawBackgroundArrow)
    {
      static constexpr float kEdgeRounding = 2.0f;
      static constexpr float kCornerRounding = 3.0f;
      static constexpr float kRotatedSideAngle = kPi * 0.25f;

      static constexpr float controlPoint1XOffset = const_math::tan(kRotatedSideAngle * 0.5f) * kCornerRounding;
      static constexpr float controlPoint2XOffset = controlPoint1XOffset * const_math::cos(kRotatedSideAngle);
      static constexpr float controlPoint2YOffset = controlPoint1XOffset * const_math::sin(kRotatedSideAngle);

      static constexpr float edgeControlPointAbsoluteOffset = const_math::tan(kRotatedSideAngle * 0.5f) * kEdgeRounding;
      static constexpr float edgeControlPointXOffset = edgeControlPointAbsoluteOffset * const_math::cos(kRotatedSideAngle);
      static constexpr float edgeControlPointYOffset = edgeControlPointAbsoluteOffset * const_math::sin(kRotatedSideAngle);

      nvgBeginPath(openGl.g);
      
      float width = (float)bounds.w;
      float height = (float)bounds.h;
      float triangleXLength = height * 0.5f;

      // right
      nvgMoveTo(openGl.g, width - kCornerRounding, 0.0f);
      nvgQuadTo(openGl.g, width, 0.0f, width, kCornerRounding);
      nvgLineTo(openGl.g, width, height - kCornerRounding);

      // bottom
      nvgQuadTo(openGl.g, width, height, width - kCornerRounding, height);
      nvgLineTo(openGl.g, triangleXLength + controlPoint1XOffset, height);

      // triangle bottom side
      nvgQuadTo(openGl.g, triangleXLength, height, triangleXLength - controlPoint2XOffset, height - controlPoint2YOffset);
      nvgLineTo(openGl.g, edgeControlPointXOffset, height / 2.0f + edgeControlPointYOffset);

      // triangle top side
      nvgQuadTo(openGl.g, 0.0f, height * 0.5f, edgeControlPointXOffset, height * 0.5f - edgeControlPointYOffset);
      nvgLineTo(openGl.g, triangleXLength - controlPoint2XOffset, controlPoint2YOffset);

      // top
      nvgQuadTo(openGl.g, triangleXLength, 0.0f, triangleXLength + controlPoint2XOffset, 0.0f);
      nvgFillColor(openGl.g, getColour(Skin::kWidgetBackground1, this));
      nvgFill(openGl.g);
    }
    else if (auto alpha = animationValues[0]; alpha != 0.0f)
    {
      nvgBeginPath(openGl.g);
      nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h, scaleValue(backgroundRounding));
      nvgFillColor(openGl.g, getColour(Skin::kWidgetBackground2, this).withAlpha(alpha));
      nvgFill(openGl.g);
    }

    return true;
  }

  bool
  Numberbox::mouseDrag(const MouseEvent& e)
  {
    static constexpr float kNormalDragMultiplier = 0.5f;

    controlFlags.isInSensitiveMode = e.mods.test(ModifierKeys::shiftModifier);
    float multiply = kNormalDragMultiplier;
    if (controlFlags.isInSensitiveMode)
      multiply *= kSlowDragMultiplier;

    auto oldSensitivity = sensitivity;
    sensitivity = (float)utils::max(bounds.w, bounds.h) / (sensitivity * multiply);

    auto result = Slider::mouseDrag(e);

    sensitivity = oldSensitivity;

    return result;
  }

  CombinationRotarySlider::CombinationRotarySlider()
  {
    addChildComponent(&rotary);

    infoSection.margin = { 6, 0, 0, 0 };
    infoSection.componentFlags.vertical = true;
    addChildComponent(&infoSection);

    label.sizingFlags |= Component::SameAsSiblingsX;
    label.control = &rotary;
    label.textPlacement = Placement::left;
    infoSection.addChildComponent(&label);

    valueEditor.sizingFlags |= Component::SameAsSiblingsX;
    valueEditor.control = &rotary;
    valueEditor.textPlacement = Placement::left;
    valueEditor.textColour = Skin::kWidgetPrimary1;
    infoSection.addChildComponent(&valueEditor);
  }

  void CombinationRotarySlider::setModifier(TextSelector *newModifier)
  {
    if (modifier)
      removeChildComponent(modifier);

    modifier = newModifier;
    rotary.controlFlags.shouldShowPopup = modifier;
    valueEditor.componentFlags.isVisible = !modifier;

    if (modifier)
      addChildComponent(modifier);
  }

  PinBoundsBox::PinBoundsBox()
  {
    lowBound.padding = { kAdditionalPinWidth / 2, 0, kAdditionalPinWidth / 2, 0 };
    lowBound.desiredSize = { PinSlider::kDefaultWidth, 0, PinSlider::kDefaultWidth, 0 };
    lowBound.sizingFlags = Component::GrowableY;
    addChildComponent(&lowBound);
    highBound.padding = { kAdditionalPinWidth / 2, 0, kAdditionalPinWidth / 2, 0 };
    highBound.desiredSize = { PinSlider::kDefaultWidth, 0, PinSlider::kDefaultWidth, 0 };
    highBound.sizingFlags = Component::GrowableY;
    addChildComponent(&highBound);
  }

  bool 
  PinBoundsBox::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this), 
      scaleValue(rounding[0]), scaleValue(rounding[1]), scaleValue(rounding[2]), scaleValue(rounding[3]));

    paintHighlightBox(this, openGl, (float)lowBound.getValue(), (float)highBound.getValue(),
      getColour(Skin::kWidgetPrimary1, this).withAlpha(0.15f), backgroundColour);

    return true;
  }

  void PinBoundsBox::paintHighlightBox(Component *component, Graphics &g, float lowBoundValue,
    float highBoundValue, Colour colour, Colour backgroundColour)
  {
    auto lowBoundShifted = utils::clamp(lowBoundValue, 0.0f, 1.0f);
    auto highBoundShifted = utils::clamp(highBoundValue, 0.0f, 1.0f);

    if (lowBoundValue < highBoundValue)
    {
      auto lowPixel = lowBoundShifted * (float)component->bounds.w;
      auto highlightWidth = highBoundShifted * (float)component->bounds.w - lowPixel;
      Rectangle fillBounds{ lowPixel, 0.0f, highlightWidth, (float)component->bounds.h };

      fillRect(g, fillBounds, colour);

    }
    else if (lowBoundValue > highBoundValue)
    {
      auto lowPixel = lowBoundShifted * (float)component->bounds.w;
      auto upperHighlightWidth = (float)component->bounds.w - lowPixel;
      auto lowerHighlightWidth = highBoundShifted * (float)component->bounds.w;

      Rectangle upperBounds{ lowPixel, 0.0f, upperHighlightWidth, (float)component->bounds.h };
      Rectangle lowerBounds{ 0.0f, 0.0f, lowerHighlightWidth, (float)component->bounds.h };

      fillRect(g, upperBounds, colour);
      fillRect(g, lowerBounds, colour);
    }
    
    (void)backgroundColour;
    // TODO: rounding at the ends
  }
}
