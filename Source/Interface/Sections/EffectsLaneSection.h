/*
	==============================================================================

		EffectsLaneSection.h
		Created: 3 Feb 2023 6:42:55pm
		Author:  theuser27

	==============================================================================
*/

#pragma once


#include "../Components/OpenGlScrollBar.h"
#include "../Components/OpenGlViewport.h"
#include "BaseSection.h"

namespace Generation
{
	class EffectsLane;
}

namespace Interface
{
	class PlainTextComponent;
	class OptionsButton;
	class RadioButton;
	class EffectsStateSection;
	class EffectModuleSection;
	class EffectsLaneSection;

	class EffectsContainer final : public BaseSection
	{
	public:
		EffectsContainer();
		~EffectsContainer() override;

		void buttonClicked(BaseButton *clickedButton) override;
		void handlePopupResult(int selection) const;
		void setLane(EffectsLaneSection *lane) noexcept { lane_ = lane; }
	private:
		EffectsLaneSection *lane_ = nullptr;

		std::unique_ptr<OptionsButton> addModulesButton_ = nullptr;

		friend class EffectsLaneSection;
	};

	class EffectsLaneSection final : public ProcessorSection, public OpenGlScrollBarListener, OpenGlViewportListener,
		public Generation::BaseProcessor::Listener
	{
	public:
		static constexpr int kTopBarHeight = 28;
		static constexpr int kBottomBarHeight = 28;

		static constexpr int kLeftEdgePadding = 12;
		static constexpr int kRightEdgePadding = 8;

		static constexpr int kModulesHorizontalVerticalPadding = 8;
		static constexpr int kBetweenModulePadding = 8;

		static constexpr int kAddModuleButtonHeight = 32;
		static constexpr int kGainMatchButtonDimensions = 10;

		static constexpr int kInsideRouding = 4;
		static constexpr int kOutlineThickness = 1;

		//static constexpr int kWidth = EffectModuleSection::kWidth + 2 * kModulesHorizontalVerticalPadding + 2 * kOutlineThickness;
		//static constexpr int kMinHeight = kTopBarHeight + kModulesHorizontalVerticalPadding + EffectModuleSection::kMinHeight +
		//	kBetweenModulePadding + kAddModuleButtonHeight + kModulesHorizontalVerticalPadding + kBottomBarHeight;

		EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *state, String name = {});
		~EffectsLaneSection() override;
		std::unique_ptr<EffectsLaneSection> createCopy();

		void resized() override;

		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		Rectangle<int> getPowerButtonBounds() const noexcept override
		{
			auto widthHeight = (int)std::round(scaleValue(kDefaultActivatorSize));
			return { getWidth() - (int)std::round(scaleValue(kRightEdgePadding)) - widthHeight,
				centerVertically(0, widthHeight, (int)std::round(scaleValue(kTopBarHeight))),
				widthHeight, widthHeight };
		}

		void scrollBarMoved([[maybe_unused]] OpenGlScrollBar *scrollBar, double rangeStart) override
		{ viewport_.setViewPosition(Point{ 0, (int)rangeStart }); }

		void visibleAreaChanged(int, int newY, int, int) override
		{
			setScrollBarRange();
			scrollBar_.setCurrentRange(newY, viewport_.getHeight());
		}
		void setScrollBarRange()
		{
			scrollBar_.setRangeLimits(0.0, container_.getHeight());
			scrollBar_.setCurrentRange(scrollBar_.getCurrentRangeStart(), 
				viewport_.getHeight(), dontSendNotification);
		}

		void scrollLane(const MouseEvent &e, const MouseWheelDetails &wheel)
		{ viewport_.mouseWheelMove(e, wheel); }

		void insertedSubProcessor(size_t index, Generation::BaseProcessor *newSubProcessor) override;
		void deletedSubProcessor(size_t index, Generation::BaseProcessor *deletedSubProcessor) override;

		void insertModule(size_t index, std::string_view newModuleType = {});
		void insertModule(size_t index, EffectModuleSection *movedModule);
		EffectModuleSection *deleteModule(size_t index, bool createUpdate = true);
		EffectModuleSection *deleteModule(const EffectModuleSection *instance, bool createUpdate = true);
		void moveModule(size_t oldIndex, size_t newIndex);

		void setEffectPositions();

		auto getNumModules() const noexcept { return effectModules_.size(); }
		std::optional<size_t> getModuleIndex(EffectModuleSection *effectModule) const
		{
			auto iterator = std::ranges::find(effectModules_, effectModule);
			return (iterator != effectModules_.end()) ?
				(size_t)(iterator - effectModules_.begin()) : std::optional<size_t>{};
		}

		// needs a point local to the EffectsLaneSection
		size_t getIndexFromScreenPosition(Point<int> point) const noexcept;

		void setLaneName(String newName);
		void addListener(EffectsLaneListener *listener) { laneListeners_.push_back(listener); }

	private:
		OpenGlViewport viewport_{};
		EffectsContainer container_{};

		gl_ptr<OpenGlQuad> outerRectangle_;
		gl_ptr<OpenGlQuad> innerRectangle_;
		gl_ptr<PlainTextComponent> laneTitle_;
		OpenGlScrollBar scrollBar_{ true };
		std::vector<EffectModuleSection *> effectModules_{};

		std::unique_ptr<PowerButton> laneActivator_ = nullptr;
		std::unique_ptr<RadioButton> gainMatchingButton_ = nullptr;
		std::unique_ptr<TextSelector> inputSelector_ = nullptr;
		std::unique_ptr<TextSelector> outputSelector_ = nullptr;

		Generation::EffectsLane *effectsLane_ = nullptr;
		EffectsStateSection *parentState_ = nullptr;

		std::vector<EffectsLaneListener *> laneListeners_{};
	};
}
