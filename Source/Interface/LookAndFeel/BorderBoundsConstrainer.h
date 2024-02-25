/*
  ==============================================================================

    BorderBoundsConstrainer.h
    Created: 9 Mar 2023 11:23:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "AppConfig.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace Interface
{
  class MainInterface;

  class BorderBoundsConstrainer final : public juce::ComponentBoundsConstrainer
  {
  public:
    BorderBoundsConstrainer() = default;

    void checkBounds(juce::Rectangle<int> &bounds, const juce::Rectangle<int> &previous, const juce::Rectangle<int> &limits,
      bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) override;

    void resizeEnd() override;

    void setGui(MainInterface *gui) noexcept { gui_ = gui; }
    void setResizableBorder(juce::ResizableBorderComponent *border) noexcept { resizableBorder_ = border; }
    void setScaleFactor(double scaleFactor) noexcept { lastScaleFactor_ = scaleFactor; }

  protected:
    static std::pair<int, int> unscaleDimensions(int width, int height, double currentScaling) noexcept
    {
      return std::pair{ juce::roundToInt((double)width / currentScaling),
        juce::roundToInt((double)height / currentScaling) };
    }

    MainInterface *gui_ = nullptr;
    juce::ResizableBorderComponent *resizableBorder_ = nullptr;
    double lastScaleFactor_ = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BorderBoundsConstrainer)
  };
}
