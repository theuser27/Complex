/*
	==============================================================================

		CurveLookAndFeel.cpp
		Created: 16 Nov 2022 6:54:15am
		Author:  theuser27

	==============================================================================
*/

#include "Framework/simd_utils.h"
#include "CurveLookAndFeel.h"
#include "Skin.h"
#include "Fonts.h"
#include "../Components/BaseSlider.h"

namespace Interface
{
	void CurveLookAndFeel::drawRotarySlider(Graphics &g, int x, int y, int width, int height,
		float sliderPositionNormalised, float startAngle, float endAngle, Slider &slider)
	{
		bool active = true;
		bool bipolar = false;
		if (auto *baseSlider = dynamic_cast<BaseSlider *>(&slider))
		{
			active = baseSlider->isActive();
			bipolar = baseSlider->isBipolar();
		}

		float short_side = std::min(width, height);
		float rounding = 0.0f;
		float max_width = short_side;
		if (auto *section = slider.findParentComponentOfClass<BaseSection>(); section)
		{
			rounding = section->findValue(Skin::kWidgetRoundedCorner);
			max_width = std::min(max_width, section->findValue(Skin::kKnobArcSize));
		}

		int inset = rounding / sqrtf(2.0f) + (short_side - max_width) / 2.0f;
		drawCurve(g, slider, x + inset, y + inset, width - 2.0f * inset, height - 2.0f * inset, active, bipolar);
	}

	void CurveLookAndFeel::drawCurve(Graphics &g, Slider &slider, int x, int y, int width, int height,
		bool active, bool bipolar)
	{
		static constexpr int kResolution = 16;
		static constexpr float kLineWidth = 2.0f;
		PathStrokeType stroke(kLineWidth, PathStrokeType::beveled, PathStrokeType::rounded);

		float curve_width = std::min(width, height);
		float x_offset = (width - curve_width) / 2.0f;
		float power = -slider.getValue();
		Path path;
		float start_x = x + x_offset + kLineWidth / 2.0f;
		float start_y = y + height - kLineWidth / 2.0f;
		path.startNewSubPath(start_x, start_y);
		float active_width = curve_width - kLineWidth;
		float active_height = curve_width - kLineWidth;

		if (bipolar)
		{
			float half_width = active_width / 2.0f;
			float half_height = active_height / 2.0f;
			for (int i = 1; i <= kResolution / 2; ++i)
			{
				float t = 2.0f * i / (float)kResolution;
				float power_t = utils::powerScale(t, -power);
				path.lineTo(start_x + t * half_width, start_y - power_t * half_height);
			}
			for (int i = 1; i <= kResolution / 2; ++i)
			{
				float t = 2.0f * i / (float)kResolution;
				float power_t = utils::powerScale(t, power);
				path.lineTo(start_x + t * half_width + half_width, start_y - power_t * half_height - half_height);
			}
		}
		else
		{
			for (int i = 1; i <= kResolution; ++i)
			{
				float t = i / (float)kResolution;
				float power_t = utils::powerScale(t, power);
				path.lineTo(start_x + t * active_width, start_y - power_t * active_height);
			}
		}

		Colour line = slider.findColour(Skin::kRotaryArc, true);
		if (!active)
			line = slider.findColour(Skin::kWidgetPrimaryDisabled, true);

		g.setColour(line);
		g.strokePath(path, stroke);
	}
}