/*
	==============================================================================

		Fonts.h
		Created: 5 Dec 2022 2:08:35am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Framework/platform_definitions.h"

namespace Interface
{
	class Fonts
	{
	public:
		static constexpr float kDDinDefaultHeight = 11.5f;
		// windows font rendering is poop
	#if COMPLEX_MSVC
		static constexpr float kInterVDefaultHeight = 12.0f;
	#elif
		static constexpr float kInterVDefaultHeight = 11.0f;
	#endif

		static constexpr float kDDinDefaultKerning = 0.5f;
		static constexpr float kInterVDefaultKerning = 0.5f;

		Font &getDDinFont() { return DDinFont_; }
		Font &getInterVFont() { return InterVFont_; }

		float getAscentFromHeight(const Font &font, float height) const noexcept;
		float getHeightFromAscent(const Font &font, float height) const noexcept;
		float getDefaultFontHeight(const Font &font) const noexcept;

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