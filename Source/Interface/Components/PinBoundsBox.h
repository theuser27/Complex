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
		PinBoundsBox(std::string_view name);

		void paintBackground(Graphics &g) override { paintBody(g, getBounds(), topRounding_, bottomRounding_); }
		void paint(Graphics &g) override;
		void resized() override;

		void sliderValueChanged(Slider *slider) override;

		void setBottomRounding(float rounding) noexcept { bottomRounding_ = rounding; }
		void setTopRounding(float rounding) noexcept { topRounding_ = rounding; }

	private:
		float topRounding_ = 3.0f;
		float bottomRounding_ = 3.0f;

		std::unique_ptr<PinSlider> lowBound_;
		std::unique_ptr<PinSlider> highBound_;
		std::unique_ptr<OpenGlImageComponent> highlight_;
		std::unique_ptr<OpenGlCorners> roundedCorners_;
	};
}
