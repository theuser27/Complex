/*
	==============================================================================

		MainInterface.h
		Created: 10 Oct 2022 6:02:52pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#include "Plugin/Complex.h"
#include "BaseSection.h"
#include "HeaderFooterSections.h"
#include "../LookAndFeel/Shaders.h"
#include "../Components/OpenGLBackground.h"
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

	class MainInterface : public BaseSection, public OpenGLRenderer, juce::DragAndDropContainer, juce::Timer
	{
	public:
		static constexpr double kMinOpenGlVersion = 1.4;

		static constexpr int kMainVisualiserHeight = 112;
		
		static constexpr int kVerticalGlobalMargin = 8;
		static constexpr int kHorizontalWindowEdgeMargin = 4;
		static constexpr int kLaneToLaneMargin = 4;
		static constexpr int kLaneToBottomSettingsMargin = 20;

		static constexpr int kMinWidth = EffectsStateSection::kMinWidth + 2 * kHorizontalWindowEdgeMargin;
		static constexpr int kMinHeight = HeaderFooterSections::kHeaderHeight + kMainVisualiserHeight + 
			kVerticalGlobalMargin + EffectsStateSection::kMinHeight + kLaneToBottomSettingsMargin + HeaderFooterSections::kFooterHeight;

		MainInterface(Plugin::ComplexPlugin &plugin);
		~MainInterface() override;

		void paintBackground(Graphics &g) override;
		void copySkinValues(const Skin &skin);
		void reloadSkin(const Skin &skin);

		void repaintChildBackground(BaseSection *child);
		void repaintOpenGlBackground(OpenGlComponent *component);
		void redoBackground();
		void checkShouldReposition(bool resize = true);
		void parentHierarchyChanged() override
		{
			BaseSection::parentHierarchyChanged();
			checkShouldReposition();
		}
		void resized() override;
		void reset() override;
		void updateAllValues() override;

		void newOpenGLContextCreated() override;
		void renderOpenGL() override;
		void openGLContextClosing() override;

		// Inherited from juce::Timer
		void timerCallback() override { plugin_.updateGUIParameters(); openGlContext_.triggerRepaint(); }

		//void effectsMoved() override;

		void notifyChange();
		void notifyFresh();
		//bool loadAudioAsSource(int index, const String &name, InputStream *audio_stream);
		void popupSelector(Component *source, Point<int> position, const PopupItems &options,
			std::function<void(int)> callback, std::function<void()> cancel);
		void dualPopupSelector(Component *source, Point<int> position, int width,
			const PopupItems &options, std::function<void(int)> callback);
		void popupDisplay(Component *source, const std::string &text,
			BubbleComponent::BubblePlacement placement, bool primary, 
			Skin::SectionOverride sectionOverride = Skin::kPopupBrowser);
		void hideDisplay(bool primary);
		void enableRedoBackground(bool enable)
		{
			enableRedoBackground_ = enable;
			if (enable)
				resized();
		}

		auto &getPlugin() noexcept { return plugin_; }

	private:
		std::unique_ptr<HeaderFooterSections> headerFooter_;
		std::unique_ptr<EffectsStateSection> effectsStateSection_;
		std::unique_ptr<SinglePopupSelector> popupSelector_;
		std::unique_ptr<DualPopupSelector> dualPopupSelector_;
		std::unique_ptr<PopupDisplay> popupDisplay1_;
		std::unique_ptr<PopupDisplay> popupDisplay2_;

		// monitor rendering scale, not plugin scale
		float displayScale_ = 1.0f;
		// monitor rendering scale from the last render callback
		float lastRenderScale_ = 0.0f;
		bool setting_all_values_ = false;
		bool unsupported_ = false;
		bool animate_ = true;
		bool enableRedoBackground_ = true;
		CriticalSection openGlCriticalSection_;
		OpenGLContext openGlContext_;
		std::unique_ptr<Shaders> shaders_;
		OpenGlWrapper openGl_{ openGlContext_ };
		Image backgroundImage_;
		OpenGlBackground background_;
		std::unique_ptr<Skin> skinInstance_ = nullptr;

		Plugin::ComplexPlugin &plugin_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainInterface)
	};
}
