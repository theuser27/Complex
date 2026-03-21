
// Created: 2022-12-14 06:59:59

#include "BaseControl.hpp"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
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
      auto valueInterval = 1.0 / (details.options->count - 1);
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
    tickAnimation(animationValues, { { componentFlags.isHovered } }, { { kHoverIncrement } });
    float thickness = knobArcThickness * (1.0f + 0.15f * animationValues[0]);
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
    overrideDimensions = [](Component *c, bool isCalculatingVertical)
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

  bool TextSelector::render(OpenGlWrapper &openGl)
  {
    static constexpr float kHoverIncrement = 0.2f;

    tickAnimation(animationValues, { { componentFlags.isHovered } }, { { kHoverIncrement } });

    if (auto alpha = animationValues[0]; alpha != 0.0f)
    {
      nvgBeginPath(openGl);
      nvgRoundedRect(openGl, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h,
        Interface::getValue(Skin::kWidgetRoundedCorner, true));
      nvgFillColor(openGl, getColour(Skin::kWidgetBackground2, this).withAlpha(alpha));
      nvgFill(openGl);
    }

    return true;
  }

  struct TextSelectorItem : PopupItem
  {
    TextSelectorItem()
    {
      overrideDimensions = [](Component *c, bool isCalculatingVertical)
      {
        auto *item = (TextSelectorItem *)c;

        i32 minSize{}, maxSize{};
        bool canTextWrap = false;
        utils::string_view text;

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
          text = option->displayName;
        }

        float height = scaleValue((float)item->desiredSize.h);
        uiRelated.cache->setFont(FontId::InterType, height);
        nvgTextAlign(uiRelated.cache->context, NVG_ALIGN_LEFT | NVG_ALIGN_CENTER);

        if (!isCalculatingVertical)
        {
          minSize = (canTextWrap) ? 0 : (i32)::ceilf(uiRelated.cache->getStringWidthFloat(text));
          maxSize = -1;
        }
        else
        {
          auto lineCount = uiRelated.cache->getStringNumberOfLines(text, (float)item->bounds.w);
          minSize = (i32)::roundf(height * (float)lineCount);
          maxSize = minSize;
        }

        return Range<i32>{ minSize, maxSize };
      };
    }

    bool 
    render(OpenGlWrapper &openGl) override
    {
      PopupItem::render(openGl);
      
      bool canTextWrap = false;
      utils::string_view text;
      Skin::ColourId textColourId = Skin::kTextComponentText1;

      if (id)
      {
        // label whose extra data is a string
        canTextWrap = true;
        text = *((utils::stringnd *)extraData);

        textColourId = Skin::kTextComponentText2;
        // draw background
        fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kPopupSelectorDelimiter, this));
      }
      else
      {
        auto *option = (Framework::IndexedData *)extraData;
        canTextWrap = !option->id;
        text = option->displayName;
      }

      renderText(text, FontId::InterType, getLocalBounds().trimmed(scaleValueRoundInt(padding.toInt())).toFloat(),
        openGl.cache, getColour(textColourId, this), Placement::left, canTextWrap);

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

    if (isDropdownOpen)
    {
      selector->resetState();
      isDropdownOpen = false;
      return true;
    }

    selector->resetState();
    lastValue = getValue();
    isDropdownOpen = true;

    auto *itemArena = selector->arena;

    PopupList *options = anew(itemArena, PopupList, { selector });
    options->desiredSize = { kPopupMinWidth, 0, utils::max_limit<i32>, utils::max_limit<i32> };
    options->padding = { 0, 4, 0, 4 };
    options->draw = [](OpenGlWrapper &openGl, PopupList *self)
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

    TextSelectorItem *name = anew(itemArena, TextSelectorItem, {});
    name->arena = itemArena;
    name->id = 1;
    name->canBeChosen = false;
    name->sizingFlags = (Component::SizingFlags)(Component::GrowableX);
    name->extraData = anew(itemArena, utils::stringnd, { itemArena, (!dropdownTitle.empty()) ?
      utils::string_view{ dropdownTitle } : details.displayName });
    name->associatedList = options;
    name->padding = { 12, 2, 12, 2 };
    name->desiredSize = { 0, 14, 0, 14 };
    options->addChildComponent(name);

    bool exitedChild = false;
    auto *option = details.options->children;
    u32 size = details.options->count;
    u32 index = 0;

    while (true)
    {
      {
        TextSelectorItem *item = anew(itemArena, TextSelectorItem, {});
        item->arena = itemArena;
        item->extraData = option;
        item->canBeChosen = option->id && option->count;
        item->associatedList = options;
        item->sizingFlags = (Component::SizingFlags)(Component::GrowableX);
        item->componentFlags.acceptsOrphanedMouseEvents = true;
        item->padding = { 12, 4, 12, 4 };
        item->desiredSize = { 0, kPrimaryTextLineHeight, 0, kPrimaryTextLineHeight };
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

      if (option->count)
        ++index;

      // going forward
      if (index < size)
      {
        option = option->next;
        continue;
      }

      // going up
      exitedChild = true;
      option = option->parent;
      if (!option->parent)
        break;
      size = option->parent->count;

      // find the index of the parent again
      index = 0;
      for (auto *child = option; index < size; ++index)
        if (child == option)
          break;
    }

    selector->list = options;
    selector->skinOverride = getSkinOverride();
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
    overrideDimensions = [](Component *c, bool isCalculatingVertical)
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
      { { componentFlags.isHovered || componentFlags.isClicked } }, 
      { { kHoverIncrement } });

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
}
