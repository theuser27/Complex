/*
	==============================================================================

		CurveLookAndFeel.h
		Created: 16 Nov 2022 6:54:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "DefaultLookAndFeel.h"

namespace Interface
{
	class CurveLookAndFeel : public DefaultLookAndFeel
	{
	public:
		~CurveLookAndFeel() override = default;

		void drawRotarySlider(Graphics &g, int x, int y, int width, int height,
			float sliderPositionNormalised, float startAngle, float endAngle, Slider &slider) override;

		void drawCurve(Graphics &g, Slider &slider, int x, int y, int width, int height, bool active, bool bipolar);

		static CurveLookAndFeel *instance()
		{
			static CurveLookAndFeel instance;
			return &instance;
		}

	private:
		CurveLookAndFeel() = default;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveLookAndFeel)
	};
}
