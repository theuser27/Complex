/*
  ==============================================================================

    EffectModuleSection.cpp
    Created: 14 Feb 2023 2:29:16am
    Author:  theuser27

  ==============================================================================
*/

#include "EffectModuleSection.h"

namespace Interface
{
  EffectModuleSection::EffectModuleSection(Generation::EffectModule *effectModule, std::array<u32, 2> order) :
		DraggableComponent(utils::getClassName<EffectModuleSection>(), order), effectModule_(effectModule)
  {
	  
  }
  void EffectModuleSection::drawGrabber(Graphics &g)
  {
  }
}
