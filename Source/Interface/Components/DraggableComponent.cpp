
// Created: 2023-02-10 05:50:16

#include "DraggableComponent.hpp"

#include "Generation/Processor.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"

namespace Interface
{
  bool
  DraggableComponent::render(OpenGlWrapper &openGl)
  {
    static constexpr float kDotDiameter = 2.0f;
    static constexpr float kDotsOffset = 6.0f;

    int dotsDiameter = scaleValueRoundInt(kDotDiameter);
    int dotsOffset = scaleValueRoundInt(kDotsOffset);

    int centeredX = utils::centerAxis(dotsDiameter + dotsOffset, bounds.w);
    int centeredY = utils::centerAxis(dotsDiameter + dotsOffset, bounds.h);

    auto centreRectangle = Rectangle{ (float)centeredX, (float)centeredY,
      (float)dotsOffset, (float)dotsOffset };

    nvgFillColor(openGl, getColour(Skin::kWidgetSecondary1, this));
    nvgBeginPath(openGl);
    nvgEllipse(openGl, centreRectangle.x, centreRectangle.y, kDotDiameter, kDotDiameter);
    nvgEllipse(openGl, centreRectangle.x, centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
    nvgEllipse(openGl, centreRectangle.getRight(), centreRectangle.y, kDotDiameter, kDotDiameter);
    nvgEllipse(openGl, centreRectangle.getRight(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
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

    // moving case
    auto event = e.getEventRelativeTo(draggedComponent);
    initialClickPosition = { event.x, event.y };
    previousOverridePosition = draggedComponent->overridePosition;
    previousPlacement = draggedComponent->placement;
    previousParentProcessorStateId = processor->parent->stateId;
    previousIndex = processor->getIndex();

    draggedComponent->placement = Placement::custom;
    draggedComponent->overridePosition = [](Component *c)
    {
      c->bounds.setPosition(c->nextPosition.x, c->nextPosition.y);
    };

    draggedComponent->parent->removeChildComponent(draggedComponent);
    surfaceToLiftTo->addChildComponent(draggedComponent);

    return true;
  }

  bool
  DraggableComponent::mouseDrag(const MouseEvent &e)
  {
    insertDraggedComponent(e, true);
    return true;
  }

  bool
  DraggableComponent::mouseUp(const MouseEvent &e)
  {
    draggedComponent->placement = previousPlacement;
    draggedComponent->overridePosition = previousOverridePosition;

    insertDraggedComponent(e, false);

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
      insertInfo.insertPlaceholder = false;
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

  void DraggableComponent::insertDraggedComponent(const MouseEvent &e, bool usePlaceholder)
  {
    COMPLEX_ASSERT(processor);

    auto event = e.getEventRelativeTo(draggedComponent);

    draggedComponent->nextPosition.x = event.x - initialClickPosition.x;
    draggedComponent->nextPosition.y = event.y - initialClickPosition.y;

    auto position = draggedComponent->getLocalBounds().getCentre();

    CommandMessages::ProcessorInsertion insertInfo{};
    insertInfo.insertPlaceholder = usePlaceholder;
    insertInfo.position = position;
    insertInfo.processor = processor;

    preOrderTreeTraversal(draggedComponent->parent, [&insertInfo](Component *c)
      {
        return c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo);
      }, position, false, draggedComponent);
    
    if (!usePlaceholder)
    {
      // finalising placement
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
    }
    else
    {
      // handle autoscroll
      CommandMessages::Autoscroll scrollInfo{ position, true, true };

      preOrderTreeTraversal(draggedComponent->parent, [&scrollInfo](Component *c)
        {
          c->handleCommandMessage(CommandMessages::HandleAutoscroll, &scrollInfo);
          return !scrollInfo.handleX && !scrollInfo.handleY;
        }, position, false, draggedComponent);
    }
  }

}
