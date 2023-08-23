/*
	==============================================================================

		PinBoundsBox.h
		Created: 27 Feb 2023 8:19:37pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"
#include "BaseSlider.h"

namespace Interface
{
	class PinBoundsBox : public BaseSection
	{
	public:
		static constexpr int kAdditionalPinWidth = 20;

		PinBoundsBox(std::string_view name, Framework::ParameterValue *lowBound, Framework::ParameterValue *highBound);

		void paintBackground(Graphics &g) override;
		void paint(Graphics &g) override;
		void resized() override;

		void sliderValueChanged(Slider *slider) override;

		void paintHighlightBox(Graphics &g, float lowBoundValue, 
			float highBoundValue, Colour colour, float shiftValue = 0.0f) const;
		void positionSliders();

		void setRounding(float rounding) noexcept { roundedCorners_->setCorners(getLocalBounds(), rounding); }
		void setTopRounding(float topRounding) noexcept { roundedCorners_->setTopCorners(getLocalBounds(), topRounding); }
		void setBottomRounding(float bottomRounding) noexcept { roundedCorners_->setTopCorners(getLocalBounds(), bottomRounding); }
		void setRounding(float topRounding, float bottomRounding) noexcept
		{ roundedCorners_->setCorners(getLocalBounds(), topRounding, bottomRounding); }
		void setRoundedCornerColour(Colour colour) noexcept { roundedCorners_->setColor(colour); }

	protected:
		std::unique_ptr<PinSlider> lowBound_ = nullptr;
		std::unique_ptr<PinSlider> highBound_ = nullptr;
		gl_ptr<OpenGlImageComponent> highlight_ = nullptr;
		gl_ptr<OpenGlCorners> roundedCorners_ = nullptr;
	};
}
