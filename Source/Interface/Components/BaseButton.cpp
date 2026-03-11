
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

    if (isOn())
    {
      activeColour = (componentFlags.isClicked) ? activeColour : colour;
      hoverColour = colour.brighter(0.6f);
    }

    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);
    if (!componentFlags.isClicked)
      colour = activeColour.interpolatedWith(hoverColour, animator.getValue(Animator::Hover));

    Colour colours[] = { colour };
    auto [fn, iconBounds] = Paths::powerButtonIcon();
    fn(*openGl.cache, colours, getLocalBounds().toFloat(), 1.0f);

    return true;
  }


  RadioButton::RadioButton()
  {
    padding = { kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
    desiredSize = { kDimensions, kDimensions, kDimensions, kDimensions };
  }

  bool RadioButton::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
    auto onNormalColor = getColour(Skin::kWidgetAccent1);
    auto offNormalColor = getColour(Skin::kPowerButtonOff);
    auto backgroundColor = getColour(Skin::kBackground);

    desiredSize = { 16, 16, 16, 16 };
    rounding = 16 / 8;
    
    animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);
    auto hoverAmount = animator.getValue(Animator::Hover);
    auto drawBounds = getLocalBounds().toFloat().trimmed(scaleValue(padding.toFloat()));
    float scaledRounding = scaleValue(rounding);

    static constexpr float kPowerShrinkRadius = 0.2f;
    //static constexpr float kPowerHoverRadius = 1.0f;

    //auto *backgroundComponent = utils::as<OpenGlQuad>(&target);
    //auto quadData = backgroundComponent->getQuadData();
    //quadData.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
    if (componentFlags.isClicked || !isOn())
    {
      //fillRect(openGl, drawBounds, backgroundColor, scaledRounding);
    }
    else if (hoverAmount != 0.0f)
    {
      //fillRect(openGl, drawBounds, backgroundColor.withMultipliedAlpha(hoverAmount), scaledRounding);
    }

    auto colour = (isOn()) ? onNormalColor : offNormalColor;
    fillRect(openGl, drawBounds.trimmed(kPowerShrinkRadius * drawBounds.w, 
      kPowerShrinkRadius * drawBounds.h), colour, scaledRounding);

    //quadData.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
    //backgroundComponent->render(openGl);

    return true;
  }
}
