/*
  ==============================================================================

    PluginButton.cpp
    Created: 14 Dec 2022 7:01:32am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseControl.hpp"

#include "Framework/parameter_bridge.hpp"
#include "Framework/parameter_value.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "OpenGlImage.hpp"
#include "OpenGlQuad.hpp"

namespace Interface
{
  bool BaseButton::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      if (!controlFlags.hasParameter)
        return true;

      mouseExit(e);

      showPopupSelector(this, Point{ e.x, e.y }, COMPLEX_MOVE(createPopupMenu()),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);

      return;
    }

    updateState(true, true);

    animator_.isClicked = true;
    return true;
  }

  bool BaseButton::mouseUp(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
      return true;

    bool wasDown = isHeldDown();
    bool isOver = isMouseOver(true);
    updateState(false, isOver);

    if (!wasDown || !isOver)
    {
      if (wasDown)
      {
        for (auto &component : openGlComponents_)
          component->getAnimator().setIsClicked(false);
      }
      return;
    }

    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->beginChangeGesture();

    beginChange(isOn());

    setValue(!isOn(), true);

    setValueToHost();
    endChange();

    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->endChangeGesture();

    for (auto &component : openGlComponents_)
      component->getAnimator().setIsClicked(false);
  }

  void BaseButton::mouseEnter(const MouseEvent &)
  {
    updateState(false, true);
    for (auto &component : openGlComponents_)
      component->getAnimator().setIsHovered(true);
  }

  void BaseButton::mouseExit(const MouseEvent &)
  {
    updateState(false, false);
    for (auto &component : openGlComponents_)
      component->getAnimator().setIsHovered(false);
  }

  String BaseButton::getTextFromValue(bool value) const noexcept
  {
    if (!details_.indexedData.empty())
    {
      auto string = details_.indexedData[(usize)value].displayName;
      return { string.data(), string.size() };
    }

    return (value) ? "On" : "Off";
  }

  void BaseButton::updateState(bool isHeldDown, bool isHoveredOver) noexcept
  {
    if (isEnabled() && isVisible() && !isCurrentlyBlockedByAnotherModalComponent())
    {
      isHeldDown_ = isHeldDown;
      isHoveredOver_ = isHoveredOver;
    }
  }

  PowerButton::PowerButton(Framework::ParameterValue *parameter) : BaseButton{ parameter }
  {
    animator_.setHoverIncrement(kHoverIncrement);
    shapeComponent_.setShapes(Paths::powerButtonIcon());
    shapeComponent_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        Colour finalColour = activeColour_;

        animator_.tick(openGl.animate);

        if (!isHeldDown_)
          finalColour = finalColour.interpolatedWith(hoverColour_, animator_.getValue(Animator::Hover));

        utils::as<PlainShapeComponent>(&target)->setColor(finalColour);
        target.render(openGl);
      });

    addOpenGlComponent(&shapeComponent_);

    setAddedHitbox(BorderSize{ kAddedMargin });
  }

  void PowerButton::setColours()
  {
    BaseButton::setColours();
    onNormalColor_ = getColour(Skin::kWidgetAccent1);
  }

  Rectangle<int> PowerButton::setSizes(int height, int)
  {
    if (drawBounds_.h != height)
    {
      drawBounds_ = { height, height };
    }
    return drawBounds_;
  }

  void PowerButton::redoImage()
  {
    Colour onHoverColor_ = onNormalColor_.brighter(0.6f);
    Colour onDownColor_ = onNormalColor_.withBrightness(0.7f);
    Colour offHoverColor_ = onNormalColor_;
    Colour offDownColor_ = onDownColor_;

    if (isOn())
    {
      activeColour_ = (isHeldDown_) ? onDownColor_ : onNormalColor_;
      hoverColour_ = onHoverColor_;
    }
    else
    {
      activeColour_ = (isHeldDown_) ? offDownColor_ : onDownColor_;
      hoverColour_ = offHoverColor_;
    }

    shapeComponent_.redrawImage();
  }

  void PowerButton::setComponentsBounds(bool redoImage)
  {
    shapeComponent_.setBounds(drawBounds_);

    if (redoImage)
      this->redoImage();
  }


  RadioButton::RadioButton(Framework::ParameterValue *parameter) : BaseButton{ parameter }
  {
    animator_.setHoverIncrement(kHoverIncrement);
    backgroundComponent_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        static constexpr float kPowerRadius = 0.8f;
        static constexpr float kPowerHoverRadius = 1.0f;

        animator_.tick(openGl.animate);

        auto hoverAmount = animator_.getValue(Animator::Hover);
        auto *backgroundComponent = utils::as<OpenGlQuad>(&target);
        auto quadData = backgroundComponent->getQuadData();
        quadData.setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
        if (isHeldDown_ || !isOn())
        {
          backgroundComponent->setColor(backgroundColor_);
          backgroundComponent->render(openGl);
        }
        else if (hoverAmount != 0.0f)
        {
          backgroundComponent->setColor(backgroundColor_.get().withMultipliedAlpha(hoverAmount));
          backgroundComponent->render(openGl);
        }

        if (isOn())
          backgroundComponent->setColor(onNormalColor_);
        else
          backgroundComponent->setColor(offNormalColor_);

        quadData.setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
        backgroundComponent->render(openGl);
      });

    addOpenGlComponent(&backgroundComponent_);

    addLabel();
    addedHitbox = Rectangle<u16>{ kAddedMargin, kAddedMargin, kAddedMargin, kAddedMargin };
  }

  void RadioButton::setColours()
  {
    BaseButton::setColours();
    onNormalColor_ = getColour(Skin::kWidgetAccent1);
    offNormalColor_ = getColour(Skin::kPowerButtonOff);
    backgroundColor_ = getColour(Skin::kBackground);
  }

  Rectangle<int> RadioButton::setSizes(int height, int)
  {
    if (drawBounds_.h != height)
    {
      drawBounds_ = { height, height };
    }
    
    setExtraElementsPositions(drawBounds_);
    if (label_)
      return drawBounds_.getUnion(label_->getBoundsSafe());
    return drawBounds_;
  }

  void RadioButton::setExtraElementsPositions(Rectangle<int> anchorBounds)
  {
    if (!label_)
      return;

    label_->updateState();
    auto labelTextWidth = label_->getTotalWidth();
    auto labelX = anchorBounds.x;
    switch (labelPlacement_)
    {
    case Placement::right:
      labelX += anchorBounds.w + scaleValueRoundInt(kLabelOffset);
      label_->setJustification(Placement::centerVertical | Placement::left);
      break;
    default:
    case Placement::above:
    case Placement::below:
    case Placement::left:
      labelX -= scaleValueRoundInt(kLabelOffset) + labelTextWidth;
      label_->setJustification(Placement::centerVertical | Placement::right);
      break;
    }

    label_->setBounds(labelX, 
      anchorBounds.y - (label_->getTotalHeight() - anchorBounds.h) / 2, 
      labelTextWidth, label_->getTotalHeight());
  }

  void RadioButton::redoImage()
  {
    if (isOn())
      backgroundComponent_.setColor(onNormalColor_);
    else
      backgroundComponent_.setColor(offNormalColor_);
  }

  void RadioButton::setComponentsBounds(bool redoImage)
  {
    backgroundComponent_.setBounds(drawBounds_);

    if (redoImage)
      this->redoImage();
  }

  OptionsButton::OptionsButton(Framework::ParameterValue *parameter, std::string displayText) :
    BaseButton{ parameter }, text_{ COMPLEX_MOVE(displayText) }
  {
  }

  OptionsButton::~OptionsButton() = default;

  bool OptionsButton::render(OpenGlWrapper &openGl) override
  {
    animator_.tick(openGl.animate);
    borderComponent_.setThickness(animator_.getValue(Animator::Hover) * 
      0.25f * normalThickness_ + normalThickness_);

    return true;
  }

  void OptionsButton::setColours()
  {
    BaseButton::setColours();
    borderColour_ = getColour(Skin::kBody);
  }

  Rectangle<int> OptionsButton::setSizes(int height, int width)
  {
    drawBounds_ = { width, height };
    return drawBounds_;
  }

  void OptionsButton::redoImage()
  {
    borderComponent_.setRounding(scaleValue(kBorderRounding));
    borderComponent_.setColor(borderColour_);

    plusComponent_.setThickness(scaleValue(1.0f / (float)kPlusRelativeSize));
    plusComponent_.setColor(getColour(Skin::kNormalText));
    
    textComponent_.setFontType(PlainTextComponent::kText);
    textComponent_.setJustification(Justification::centredLeft);
    textComponent_.setText(text_);
  }

  void OptionsButton::setComponentsBounds(bool redoImage)
  {
    borderComponent_.setBounds(drawBounds_);
    int plusSize = scaleValueRoundInt(kPlusRelativeSize);
    int halfHeight = drawBounds_.h / 2;
    Rectangle plusBounds{ drawBounds_.x + halfHeight, drawBounds_.y + halfHeight - plusSize / 2, plusSize, plusSize };
    plusComponent_.setBounds(plusBounds);
    Rectangle textBounds{ plusBounds.getRight() + halfHeight, 0, getWidth() - plusBounds.getRight() - halfHeight, getHeight() };
    textComponent_.setBounds(textBounds);

    if (redoImage)
      this->redoImage();
  }

  ActionButton::ActionButton(std::string name, std::string displayText) : BaseButton{ nullptr },
     textComponent_{ "Action Button Text", displayText }, text_{ COMPLEX_MOVE(displayText) }
  {
    setName(name);

    addOpenGlComponent(&fillComponent_);
    addOpenGlComponent(&textComponent_);
  }

  ActionButton::~ActionButton() = default;

  void ActionButton::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
      return;
    
    updateState(true, true);

    for (auto &component : openGlComponents_)
      component->getAnimator().setIsClicked(true);
  }

  void ActionButton::mouseUp(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
      return;

    bool wasDown = isHeldDown();
    bool isOver = isMouseOver(true);
    updateState(false, isOver);

    if (!wasDown || !isOver)
    {
      if (wasDown)
      {
        for (auto &component : openGlComponents_)
          component->getAnimator().setIsClicked(false);
      }
      return;
    }

    action_();

    setValue(!isOn(), NotificationType::sendNotificationSync);

    for (auto &component : openGlComponents_)
      component->getAnimator().setIsClicked(false);
  }

  void ActionButton::setColours()
  {
    BaseButton::setColours();
    fillColour_ = getColour(Skin::kActionButtonPrimary);
    textColour_ = getColour(Skin::kActionButtonText);
  }

  Rectangle<int> ActionButton::setSizes(int height, int width)
  {
    drawBounds_ = { width, height };
    return drawBounds_;
  }

  void ActionButton::redoImage()
  {
    fillComponent_.setRounding(scaleValue(kBorderRounding));
    fillComponent_.setColor(fillColour_);
    
    textComponent_.setFontType(PlainTextComponent::kText);
    textComponent_.setTextColour(textColour_);
    textComponent_.setJustification(Placement::center);
    textComponent_.setText(text_);
  }

  void ActionButton::setComponentsBounds(bool redoImage)
  {
    fillComponent_.setBounds(drawBounds_);
    textComponent_.setBounds(drawBounds_);

    if (redoImage)
      this->redoImage();
  }
}
