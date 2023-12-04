/*
  ==============================================================================

    BorderBoundsConstrainer.cpp
    Created: 4 Dec 2023 12:52:32am
    Author:  theuser27

  ==============================================================================
*/

#include "BorderBoundsConstrainer.h"
#include "Framework/load_save.h"
#include "../Sections/MainInterface.h"

namespace Interface
{
  void BorderBoundsConstrainer::checkBounds(Rectangle<int> &bounds, const Rectangle<int> &previous, 
    const Rectangle<int> &limits, bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight)
  {
    ComponentBoundsConstrainer::checkBounds(bounds, previous, limits,
      isStretchingTop, isStretchingLeft, isStretchingBottom, isStretchingRight);

    Rectangle<int> displayArea = Desktop::getInstance().getDisplays().getTotalBounds(true);
    if (gui_)
      if (auto *peer = gui_->getPeer())
        peer->getFrameSize().subtractFrom(displayArea);

    // TODO: make resizing snap horizontally to the width of individual lanes 
  }

  void BorderBoundsConstrainer::resizeEnd()
  {
    if (!gui_)
      return;

    auto [unscaledWidth, unscaledHeight] = unscaleDimensions(gui_->getWidth(), gui_->getHeight(), gui_->getScaling());
    Framework::LoadSave::saveWindowSize(unscaledWidth, unscaledHeight);

    if (resizableBorder_)
      resizableBorder_->setBounds(gui_->getParentComponent()->getLocalBounds());
  }
}