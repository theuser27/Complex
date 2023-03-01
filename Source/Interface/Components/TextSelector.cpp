/*
  ==============================================================================

    TextSelector.cpp
    Created: 31 Dec 2022 5:34:54pm
    Author:  Lenovo

  ==============================================================================
*/

#include "TextSelector.h"

#include "../LookAndFeel/Skin.h"

namespace Interface
{
  void TextSelector::mouseDown(const juce::MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      ParameterSlider::mouseDown(e);
      return;
    }

    std::span<const std::string_view> lookup = details_.stringLookup;
    if (!long_lookup_.empty())
      lookup = long_lookup_;

    PopupItems options;
    for (int i = 0; i <= getMaximum(); ++i)
      options.addItem(i, lookup[i].data());

    parent_->showPopupSelector(this, Point<int>(0, getHeight()), options, [=](int value) { setValue(value); });
  }

  void TextSelector::mouseUp(const MouseEvent &e)
  {
    if (e.mods.isPopupMenu())
    {
      ParameterSlider::mouseUp(e);
      return;
    }
  }
}
