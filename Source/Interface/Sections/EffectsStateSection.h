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
		static constexpr int kTopToLaneSelectorMargin = 8;

		EffectsStateSection(Generation::EffectsState &state);

		void registerModule(std::unique_ptr<EffectModuleSection> effectModuleSection)
		{
			effectModuleSection->getDraggableComponent().setListener(this);
			effectModules_.emplace_back(std::move(effectModuleSection));
		}
		auto unregisterModule(EffectModuleSection *effectModuleSection)
		{
			auto iter = std::ranges::find_if(effectModules_, [effectModuleSection](const auto &pointer) 
				{ return effectModuleSection == pointer.get(); });
			COMPLEX_ASSERT(iter != effectModules_.end() && "The effectModule to be unregistered wasn't found");
			auto pointer = std::unique_ptr<EffectModuleSection>(iter->release());
			effectModules_.erase(iter);
			return pointer;
		}

		void paintBackground(Graphics &g) override { paintChildrenBackgrounds(g); }
		void resized() override;

		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		void prepareToMove(EffectModuleSection *effectModule, const MouseEvent &e, bool isCopying = false) override;
		void draggingComponent(EffectModuleSection *effectModule, const MouseEvent &e) override;
		void releaseComponent(EffectModuleSection *effectModule, const MouseEvent &e) override;

	private:
		std::array<std::unique_ptr<EffectsLaneSection>, kMaxNumChains> lanes_{};
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
