/*
	==============================================================================

		Fonts.h
		Created: 5 Dec 2022 2:08:35am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Interface
{
	class Fonts
	{
	public:
		static constexpr float kDDinDefaultHeight = 11.5f;
		static constexpr float kInterVDefaultHeight = 12.0f;

		static constexpr float kDDinDefaultKerning = 0.5f;
		static constexpr float kInterVDefaultKerning = 0.5f;

		Font &getDDinFont() { return DDinFont_; }
		Font &getInterVFont() { return InterVFont_; }

		float getAscentFromHeight(const Font &font, float height) const noexcept;
		float getHeightFromAscent(const Font &font, float ascent) const noexcept;
		float getDefaultFontHeight(const Font &font) const noexcept;

		void setFontFromHeight(Font &font, float height) const;
		void setFontFromAscent(Font &font, float ascent) const
		{ setFontFromHeight(font, getHeightFromAscent(font, ascent)); }

		static Fonts *instance()
		{
			static Fonts instance;
			return &instance;
		}

	private:
		Fonts();

		Font DDinFont_;
		Font InterVFont_;
	};
}