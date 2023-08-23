/*
  ==============================================================================

    BorderBoundsConstrainer.h
    Created: 9 Mar 2023 11:23:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Framework/load_save.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
  class BorderBoundsConstrainer : public ComponentBoundsConstrainer
  {
  public:
    BorderBoundsConstrainer() = default;

    void checkBounds(Rectangle<int> &bounds, const Rectangle<int> &previous, const Rectangle<int> &limits,
      bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight) override
    {


      ComponentBoundsConstrainer::checkBounds(bounds, previous, limits,
        isStretchingTop, isStretchingLeft, isStretchingBottom, isStretchingRight);

      Rectangle<int> displayArea = Desktop::getInstance().getDisplays().getTotalBounds(true);
      if (gui_)
        if (auto *peer = gui_->getPeer())
          peer->getFrameSize().subtractFrom(displayArea);
      
      // TODO: make resizing snap horizontally to the width of individual lanes 
    }

    void resizeEnd() override
    {
      if (!gui_)
        return;

      auto [unscaledWidth, unscaledHeight] = unscaleDimensions(gui_->getWidth(), gui_->getHeight(), gui_->getScaling());
    	Framework::LoadSave::saveWindowSize(unscaledWidth, unscaledHeight);

      if (resizableBorder_)
        resizableBorder_->setBounds(gui_->getParentComponent()->getLocalBounds());
    }

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
