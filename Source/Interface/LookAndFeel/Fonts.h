/*
	==============================================================================

		Fonts.h
		Created: 5 Dec 2022 2:08:35am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace Interface
{
	class Fonts
	{
	public:
		static constexpr float kDDinDefaultHeight = 11.5f;
		static constexpr float kInterVDefaultHeight = 12.0f;

		static constexpr float kDDinDefaultKerning = 0.5f;
		static constexpr float kInterVDefaultKerning = 0.5f;

		juce::Font &getDDinFont() { return DDinFont_; }
		juce::Font &getInterVFont() { return InterVFont_; }

		float getAscentFromHeight(const juce::Font &font, float height) const noexcept;
		float getHeightFromAscent(const juce::Font &font, float ascent) const noexcept;
		float getDefaultFontHeight(const juce::Font &font) const noexcept;

		void setFontFromHeight(juce::Font &font, float height) const;
		void setFontFromAscent(juce::Font &font, float ascent) const
		{ setFontFromHeight(font, getHeightFromAscent(font, ascent)); }

		static Fonts *instance()
		{
			static Fonts instance;
			return &instance;
		}

	private:
		Fonts();

		juce::Font DDinFont_;
		juce::Font InterVFont_;
	};
}