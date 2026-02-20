/*
  ==============================================================================

    LaneSelector.cpp
    Created: 3 May 2023 4:47:08am
    Author:  theuser27

  ==============================================================================
*/

#include "LaneSelector.hpp"

#include "BaseSlider.hpp"

namespace Interface
{
  class LaneSelector::Slider final : public Slider
  {
  public:
    Slider() : Slider{ nullptr }
    {

    }
    ~Slider() noexcept override = default;

    bool hitTest([[maybe_unused]] int x, [[maybe_unused]] int y) override
    {
      return false;
    }

    Rectangle<int> setSizes([[maybe_unused]] int height, [[maybe_unused]] int width) override
    {
      return {};
    }
    void setExtraElementsPositions([[maybe_unused]] Rectangle<int> anchorBounds) override
    {

    }
    void redoImage() override
    {

    }
    void setComponentsBounds([[maybe_unused]] bool redoImage) override
    {

    }
  };

  LaneSelector::LaneSelector() : OpenGlContainer{ "LaneSelector" },
    slider_{ utils::up<Slider>::create() } { }
  LaneSelector::~LaneSelector() = default;

  void LaneSelector::laneTurnedOnOff([[maybe_unused]] EffectsLaneSection *lane, [[maybe_unused]] bool isOn)
  {

  }


}
