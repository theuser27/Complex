/*
	==============================================================================

		BaseSection.h
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Generation/BaseProcessor.h"

#include "../LookAndFeel/Skin.h"
#include "../Components/BaseButton.h"
#include "../Components/BaseSlider.h"
#include "Plugin/Renderer.h"

namespace Interface
{
	class ModulationButton;

	class BaseSection : public Component, public BaseSlider::Listener, public BaseButton::Listener, 
		public TextSelector::TextSelectorListener
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

		class OffOverlayQuad : public OpenGlMultiQuad
		{
		public:
			OffOverlayQuad() : OpenGlMultiQuad(1, Shaders::kColorFragment, typeid(OffOverlayQuad).name()) 
			{ setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f); }

			void paintBackground(Graphics &) override { }
		};

		BaseSection(std::string_view name);

		void resized() override;
		void paint(Graphics &) override { }

		void sliderValueChanged([[maybe_unused]] BaseSlider *movedSlider) override { }
		void buttonClicked([[maybe_unused]] BaseButton *clickedButton) override { }
		void guiChanged([[maybe_unused]] BaseSlider *slider) override { }
		void guiChanged(BaseButton *button) override;
		void resizeForText(TextSelector *textSelector, int requestedWidthChange) override;

		// paint anything that doesn't move/is static
		virtual void paintBackground(Graphics &g);
		virtual void repaintBackground();
		void paintOpenGlBackground(Graphics &g, OpenGlComponent *openGlComponent);
		void repaintOpenGlBackground(OpenGlComponent *openGlComponent);
		void paintOpenGlChildrenBackgrounds(Graphics &g);

		Path getRoundedPath(Rectangle<float> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(Graphics &g, Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBorder(Graphics &g, Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(Graphics &g) const { paintBody(g, getLocalBounds()); }
		void paintBorder(Graphics &g) const { paintBorder(g, getLocalBounds()); }

		void paintTabShadow(Graphics &g, Rectangle<int> bounds);
		void paintTabShadow(Graphics &g) { paintTabShadow(g, getLocalBounds()); }
		virtual void paintBackgroundShadow(Graphics &) { }
		void paintKnobShadows(Graphics &g);
		void setScaling(float scale);
		float getComponentShadowWidth() const noexcept { return scaling_ * 2.0f; }

		Rectangle<int> getDividedAreaBuffered(Rectangle<int> full_area, int num_sections, int section, int buffer);
		Rectangle<int> getDividedAreaUnbuffered(Rectangle<int> full_area, int num_sections, int section, int buffer);
		Rectangle<int> getLabelBackgroundBounds(Rectangle<int> bounds, bool text_component = false);
		Rectangle<int> getLabelBackgroundBounds(Component *component, bool text_component = false)
		{ return getLabelBackgroundBounds(component->getBounds(), text_component); }
		
		// main opengl render loop
		virtual void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate = false);
		void destroyAllOpenGlComponents();

		void showPopupSelector(const Component *source, Point<int> position, PopupItems options,
			std::function<void(int)> callback, std::function<void()> cancel = {}) const;
		void showPopupDisplay(Component *source, String text,
			BubbleComponent::BubblePlacement placement, bool primary);
		void hidePopupDisplay(bool primary);

		virtual void setActive(bool active);
		bool isActive() const { return active_; }

		virtual void updateAllValues();
		PowerButton *activator() const { return activator_; }

		virtual void reset();
		virtual void loadFile([[maybe_unused]] const File &file) { }
		virtual File getCurrentFile() { return {}; }

		virtual void addSubSection(BaseSection *section, bool show = true);
		virtual void removeSubSection(BaseSection *section);
		void addControl(BaseControl *control);
		void removeControl(BaseControl *control);
		void addOpenGlComponent(gl_ptr<OpenGlComponent> openGlComponent, bool toBeginning = false);
		void removeOpenGlComponent(OpenGlComponent *openGlComponent);

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
		auto getSectionOverride() const noexcept { return skinOverride_; }
		auto *getParent() const noexcept { return parent_; }

		auto *getControl(nested_enum::NestedEnum auto enumValue) { return controls_.at(enumValue.enum_string(false)); }

		void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
		void setParent(const BaseSection *parent) noexcept { parent_ = parent; }
		void setRenderer(Renderer *renderer) noexcept
		{
			renderer_ = renderer;
			for (auto &subSection : subSections_)
				subSection->setRenderer(renderer_);
		}

		// helper functions
		Renderer *getInterfaceLink() const noexcept { return renderer_; }
		float getValue(Skin::ValueId valueId) const { return renderer_->getSkin()->getValue(this, valueId); }
		Colour getColour(Skin::ColorId colorId) const { return renderer_->getSkin()->getColor(this, colorId); }
		float scaleValue(float value) const noexcept { return scaling_ * value; }
		int scaleValueRoundInt(float value) const noexcept { return (int)std::round(scaling_ * value); }
		// returns the xPosition of the horizontally centered element
		static int centerHorizontally(int xPosition, int elementWidth, int containerWidth) noexcept
		{ return xPosition + (containerWidth - elementWidth) / 2; }
		// returns the yPosition of the vertically centered element
		static int centerVertically(int yPosition, int elementHeight, int containerHeight) noexcept
		{ return yPosition + (containerHeight - elementHeight) / 2; }

	protected:
		void setActivator(PowerButton *activator);
		void createOffOverlay();
		void createBackground();

		virtual Rectangle<int> getPowerButtonBounds() const noexcept
		{
			return { 0, 0, (int)std::round(scaleValue(kDefaultActivatorSize)),
				(int)std::round(scaleValue(kDefaultActivatorSize)) };
		}

		float getDisplayScale() const;

		std::vector<BaseSection *> subSections_{};
		std::vector<gl_ptr<OpenGlComponent>> openGlComponents_{};
		gl_ptr<OpenGlBackground> background_ = nullptr;
		gl_ptr<OffOverlayQuad> offOverlayQuad_ = nullptr;

		std::map<std::string_view, BaseControl *> controls_{};
		std::map<std::string_view, ModulationButton *> modulationButtons_{};

		PowerButton *activator_ = nullptr;

		Renderer *renderer_ = nullptr;
		const BaseSection *parent_ = nullptr;

		Skin::SectionOverride skinOverride_ = Skin::kNone;
		float scaling_ = 1.0f;
		bool active_ = true;
	};

	class ProcessorSection : public BaseSection
	{
	public:
		ProcessorSection(std::string_view name, Generation::BaseProcessor *processor) :
			BaseSection(name), processor_(processor) { }

		[[nodiscard]] std::optional<u64> getProcessorId() const noexcept { return (processor_) ? 
			processor_->getProcessorId() : std::optional<u64>{ std::nullopt }; }
		[[nodiscard]] auto getProcessor() const noexcept { return processor_; }
	protected:
		Generation::BaseProcessor *processor_ = nullptr;
	};
}
