/*
  ==============================================================================

    EffectModuleSection.h
    Created: 14 Feb 2023 2:29:16am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Generation/EffectModules.h"
#include "../Components/DraggableEffect.h"
#include "../Components/Spectrogram.h"

namespace Interface
{
  class SpectralMaskComponent
  {
  public:


  private:
    Spectrogram spectrogram{};
    PinSlider lowBound{ Framework::baseEffectParameterList[(u32)Framework::BaseEffectParameters::LowBound].name.data() };
    PinSlider highBound{ Framework::baseEffectParameterList[(u32)Framework::BaseEffectParameters::HighBound].name.data() };
  };

	class EffectModuleSection : DraggableComponent
	{
  public:
    EffectModuleSection(Generation::EffectModule *effectModule, std::array<u32, 2> order);

    void drawGrabber(Graphics &g) override;

  private:

    Generation::EffectModule *effectModule_ = nullptr;
	};

  
}

REFL_AUTO(type(Interface::EffectModuleSection))
