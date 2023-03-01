/*
	==============================================================================

		BaseSection.h
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Framework/utils.h"
#include "Generation/BaseProcessor.h"

#include "../LookAndFeel/Skin.h"
#include "../Components/OpenGLMultiQuad.h"
#include "../Components/BaseButton.h"

namespace Interface
{
	class OpenGlComponent;
	class BaseSlider;
	class ModulationButton;
	class PresetSelector;

	struct PopupItems
	{
		std::vector<PopupItems> items{};
		std::string name{};
		int id = 0;
		bool selected = false;
		bool active = false;

		PopupItems() = default;
		PopupItems(std::string name) : name(std::move(name)) { }

		PopupItems(int id, std::string name, bool selected = false, bool active = false)
		{
			this->name = std::move(name);
			this->id = id;
			this->selected = selected;
			this->active = active;
		}

		void addItem(int subId, std::string subName, bool subSelected = false, bool active = false)
		{ items.emplace_back(subId, std::move(subName), subSelected, active); }

		void addItem(const PopupItems &item) { items.push_back(item); }
		void addItem(PopupItems &&item) { items.emplace_back(std::move(item)); }
		u32 size() const { return (u32)items.size(); }
	};

	class BaseSection : public juce::Component, public Slider::Listener, public Button::Listener,
		public BaseButton::ButtonListener, public utils::Downcastable
	{
	public:
		static constexpr int kDefaultPowerButtonOffset = 0;
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

		class OffOverlay : public OpenGlQuad
		{
		public:
			OffOverlay() : OpenGlQuad(Shaders::kColorFragment) { }

			void paint(Graphics &g) override { }
		};

		BaseSection(std::string_view name);
		~BaseSection() override = default;

		void setParent(const BaseSection *parent) noexcept { parent_ = parent; }
		float findValue(Skin::ValueId valueId) const
		{
			if (valueLookup_.contains(valueId))
			{
				if (Skin::shouldScaleValue(valueId))
					return size_ratio_ * valueLookup_.at(valueId);
				return valueLookup_.at(valueId);
			}
			if (parent_)
				return parent_->findValue(valueId);

			return 0.0f;
		}

		void resized() override;
		void paint(Graphics &g) override { }

		// paint anything that doesn't move/is static
		virtual void paintBackground(Graphics &g);
		virtual void repaintBackground();

		virtual void setSkinValues(const Skin &skin, bool topLevel);
		void setSkinOverride(Skin::SectionOverride skinOverride) { skinOverride_ = skinOverride; }

		void showPopupSelector(Component *source, Point<int> position, const PopupItems &options,
			std::function<void(int)> callback, std::function<void()> cancel = { });
		void showPopupDisplay(Component *source, const std::string &text,
			BubbleComponent::BubblePlacement placement, bool primary);
		void hidePopupDisplay(bool primary);

		Path getRoundedPath(Rectangle<float> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(Graphics &g, Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBorder(Graphics &g, Rectangle<int> bounds, float topRounding = 0.0f, float bottomRounding = 0.0f) const;
		void paintBody(Graphics &g) const { paintBody(g, getLocalBounds()); }
		void paintBorder(Graphics &g) const { paintBorder(g, getLocalBounds()); }

		void paintTabShadow(Graphics &g, Rectangle<int> bounds);
		void paintTabShadow(Graphics &g) { paintTabShadow(g, getLocalBounds()); }
		virtual void paintBackgroundShadow(Graphics &g) { }
		void paintKnobShadows(Graphics &g);
		virtual void setSizeRatio(float ratio);
		int getComponentShadowWidth() const noexcept { return std::round(size_ratio_ * 2.0f); }

		Font getLabelFont() const noexcept;
		void setLabelFont(Graphics &g);
		void drawLabelConnectionForComponents(Graphics &g, Component *left, Component *right);
		void drawLabelBackground(Graphics &g, Rectangle<int> bounds, bool text_component = false);
		void drawLabelBackgroundForComponent(Graphics &g, Component *component) { drawLabelBackground(g, component->getBounds()); }
		Rectangle<int> getDividedAreaBuffered(Rectangle<int> full_area, int num_sections, int section, int buffer);
		Rectangle<int> getDividedAreaUnbuffered(Rectangle<int> full_area, int num_sections, int section, int buffer);
		Rectangle<int> getLabelBackgroundBounds(Rectangle<int> bounds, bool text_component = false);
		Rectangle<int> getLabelBackgroundBounds(Component *component, bool text_component = false)
		{
			return getLabelBackgroundBounds(component->getBounds(), text_component);
		}
		void drawLabel(Graphics &g, String text, Rectangle<int> component_bounds, bool text_component = false);
		void drawLabelForComponent(Graphics &g, String text, Component *component, bool text_component = false)
		{
			drawLabel(g, std::move(text), component->getBounds(), text_component);
		}
		void drawTextBelowComponent(Graphics &g, String text, Component *component, int space, int padding = 0);

		void paintChildrenBackgrounds(Graphics &g);
		void paintOpenGlChildrenBackgrounds(Graphics &g);
		void paintChildBackground(Graphics &g, BaseSection *child);
		void paintChildShadow(Graphics &g, BaseSection *child);
		void paintChildrenShadows(Graphics &g);
		void paintOpenGlBackground(Graphics &g, OpenGlComponent *openGlComponent);
		void drawTextComponentBackground(Graphics &g, Rectangle<int> bounds, bool extend_to_label);
		virtual void initOpenGlComponents(OpenGlWrapper &open_gl);
		virtual void renderOpenGlComponents(OpenGlWrapper &open_gl, bool animate);
		virtual void destroyOpenGlComponents(OpenGlWrapper &open_gl);

		void sliderValueChanged(Slider *movedSlider) override { }
		void buttonClicked(Button *clickedButton) override { }
		void guiChanged(BaseButton* button) override;

		virtual void setActive(bool active);
		bool isActive() const { return active_; }
		virtual void animate(bool animate)
		{
			for (auto &sub_section : subSections_)
				sub_section->animate(animate);
		}

		void updateAllValues();
		GeneralButton *activator() const { return activator_; }

		virtual void reset();
		virtual void loadFile(const File &file) {}
		virtual File getCurrentFile() { return {}; }

		void addSubSection(BaseSection *section, bool show = true);
		void removeSubSection(BaseSection *section);

		float getPadding() const noexcept { return findValue(Skin::kPadding); }
		float getPowerButtonOffset() const noexcept { return size_ratio_ * kDefaultPowerButtonOffset; }
		float getKnobSectionHeight() const noexcept { return findValue(Skin::kKnobSectionHeight); }
		float getSliderWidth() const noexcept { return findValue(Skin::kSliderWidth); }
		float getSliderOverlap() const noexcept;
		float getSliderOverlapWithSpace() const noexcept { return getSliderOverlap() - std::truncf(getWidgetMargin()); }
		float getTextComponentHeight() const noexcept { return findValue(Skin::kTextComponentHeight); }
		float getStandardKnobSize() const noexcept { return findValue(Skin::kKnobArcSize); }
		float getTotalKnobHeight() const noexcept { return getStandardKnobSize(); }
		float getTextSectionYOffset() const noexcept { return (getKnobSectionHeight() - getTextComponentHeight()) / 2.0f; }
		float getModButtonWidth() const noexcept { return findValue(Skin::kModulationButtonWidth); }
		float getModFontSize() const noexcept { return findValue(Skin::kModulationFontSize); }
		float getWidgetMargin() const noexcept { return findValue(Skin::kWidgetMargin); }
		float getWidgetRounding() const noexcept { return findValue(Skin::kWidgetMargin); }
		float getSizeRatio() const noexcept { return size_ratio_; }
		int getPopupWidth() const noexcept { return (int)((float)kDefaultPopupMenuWidth * size_ratio_); }

		void setSkinValues(std::map<Skin::ValueId, float> values) { valueLookup_ = std::move(values); }
		void setSkinValue(Skin::ValueId id, float value) { valueLookup_[id] = value; }

	protected:
		void addButton(BaseButton *button, bool show = true);
		void addSlider(BaseSlider *slider, bool show = true, bool listen = true);
		void addOpenGlComponent(OpenGlComponent *openGlComponent, bool toBeginning = false);
		void setActivator(GeneralButton *activator);
		void createOffOverlay();
		void paintJointControlBackground(Graphics &g, int x, int y, int width, int height);
		void paintJointControl(Graphics &g, int x, int y, int width, int height, const std::string &name);
		void placeJointControls(int x, int y, int width, int height, BaseSlider *left, BaseSlider *right, Component *widget = nullptr);
		void placeRotaryOption(Component *option, BaseSlider *rotary);
		void placeKnobsInArea(Rectangle<int> area, std::vector<Component *> knobs);

		Rectangle<int> getPowerButtonBounds() const noexcept;
		float getDisplayScale() const;
		virtual int getPixelMultiple() const { return (parent_) ? parent_->getPixelMultiple() : 1; }

		std::map<Skin::ValueId, float> valueLookup_{};

		std::vector<BaseSection *> subSections_{};
		std::vector<OpenGlComponent *> openGlComponents_{};

		std::map<std::string_view, BaseSlider *> sliders_{};
		std::map<std::string_view, BaseButton *> buttons_{};
		std::map<std::string_view, ModulationButton *> modulationButtons_{};

		const BaseSection *parent_ = nullptr;
		GeneralButton *activator_ = nullptr;

		std::unique_ptr<OffOverlay> off_overlay_ = nullptr;

		Skin::SectionOverride skinOverride_ = Skin::kNone;
		float size_ratio_ = 1.0f;
		bool active_ = true;
	};
}

REFL_AUTO(type(Interface::BaseSection))
