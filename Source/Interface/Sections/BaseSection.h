/*
	==============================================================================

		BaseSection.h
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/Miscellaneous.h"
#include "../Components/OpenGlContainer.h"

namespace Generation
{
	class BaseProcessor;
}

namespace Interface
{
	class BaseControl;
	class Renderer;
	class PowerButton;
	class OpenGlBackground;
	class OffOverlayQuad;

	class BaseSection : public OpenGlContainer, public SliderListener, public ButtonListener, public TextSelectorListener
	{
	public:
		static constexpr int kDefaultActivatorSize = 12;
		static constexpr float kPowerButtonPaddingPercent = 0.29f;
		static constexpr float kTransposeHeightPercent = 0.5f;
		static constexpr float kTuneHeightPercent = 0.4f;
		static constexpr float kJointModulationRadiusPercent = 0.1f;
		static constexpr float kJointModulationExtensionPercent = 0.6666f;
		static constexpr float kPitchLabelPercent = 0.33f;
		static constexpr float kJointLabelHeightPercent = 0.4f;
		static constexpr double kTransposeMouseSensitivity = 0.2;
		static constexpr float kJointLabelBorderRatioX = 0.05f;

		static constexpr int kDefaultBodyRounding = 4;
		static constexpr int kDefaultLabelHeight = 10;
		static constexpr int kDefaultLabelBackgroundHeight = 16;
		static constexpr int kDefaultLabelBackgroundWidth = 56;
		static constexpr int kDefaultLabelBackgroundRounding = 4;
		static constexpr int kDefaultPadding = 2;
		static constexpr int kDefaultPopupMenuWidth = 150;
		static constexpr int kDefaultDualPopupMenuWidth = 340;
		static constexpr int kDefaultStandardKnobSize = 32;
		static constexpr int kDefaultKnobThickness = 2;
		static constexpr float kDefaultKnobModulationAmountThickness = 2.0f;
		static constexpr int kDefaultKnobModulationMeterSize = 43;
		static constexpr int kDefaultKnobModulationMeterThickness = 4;
		static constexpr int kDefaultModulationButtonWidth = 64;
		static constexpr int kDefaultModFontSize = 10;
		static constexpr int kDefaultKnobSectionHeight = 64;
		static constexpr int kDefaultSliderWidth = 24;
		static constexpr int kDefaultTextWidth = 80;
		static constexpr int kDefaultTextHeight = 24;
		static constexpr int kDefaultWidgetMargin = 6;
		static constexpr float kDefaultWidgetFillFade = 0.3f;
		static constexpr float kDefaultWidgetLineWidth = 4.0f;
		static constexpr float kDefaultWidgetFillCenter = 0.0f;

		BaseSection(std::string_view name);
		~BaseSection() override;

		void setBounds(int x, int y, int width, int height) final { BaseComponent::setBounds(x, y, width, height); }
		using OpenGlContainer::setBounds;
		void resized() override;
		void paint(juce::Graphics &) override { }

		void sliderValueChanged([[maybe_unused]] BaseSlider *movedSlider) override { }
		void buttonClicked([[maybe_unused]] BaseButton *clickedButton) override { }
		void guiChanged([[maybe_unused]] BaseSlider *slider) override { }
		void guiChanged(BaseButton *button) override;
		void resizeForText(TextSelector *textSelector, int requestedWidthChange) override;

		// paint anything that doesn't move/is static
		virtual void paintBackground(juce::Graphics &) { }
		virtual void repaintBackground();

		juce::Path getRoundedPath(juce::Rectangle<float> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(juce::Graphics &g, juce::Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBorder(juce::Graphics &g, juce::Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(juce::Graphics &g) const { paintBody(g, getLocalBounds()); }
		void paintBorder(juce::Graphics &g) const { paintBorder(g, getLocalBounds()); }

		void paintTabShadow(juce::Graphics &g, juce::Rectangle<int> bounds);
		void paintTabShadow(juce::Graphics &g) { paintTabShadow(g, getLocalBounds()); }
		virtual void paintBackgroundShadow(juce::Graphics &) { }
		float getComponentShadowWidth() const noexcept { return scaling_ * 2.0f; }
		
		// main opengl render loop
		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate = false) override;
		void destroyAllOpenGlComponents() final;

		void showPopupSelector(const BaseComponent *source, juce::Point<int> position, PopupItems options,
			std::function<void(int)> callback, std::function<void()> cancel = {}) const;
		void showPopupDisplay(BaseComponent *source, juce::String text,
			juce::BubbleComponent::BubblePlacement placement, bool primary);
		void hidePopupDisplay(bool primary);

		virtual void setActive(bool active);
		bool isActive() const noexcept { return active_; }

		virtual void updateAllValues();
		PowerButton *activator() const { return activator_; }

		virtual void reset();

		virtual void addSubSection(BaseSection *section, bool show = true);
		virtual void removeSubSection(BaseSection *section, bool removeChild = false);
		void addControl(BaseControl *control);
		void removeControl(BaseControl *control, bool removeChild = false);

		float getScaling() const noexcept { return scaling_; }
		float getPadding() const noexcept { return getValue(Skin::kPadding); }
		float getKnobSectionHeight() const noexcept { return getValue(Skin::kKnobSectionHeight); }
		float getSliderWidth() const noexcept { return getValue(Skin::kSliderWidth); }
		float getTextComponentHeight() const noexcept { return getValue(Skin::kTextComponentHeight); }
		float getStandardKnobSize() const noexcept { return getValue(Skin::kKnobArcSize); }
		float getTotalKnobHeight() const noexcept { return getStandardKnobSize(); }
		float getTextSectionYOffset() const noexcept { return (getKnobSectionHeight() - getTextComponentHeight()) / 2.0f; }
		float getModButtonWidth() const noexcept { return getValue(Skin::kModulationButtonWidth); }
		float getModFontSize() const noexcept { return getValue(Skin::kModulationFontSize); }
		float getWidgetMargin() const noexcept { return getValue(Skin::kWidgetMargin); }
		float getWidgetRounding() const noexcept { return getValue(Skin::kWidgetMargin); }
		int getPopupWidth() const noexcept { return scaleValueRoundInt(kDefaultPopupMenuWidth); }

		[[nodiscard]] auto *getControl(std::string_view enumName) { return controls_.at(enumName); }

		void setSkinOverride(Skin::SectionOverride skinOverride) noexcept final;
		void setRenderer(Renderer *renderer) noexcept final;
		void setScaling(float scale) noexcept final;

	protected:
		void setActivator(PowerButton *activator);
		void createOffOverlay();
		void createBackground();

		virtual juce::Rectangle<int> getPowerButtonBounds() const noexcept
		{
			return { 0, 0, (int)std::round(scaleValue(kDefaultActivatorSize)),
				(int)std::round(scaleValue(kDefaultActivatorSize)) };
		}

		std::vector<BaseSection *> subSections_{};
		gl_ptr<OpenGlBackground> background_ = nullptr;
		gl_ptr<OffOverlayQuad> offOverlayQuad_ = nullptr;

		std::map<std::string_view, BaseControl *> controls_{};

		PowerButton *activator_ = nullptr;

		bool active_ = true;
	};

	class ProcessorSection : public BaseSection
	{
	public:
		ProcessorSection(std::string_view name, Generation::BaseProcessor *processor) :
			BaseSection(name), processor_(processor) { }

		[[nodiscard]] std::optional<u64> getProcessorId() const noexcept;
		[[nodiscard]] auto getProcessor() const noexcept { return processor_; }
	protected:
		Generation::BaseProcessor *processor_ = nullptr;
	};
}
