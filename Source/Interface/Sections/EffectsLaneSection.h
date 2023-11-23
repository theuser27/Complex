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
	class EffectsLaneSection;

	class EffectsViewport : public Viewport
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void effectsScrolled(int position) = 0;
		};

		class EffectsContainer : public BaseSection
		{
		public:
			EffectsContainer() : BaseSection(typeid(EffectsContainer).name())
			{
				setSkinOverride(Skin::kEffectsLane);

				addModulesButton_ = std::make_unique<OptionsButton>(nullptr, "Add Modules Button");
				addModulesButton_->removeLabel();
				addControl(addModulesButton_.get());
			}

			void buttonClicked(BaseButton *clickedButton) override;
			void handlePopupResult(int selection) const;
			void setLane(EffectsLaneSection *lane) noexcept { lane_ = lane; }
		private:
			EffectsLaneSection *lane_ = nullptr;

			std::unique_ptr<OptionsButton> addModulesButton_ = nullptr;

			friend class EffectsLaneSection;
		};

		EffectsViewport() : Viewport(typeid(EffectsViewport).name())
		{
			container_.setAlwaysOnTop(true);
			setViewedComponent(&container_);
			addAndMakeVisible(&container_);
		}

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
		EffectsContainer container_{};

		std::vector<Listener *> listeners_;

		friend class EffectsLaneSection;
	};

	class EffectsLaneSection : public ProcessorSection, public ScrollBar::Listener, EffectsViewport::Listener,
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

		static constexpr int kWidth = EffectModuleSection::kWidth + 2 * kModulesHorizontalVerticalPadding + 2 * kOutlineThickness;
		static constexpr int kMinHeight = kTopBarHeight + kModulesHorizontalVerticalPadding + EffectModuleSection::kMinHeight +
			kBetweenModulePadding + kAddModuleButtonHeight + kModulesHorizontalVerticalPadding + kBottomBarHeight;

		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) = 0;
		};

		EffectsLaneSection(Generation::EffectsLane *effectsLane, EffectsStateSection *state, String name = {});
		std::unique_ptr<EffectsLaneSection> createCopy();

		void resized() override;

		Rectangle<int> getPowerButtonBounds() const noexcept override
		{
			auto widthHeight = (int)std::round(scaleValue(kDefaultActivatorSize));
			return { getWidth() - (int)std::round(scaleValue(kRightEdgePadding)) - widthHeight,
				centerVertically(0, widthHeight, (int)std::round(scaleValue(kTopBarHeight))),
				widthHeight, widthHeight };
		}

		void scrollBarMoved([[maybe_unused]] ScrollBar *scrollBar, double rangeStart) override
		{ viewport_.setViewPosition(Point{ 0, (int)rangeStart }); }

		void effectsScrolled(int position) override
		{
			setScrollBarRange();
			scrollBar_.setCurrentRange(position, viewport_.getHeight());
		}
		void setScrollBarRange()
		{
			scrollBar_.setRangeLimits(0.0, viewport_.container_.getHeight());
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
		size_t getIndexFromScreenPosition(Point<int> point) const noexcept;

		void setLaneName(String newName) { laneTitle_->setText(std::move(newName)); }
		void addListener(Listener *listener) { laneListeners_.push_back(listener); }

	private:
		EffectsViewport viewport_{};

		gl_ptr<OpenGlQuad> outerRectangle;
		gl_ptr<OpenGlQuad> innerRectangle;
		gl_ptr<PlainTextComponent> laneTitle_;
		OpenGlScrollBar scrollBar_{};
		std::vector<EffectModuleSection *> effectModules_{};

		std::unique_ptr<PowerButton> laneActivator_ = nullptr;
		std::unique_ptr<RadioButton> gainMatchingButton_ = nullptr;
		std::unique_ptr<TextSelector> inputSelector_ = nullptr;
		std::unique_ptr<TextSelector> outputSelector_ = nullptr;

		Generation::EffectsLane *effectsLane_ = nullptr;
		EffectsStateSection *parentState_ = nullptr;

		std::vector<Listener *> laneListeners_{};
	};
}
