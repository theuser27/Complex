
// Created: 2023-02-10 05:50:16

#include "DraggableComponent.hpp"

#include "Generation/Processor.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../Sections/MainInterface.hpp"

namespace Interface
{
  bool
  DraggableComponent::render(OpenGlWrapper &openGl)
  {
    static constexpr float kDotDiameter = 1.0f;
    static constexpr float kDotsOffset = 6.0f;

    float dotsDiameter = scaleValueRound(kDotDiameter);
    float dotsOffset = scaleValueRound(kDotsOffset);

    float centeredX = utils::centerAxis(2 * dotsDiameter + dotsOffset, (float)bounds.w);
    float centeredY = utils::centerAxis(2 * dotsDiameter + dotsOffset, (float)bounds.h);

    auto centreRectangle = Rectangle{ (float)centeredX, (float)centeredY,
      (float)dotsOffset, (float)dotsOffset };

    nvgFillColor(openGl, getColour(Skin::kWidgetSecondary1, this));
    nvgBeginPath(openGl);
    nvgEllipse(openGl, centreRectangle.x, centreRectangle.y, dotsDiameter, dotsDiameter);
    nvgFill(openGl);
    nvgBeginPath(openGl);
    nvgEllipse(openGl, centreRectangle.x, centreRectangle.getBottom(), dotsDiameter, dotsDiameter);
    nvgFill(openGl);
    nvgBeginPath(openGl);
    nvgEllipse(openGl, centreRectangle.getRight(), centreRectangle.y, dotsDiameter, dotsDiameter);
    nvgFill(openGl);
    nvgBeginPath(openGl);
    nvgEllipse(openGl, centreRectangle.getRight(), centreRectangle.getBottom(), dotsDiameter, dotsDiameter);
    nvgFill(openGl);

    return true;
  }

  bool
  DraggableComponent::mouseEnter(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::AllScroll);
    return true;
  }

  bool
  DraggableComponent::mouseExit(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
    return true;
  }

  bool
  DraggableComponent::mouseDown(const MouseEvent &e)
  {
    COMPLEX_ASSERT(draggedComponent);
    COMPLEX_ASSERT(processor);
    COMPLEX_ASSERT(surfaceToLiftTo);

    if (isDragging)
      return true;

    // copying case
    if (e.mods.test(ModifierKeys::ctrlModifier) && copyingDraggedComponent)
    {
      DraggableComponent *newDraggableComponent = copyingDraggedComponent(draggedComponent);
      newDraggableComponent->draggedComponent->bounds = newDraggableComponent->draggedComponent
        ->getRelativeArea(draggedComponent, draggedComponent->bounds);
      newDraggableComponent->isCopying = true;

      setClickedComponent(uiRelated.renderer, newDraggableComponent);

      auto event = e;
      event.mods = event.mods.withoutFlags(ModifierKeys::ctrlModifier);
      return newDraggableComponent->mouseDown(event.getEventRelativeTo(newDraggableComponent));
    }

    isDragging = true;

    auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;
    placeholder->source = draggedComponent;
    placeholder->placement = draggedComponent->placement;
    draggedComponent->parent->addChildComponent(placeholder, draggedComponent);

    // moving case
    auto event = e.getEventRelativeTo(surfaceToLiftTo);
    initialClickPosition = Point{ event.x, event.y } - draggedComponent->getRelativePoint(this, { e.x, e.y });
    draggedComponent->nextPosition = initialClickPosition;

    previousOverridePosition = draggedComponent->overridePosition;
    previousPlacement = draggedComponent->placement;
    previousParentProcessorStateId = processor->parent->stateId;
    previousIndex = processor->getIndex();

    draggedComponent->placement = Placement::custom;
    draggedComponent->overridePosition = [](Component *c)
    {
      c->bounds.setPosition(c->nextPosition.x, c->nextPosition.y);
      return true;
    };

    draggedComponent->parent->removeChildComponent(draggedComponent);
    surfaceToLiftTo->addChildComponent(draggedComponent);

    return true;
  }

  bool
  DraggableComponent::mouseDrag(const MouseEvent &e)
  {
    COMPLEX_ASSERT(processor);

    draggedComponent->nextPosition = initialClickPosition + e.getOffsetFromDragStart();
    auto position = draggedComponent->getLocalBounds().getCentre();
    auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;

    CommandMessages::ProcessorInsertion insertInfo{ .position = position, .processor = processor,
      .placeholder = placeholder, .isMovingUpX = e.directionX < 0, .isMovingUpY = e.directionY < 0 };

    preOrderTreeTraversal(draggedComponent->parent, [&insertInfo](Component *c)
    {
      return c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo);
    }, position, false, draggedComponent);

    // handle autoscroll
    CommandMessages::Autoscroll scrollInfo{ position, true, true };

    preOrderTreeTraversal(draggedComponent->parent, [&scrollInfo](Component *c)
    {
      c->handleCommandMessage(CommandMessages::HandleAutoscroll, &scrollInfo);
      return !scrollInfo.handleX && !scrollInfo.handleY;
    }, position, false, draggedComponent);

    return true;
  }

  bool
  DraggableComponent::mouseUp(const MouseEvent &e)
  {
    draggedComponent->placement = previousPlacement;
    draggedComponent->overridePosition = previousOverridePosition;

    COMPLEX_ASSERT(processor);

    // finalising placement
    draggedComponent->nextPosition = initialClickPosition + e.getOffsetFromDragStart();
    auto position = draggedComponent->getLocalBounds().getCentre();
    auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;
    placeholder->parent->removeChildComponent(placeholder);

    CommandMessages::ProcessorInsertion insertInfo{ .position = position, .processor = processor,
      .placeholder = nullptr, .isMovingUpX = e.directionX < 0, .isMovingUpY = e.directionY < 0 };

    preOrderTreeTraversal(draggedComponent->parent, [&insertInfo](Component *c)
    {
      return c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo);
    }, position, false, draggedComponent);

    auto &plugin = *processor->state->plugin;
    auto *transactionArena = plugin.undoManager.beginNewTransaction();
    if (!isCopying)
    {
      plugin.undoManager.perform(anew(transactionArena, Framework::MoveProcessorUpdate,
        { processor->state, previousParentProcessorStateId, previousIndex,
        processor->parent->stateId, processor->getIndex(), true }));
    }
    else
    {
      plugin.undoManager.perform(anew(transactionArena, Framework::AddProcessorUpdate,
        { processor, processor->parent->stateId, processor->getIndex() }));
      isCopying = false;
    }

    isDragging = false;

    return true;
  }

  bool
  DraggableComponent::mouseWheelMove(const MouseEvent &e)
  {
    if (!componentFlags.isClicked)
      return Component::mouseWheelMove(e);

    auto success = preOrderTreeTraversal(draggedComponent->parent,
      [&e](Component *c) { return c->mouseWheelMove(e.getEventRelativeTo(c)); },
      draggedComponent->bounds.getCentre(), true, draggedComponent);

    if (isDragging)
    {
      auto event = e;
      event.directionX = (i8)utils::clamp((i32)::round(-e.wheelDeltaX), -1, 1);
      event.directionY = (i8)utils::clamp((i32)::round(-e.wheelDeltaY), -1, 1);
      mouseDrag(event);
    }

    return success;
  }

  bool
  DraggableComponent::keyPressed(const KeyPress &keyPress)
  {
    if (keyPress.keyCode != PUGL_KEY_ESCAPE || !componentFlags.isClicked)
      return false;

    draggedComponent->placement = previousPlacement;
    draggedComponent->overridePosition = previousOverridePosition;

    if (!isCopying)
    {
      auto *previousParent = processor->state->getProcessor(previousParentProcessorStateId);

      CommandMessages::ProcessorInsertion insertInfo{};
      insertInfo.useIndex = true;
      insertInfo.index = (u32)previousIndex;
      insertInfo.processor = processor;

      (void)preOrderTreeTraversal(previousParent->component, [&insertInfo](Component *c)
        {
          auto copy = insertInfo;
          if (c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &copy))
          {
            insertInfo = copy;
            return true;
          }
          return false;
        }, false);
    }
    else
    {
      // warning when this finishes the current object will not exist anymore
      // DO NOT TOUCH ANYTHING AFTER THIS!
      processor->state->deleteProcessor(processor);
    }

    return true;
  }
}
