/*
  ==============================================================================

    PinBoundsBox.hpp
    Created: 27 Feb 2023 8:19:37pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"
#include "BaseControl.hpp"

namespace Framework
{
  class ParameterValue;
}

namespace Interface
{
  struct OpenGlQuad;
  struct OpenGlCorners;
  class PinSlider;

  class PinBoundsBox : public Component
  {
  public:
    static constexpr int kAdditionalPinWidth = 20;

    PinBoundsBox(Framework::ParameterValue *lowBound, Framework::ParameterValue *highBound);

    bool render(OpenGlWrapper &openGl) override;
    void paint(Graphics &g) override;
    void resized() override;

    void controlValueChanged(Control *control) override;

    void paintHighlightBox(Graphics &g, float lowBoundValue, 
      float highBoundValue, Colour colour, float shiftValue = 0.0f) const;
    void positionSliders();

    void setRounding(float rounding) noexcept;
    void setTopRounding(float topRounding) noexcept;
    void setBottomRounding(float bottomRounding) noexcept;
    void setRounding(float topRounding, float bottomRounding) noexcept;
    void setRoundedCornerColour(Colour colour) noexcept;

    float rounding[4]{};

    PinSlider lowBound_;
    PinSlider highBound_;

    Colour primaryColour_{};
    Colour secondaryColour_{};
    Colour ternaryColour_{};
  };
}
