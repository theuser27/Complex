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
    Font &getInterVFont() { return InterVFont_; }

    static Fonts *instance()
    {
      static Fonts instance;
      return &instance;
    }

  private:
    Fonts();

    Font DDinFont_;
    Font InterVFont_;
  };
}