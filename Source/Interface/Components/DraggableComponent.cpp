/*
  ==============================================================================

    DraggableComponent.cpp
    Created: 10 Feb 2023 5:50:16am
    Author:  theuser27

  ==============================================================================
*/

#include "DraggableComponent.hpp"
#include "../Sections/EffectModuleSection.hpp"

namespace Interface
{
  void DraggableComponent::paint(juce::Graphics &g)
  {
    static constexpr float kDotDiameter = 2.0f;
    static constexpr float kDotsOffset = 6.0f;

    auto dotsDiameter = scaleValueRoundInt(kDotDiameter);
    auto dotsOffset = scaleValueRoundInt(kDotsOffset);

    auto centeredX = utils::centerAxis(dotsDiameter + dotsOffset, getWidth());
    auto centeredY = utils::centerAxis(dotsDiameter + dotsOffset, getHeight());

    auto centreRectangle = juce::Rectangle{ centeredX, centeredY, dotsOffset, dotsOffset }.toFloat();

    g.setColour(getDraggedComponent()->getColour(Skin::kWidgetSecondary1));
    g.fillEllipse(centreRectangle.getX(), centreRectangle.getY(), kDotDiameter, kDotDiameter);
    g.fillEllipse(centreRectangle.getX(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
    g.fillEllipse(centreRectangle.getRight(), centreRectangle.getY(), kDotDiameter, kDotDiameter);
    g.fillEllipse(centreRectangle.getRight(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
  }

  void DraggableComponent::mouseMove(const juce::MouseEvent &)
  {
    setMouseCursor(juce::MouseCursor::StandardCursorType::DraggingHandCursor);
  }

  void DraggableComponent::mouseDown(const juce::MouseEvent &e)
  {
    // this might create a copy so we need to get the actual component we're dragging
    currentlyDraggedComponent_ = listener_->prepareToMove(draggedComponent_, e, e.mods.isCommandDown());
    COMPLEX_ASSERT(currentlyDraggedComponent_);
    currentlyDraggedComponent_->setAlwaysOnTop(true);
    currentlyDraggedComponent_->setIgnoreClip(ignoreClipIncluding_);
    initialPosition_ = currentlyDraggedComponent_->getPosition();
  }

  void DraggableComponent::mouseDrag(const juce::MouseEvent &e)
  {
    currentlyDraggedComponent_->setTopLeftPosition(initialPosition_ + e.getOffsetFromDragStart());
    listener_->draggingComponent(currentlyDraggedComponent_, e);
  }

  void DraggableComponent::mouseUp(const juce::MouseEvent &e)
  {
    COMPLEX_ASSERT(currentlyDraggedComponent_);
    listener_->releaseComponent(currentlyDraggedComponent_, e);
    currentlyDraggedComponent_->setIgnoreClip(nullptr);
    currentlyDraggedComponent_->setAlwaysOnTop(false);

    currentlyDraggedComponent_ = nullptr;
  }

  void DraggableComponent::mouseExit(const juce::MouseEvent &)
  {
    setMouseCursor(juce::MouseCursor::StandardCursorType::NormalCursor);
  }

  void DraggableComponent::mouseWheelMove(const juce::MouseEvent &e, 
    const juce::MouseWheelDetails &wheel)
  {
    if (!currentlyDraggedComponent_)
    {
      BaseComponent::mouseWheelMove(e, wheel);
      return;
    }

    initialPosition_ += listener_->mouseWheelWhileDragging(currentlyDraggedComponent_, e, wheel);
    currentlyDraggedComponent_->setTopLeftPosition(initialPosition_ + e.getOffsetFromDragStart());
  }

}
