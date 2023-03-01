/*
  ==============================================================================

    TextSelector.h
    Created: 31 Dec 2022 5:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "BaseSlider.h"

namespace Interface
{
  class TextSelector : public BaseSlider
  {
  public:
    TextSelector(String name) : BaseSlider(std::move(name)) { setShouldShowPopup(false); }

    void mouseDown(const MouseEvent &e) override;
    void mouseUp(const MouseEvent &e) override;

    void setLongStringLookup(std::span<std::string_view> lookup) { long_lookup_ = lookup; }

  protected:
    std::span<std::string_view> long_lookup_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextSelector)
  };

  //class PaintPatternSelector : public TextSelector
  //{
  //public:
  //  PaintPatternSelector(String name) : TextSelector(name) { }

  //  void paint(Graphics &g) override;

  //  void setPadding(int padding) { padding_ = padding; }

  //private:
  //  int padding_ = 0;
  //};
}
