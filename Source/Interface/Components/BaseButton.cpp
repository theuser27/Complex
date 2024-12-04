/*
  ==============================================================================

    PluginButton.cpp
    Created: 14 Dec 2022 7:01:32am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseButton.hpp"

#include "Framework/parameter_bridge.hpp"
#include "Framework/parameter_value.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "OpenGlImage.hpp"
#include "OpenGlQuad.hpp"

namespace Interface
{
  BaseButton::BaseButton(Framework::ParameterValue *parameter)
  {
    setRepaintsOnMouseActivity(false);
    
    if (!parameter)
      return;

    hasParameter_ = true;
    canInputValue_ = false;

    auto name = parameter->getParameterDetails().id;
    setName({ name.data(), name.size() });
    setParameterLink(parameter->getParameterLink());
    setParameterDetails(parameter->getParameterDetails());
    setValueRaw(parameterLink_->parameter->getNormalisedValue());
  }

  BaseButton::~BaseButton() = default;

  void BaseButton::mouseDown(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      if (!hasParameter())
        return;

      mouseExit(e);

      showPopupSelector(this, e.getPosition(), std::move(createPopupMenu()),
        [this](int selection) { handlePopupResult(selection); }, {}, kMinPopupWidth);

      return;
    }

    updateState(true, true);

    for (auto &component : openGlComponents_)
      component->getAnimator().setIsClicked(true);
  }

  void BaseButton::mouseUp(const MouseEvent &e)
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

    if (parameterLink_ && parameterLink_->hostControl)
      parameterLink_->hostControl->beginChangeGesture();

    beginChange(getValue());

    setValue(!getValue(), NotificationType::sendNotificationSync);

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

  void BaseButton::setValue(double shouldBeOn, NotificationType notification)
  {
    if (shouldBeOn == getValueRaw())
      return;

    setValueRaw(shouldBeOn);

    if (notification != NotificationType::dontSendNotification)
    {
      valueChanged();
    }
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
    shapeComponent_.getAnimator().setHoverIncrement(kHoverIncrement);
    shapeComponent_.setShapes(Paths::powerButtonIcon());
    shapeComponent_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        Colour finalColour = activeColour_;

        auto &animator = target.getAnimator();
        animator.tick(openGl.animate);

        if (!isHeldDown_)
          finalColour = finalColour.interpolatedWith(hoverColour_, animator.getValue(Animator::Hover));

        utils::as<PlainShapeComponent>(&target)->setColor(finalColour);
        target.render(openGl);
      });

    addOpenGlComponent(&shapeComponent_);

    setAddedHitbox(BorderSize{ kAddedMargin });
  }

  PowerButton::~PowerButton() = default;

  void PowerButton::setColours()
  {
    BaseButton::setColours();
    onNormalColor_ = getColour(Skin::kWidgetAccent1);
  }

  juce::Rectangle<int> PowerButton::setSizes(int height, int)
  {
    if (drawBounds_.getHeight() != height)
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

    if (getValue())
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
    backgroundComponent_.getAnimator().setHoverIncrement(kHoverIncrement);
    backgroundComponent_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        static constexpr float kPowerRadius = 0.8f;
        static constexpr float kPowerHoverRadius = 1.0f;

        auto &animator = target.getAnimator();
        animator.tick(openGl.animate);

        auto hoverAmount = animator.getValue(Animator::Hover);
        auto *backgroundComponent = utils::as<OpenGlQuad>(&target);
        backgroundComponent->setQuad(0, -kPowerHoverRadius, -kPowerHoverRadius, 2.0f * kPowerHoverRadius, 2.0f * kPowerHoverRadius);
        if (isHeldDown_ || !getValue())
        {
          backgroundComponent->setColor(backgroundColor_);
          backgroundComponent->render(openGl);
        }
        else if (hoverAmount != 0.0f)
        {
          backgroundComponent->setColor(backgroundColor_.get().withMultipliedAlpha(hoverAmount));
          backgroundComponent->render(openGl);
        }

        if (getValue())
          backgroundComponent->setColor(onNormalColor_);
        else
          backgroundComponent->setColor(offNormalColor_);

        backgroundComponent->setQuad(0, -kPowerRadius, -kPowerRadius, 2.0f * kPowerRadius, 2.0f * kPowerRadius);
        backgroundComponent->render(openGl);
      });

    addOpenGlComponent(&backgroundComponent_);

    addLabel();
    setAddedHitbox(BorderSize{ kAddedMargin });
  }

  RadioButton::~RadioButton() = default;

  void RadioButton::setColours()
  {
    BaseButton::setColours();
    onNormalColor_ = getColour(Skin::kWidgetAccent1);
    offNormalColor_ = getColour(Skin::kPowerButtonOff);
    backgroundColor_ = getColour(Skin::kBackground);
  }

  juce::Rectangle<int> RadioButton::setSizes(int height, int)
  {
    if (drawBounds_.getHeight() != height)
    {
      drawBounds_ = { height, height };
    }
    
    setExtraElementsPositions(drawBounds_);
    if (label_)
      return drawBounds_.getUnion(label_->getBounds());
    return drawBounds_;
  }

  void RadioButton::setExtraElementsPositions(juce::Rectangle<int> anchorBounds)
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

    label_->setBounds(labelX, 
      anchorBounds.getY() - (label_->getTotalHeight() - anchorBounds.getHeight()) / 2, 
      labelTextWidth, label_->getTotalHeight());
  }

  void RadioButton::redoImage()
  {
    if (getValue())
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

  void RadioButton::setRounding(float rounding) noexcept { backgroundComponent_.setRounding(rounding); }


  OptionsButton::OptionsButton(Framework::ParameterValue *parameter, String name, String displayText) :
    BaseButton{ parameter }, textComponent_{ "Options Button Text", displayText }
  {
    if (!parameter)
      setName(name);

    setText(std::move(displayText));

    addOpenGlComponent(&borderComponent_);
    addOpenGlComponent(&plusComponent_);
    addOpenGlComponent(&textComponent_);
  }

  OptionsButton::~OptionsButton() = default;

  void OptionsButton::setColours()
  {
    BaseButton::setColours();
    borderColour_ = getColour(Skin::kBody);
  }

  juce::Rectangle<int> OptionsButton::setSizes(int height, int width)
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
    int halfHeight = drawBounds_.getHeight() / 2;
    juce::Rectangle plusBounds{ drawBounds_.getX() + halfHeight, drawBounds_.getY() + halfHeight - plusSize / 2, plusSize, plusSize };
    plusComponent_.setBounds(plusBounds);
    juce::Rectangle textBounds{ plusBounds.getRight() + halfHeight, 0, getWidth() - plusBounds.getRight() - halfHeight, getHeight() };
    textComponent_.setBounds(textBounds);

    if (redoImage)
      this->redoImage();
  }

  ActionButton::ActionButton(String name, String displayText) : BaseButton{ nullptr },
     textComponent_{ "Action Button Text", displayText }, text_{ std::move(displayText) }
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

    setValue(!getValue(), NotificationType::sendNotificationSync);

    for (auto &component : openGlComponents_)
      component->getAnimator().setIsClicked(false);
  }

  void ActionButton::setColours()
  {
    BaseButton::setColours();
    fillColour_ = getColour(Skin::kActionButtonPrimary);
    textColour_ = getColour(Skin::kActionButtonText);
  }

  juce::Rectangle<int> ActionButton::setSizes(int height, int width)
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
    textComponent_.setJustification(Justification::centred);
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
