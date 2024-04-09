/*
  ==============================================================================

    Overlay.h
    Created: 14 Dec 2022 5:11:59am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"

namespace Interface
{
  class OverlayBackgroundRenderer;

  class Overlay final : public BaseSection
  {
  public:

    Overlay(std::string_view name);

    void resized() override;

  protected:
    gl_ptr<OverlayBackgroundRenderer> background_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Overlay)
  };
}
