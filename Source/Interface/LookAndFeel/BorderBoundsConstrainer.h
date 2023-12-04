/*
  ==============================================================================

    BorderBoundsConstrainer.h
    Created: 9 Mar 2023 11:23:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Interface
{
  class MainInterface;

  class BorderBoundsConstrainer : public ComponentBoundsConstrainer
  {
  public:
    BorderBoundsConstrainer() = default;

    void checkBounds(Rectangle<int> &bounds, const Rectangle<int> &previous, const Rectangle<int> &limits,
      bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) override;

    void resizeEnd() override;

    void setGui(MainInterface *gui) noexcept { gui_ = gui; }
    void setResizableBorder(ResizableBorderComponent *border) noexcept { resizableBorder_ = border; }
    void setScaleFactor(double scaleFactor) noexcept { lastScaleFactor_ = scaleFactor; }

  protected:
    static std::pair<int, int> unscaleDimensions(int width, int height, double currentScaling) noexcept
    {
      return std::pair{ roundToInt((double)width / currentScaling),
      	roundToInt((double)height / currentScaling) };
    }

    MainInterface *gui_ = nullptr;
    ResizableBorderComponent *resizableBorder_ = nullptr;
    double lastScaleFactor_ = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BorderBoundsConstrainer)
  };
}
