/*
  ==============================================================================

    LaneSelector.hpp
    Created: 3 May 2023 4:47:08am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.hpp"

namespace Interface
{
  class LaneSelector final : public OpenGlContainer, public EffectsLaneListener
  {
  public:
    LaneSelector() : OpenGlContainer{ "LaneSelector" } { }

    void resized() override;

    void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) override;

  private:
    // TODO: custom slider class for the selector + lane boxes
  };
}
