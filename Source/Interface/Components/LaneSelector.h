/*
	==============================================================================

		LaneSelector.h
		Created: 3 May 2023 4:47:08am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"

namespace Interface
{
	class LaneSelector final : public BaseSection, public EffectsLaneListener
	{
	public:
		LaneSelector() : BaseSection(typeid(LaneSelector).name()) { }

		void resized() override;
		void paintBackground(juce::Graphics &g) override;

		void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) override;

	private:
		// TODO: custom slider class for the selector + lane boxes
	};
}
