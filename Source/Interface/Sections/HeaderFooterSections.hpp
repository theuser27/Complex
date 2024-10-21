/*
  ==============================================================================

    HeaderFooterSections.hpp
    Created: 23 May 2023 6:00:44am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "BaseSection.hpp"
#include "../Components/BaseControl.hpp"

namespace Generation
{
  class SoundEngine;
}

namespace Interface
{
  class OpenGlQuad;
  class NumberBox;
  class ActionButton;
  class TextSelector;
  class Spectrogram;

  class HeaderFooterSections final : public BaseSection, public TextSelectorListener
  {
  public:
    static constexpr int kHeaderHorizontalEdgePadding = 10;
    static constexpr int kHeaderNumberBoxMargin = 12;

    static constexpr int kFooterHPadding = 16;

    static constexpr int kLabelToControlMargin = 4;

    HeaderFooterSections(Generation::SoundEngine &soundEngine);
    ~HeaderFooterSections() override;

    void resized() override;
    void resizeForText(TextSelector *textSelector, int widthChange, Placement anchor) override;

  private:
    utils::up<OpenGlQuad> bottomBarColour_;
    utils::up<Spectrogram> spectrogram_;

    utils::up<NumberBox> mixNumberBox_;
    utils::up<NumberBox> gainNumberBox_;
    ControlContainer topContainer_;

    utils::up<NumberBox> blockSizeNumberBox_;
    utils::up<NumberBox> overlapNumberBox_;
    utils::up<TextSelector> windowTypeSelector_;
    utils::up<NumberBox> windowAlphaNumberBox_;
    ControlContainer bottomContainer_;
  };
}
