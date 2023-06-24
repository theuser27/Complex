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
        if (ComponentPeer *peer = gui_->getPeer())
          peer->getFrameSize().subtractFrom(displayArea);
      
      // TODO: make resizing snap horizontally to the width of individual lanes 
    }

    void resizeStart() override
    {
      if (gui_)
        gui_->enableRedoBackground(false);
    }
    void resizeEnd() override
    {
      if (gui_)
      {
        Framework::LoadSave::saveWindowSize(gui_->getWidth(), gui_->getHeight());
        gui_->enableRedoBackground(true);
      }
    }

    void setGui(MainInterface *gui) noexcept { gui_ = gui; }
    void setScaleFactor(double scaleFactor) noexcept { lastScaleFactor_ = scaleFactor; }

  protected:
    static auto unscaleDimensions(const Rectangle<int> &bounds, double currentScaling) noexcept
    {
      std::pair unscaledDimensions = { (double)bounds.getWidth() / currentScaling, (double)bounds.getHeight() / currentScaling };
      return Rectangle{ bounds.getX(), bounds.getY(), roundToInt(unscaledDimensions.first), roundToInt(unscaledDimensions.second) };
    }

    MainInterface *gui_ = nullptr;
    double lastScaleFactor_ = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BorderBoundsConstrainer)
  };
}
