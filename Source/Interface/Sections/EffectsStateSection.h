/*
	==============================================================================

		EffectsStateSection.h
		Created: 3 Feb 2023 6:41:53pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Components/DraggableComponent.h"
#include "../Components/LaneSelector.h"
#include "EffectsLaneSection.h"

namespace Interface
{
	class EffectsStateSection : public ProcessorSection, public DraggableComponent::Listener
	{
	public:
		static constexpr int kLaneSelectorHeight = 38;

		static constexpr int kLaneSelectorToLanesMargin = 8;
		static constexpr int kLaneToLaneMargin = 4;

		static constexpr int kMinWidth = EffectsLaneSection::kWidth;
		static constexpr int kMinHeight = kLaneSelectorHeight + kLaneSelectorToLanesMargin + EffectsLaneSection::kMinHeight;

		EffectsStateSection(Generation::EffectsState &state);

		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		void resized() override;

		// Inherited via DraggableComponent::Listener
		void prepareToMove(EffectModuleSection *effectModule, const MouseEvent &e, bool isCopying = false) override;
		void draggingComponent(EffectModuleSection *effectModule, const MouseEvent &e) override;
		void releaseComponent(EffectModuleSection *effectModule, const MouseEvent &e) override;

		void createLane(EffectsLaneSection *laneToCopy = nullptr);
		void deleteLane(EffectsLaneSection *laneToDelete);
		void registerModule(std::unique_ptr<EffectModuleSection> effectModuleSection);
		std::unique_ptr<EffectModuleSection> unregisterModule(EffectModuleSection *effectModuleSection);

	private:
		std::array<std::unique_ptr<EffectsLaneSection>, kMaxNumLanes> lanes_{};
		std::vector<std::unique_ptr<EffectModuleSection>> effectModules_{};

		LaneSelector laneSelector_{};
		u32 laneViewStartIndex_ = 0;

		EffectModuleSection *currentlyMovedModule_ = nullptr;
		std::pair<size_t, size_t> dragStartIndices_{};
		std::pair<size_t, size_t> dragEndIndices_{};
		juce::Point<int> movedModulePosition_{};
		bool isCopyingModule_ = false;

		OpenGlImage background_{};
		CriticalSection openGlCriticalSection_{};

		Generation::EffectsState &state_;
	};
}
