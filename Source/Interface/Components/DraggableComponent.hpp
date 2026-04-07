
// Created: 2023-02-10 05:50:16

#pragma once

#include "../LookAndFeel/Component.hpp"

namespace Interface
{
  class DraggableComponent final : public Component
  {
  public:
    bool render(OpenGlWrapper &openGl) override;

    bool mouseEnter(const MouseEvent &e) override;
    bool mouseDown(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &e) override;

    bool keyPressed(const KeyPress &keyPress) override;

    void insertDraggedComponent(const MouseEvent &e, bool usePlaceholder);

    Component *draggedComponent{};
    Generation::Processor *processor{};
    Component *surfaceToLiftTo{};
    // copy draggedComponent and return the new DraggableComponent inside
    DraggableComponent *(*copyingDraggedComponent)(Component *c){};
    
    Point<i32> initialClickPosition{};
    void (*previousOverridePosition)(Component *c){};
    u64 previousParentProcessorStateId{};
    usize previousIndex{};
    Placement previousPlacement{};
    bool isCopying{};
  };
}
