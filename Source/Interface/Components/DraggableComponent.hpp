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

  class DraggableComponent final : public BaseComponent
  {
  public:
    class Listener
    {
    public:
      virtual ~Listener() = default;
      virtual EffectModuleSection *prepareToMove(EffectModuleSection *component, const juce::MouseEvent &e, bool isCopying) = 0;
      virtual void draggingComponent([[maybe_unused]] EffectModuleSection *component, [[maybe_unused]] const juce::MouseEvent &e) { }
      virtual void releaseComponent(EffectModuleSection *component, const juce::MouseEvent &e) = 0;
      virtual juce::Point<int> mouseWheelWhileDragging(EffectModuleSection *component,
        const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) = 0;
    };

    DraggableComponent() noexcept { setInterceptsMouseClicks(true, false); }

    void paint(juce::Graphics &g) override;

    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &e, 
      const juce::MouseWheelDetails &wheel) override;

    auto *getDraggedComponent() const noexcept { return draggedComponent_; }
    void setDraggedComponent(EffectModuleSection *draggedComponent) noexcept { draggedComponent_ = draggedComponent; }
    void setIgnoreClip(BaseComponent *ignoreClipIncluding) noexcept { ignoreClipIncluding_ = ignoreClipIncluding; }
    void setListener(Listener *listener) noexcept { listener_ = listener; }

  private:
    BaseComponent *ignoreClipIncluding_ = nullptr;
    EffectModuleSection *draggedComponent_ = nullptr;
    EffectModuleSection *currentlyDraggedComponent_ = nullptr;
    juce::Point<int> initialPosition_{};
    Listener *listener_ = nullptr;
  };
}
