/*
	==============================================================================

		PinBoundsBox.h
		Created: 27 Feb 2023 8:19:37pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"

namespace Interface
{
	class OpenGlQuad;
	class OpenGlCorners;
	class PinSlider;

	class PinBoundsBox : public BaseSection
	{
	public:
		static constexpr int kAdditionalPinWidth = 20;

		PinBoundsBox(std::string_view name, Framework::ParameterValue *lowBound, Framework::ParameterValue *highBound);
		~PinBoundsBox() override;

		void paintBackground(Graphics &g) override;
		void paint(Graphics &g) override;
		void resized() override;

		void sliderValueChanged(BaseSlider *slider) override;

		void paintHighlightBox(Graphics &g, float lowBoundValue, 
			float highBoundValue, Colour colour, float shiftValue = 0.0f) const;
		void positionSliders();

		void setRounding(float rounding) noexcept;
		void setTopRounding(float topRounding) noexcept;
		void setBottomRounding(float bottomRounding) noexcept;
		void setRounding(float topRounding, float bottomRounding) noexcept;
		void setRoundedCornerColour(Colour colour) noexcept;

	protected:
		std::unique_ptr<PinSlider> lowBound_ = nullptr;
		std::unique_ptr<PinSlider> highBound_ = nullptr;
		//gl_ptr<OpenGlQuad> highlight_ = nullptr;
		gl_ptr<OpenGlImageComponent> highlight_ = nullptr;
		gl_ptr<OpenGlCorners> roundedCorners_ = nullptr;

		Colour primaryColour_{};
		Colour secondaryColour_{};
		Colour ternaryColour_{};
	};
}
