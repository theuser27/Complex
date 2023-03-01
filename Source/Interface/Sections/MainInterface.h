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
#include "../LookAndFeel/Shaders.h"
#include "../Components/OpenGLBackground.h"

namespace Interface
{
	class AboutSection;
	class BankExporter;
	class BendSection;
	class DeleteSection;
	class ExpiredSection;
	class ExtraModSection;
	class HeaderSection;
	class KeyboardInterface;
	class MasterControlsInterface;
	class ModulationInterface;
	class ModulationManager;
	class PortamentoSection;
	class PresetBrowser;
	class SaveSection;
	class SynthesisInterface;
	struct GuiData;
	class PluginSlider;
	class WavetableEditSection;
	class VoiceSection;
	class PopupDisplay;
	class SinglePopupSelector;
	class DualPopupSelector;

	class MainInterface : public BaseSection, public OpenGLRenderer, DragAndDropContainer
	{
	public:
		static constexpr double kMinOpenGlVersion = 1.4;

		MainInterface(GuiData *guiData);

		MainInterface();
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
		void animate(bool animate) override;
		void reset() override;

		void newOpenGLContextCreated() override;
		void renderOpenGL() override;
		void openGLContextClosing() override;

		//void effectsMoved() override;

		void notifyChange();
		void notifyFresh();
		//bool loadAudioAsSource(int index, const String &name, InputStream *audio_stream);
		void popupSelector(Component *source, Point<int> position, const PopupItems &options,
			std::function<void(int)> callback, std::function<void()> cancel);
		void dualPopupSelector(Component *source, Point<int> position, int width,
			const PopupItems &options, std::function<void(int)> callback);
		void popupDisplay(Component *source, const std::string &text,
			BubbleComponent::BubblePlacement placement, bool primary);
		void hideDisplay(bool primary);
		void enableRedoBackground(bool enable)
		{
			enable_redo_background_ = enable;
			if (enable)
				resized();
		}

		float getResizingScale() const { return width_ * 1.0f / resized_width_; }
		int getPixelMultiple() const override { return pixel_multiple_; }

	private:

		std::map<std::string, PluginSlider *> slider_lookup_;
		std::map<std::string, Button *> button_lookup_;

		std::unique_ptr<HeaderSection> header_;
		//std::unique_ptr<EffectsInterface> effects_interface_;
		std::unique_ptr<SinglePopupSelector> popup_selector_;
		std::unique_ptr<DualPopupSelector> dual_popup_selector_;
		std::unique_ptr<PopupDisplay> popup_display_1_;
		std::unique_ptr<PopupDisplay> popup_display_2_;

		int width_ = 0;
		int resized_width_ = 0;
		float last_render_scale_ = 0.0f;
		float display_scale_ = 1.0f;
		int pixel_multiple_ = 1;
		bool setting_all_values_ = false;
		bool unsupported_ = false;
		bool animate_ = true;
		bool enable_redo_background_ = true;
		CriticalSection open_gl_critical_section_;
		OpenGLContext open_gl_context_;
		std::unique_ptr<Shaders> shaders_;
		OpenGlWrapper open_gl_{ open_gl_context_ };
		Image background_image_;
		OpenGlBackground background_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainInterface)
	};
}
