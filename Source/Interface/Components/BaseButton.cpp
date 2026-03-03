
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
    fn(*openGl.cache, colours, bounds.toFloat(), 1.0f);

    return true;
  }


  RadioButton::RadioButton()
  {
    padding = { kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
  }

  bool RadioButton::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
    //auto onNormalColor = getColour(Skin::kWidgetAccent1);
    //auto offNormalColor = getColour(Skin::kPowerButtonOff);
    //auto backgroundColor = getColour(Skin::kBackground);
    //
    //animator.tick(componentFlags.isHovered, componentFlags.isClicked, kHoverIncrement, 0.0f);


    //static constexpr float kPowerRadius = 0.8f;
    //static constexpr float kPowerHoverRadius = 1.0f;

    //animator_.tick(openGl.animate);

    //auto hoverAmount = animator_.getValue(Animator::Hover);
    //auto *backgroundComponent = utils::as<OpenGlQuad>(&target);
    //auto quadData = backgroundComponent->getQuadData();
    //quadData.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
    //if (isHeldDown_ || !isOn())
    //{
    //  backgroundComponent->setColor(backgroundColor_);
    //  backgroundComponent->render(openGl);
    //}
    //else if (hoverAmount != 0.0f)
    //{
    //  backgroundComponent->setColor(backgroundColor_.get().withMultipliedAlpha(hoverAmount));
    //  backgroundComponent->render(openGl);
    //}

    //if (isOn())
    //  backgroundComponent->setColor(onNormalColor_);
    //else
    //  backgroundComponent->setColor(offNormalColor_);

    //quadData.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
    //backgroundComponent->render(openGl);

    return true;
  }
}
