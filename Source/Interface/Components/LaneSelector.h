/*
	==============================================================================

		LaneSelector.h
		Created: 3 May 2023 4:47:08am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"
#include "../Sections/EffectsLaneSection.h"

namespace Interface
{
	class LaneSelector : public BaseSection, public EffectsLaneSection::Listener
	{
	public:
		class Listener
		{
			virtual ~Listener() = default;
			virtual void addNewLane() = 0;
			virtual void cloneLane(EffectsLaneSection *lane) = 0;
			virtual void removeLane(EffectsLaneSection *lane) = 0;
			virtual void selectorChangedStartIndex(u32 newStartIndex) = 0;
		};

		LaneSelector() : BaseSection("Lane Selector") { }

		void resized() override;
		void paintBackground(Graphics &g) override;

		void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) override;

	private:
		// TODO: custom slider class for the selector + lane boxes
	};
}
