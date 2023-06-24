/*
	==============================================================================

		EffectsLaneSection.h
		Created: 3 Feb 2023 6:42:55pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Generation/EffectsState.h"
#include "EffectModuleSection.h"

namespace Interface
{
	class EffectsStateSection;

	class EffectsViewport : public Viewport
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void effectsScrolled(int position) = 0;
		};

		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override
		{
			// TODO: if the user is holding ctrl (+ shift) and scrolling,
			// redirect the scroll to the slider underneath if there is one
			Viewport::mouseWheelMove(e, wheel);
		}

		void addListener(Listener *listener) { listeners_.push_back(listener); }
		void visibleAreaChanged(const Rectangle<int> &visible_area) override
		{
			for (Listener *listener : listeners_)
				listener->effectsScrolled(visible_area.getY());

			Viewport::visibleAreaChanged(visible_area);
		}

	private:
		std::vector<Listener *> listeners_;
	};

	class EffectsLaneSection : public ProcessorSection, public ScrollBar::Listener, EffectsViewport::Listener,
		public Generation::BaseProcessor::Listener
	{
	public:
		static constexpr int kWidth = 418;
		static constexpr int kTopBarHeight = 28;
		static constexpr int kBottomBarHeight = 28;
		static constexpr int kAddModuleButtonHeight = 32;

		static constexpr int kModulesHorizontalVerticalPadding = 8;
		static constexpr int kBetweenModulePadding = 8;

		static constexpr int kInsideRouding = 4;
		static constexpr float kOutlineThickness = 1.0f;

		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) = 0;
		};

		EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *state, String name = {});
		std::unique_ptr<EffectsLaneSection> createCopy();

		void paintBackground(Graphics &g) override;
		void resized() override;
		void redoBackgroundImage();

		void initOpenGlComponents(OpenGlWrapper &openGl) override;
		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) override;
		void destroyOpenGlComponents(OpenGlWrapper &openGl) override;

		void scrollBarMoved([[maybe_unused]] ScrollBar *scrollBar, double rangeStart) override
		{ viewport_.setViewPosition(Point{ 0, (int)rangeStart }); }

		void effectsScrolled(int position) override
		{
			setScrollBarRange();
			scrollBar_.setCurrentRange(position, viewport_.getHeight());
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
		size_t getIndexFromScreenPosition(juce::Point<int> point) const noexcept;

		void setLaneName(String newName) { laneName_ = std::move(newName); }
		void addListener(Listener *listener) { laneListeners_.push_back(listener); }

	private:
		class EffectsContainer : public BaseSection
		{
		public:
			EffectsContainer() : BaseSection("container") { }
			void paintBackground(Graphics &g) override
			{
				g.fillAll(findColour(Skin::kBackground, true));
				paintChildrenShadows(g);
				paintChildrenBackgrounds(g);
			}
		};

		EffectsViewport viewport_{};
		EffectsContainer container_{};
		OpenGlImage background_{};
		CriticalSection openGlCriticalSection_{};

		OpenGlScrollBar scrollBar_{};
		std::vector<EffectModuleSection *> effectModules_{};

		std::unique_ptr<GeneralButton> gainMatchingButton_ = nullptr;
		std::unique_ptr<GeneralButton> laneActivator_ = nullptr;
		std::unique_ptr<TextSelector> inputSelector_ = nullptr;
		std::unique_ptr<TextSelector> outputSelector_ = nullptr;

		Generation::EffectsLane *effectsLane_ = nullptr;
		String laneName_{};

		EffectsStateSection *parentState_ = nullptr;

		std::vector<Listener *> laneListeners_{};
	};
}
