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
		PinBoundsBox(std::string_view name, Framework::ParameterValue *lowBound, Framework::ParameterValue *highBound);

		void paintBackground(Graphics &g) override
		{
			g.setColour(findColour(Skin::kBody, true));
			g.fillRect(getBounds());
		}
		void paint(Graphics &g) override;
		void resized() override;

		void sliderValueChanged(Slider *slider) override;

		void paintHighlightBox(Graphics &g, float lowBoundValue, float highBoundValue, Colour colour) const;

		void setRounding(float topRounding, float bottomRounding) noexcept
		{ roundedCorners_->setCorners(getLocalBounds(), topRounding, bottomRounding); }
		void setTopRounding(float topRounding) noexcept { roundedCorners_->setTopCorners(getLocalBounds(), topRounding); }
		void setBottomRounding(float bottomRounding) noexcept { roundedCorners_->setTopCorners(getLocalBounds(), bottomRounding); }

	protected:
		std::unique_ptr<PinSlider> lowBound_ = nullptr;
		std::unique_ptr<PinSlider> highBound_ = nullptr;
		std::unique_ptr<OpenGlImageComponent> highlight_ = nullptr;
		std::unique_ptr<OpenGlCorners> roundedCorners_ = nullptr;
	};
}
