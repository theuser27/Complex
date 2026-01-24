/*
  ==============================================================================

    LaneSelector.hpp
    Created: 3 May 2023 4:47:08am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
  struct OpenGlQuad;
  class EffectsStateSection;

  class LaneSelector final : public Component
  {
  public:
    LaneSelector();
    ~LaneSelector() noexcept override;

    void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn);

  private:
    class Slider;

    utils::up<Slider> slider_;
    utils::vector<OpenGlQuad> laneBackgrounds_;
    EffectsStateSection *stateSection = nullptr;
    // TODO: custom slider class for the selector + lane boxes
  };
}
