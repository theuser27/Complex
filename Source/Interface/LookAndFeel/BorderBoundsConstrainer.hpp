/*
  ==============================================================================

    BorderBoundsConstrainer.hpp
    Created: 9 Mar 2023 11:23:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Interface
{
  class MainInterface;
  class Renderer;

  class BorderBoundsConstrainer final : public juce::ComponentBoundsConstrainer
  {
  public:
    void checkBounds(juce::Rectangle<int> &bounds, const juce::Rectangle<int> &previous, const juce::Rectangle<int> &limits,
      bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) override;

    void resizeStart() override;
    void resizeEnd() override;

    void setGui(MainInterface *gui) noexcept { gui_ = gui; }
    void setRenderer(Renderer *renderer) noexcept { renderer_ = renderer; }

  protected:
    static std::pair<int, int> unscaleDimensions(int width, int height, double currentScaling) noexcept
    {
      return std::pair{ (int)std::round((double)width / currentScaling),
        (int)std::round((double)height / currentScaling) };
    }

    MainInterface *gui_ = nullptr;
    Renderer *renderer_ = nullptr;
  };
}
