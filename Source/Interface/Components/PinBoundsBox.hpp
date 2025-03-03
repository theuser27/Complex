/*
  ==============================================================================

    PinBoundsBox.hpp
    Created: 27 Feb 2023 8:19:37pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.hpp"
#include "OpenGlQuad.hpp"
#include "OpenGlImage.hpp"

namespace Framework
{
  class ParameterValue;
}

namespace Interface
{
  class OpenGlQuad;
  class OpenGlCorners;
  class PinSlider;

  class PinBoundsBox : public BaseSection
  {
  public:
    static constexpr int kAdditionalPinWidth = 20;

    PinBoundsBox(utils::string_view name, Framework::ParameterValue *lowBound, 
      Framework::ParameterValue *highBound);
    ~PinBoundsBox() override;

    void paintBackground(Graphics &g) override;
    void paint(Graphics &g) override;
    void resized() override;

    void controlValueChanged(BaseControl *control) override;

    void paintHighlightBox(Graphics &g, float lowBoundValue, 
      float highBoundValue, Colour colour, float shiftValue = 0.0f) const;
    void positionSliders();

    void setRounding(float rounding) noexcept;
    void setTopRounding(float topRounding) noexcept;
    void setBottomRounding(float bottomRounding) noexcept;
    void setRounding(float topRounding, float bottomRounding) noexcept;
    void setRoundedCornerColour(Colour colour) noexcept;

  protected:
    utils::up<PinSlider> lowBound_;
    utils::up<PinSlider> highBound_;
    OpenGlImage highlight_{ "highlight" };
    OpenGlCorners roundedCorners_{};

    Colour primaryColour_{};
    Colour secondaryColour_{};
    Colour ternaryColour_{};
  };
}
