/*
  ==============================================================================

    Fonts.cpp
    Created: 23 Jun 2023 7:07:51pm
    Author:  theuser27

  ==============================================================================
*/

#include "Fonts.h"
#include "Framework/common.h"

namespace Interface
{
  Fonts::Fonts() :
    DDinFont_(Typeface::createSystemTypefaceFor(
      BinaryData::DDIN_otf, BinaryData::DDIN_otfSize)),
    InterVFont_(Typeface::createSystemTypefaceFor(
      BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize))
  {
    static constexpr float kDDinDefaultHeight = 11.5f;
    // windows font rendering is poop
	#if COMPLEX_MSVC
  	static constexpr float kInterVDefaultHeight = 12.0f;
	#elif
    static constexpr float kInterVDefaultHeight = 11.0f;
	#endif
  	static constexpr float kDDinDefaultKerning = 0.5f;
    static constexpr float kInterVDefaultKerning = 0.5f;

    DDinFont_.setBold(true);
    DDinFont_.setHeight(kDDinDefaultHeight);
    DDinFont_.setExtraKerningFactor((kDDinDefaultHeight + kDDinDefaultKerning) / kDDinDefaultHeight - 1.0f);

    InterVFont_.setHeight(kInterVDefaultHeight);
    InterVFont_.setExtraKerningFactor((kInterVDefaultHeight + kInterVDefaultKerning) / kInterVDefaultHeight - 1.0f);
  }
}
