
// Created: 2022-12-14 06:59:59

#include "BaseControl.hpp"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../LookAndFeel/Shaders.hpp"
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

    beginChange(getValue());

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

    double newPos = getValue() + mouseDiff * (1.0 / sensitivity);
    newPos = (controlFlags.canLoopAround) ? newPos - ::floor(newPos) : utils::clamp(newPos, 0.0, 1.0);

    setValue(newPos, true);
    setValueToHost();

    return true;
  }

  bool 
  Slider::mouseUp(const MouseEvent &e)
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
    auto valueDelta = currentValue + 0.15 * mouseWheelDelta * (e.wheelIsReversed ? -1.0 : 1.0);
    valueDelta = (controlFlags.canLoopAround) ? valueDelta - ::floor(valueDelta) : utils::clamp(valueDelta, 0.0, 1.0);
    valueDelta -= currentValue;
    if (valueDelta == 0.0)
      return true;
    
    double valueInterval = 0.0;
    if (details.scale == Framework::ParameterScale::Indexed)
      valueInterval = 1.0 / (details.options->count - 1);

    auto newValue = currentValue + utils::max(valueInterval, ::fabs(valueDelta)) * (valueDelta < 0.0 ? -1.0 : 1.0);

    bool isMapped = parameterLink && parameterLink->hostControl;
    if (isMapped)
      parameterLink->hostControl->beginChangeGesture();

    if (!controlFlags.hasBegunChange)
      beginChange(currentValue);

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

  RotarySlider::RotarySlider()
  {
    controlFlags.canUseScrollWheel = true;
  }
  
  bool 
  RotarySlider::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.15f;

    (void)openGl;
    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);
    float thickness = knobArcThickness * (1.0f + 0.15f * animator.getValue(Animator::Hover));
    (void)thickness;

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
  }

  bool PinSlider::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
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

  bool 
  PinSlider::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return Slider::mouseDown(e);

    auto mouseEvent = e.getEventRelativeTo(parent);
    lastMouseDragPosition = { mouseEvent.x, mouseEvent.y };
    runningTotal = getValue();

    return Slider::mouseDown(mouseEvent);
  }

  bool 
  PinSlider::mouseDrag(const MouseEvent &e)
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

    return true;
  }

  TextSelector::TextSelector()
  {
    controlFlags.canUseScrollWheel = true;

    text.control = this;
    addChildComponent(&text);

    downArrow.margin = { 6, 0, 0, 0 };
    downArrow.draw = [](OpenGlWrapper &openGl, Component *c, Component *self)
    {
      (void)openGl;
      (void)c;
      (void)self;
      //nvgStrokeColor(openGl.g, getColour(Skin::kWidgetPrimary1, c));
      //
      //auto bounds = self->bounds.toFloat();
      //float arrowWidth = bounds.h * kHeightToArrowWidthRatio;
      //float arrowHeight = ::roundf(kArrowWidthHeightRatio * arrowWidth);

      //bounds.y = bounds.h / 2 - arrowHeight / 2;
      //bounds.w = ::roundf(arrowWidth);
      //bounds.h = arrowHeight;

      //nvgBeginPath(openGl.g);
      //nvgMoveTo(openGl.g, );
    };
    addChildComponent(&downArrow);
  }

  bool TextSelector::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.2f;

    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);

    auto backgroundColour = getColour(Skin::kWidgetBackground1, this)
      .withMultipliedAlpha(animator.getValue(Animator::Hover));
    nvgFillColor(openGl.g, backgroundColour);
    nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h, 
      Interface::getValue(Skin::kWidgetRoundedCorner, true));

    return true;
  }

  struct TextSelectorItem : PopupItem
  {
    TextSelectorItem()
    {
      getDimensions = [](Component *c, i32 *availableWidth)
      {
        auto *item = (TextSelectorItem *)c;

        i32 minSize{}, maxSize{};
        bool canTextWrap = false;
        utils::string_view text;
        float height = kSecondaryTextLineHeight;

        if (item->id)
        {
          // label whose extra data is a string

          canTextWrap = true;
          text = *((utils::stringnd *)item->extraData);
        }
        else
        {
          auto *option = (Framework::IndexedData *)item->extraData;
          canTextWrap = !option->id;
          height = !option->id ? height : kPrimaryTextLineHeight;
          text = option->displayName;
        }

        uiRelated.cache->setFont(FontId::InterType, scaleValue(height));

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

        return Range<i32>{ minSize, maxSize };
      };
    }

    bool 
    render(OpenGlWrapper &openGl) override
    {
      (void)openGl;

      // TODO:

      return true;
    }
  };

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

    if (controlFlags.isInModalState)
    {
      selector->resetState();
      return true;
    }

    lastValue = getValue();
    controlFlags.isInModalState = true;

    auto *itemArena = selector->arena;

    TextSelectorItem *options = anew(itemArena, TextSelectorItem, {});
    options->sublistMinSize = { kPopupMinWidth, 0 };

    TextSelectorItem *name = anew(itemArena, TextSelectorItem, {});
    name->arena = itemArena;
    name->id = 1;
    name->canBeChosen = false;
    name->extraData = anew(itemArena, utils::stringnd, { itemArena, (!dropdownTitle.empty()) ?
      utils::string_view{ dropdownTitle } : details.displayName });
    options->addChildComponent(name);

    bool exitedChild = false;
    auto *option = details.options;
    u32 size = option->count;
    u32 index = 0;

    while (true)
    {
      {
        TextSelectorItem *item = anew(itemArena, TextSelectorItem, {});
        item->arena = itemArena;
        item->extraData = option;
        item->canBeChosen = option->id;
        item->sizingFlags = (Component::SizingFlags)(Component::CustomDimensions | Component::SameAsSiblingsX);
        options->addChildComponent(item);
      }

      // going down
      if (!exitedChild && option->children && option->count)
      {
        option = option->children;
        size = option->count;
        index = 0;
        continue;
      }

      // going forward
      if (index < size)
      {
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

    selector->items = options;
    selector->skinOverride = skinOverride;
    selector->callback = [this](PopupSelector *, PopupItem *selectedItem)
    {
      if (parameterLink && parameterLink->hostControl)
        parameterLink->hostControl->beginChangeGesture();

      auto *option = (Framework::IndexedData *)selectedItem->extraData;

      beginChange(getValue());
      auto unscaledValue = unscaleValue(Framework::getValueFromOptionId(option->id, details), 
        details, getPlugin(uiRelated.renderer).getSampleRate());
      setValue(unscaledValue, true);
      setValueToHost();
      endChange();

      controlFlags.isInModalState = false;

      if (parameterLink && parameterLink->hostControl)
        parameterLink->hostControl->endChangeGesture();
    };
    selector->cancel = [this](PopupSelector *)
    {
      controlFlags.isInModalState = false;
    };
    selector->summon(this, { e.x, e.y });
  }

  bool 
  TextSelector::mouseUp(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return Slider::mouseUp(e);
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
    sizingFlags |= Component::CustomDimensions;
    getDimensions = [](Component *c, i32 *availableWidth)
    {
      auto *self = (Numberbox *)c;
      i32 max = self->editor.bounds.h;

      if (!availableWidth)
      {
        max = self->editor.bounds.w;

        float height = (float)self->editor.desiredSize.y;

        if (self->drawBackgroundArrow)
        {
          self->padding.x = (u16)::ceilf(height * (kTriangleWidthRatio + kTriangleToValueMarginRatio));
          self->padding.w = (u16)::ceilf(kValueToEndMarginRatio * height);
        }
        else
        {
          self->padding.x = (u16)::ceilf(height * 0.5f);
          self->padding.w = self->padding.x;
        }
      }

      return Range<i32>{ max, max };
    };

    editor.control = this;
    editor.textColour = Skin::kWidgetPrimary1;
    textEntry = &editor;
    addChildComponent(&editor);
  }

  bool 
  Numberbox::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.2f;

    float rounding = Interface::getValue(Skin::kWidgetRoundedCorner, true, this);
    (void)rounding;
    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);


    //nvgBeginPath(openGl.g);
    //nvgRect(openGl, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h);
    //nvgFillColor(openGl, Colours::white);
    //nvgFill(openGl);

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
    else if (auto alpha = animator.getValue(Animator::Hover); alpha != 0.0f)
    {
      auto [x, y, w, h] = bounds.toFloat();
      nvgRoundedRect(openGl.g, x, y, w, h, scaleValue(backgroundRounding));
      nvgFillColor(openGl.g, getColour(Skin::kWidgetBackground2).withAlpha(alpha));
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
}
