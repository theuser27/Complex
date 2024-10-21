/*
  ==============================================================================

    BorderBoundsConstrainer.cpp
    Created: 4 Dec 2023 12:52:32am
    Author:  theuser27

  ==============================================================================
*/

#include "BorderBoundsConstrainer.hpp"

#include "Framework/load_save.hpp"
#include "Plugin/Renderer.hpp"
#include "../Sections/MainInterface.hpp"

namespace Interface
{
  void BorderBoundsConstrainer::checkBounds(juce::Rectangle<int> &bounds, const juce::Rectangle<int> &previous,
    const juce::Rectangle<int> &limits, bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight)
  {
    ComponentBoundsConstrainer::checkBounds(bounds, previous, limits,
      isStretchingTop, isStretchingLeft, isStretchingBottom, isStretchingRight);

    juce::Rectangle<int> displayArea = juce::Desktop::getInstance().getDisplays().getTotalBounds(true);
    if (gui_)
      if (auto *peer = gui_->getPeer())
        peer->getFrameSize().subtractFrom(displayArea);

    // TODO: make resizing snap horizontally to the width of individual lanes 
  }

  void BorderBoundsConstrainer::resizeStart()
  {
    if (!renderer_)
      return;

    renderer_->setIsResizing(true);
  }

  void BorderBoundsConstrainer::resizeEnd()
  {
    if (!gui_ || !renderer_)
      return;

    auto [unscaledWidth, unscaledHeight] = unscaleDimensions(gui_->getWidth(), gui_->getHeight(), uiRelated.scale);
    Framework::LoadSave::saveWindowSize(unscaledWidth, unscaledHeight);

    renderer_->setIsResizing(false);
  }
}