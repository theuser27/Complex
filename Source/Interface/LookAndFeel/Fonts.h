/*
  ==============================================================================

    Fonts.h
    Created: 5 Dec 2022 2:08:35am
    Author:  Lenovo

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Interface
{
  class Fonts
  {
  public:

    Font &getDDinFont() { return DDinFont_; }
    Font &getInterMediumFont() { return InterMediumFont_; }
    Font &getInterBoldFont() { return InterBoldFont_; }

    static Fonts *instance()
    {
      static Fonts instance;
      return &instance;
    }

  private:
    Fonts() :
      DDinFont_(Typeface::createSystemTypefaceFor(
        BinaryData::DDINBold_otf, BinaryData::DDINBold_otfSize)),
      InterMediumFont_(Typeface::createSystemTypefaceFor(
        BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize)),
      InterBoldFont_(Typeface::createSystemTypefaceFor(
        BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize))
    {

      Array<int> glyphs;
      Array<float> x_offsets;
      DDinFont_.getGlyphPositions("test", glyphs, x_offsets);
      InterMediumFont_.getGlyphPositions("test", glyphs, x_offsets);
      InterBoldFont_.getGlyphPositions("test", glyphs, x_offsets);
    }

    Font DDinFont_;
    Font InterMediumFont_;
    Font InterBoldFont_;
  };
}