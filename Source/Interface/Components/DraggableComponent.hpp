/*
  ==============================================================================

    DraggableComponent.hpp
    Created: 10 Feb 2023 5:50:16am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  class EffectModuleSection;

  class DraggableComponent final : public Component
  {
  public:
    class Listener
    {
    public:
      virtual ~Listener() = default;
      virtual EffectModuleSection *prepareToMove(EffectModuleSection *component, const MouseEvent &e, bool isCopying) = 0;
      virtual void draggingComponent([[maybe_unused]] EffectModuleSection *component, [[maybe_unused]] const MouseEvent &e) { }
      virtual void releaseComponent(EffectModuleSection *component, const MouseEvent &e) = 0;
      virtual Point<int> mouseWheelWhileDragging(EffectModuleSection *component, const MouseEvent &e) = 0;
    };

    bool render(OpenGlWrapper &openGl) override;

    bool mouseEnter(const MouseEvent &e) override;
    bool mouseDown(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseUp(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &e) override;

    Component *ignoreClipIncluding_ = nullptr;
    EffectModuleSection *draggedComponent_ = nullptr;
    EffectModuleSection *currentlyDraggedComponent_ = nullptr;
    Point<int> initialPosition_{};
    Listener *listener_ = nullptr;
  };
}
