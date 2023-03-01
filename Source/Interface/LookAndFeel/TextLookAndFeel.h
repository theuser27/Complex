/*
	==============================================================================

		TextLookAndFeel.h
		Created: 16 Nov 2022 6:47:50am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "DefaultLookAndFeel.h"

namespace Interface
{
	class TextLookAndFeel : public DefaultLookAndFeel
	{
	public:

		void drawRotarySlider(Graphics &g, int x, int y, int width, int height,
			float sliderPositionNormalised, float startAngle, float endAngle,
			Slider &slider) override;

		void drawToggleButton(Graphics &g, ToggleButton &button, bool hover, bool isDown) override;

		void drawTickBox(Graphics &g, Component &component, float x, float y, float w, float h, bool ticked,
			bool enabled, bool mouseOver, bool buttonDown) override;

		void drawLabel(Graphics &g, Label &label) override;

		void drawComboBox(Graphics &g, int width, int height, bool isDown,
			int buttonX, int buttonY, int buttonW, int buttonH, ComboBox &box) override;

		static TextLookAndFeel *instance()
		{
			static TextLookAndFeel instance;
			return &instance;
		}

	private:
		TextLookAndFeel() = default;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextLookAndFeel)
	};
}