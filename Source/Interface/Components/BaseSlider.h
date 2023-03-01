/*
	==============================================================================

		PluginSlider.h
		Created: 14 Dec 2022 6:59:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Framework/parameter_value.h"
#include "../LookAndFeel/CurveLookAndFeel.h"
#include "../LookAndFeel/TextLookAndFeel.h"
#include "OpenGlImageComponent.h"
#include "OpenGlMultiQuad.h"
#include "../Sections/BaseSection.h"

namespace Interface
{
	class InterfaceEngineLink;

	class BaseSlider : public Slider, public TextEditor::Listener, public ParameterUI
	{
	public:
		static constexpr float kRotaryAngle = 0.75f * kPi;

		static constexpr float kDefaultTextEntryWidthPercent = 0.9f;
		static constexpr float kTextEntryHeightPercent = 0.6f;

		static constexpr int kDefaultFormatLength = 5;
		static constexpr int kDefaultFormatDecimalPlaces = 5;

		static constexpr float kSlowDragMultiplier = 0.1f;
		static constexpr float kDefaultSensitivity = 1.0f;

		enum MenuId
		{
			kCancel = 0,
			kArmMidiLearn,
			kClearMidiLearn,
			kDefaultValue,
			kManualEntry,
			kClearModulations,
			kMapFirstSlot,
			kMappingList,
			kClearMapping
		};

		class SliderListener
		{
		public:
			virtual ~SliderListener() = default;
			virtual void hoverStarted(BaseSlider *slider) { }
			virtual void hoverEnded(BaseSlider *slider) { }
			virtual void mouseDown(BaseSlider *slider) { }
			virtual void mouseUp(BaseSlider *slider) { }
			virtual void beginModulationEdit(BaseSlider *slider) { }
			virtual void endModulationEdit(BaseSlider *slider) { }
			virtual void menuFinished(BaseSlider *slider) { }
			virtual void focusLost(BaseSlider *slider) { }
			virtual void doubleClick(BaseSlider *slider) { }
			virtual void modulationsChanged(std::string_view name) { }
			virtual void modulationAmountChanged(BaseSlider *slider) { }
			virtual void modulationRemoved(BaseSlider *slider) { }
			virtual void guiChanged(BaseSlider *slider) { }
		};

		BaseSlider(String name);
		~BaseSlider() override = default;

		// Inherited from juce::Slider
		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseDoubleClick(const MouseEvent &e) override;
		double snapValue(double attemptedValue, DragMode dragMode) override;

		String getTextFromValue(double value) override;
		double getValueFromText(const String &text) override;
		String getRawTextFromValue(double value);
		String getSliderTextFromValue(double value);

		// override this to define drawing logic for the image component
		void paint(Graphics &g) override { }

		void resized() override
		{
			Slider::resized();
			setColors();
			setSliderBounds();
		}

		void valueChanged() override
		{
			Slider::valueChanged();
			redoImage();
			notifyGuis();
		}

		// Inherited from juce::Component
		void focusLost(FocusChangeType cause) override;
		void parentHierarchyChanged() override
		{
			interfaceEngineLink_ = findParentComponentOfClass<InterfaceEngineLink>();
			parent_ = findParentComponentOfClass<BaseSection>();
			Slider::parentHierarchyChanged();
		}

		// Inherited from juce::TextEditor::Listener
		void textEditorTextChanged(TextEditor &) override { textEntry_->redoImage(); }
		void textEditorReturnKeyPressed(TextEditor &editor) override
		{
			if (textEntry_ && !textEntry_->getText().isEmpty())
			{
				auto value = getValueFromText(textEntry_->getText());
				setValue(value, NotificationType::sendNotificationSync);
				if (auto hostControl = parameterLink_->hostControl)
					hostControl->setValueFromUI((float)value);
			}
				
			textEntry_->setVisible(false);

			for (SliderListener *listener : sliderListeners_)
				listener->menuFinished(this);
		}

		// Inherited from ParameterUI
		bool updateValue() override
		{
			if (!has_parameter_assignment_)
				return false;

			bool needsUpdate = getValue() != getValueSafe();
			if (needsUpdate)
			{
				setValue(getValueSafe(), NotificationType::dontSendNotification);
				valueChanged();
			}
			return needsUpdate;
		}

		void setValueFromUI(double newValue) noexcept
		{
			setValueSafe(newValue);
			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->setValueFromUI((float)newValue);
		}

		virtual void redoImage() = 0;
		virtual void setSliderBounds() = 0;
		virtual void drawShadow(Graphics &g) { }
		virtual void showTextEntry();

		void snapToValue(bool snap, float value = 0.0)
		{
			snap_to_value_ = snap;
			snap_value_ = value;
		}

		String formatValue(double value) const noexcept;

		void setDefaultRange();

		void showPopup(bool primary);
		void hidePopup(bool primary);
		void handlePopupResult(int result);

		std::vector<Framework::ParameterBridge *> getMappedParameters() const noexcept;

		void overrideValue(Skin::ValueId valueId, float value) { valueLookup_[valueId] = value; }
		float findValue(Skin::ValueId valueId) const
		{
			if (valueLookup_.contains(valueId))
				return valueLookup_.at(valueId);
			if (parent_)
				return parent_->findValue(valueId);
			return 0.0f;
		}

		void addSliderListener(SliderListener *listener) noexcept
		{
			sliderListeners_.push_back(listener);
		}

	protected:
		PopupItems createPopupMenu() const noexcept;

		void notifyModulationAmountChanged()
		{
			for (SliderListener *listener : sliderListeners_)
				listener->modulationAmountChanged(this);
		}

		void notifyModulationRemoved()
		{
			for (SliderListener *listener : sliderListeners_)
				listener->modulationRemoved(this);
		}

		void notifyModulationsChanged()
		{
			for (SliderListener *listener : sliderListeners_)
				listener->modulationsChanged(getName().toRawUTF8());
		}

		void notifyGuis() noexcept
		{
			for (SliderListener *listener : sliderListeners_)
				listener->guiChanged(this);
		}



	public:
		bool isText() const { return &getLookAndFeel() == TextLookAndFeel::instance(); }
		bool isTextOrCurve() const { return isText() || &getLookAndFeel() == CurveLookAndFeel::instance(); }
		bool isBipolar() const noexcept { return bipolar_; }
		bool isActive() const noexcept { return active_; }
		bool isModulationBipolar() const noexcept { return bipolar_modulation_; }
		bool isModulationStereo() const noexcept { return stereo_modulation_; }
		bool isModulationBypassed() const noexcept { return bypass_modulation_; }
		float isMouseHovering() const noexcept { return hovering_; }
		bool isModulationBarRight() const noexcept { return modulation_bar_right_; }
		bool hasModulationArea() const noexcept { return modulation_area_.getWidth(); }
		bool isUsingImage() const noexcept { return useImage_; }
		bool isUsingQuad() const noexcept { return useQuad_; }
		bool isImageOnTop() const noexcept { return isImageOnTop_; }


		float getKnobSizeScale() const { return knobSizeScale_; }
		float getTextHeightPercentage() const noexcept { return textHeightPercentage_; }
		float getModulationAmount() const { return modulationAmount_; }
		auto *getImageComponent() { return &image_component_; }
		auto *getQuadComponent() { return &slider_quad_; }
		auto *getTextEditorComponent() { return textEntry_->getImageComponent(); }
		auto *getSectionParent() { return parent_; }
		double getSensitivity() const noexcept { return sensitivity_; }
		auto getPopupPlacement() const noexcept { return popup_placement_; }
		auto getModulationPlacement() const noexcept { return modulation_control_placement_; }
		Rectangle<int> getModulationMeterBounds() const;
		auto *getExtraModulationTarget() { return extra_modulation_target_; }
		auto getModulationArea() const noexcept { return (modulation_area_.getWidth()) ? modulation_area_ : getLocalBounds(); }

		virtual Colour getSelectedColor() const = 0;
		virtual Colour getUnselectedColor() const { return Colours::transparentBlack; };
		virtual Colour getThumbColor() const = 0;
		virtual Colour getBackgroundColor() const { return findColour(Skin::kWidgetBackground, true); }
		virtual Colour getModColor() const { return findColour(Skin::kModulationMeterControl, true); }


		void setUseImage(bool useImage) noexcept { useImage_ = useImage; image_component_.setActive(true); }
		void setUseQuad(bool useQuad) noexcept { useQuad_ = useQuad; slider_quad_.setActive(true); }
		void setIsImageOnTop(bool isImageOnTop) noexcept { isImageOnTop_ = isImageOnTop; }
		void setKnobSizeScale(float scale) noexcept { knobSizeScale_ = scale; }
		void setTextHeightPercentage(float percentage) noexcept { textHeightPercentage_ = percentage; }
		void setMaxArc(float arc) { slider_quad_.setMaxArc(arc); }
		void setAlpha(float alpha, bool reset = false) { slider_quad_.setAlpha(alpha, reset); }
		void setDrawWhenNotVisible(bool draw) { slider_quad_.setDrawWhenNotVisible(draw); }
		void setTextEntrySizePercent(float width_percent) { textEntryWidthPercent_ = width_percent; redoImage(); }
		void setModulationAmount(float modulation)
		{
			modulationAmount_ = modulation;
			if (modulationAmount_ != 0.0f)
			{
				slider_quad_.setModColor(modColor_);
				slider_quad_.setBackgroundColor(backgroundColor_);
			}
			else
			{
				slider_quad_.setModColor(Colours::transparentBlack);
				slider_quad_.setBackgroundColor(Colours::transparentBlack);
			}
		}

		void setBipolar(bool bipolar = true)
		{
			if (bipolar_ == bipolar)
				return;

			bipolar_ = bipolar;
			redoImage();
		}

		void setActive(bool active = true)
		{
			if (active_ == active)
				return;

			active_ = active;
			setColors();
			redoImage();
		}

		void setColors()
		{
			if (getWidth() <= 0)
				return;

			thumbColor_ = getThumbColor();
			selectedColor_ = getSelectedColor();
			unselectedColor_ = getUnselectedColor();
			backgroundColor_ = getBackgroundColor();
			modColor_ = getModColor();
		}

		void setShouldShowPopup(bool shouldShowPopup) noexcept { shouldShowPopup_ = shouldShowPopup; }
		void setSensitivity(double sensitivity) noexcept { sensitivity_ = sensitivity; }
		void setPopupPlacement(BubbleComponent::BubblePlacement placement) { popup_placement_ = placement; }
		void setModulationPlacement(BubbleComponent::BubblePlacement placement) { modulation_control_placement_ = placement; }
		void setMaxDisplayCharacters(int characters) noexcept { max_display_characters_ = characters; }
		void setMaxDecimalPlaces(int decimal_places) noexcept { max_decimal_places_ = decimal_places; }
		void setShowPopupOnHover(bool show) noexcept { show_popup_on_hover_ = show; }
		void setPopupPrefix(String prefix) noexcept { popup_prefix_ = std::move(prefix); }
		void setModulationBarRight(bool right) noexcept { modulation_bar_right_ = right; }
		void setModulationArea(Rectangle<int> area) noexcept { modulation_area_ = area; }
		void setExtraModulationTarget(Component *component) noexcept { extra_modulation_target_ = component; }
		void useSuffix(bool use) noexcept { use_suffix_ = use; }


	protected:
		Colour thumbColor_;
		Colour selectedColor_;
		Colour unselectedColor_;
		Colour backgroundColor_;
		Colour modColor_;

		float knobSizeScale_ = 1.0f;
		float modulationAmount_ = 0.0f;
		bool active_ = true;
		bool bipolar_ = false;
		float textHeightPercentage_ = 0.0f;
		float textEntryWidthPercent_ = kDefaultTextEntryWidthPercent;

		bool shouldShowPopup_ = true;
		bool show_popup_on_hover_ = false;
		String popup_prefix_{};

		bool bipolar_modulation_ = false;
		bool stereo_modulation_ = false;
		bool bypass_modulation_ = false;
		bool modulation_bar_right_ = true;
		Rectangle<int> modulation_area_{};
		bool sensitive_mode_ = false;
		bool snap_to_value_ = false;
		bool hovering_ = false;
		bool has_parameter_assignment_ = false;
		bool use_suffix_ = true;
		float snap_value_ = 0.0f;
		double sensitivity_ = kDefaultSensitivity;
		BubbleComponent::BubblePlacement popup_placement_ = BubbleComponent::below;
		BubbleComponent::BubblePlacement modulation_control_placement_ = BubbleComponent::below;
		int max_display_characters_ = kDefaultFormatLength;
		int max_decimal_places_ = kDefaultFormatDecimalPlaces;

		Component *extra_modulation_target_ = nullptr;
		InterfaceEngineLink *interfaceEngineLink_ = nullptr;

		bool useImage_ = false;
		bool useQuad_ = false;
		bool isImageOnTop_ = true;
		OpenGlQuad slider_quad_{ Shaders::kRotarySliderFragment };
		OpenGlImageComponent image_component_;
		std::unique_ptr<OpenGlTextEditor> textEntry_ = nullptr;

		std::map<Skin::ValueId, float> valueLookup_{};

		BaseSection *parent_ = nullptr;
		std::vector<SliderListener *> sliderListeners_{};

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseSlider)
	};

	class RotarySlider : public BaseSlider
	{
	public:
		static constexpr double kDefaultRotaryDragLength = 200.0;

		RotarySlider(String name) : BaseSlider(std::move(name))
		{
			setUseQuad(true);
			slider_quad_.setFragmentShader(Shaders::kRotarySliderFragment);
		}

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
				setMouseDragSensitivity((int)(kDefaultRotaryDragLength / sensitivity_));

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;
			setDefaultRange();

			sensitive_mode_ = e.mods.isCommandDown();
			if (sensitive_mode_)
				multiply *= kSlowDragMultiplier;

			setMouseDragSensitivity((int)(kDefaultRotaryDragLength / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setSliderBounds() override;
		void drawShadow(Graphics &g) override;
		void showTextEntry() override;

		Colour getSelectedColor() const override
		{
			if (active_)
				return findColour(Skin::kRotaryArc, true);
			return findColour(Skin::kRotaryArcDisabled, true);
		}
		Colour getUnselectedColor() const override
		{
			if (active_)
				return findColour(Skin::kRotaryArcUnselected, true);
			return findColour(Skin::kRotaryArcUnselectedDisabled, true);
		}
		Colour getThumbColor() const override
		{ return findColour(Skin::kRotaryHand, true); }

	};

	class LinearSlider : public BaseSlider
	{
	public:
		static constexpr float kLinearWidthPercent = 0.26f;
		static constexpr float kLinearHandlePercent = 1.2f;
		static constexpr float kLinearModulationPercent = 0.1f;

		LinearSlider(String name) : BaseSlider(std::move(name))
		{
			setUseQuad(true);
			slider_quad_.setFragmentShader(Shaders::kHorizontalSliderFragment);
		}

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setSliderSnapsToMousePosition(false);
				setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;
			setDefaultRange();

			sensitive_mode_ = e.mods.isCommandDown();
			if (sensitive_mode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setSliderBounds() override;
		void showTextEntry() override;

		Colour getSelectedColor() const override
		{
			if (active_)
				return findColour(Skin::kLinearSlider, true);
			return findColour(Skin::kLinearSliderDisabled, true);
		}
		Colour getUnselectedColor() const override { return findColour(Skin::kLinearSliderUnselected, true); }
		Colour getThumbColor() const override
		{
			if (active_)
				return findColour(Skin::kLinearSliderThumb, true);
			return findColour(Skin::kLinearSliderThumbDisabled, true);
		}

		void setHorizontal() { setSliderStyle(Slider::LinearBar); }
		void setVertical() { setSliderStyle(Slider::LinearBarVertical); }

	};

	class ImageSlider : public BaseSlider
	{
	public:
		ImageSlider(String name) : BaseSlider(std::move(name)) { setUseImage(true); }

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setSliderSnapsToMousePosition(false);
				setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;
			setDefaultRange();

			sensitive_mode_ = e.mods.isCommandDown();
			if (sensitive_mode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void paint(Graphics &g) override;
		void redoImage() override { image_component_.redrawImage(); }

	};

	class PinSlider : public BaseSlider
	{
	public:
		PinSlider(String name) : BaseSlider(std::move(name))
		{
			setUseQuad(true);
			slider_quad_.setFragmentShader(Shaders::kPinSliderFragment);
			setUseImage(true);
		}

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setSliderSnapsToMousePosition(false);
				setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;
			setDefaultRange();

			sensitive_mode_ = e.mods.isCommandDown();
			if (sensitive_mode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void paint(Graphics &g) override;
		void redoImage() override;
		void setSliderBounds() override;


		Colour getSelectedColor() const override
		{
			if (active_)
				return findColour(Skin::kPinSlider, true);
			return findColour(Skin::kPinSliderDisabled, true);
		}

		Colour getThumbColor() const override
		{
			if (active_)
				return findColour(Skin::kPinThumb, true);
			return findColour(Skin::kPinThumbDisabled, true);
		}

	};

	class ModulationSlider : public BaseSlider
	{
	public:
		ModulationSlider(String name) : BaseSlider(std::move(name))
		{
			setUseQuad(true);
			slider_quad_.setFragmentShader(Shaders::kModulationKnobFragment);
		}

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setSliderSnapsToMousePosition(false);
				setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;
			setDefaultRange();

			sensitive_mode_ = e.mods.isCommandDown();
			if (sensitive_mode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setSliderBounds() override
		{
			float radius = 1.0f - 1.0f / (float)getWidth();
			slider_quad_.setQuad(0, -radius, -radius, 2.0f * radius, 2.0f * radius);
		}


		Colour getSelectedColor() const override
		{
			Colour background = findColour(Skin::kWidgetBackground, true);
			if (active_)
				return findColour(Skin::kRotaryArc, true).interpolatedWith(background, 0.5f);
			return findColour(Skin::kRotaryArcDisabled, true).interpolatedWith(background, 0.5f);
		}
		Colour getUnselectedColor() const override { return findColour(Skin::kWidgetBackground, true); }
		Colour getThumbColor() const override { return findColour(Skin::kRotaryArc, true); }

	};

}
