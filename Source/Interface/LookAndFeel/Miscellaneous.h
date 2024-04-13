/*
	==============================================================================

		Miscellaneous.h
		Created: 9 Feb 2024 2:01:31am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <string>
#include <vector>
#include "Framework/platform_definitions.h"

namespace juce
{
	class String;
}

namespace Generation
{
	class BaseProcessor;
	class BaseProcessorListener
	{
	public:
		virtual ~BaseProcessorListener() = default;
		virtual void insertedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *newSubProcessor) { }
		virtual void deletedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *deletedSubProcessor) { }
		virtual void updatedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *oldSubProcessor,
			[[maybe_unused]] BaseProcessor *newSubProcessor) { }
	};
}

namespace Interface
{
	class BaseSlider;
	class TextSelector;
	class BaseButton;
	class OpenGlTextEditor;
	class OpenGlScrollBar;
	class EffectsLaneSection;

	// HeaderFooter sizes
	static constexpr int kHeaderHeight = 40;
	static constexpr int kFooterHeight = 24;

	// EffectModule sizes
	inline constexpr int kSpectralMaskContractedHeight = 20;
	inline constexpr int kEffectModuleMainBodyHeight = 144;
	
	inline constexpr int kSpectralMaskMargin = 2;

	inline constexpr int kEffectModuleWidth = 400;
	inline constexpr int kEffectModuleMinHeight = kSpectralMaskContractedHeight + 
		kSpectralMaskMargin + kEffectModuleMainBodyHeight;

	// EffectsLane sizes
	inline constexpr int kEffectsLaneTopBarHeight = 28;
	inline constexpr int kEffectsLaneBottomBarHeight = 28;
	inline constexpr int kAddModuleButtonHeight = 32;

	inline constexpr int kModuleLaneMargin = 8;
	inline constexpr int kBetweenModuleMargin = 8;
	inline constexpr int kEffectsLaneOutlineThickness = 1;

	inline constexpr int kEffectsLaneWidth = kEffectModuleWidth + 2 * kModuleLaneMargin + 2 * kEffectsLaneOutlineThickness;
	inline constexpr int kEffectsLaneMinHeight = kEffectsLaneTopBarHeight + kModuleLaneMargin + kEffectModuleMinHeight +
		kBetweenModuleMargin + kAddModuleButtonHeight + kModuleLaneMargin + kEffectsLaneBottomBarHeight;

	// EffectsState sizes
	static constexpr int kLaneSelectorHeight = 38;
	static constexpr int kLaneSelectorToLanesMargin = 8;

	static constexpr int kEffectsStateMinWidth = kEffectsLaneWidth;
	static constexpr int kEffectsStateMinHeight = kLaneSelectorHeight + kLaneSelectorToLanesMargin + kEffectsLaneMinHeight;

	// MainInterface sizes
	static constexpr int kMainVisualiserHeight = 112;

	static constexpr int kVerticalGlobalMargin = 8;
	static constexpr int kHorizontalWindowEdgeMargin = 4;
	static constexpr int kLaneToBottomSettingsMargin = 20;

	static constexpr int kMinWidth = kEffectsStateMinWidth + 2 * kHorizontalWindowEdgeMargin;
	static constexpr int kMinHeight = kHeaderHeight + kMainVisualiserHeight +
		kVerticalGlobalMargin + kEffectsStateMinHeight + kLaneToBottomSettingsMargin + kFooterHeight;

	class SliderListener
	{
	public:
		virtual ~SliderListener() = default;
		virtual void sliderValueChanged(BaseSlider *slider) = 0;
		virtual void hoverStarted([[maybe_unused]] BaseSlider *slider) { }
		virtual void hoverEnded([[maybe_unused]] BaseSlider *slider) { }
		virtual void mouseDown([[maybe_unused]] BaseSlider *slider) { }
		virtual void mouseUp([[maybe_unused]] BaseSlider *slider) { }
		virtual void automationMappingChanged([[maybe_unused]] BaseSlider *slider) { }
		virtual void beginModulationEdit([[maybe_unused]] BaseSlider *slider) { }
		virtual void endModulationEdit([[maybe_unused]] BaseSlider *slider) { }
		virtual void menuFinished([[maybe_unused]] BaseSlider *slider) { }
		virtual void doubleClick([[maybe_unused]] BaseSlider *slider) { }
		virtual void modulationsChanged([[maybe_unused]] const juce::String &name) { }
		virtual void modulationAmountChanged([[maybe_unused]] BaseSlider *slider) { }
		virtual void modulationRemoved([[maybe_unused]] BaseSlider *slider) { }
		virtual void guiChanged([[maybe_unused]] BaseSlider *slider) { }
	};

	class ButtonListener
	{
	public:
		virtual ~ButtonListener() = default;
		virtual void buttonClicked(BaseButton *) = 0;
		virtual void guiChanged([[maybe_unused]] BaseButton *button) { }
	};

	class TextSelectorListener
	{
	public:
		virtual ~TextSelectorListener() = default;
		virtual void resizeForText(TextSelector *textSelector, int requestedWidthChange) = 0;
	};

	class OpenGlScrollBarListener
	{
	public:
		virtual ~OpenGlScrollBarListener() = default;
		virtual void scrollBarMoved(OpenGlScrollBar *scrollBarThatHasMoved, double newRangeStart) = 0;
	};

	class OpenGlViewportListener
	{
	public:
		virtual ~OpenGlViewportListener() = default;
		virtual void visibleAreaChanged(int newX, int newY, int newWidth, int newHeight) = 0;
	};

	class OpenGlTextEditorListener
	{
	public:
		/** Destructor. */
		virtual ~OpenGlTextEditorListener() = default;

		/** Called when the user changes the text in some way. */
		virtual void textEditorTextChanged(OpenGlTextEditor &) { }

		/** Called when the user presses the return key. */
		virtual void textEditorReturnKeyPressed(OpenGlTextEditor &) { }

		/** Called when the user presses the escape key. */
		virtual void textEditorEscapeKeyPressed(OpenGlTextEditor &) { }

		/** Called when the text editor loses focus. */
		virtual void textEditorFocusLost(OpenGlTextEditor &) { }
	};

	class EffectsLaneListener
	{
	public:
		virtual ~EffectsLaneListener() = default;
		virtual void laneTurnedOnOff(EffectsLaneSection *lane, bool isOn) = 0;
	};

	class LaneSelectorListener
	{
		virtual ~LaneSelectorListener() = default;
		virtual void addNewLane() = 0;
		virtual void cloneLane(EffectsLaneSection *lane) = 0;
		virtual void removeLane(EffectsLaneSection *lane) = 0;
		virtual void selectorChangedStartIndex(u32 newStartIndex) = 0;
	};

	class SpectralMaskListener
	{
	public:
		virtual ~SpectralMaskListener() = default;
		virtual void expansionChange(bool isExpanded) = 0;
	};

	struct PopupItems
	{
		std::vector<PopupItems> items{};
		std::string name{};
		int id = 0;
		bool selected = false;
		bool isActive = false;

		PopupItems() = default;
		PopupItems(std::string name) : name(std::move(name)) { }

		PopupItems(int id, std::string name, bool selected = false, bool active = false) :
			name(std::move(name)), id(id), selected(selected), isActive(active) { }

		void addItem(int subId, std::string subName, bool subSelected = false, bool active = false)
		{ items.emplace_back(subId, std::move(subName), subSelected, active); }

		void addItem(const PopupItems &item) { items.push_back(item); }
		void addItem(PopupItems &&item) { items.emplace_back(std::move(item)); }
		int size() const noexcept { return (int)items.size(); }
	};

}
