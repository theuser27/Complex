
// Created: 2022-12-14 07:01:32

#include "BaseControl.hpp"

#include "Framework/parameter_bridge.hpp"
#include "Framework/parameter_value.hpp"
#include "../LookAndFeel/ui_constants.hpp"

namespace Interface
{
  bool 
  Button::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
    {
      if (!controlFlags.hasParameter)
        return true;

      componentFlags.isClicked = false;
      mouseExit(e);

      createPopupMenu(getPopupSelector(), Point{ e.x, e.y });

      return true;
    }

    return true;
  }

  bool 
  Button::mouseUp(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::popupMenuClickModifier))
      return true;

    if (!componentFlags.isClicked || !componentFlags.isHovered)
      return false;

    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->beginChangeGesture();

    beginChange(isOn());

    setValue(!isOn(), true);

    setValueToHost();
    endChange();

    if (parameterLink && parameterLink->hostControl)
      parameterLink->hostControl->endChangeGesture();

    return true;
  }

  PowerButton::PowerButton()
  {
    padding = { kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
  }

  bool PowerButton::render(OpenGlWrapper &openGl)
  {
    auto colour = getColour(Skin::kWidgetAccent1);

    Colour activeColour = colour.withBrightness(0.7f);
    Colour hoverColour = colour;

    activeColour = (componentFlags.isClicked) ? activeColour : colour;
    hoverColour = colour.brighter(0.6f);

    tickAnimation(animationValues, { { componentFlags.isHovered || componentFlags.isClicked } }, { { kHoverIncrement} });
    if (!componentFlags.isClicked)
      colour = activeColour.interpolatedWith(hoverColour, animationValues[0]);

    Colour colours[] = { colour };
    auto [fn, iconBounds] = Paths::powerButtonIcon();
    fn(*openGl.cache, colours, getLocalBounds().toFloat().trim(scaleValue(padding.toFloat())), scaleValue(1.0f));

    return true;
  }


  RadioButton::RadioButton()
  {
    padding = { kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
    desiredSize = { kDimensions, kDimensions, kDimensions, kDimensions };
  }

  bool RadioButton::render(OpenGlWrapper &openGl)
  {
    auto onNormalColor = getColour(Skin::kWidgetAccent1);
    auto offNormalColor = getColour(Skin::kPowerButtonOff);
    auto backgroundColor = getColour(Skin::kBackground);
    
    tickAnimation(animationValues, { { componentFlags.isHovered } }, { { kHoverIncrement} });
    auto hoverAmount = animationValues[0];
    auto drawBounds = getLocalBounds().toFloat().trimmed(scaleValue(padding.toFloat()));
    
    float rounding = utils::min(drawBounds.w, drawBounds.h) * roundingRatio;

    //static constexpr float kPowerShrinkRadius = 0.2f;
    static constexpr float kPowerHoverRadius = 0.25f;

    //auto *backgroundComponent = utils::as<OpenGlQuad>(&target);
    //auto quadData = backgroundComponent->getQuadData();
    //quadData.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
    if (componentFlags.isClicked || !isOn())
    {
      //fillRect(openGl, drawBounds.expanded(kPowerHoverRadius * drawBounds.w, 
      //  kPowerHoverRadius * drawBounds.h), backgroundColor, rounding * kPowerHoverRadius);
    }
    else if (hoverAmount != 0.0f)
    {
      //fillRect(openGl, drawBounds.expanded(kPowerHoverRadius * drawBounds.w,
      //  kPowerHoverRadius * drawBounds.h), backgroundColor.withMultipliedAlpha(hoverAmount), 
      //  rounding + rounding * kPowerHoverRadius);
    }

    auto colour = (isOn()) ? onNormalColor : offNormalColor;
    fillRect(openGl, drawBounds, colour, rounding);

    //quadData.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
    //backgroundComponent->render(openGl);

    return true;
  }
}
