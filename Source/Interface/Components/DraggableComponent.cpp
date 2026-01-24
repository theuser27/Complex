/*
  ==============================================================================

    DraggableComponent.cpp
    Created: 10 Feb 2023 5:50:16am
    Author:  theuser27

  ==============================================================================
*/

#include "DraggableComponent.hpp"

#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../Sections/EffectModuleSection.hpp"

namespace Interface
{
  bool DraggableComponent::render(OpenGlWrapper &openGl)
  {
    static constexpr float kDotDiameter = 2.0f;
    static constexpr float kDotsOffset = 6.0f;

    int dotsDiameter = scaleValueRoundInt(kDotDiameter);
    int dotsOffset = scaleValueRoundInt(kDotsOffset);

    int centeredX = utils::centerAxis(dotsDiameter + dotsOffset, bounds.w);
    int centeredY = utils::centerAxis(dotsDiameter + dotsOffset, bounds.h);

    auto centreRectangle = Rectangle{ (float)centeredX, (float)centeredY, 
      (float)dotsOffset, (float)dotsOffset };

    nvgFillColor(g.context, getColour(Skin::kWidgetSecondary1));
    nvgEllipse(g.context, centreRectangle.x, centreRectangle.y, kDotDiameter, kDotDiameter);
    nvgEllipse(g.context, centreRectangle.x, centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
    nvgEllipse(g.context, centreRectangle.getRight(), centreRectangle.y, kDotDiameter, kDotDiameter);
    nvgEllipse(g.context, centreRectangle.getRight(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);

    return true;
  }

  bool DraggableComponent::mouseEnter(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::DraggingHandCursor);
    return true;
  }

  void DraggableComponent::mouseDown(const MouseEvent &e)
  {
    // this might create a copy so we need to get the actual component we're dragging
    currentlyDraggedComponent_ = listener_->prepareToMove(draggedComponent_, e, e.mods.isCommandDown());
    COMPLEX_ASSERT(currentlyDraggedComponent_);
    currentlyDraggedComponent_->layerIndex = 1;
    currentlyDraggedComponent_->setIgnoreClip(ignoreClipIncluding_);
    initialPosition_ = currentlyDraggedComponent_->getPosition();
  }

  void DraggableComponent::mouseDrag(const MouseEvent &e)
  {
    currentlyDraggedComponent_->setPosition(initialPosition_ + e.getOffsetFromDragStart());
    listener_->draggingComponent(currentlyDraggedComponent_, e);
  }

  void DraggableComponent::mouseUp(const MouseEvent &e)
  {
    COMPLEX_ASSERT(currentlyDraggedComponent_);
    listener_->releaseComponent(currentlyDraggedComponent_, e);
    currentlyDraggedComponent_->setIgnoreClip(nullptr);
    currentlyDraggedComponent_->layerIndex = 0;

    currentlyDraggedComponent_ = nullptr;
  }

  bool DraggableComponent::mouseExit(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::NormalCursor);
    return true;
  }

  void DraggableComponent::mouseWheelMove(const MouseEvent &e)
  {
    if (!currentlyDraggedComponent_)
    {
      Component::mouseWheelMove(e);
      return;
    }

    initialPosition_ += listener_->mouseWheelWhileDragging(currentlyDraggedComponent_, e);
    currentlyDraggedComponent_->setPosition(initialPosition_ + e.getOffsetFromDragStart());
  }

}
