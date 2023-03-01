/*
  ==============================================================================

    DraggableEffect.h
    Created: 10 Feb 2023 5:50:16am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"
#include "OpenGlImageComponent.h"
#include "BaseButton.h"
#include "BaseSlider.h"

namespace Interface
{
  class DraggableComponent : public BaseSection
  {
  public:
    class Listener
    {
    public:
      virtual ~Listener() = default;
      virtual void componentDragged(DraggableComponent *component, bool enabled) = 0;
    };

    DraggableComponent(std::string_view name, std::array<u32, 2> order) : BaseSection(name), order_(order)
    {
      setInterceptsMouseClicks(true, true);
    }

  	void paint(Graphics &g) override { drawGrabber(g); }
    void paintBackground(Graphics &g) override { }

    void mouseMove(const MouseEvent &e) override;
  	void mouseDown(const MouseEvent &e) override;
    void mouseDrag(const MouseEvent &e) override;
    void mouseUp(const MouseEvent &e) override;

    virtual void drawGrabber(Graphics &g) { }

    auto getOrder() const noexcept { return order_; }
    auto getDragHitbox() const noexcept { return dragHitbox_; }

    void setDragHitBox(Rectangle<int> dragHitbox) noexcept { dragHitbox_ = dragHitbox; }
    void setOrder(std::array<u32, 2> order) noexcept { order_ = order; }
    void setIsHovering(bool isHovering) noexcept { isHovering_ = isHovering; }


  protected:
    Rectangle<int> dragHitbox_{};
    std::array<u32, 2> order_{};
    bool isHovering_ = false;
  };

  class DragDropEffectOrder : public BaseSection
  {
  public:
    static constexpr int kEffectPadding = 6;

    class Listener
    {
    public:
      virtual ~Listener() { }
      virtual void orderChanged(DragDropEffectOrder *order) = 0;
      virtual void effectEnabledChanged(int order_index, bool enabled) = 0;
    };

    DragDropEffectOrder(String name);
    virtual ~DragDropEffectOrder();

    void resized() override;
    void paintBackground(Graphics &g) override;

    void renderOpenGlComponents(OpenGlWrapper &open_gl, bool animate) override;

    void mouseMove(const MouseEvent &e) override;
    void mouseDown(const MouseEvent &e) override;
    void mouseDrag(const MouseEvent &e) override;
    void mouseUp(const MouseEvent &e) override;
    void mouseExit(const MouseEvent &e) override;

    void moveEffect(int start_index, int end_index);
    void setStationaryEffectPosition(int index);
    void addListener(Listener *listener) { listeners_.push_back(listener); }

    int getEffectIndex(int index) const;
    Component *getEffect(int index) const;
    bool effectEnabled(int index) const;
    int getEffectIndexFromY(float y) const;

  private:
    int getEffectY(int index) const;

    std::vector<Listener *> listeners_;
    DraggableComponent *currently_dragged_;
    DraggableComponent *currently_hovered_;

    std::array<u32, 2> last_dragged_index_;
    std::pair<int, int> mouse_down_xy_;
    std::pair<int, int> dragged_starting_xy_;
    std::vector<std::unique_ptr<DraggableComponent>> effect_list_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DragDropEffectOrder)
  };
}
