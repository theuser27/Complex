/*
	==============================================================================

		MainInterface.h
		Created: 10 Oct 2022 6:02:52pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#include "BaseSection.h"
#include "HeaderFooterSections.h"
#include "EffectsStateSection.h"

namespace Interface
{
	class AboutSection;
	class DeleteSection;
	class ExtraModSection;
	class EffectsStateSection;
	class MasterControlsInterface;
	class ModulationInterface;
	class ModulationManager;
	class PresetBrowser;
	class SaveSection;
	class PopupDisplay;
	class SinglePopupSelector;
	class DualPopupSelector;

	class MainInterface : public BaseSection, public juce::DragAndDropContainer
	{
	public:
		static constexpr int kMainVisualiserHeight = 112;
		
		static constexpr int kVerticalGlobalMargin = 8;
		static constexpr int kHorizontalWindowEdgeMargin = 4;
		static constexpr int kLaneToLaneMargin = 4;
		static constexpr int kLaneToBottomSettingsMargin = 20;

		static constexpr int kMinWidth = EffectsStateSection::kMinWidth + 2 * kHorizontalWindowEdgeMargin;
		static constexpr int kMinHeight = HeaderFooterSections::kHeaderHeight + kMainVisualiserHeight + 
			kVerticalGlobalMargin + EffectsStateSection::kMinHeight + kLaneToBottomSettingsMargin + HeaderFooterSections::kFooterHeight;

		MainInterface(Renderer *renderer);

		void reloadSkin(const Skin &skin);

		void resized() override;
		void reset() override;

		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) override 
		{ BaseSection::renderOpenGlComponents(openGl, animate); }

		//void effectsMoved() override;

		void notifyChange();
		void notifyFresh();
		//bool loadAudioAsSource(int index, const String &name, InputStream *audio_stream);
		void popupSelector(const Component *source, Point<int> position, PopupItems options,
			std::function<void(int)> callback, std::function<void()> cancel);
		void dualPopupSelector(Component *source, Point<int> position, int width,
			const PopupItems &options, std::function<void(int)> callback);
		void popupDisplay(Component *source, String text,
			BubbleComponent::BubblePlacement placement, bool primary, 
			Skin::SectionOverride sectionOverride = Skin::kPopupBrowser);
		void hideDisplay(bool primary);

	private:
		std::unique_ptr<HeaderFooterSections> headerFooter_;
		std::unique_ptr<EffectsStateSection> effectsStateSection_;
		std::unique_ptr<SinglePopupSelector> popupSelector_;
		std::unique_ptr<DualPopupSelector> dualPopupSelector_;
		std::unique_ptr<PopupDisplay> popupDisplay1_;
		std::unique_ptr<PopupDisplay> popupDisplay2_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainInterface)
	};
}
