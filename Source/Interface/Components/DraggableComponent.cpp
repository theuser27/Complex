
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

    //if (isDragging)
    //{
    //  COMPLEX_ASSERT(processor);

    //  //draggedComponent->nextPosition = initialClickPosition + lastDragEvent.getOffsetFromDragStart();
    //  auto position = draggedComponent->getLocalBounds().getCentre();
    //  auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;

    //  CommandMessages::ProcessorInsertion insertInfo{
    //    .position = position, .processor = processor, .placeholder = placeholder,
    //    .isMovingUpX = lastDragEvent.directionX < 0, .isMovingUpY = lastDragEvent.directionY < 0 };

    //  preOrderTreeTraversal(draggedComponent->parent, [&insertInfo](Component *c)
    //  {
    //    return c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo);
    //  }, position, false, draggedComponent);
    //}

    return true;
  }

  bool
  DraggableComponent::mouseEnter(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::AllScroll);
    return true;
  }

  bool
  DraggableComponent::mouseExit(const MouseEvent &e)
  {
    if (!componentFlags.isClicked || !e.mods.test(ModifierKeys::leftButtonModifier))
      setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
    return true;
  }

  static Component *
  tryToInsertDraggableComponent(Component *componentToInsert, Component *lastInsertedInto,
    Generation::Processor *processor, Component *placeholder, bool isMovingUpX, bool isMovingUpY)
  {
    auto position = componentToInsert->getLocalBounds().getCentre();
    CommandMessages::ProcessorInsertion insertInfo{ .position = position,
      .processor = processor, .placeholder = placeholder,
      .isMovingUpX = isMovingUpX,
      .isMovingUpY = isMovingUpY };

    bool success = preOrderTreeTraversal(componentToInsert->parent, [&](Component *c)
      {
        //insertInfo.position = position - c->getRelativePoint(componentToInsert);
        bool s = c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo);
        if (s)
          lastInsertedInto = c;
        return s;
      }, position + componentToInsert->bounds.getPosition(), false, componentToInsert, true);

    // if for some reason the dragged component is outside of bounds 
    // we can fallback on wherever it was already inserted and move up
    if (!success && lastInsertedInto)
    {
      while (lastInsertedInto != componentToInsert->parent)
      {
        if (lastInsertedInto->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &insertInfo))
          break;
        lastInsertedInto = lastInsertedInto->parent;
      }      
    }

    return lastInsertedInto;
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
      newDraggableComponent->draggedComponent->bounds = draggedComponent->bounds;
      draggedComponent->parent->addChildComponent(newDraggableComponent->draggedComponent, draggedComponent);
      newDraggableComponent->surfaceToLiftTo = surfaceToLiftTo;
      newDraggableComponent->draggedComponent->placement = draggedComponent->placement;
      newDraggableComponent->isCopying = true;

      setClickedComponent(uiRelated.renderer, newDraggableComponent);

      auto event = e;
      event.mods = event.mods.withoutFlags(ModifierKeys::ctrlModifier);
      return newDraggableComponent->mouseDown(event.getEventRelativeTo(newDraggableComponent));
    }

    isDragging = true;

    auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;
    placeholder->reference = this;
    placeholder->placement = draggedComponent->placement;
    placeholder->sizingFlags = draggedComponent->sizingFlags;
    placeholder->desiredSize = draggedComponent->desiredSize;
    placeholder->draw = [](OpenGlWrapper &openGl, Component *, Component *self, Point<i32>)
    {
      auto colour = getColour(Skin::kLightenScreen, self);

      fillRect(openGl, self->getLocalBounds().toFloat(), colour.dimmer(0.3f), scaleValue(4.0f));
      strokeRect(openGl, self->getLocalBounds().toFloat(), scaleValue(1.0f), colour, scaleValue(4.0f));

      return true;
    };
    placeholder->overrideSize = [](Component *c, bool isCalculatingVertical)
    {
      auto *draggableComponent = (DraggableComponent *)((DrawComponent *)c)->reference;
      auto *draggedComponent = draggableComponent->draggedComponent;

      c->padding = draggedComponent->padding;
      c->margin = draggedComponent->margin;

      if (!isCalculatingVertical)
        return Range<i32>{ -1, (draggedComponent->sizingFlags & Component::GrowableX) ? 
          -1 : draggedComponent->lastBounds.w };
      else
        return Range<i32>{ -1, (draggedComponent->sizingFlags & Component::GrowableY) ?
          -1 : draggedComponent->lastBounds.h };
    };
    draggedComponent->parent->addChildComponent(placeholder, draggedComponent);

    // moving case
    auto event = e.getEventRelativeTo(surfaceToLiftTo);
    initialClickPosition = Point{ event.x, event.y } - draggedComponent->getRelativePoint(this, { e.x, e.y });
    draggedComponent->nextPosition = initialClickPosition;
    directionChangePoint = initialClickPosition;

    previousOverridePosition = draggedComponent->overridePosition;
    previousOverrideSize = draggedComponent->overrideSize;
    previousPlacement = draggedComponent->placement;
    if (!isCopying)
    {
      previousParentProcessorStateId = processor->parent->stateId;
      previousIndex = processor->getIndex();
    }

    draggedComponent->placement = Placement::custom;
    draggedComponent->overridePosition = [](Component *c)
    {
      c->bounds = c->bounds.withPosition(c->nextPosition.x, c->nextPosition.y);
      return true;
    };
    draggedComponent->componentFlags.keepSize = true;
    //draggedComponent->overrideSize = [](Component *c, bool isCalculatingVertical)
    //{
    //  if (!isCalculatingVertical)
    //    return Range<i32>{ c->lastBounds.w, c->lastBounds.w };
    //  else
    //    return Range<i32>{ c->lastBounds.h, c->lastBounds.h };
    //};

    // if this is the draggable component we must keep focus otherwise our click will be discarded
    draggedComponent->parent->removeChildComponent(draggedComponent, draggedComponent == this);
    surfaceToLiftTo->addChildComponent(draggedComponent);
    
    // set the dragged component to its correct position
    draggedComponent->bounds = draggedComponent->bounds.withPosition(initialClickPosition);

    registerCallback(uiRelated.renderer, this, [](Component *c)
      {
        auto *self = (DraggableComponent *)c;
        if (!self->isDragging)
          return;

        COMPLEX_ASSERT(self->processor);

        //draggedComponent->nextPosition = initialClickPosition + lastDragEvent.getOffsetFromDragStart();
        auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;
        auto *insertedIntoComponent = tryToInsertDraggableComponent(self->draggedComponent, 
          placeholder->parent, self->processor, placeholder, self->directionX, self->directionY);

        utils::vector<Component *> parentComponentPath{ localScratch, 8 };
        while (insertedIntoComponent != self->surfaceToLiftTo->parent)
        {
          parentComponentPath.emplaceBack(insertedIntoComponent);
          insertedIntoComponent = insertedIntoComponent->parent;
        }

        // handle autoscroll
        auto position = self->draggedComponent->bounds.getCentre();
        CommandMessages::Autoscroll scrollInfo{ {}, true, true };
        for (usize i = parentComponentPath.size(); i; --i)
        {
          auto *parentComponent = parentComponentPath[i - 1];
          scrollInfo.position = parentComponent->getRelativePoint(self->draggedComponent->parent, position);
          parentComponent->handleCommandMessage(CommandMessages::HandleAutoscroll, &scrollInfo);
          if (!scrollInfo.handleX && !scrollInfo.handleY)
            break;
        }
      });

    return true;
  }

  bool
  DraggableComponent::mouseDrag(const MouseEvent &e)
  {
    auto newPosition = initialClickPosition + e.getOffsetFromDragStart();
    draggedComponent->nextPosition = newPosition;

    if ((wasMovingUpX && e.directionX > 0) || (!wasMovingUpX && e.directionX < 0))
    {
      wasMovingUpX = !wasMovingUpX;
      directionChangePoint.x = newPosition.x;
    }
    if ((wasMovingUpY && e.directionY > 0) || (!wasMovingUpY && e.directionY < 0))
    {
      wasMovingUpY = !wasMovingUpY;
      directionChangePoint.y = newPosition.y;
    }

    if (directionX && newPosition.x - directionChangePoint.x > draggedComponent->bounds.w / 4)
      directionX = false;
    else if (!directionX && newPosition.x - directionChangePoint.x < -draggedComponent->bounds.w / 4)
      directionX = true;

    if (directionY && newPosition.y - directionChangePoint.y > draggedComponent->bounds.h / 4)
      directionY = false;
    else if (!directionY && newPosition.y - directionChangePoint.y < -draggedComponent->bounds.h / 4)
      directionY = true;

    //COMPLEX_DEBUG_LOG("wasMovingUpX: %d, wasMovingUpY: %d, new - directionChange: { %d, %d }\n", 
    //  wasMovingUpX, wasMovingUpY, newPosition.x - directionChangePoint.x, newPosition.y - directionChangePoint.y);

    return true;
  }

  bool
  DraggableComponent::mouseUp(const MouseEvent &e)
  {
    draggedComponent->placement = previousPlacement;
    draggedComponent->overridePosition = previousOverridePosition;
    draggedComponent->componentFlags.keepSize = false;

    COMPLEX_ASSERT(processor);

    deregisterCallback(uiRelated.renderer, this);

    // finalising placement
    draggedComponent->nextPosition = initialClickPosition + e.getOffsetFromDragStart();
    auto *placeholder = &getGui(uiRelated.renderer)->placeholderInsert;
    auto finalParentComponent = placeholder->parent;
    placeholder->parent->removeChildComponent(placeholder);

    tryToInsertDraggableComponent(draggedComponent, finalParentComponent, 
      processor, nullptr, directionX, directionY);

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
      draggedComponent->bounds.getCentre(), true, draggedComponent, true);

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
    draggedComponent->componentFlags.keepSize = false;

    deregisterCallback(uiRelated.renderer, this);

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
        }, false, true);
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
