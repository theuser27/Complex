/*
	==============================================================================

		Fonts.cpp
		Created: 23 Jun 2023 7:07:51pm
		Author:  theuser27

	==============================================================================
*/

#include "Fonts.h"
#include "Framework/common.h"

namespace Interface
{
	float Fonts::getAscentFromHeight(const Font &font, float height) const noexcept
	{
		if (font.getTypefaceName() == DDinFont_.getTypefaceName())
			return height * 8.0f / kDDinDefaultHeight;
		if (font.getTypefaceName() == InterVFont_.getTypefaceName())
			return height * 8.0f / kInterVDefaultHeight;

		COMPLEX_ASSERT_FALSE("Unknown font was provided to get ascent for");
		return 1.0f;
	}

	float Fonts::getHeightFromAscent(const Font &font, float ascent) const noexcept
	{
		if (font.getTypefaceName() == DDinFont_.getTypefaceName())
			return ascent * kDDinDefaultHeight / 8.0f;
		if (font.getTypefaceName() == InterVFont_.getTypefaceName())
			return ascent * kInterVDefaultHeight / 8.0f;

		COMPLEX_ASSERT_FALSE("Unknown font was provided to get height for");
		return 1.0f;
	}

	float Fonts::getDefaultFontHeight(const Font &font) const noexcept
	{
		if (font.getTypefaceName() == DDinFont_.getTypefaceName())
			return kDDinDefaultHeight;
		if (font.getTypefaceName() == InterVFont_.getTypefaceName())
			return kInterVDefaultHeight;

		COMPLEX_ASSERT_FALSE("Unknown font was provided to get default height for");
		return 11.0f;
	}

	void Fonts::setFontFromHeight(Font &font, float height) const
	{
		font.setHeight(height);

		if (font.getTypefaceName() == DDinFont_.getTypefaceName())
			font.setExtraKerningFactor((height + kDDinDefaultKerning) / height - 1.0f);
		else if (font.getTypefaceName() == InterVFont_.getTypefaceName())
			font.setExtraKerningFactor((height + kInterVDefaultKerning) / height - 1.0f);
		else COMPLEX_ASSERT_FALSE("Unknown font was provided to set");
	}

	Fonts::Fonts() :
		DDinFont_(Typeface::createSystemTypefaceFor(
			BinaryData::DDINBold_ttf, BinaryData::DDINBold_ttfSize)),
		InterVFont_(Typeface::createSystemTypefaceFor(
			BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize))
	{
		DDinFont_.setHeight(kDDinDefaultHeight);
		DDinFont_.setExtraKerningFactor((kDDinDefaultHeight + kDDinDefaultKerning) / kDDinDefaultHeight - 1.0f);

		InterVFont_.setHeight(kInterVDefaultHeight);
		InterVFont_.setExtraKerningFactor((kInterVDefaultHeight + kInterVDefaultKerning) / kInterVDefaultHeight - 1.0f);
	}
}
