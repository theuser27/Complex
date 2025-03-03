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
  class OpenGlQuad;
  class EffectsStateSection;

  class LaneSelector final : public OpenGlContainer, public EffectsLaneListener
  {
  public:
    LaneSelector();
    ~LaneSelector() override;

    void resized() override;

    void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) override;

  private:
    class Slider;

    utils::up<Slider> slider_;
    std::vector<OpenGlQuad> laneBackgrounds_;
    EffectsStateSection *stateSection = nullptr;
    // TODO: custom slider class for the selector + lane boxes
  };
}
