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
#include "Framework/parameter_bridge.h"
#include "OpenGlImageComponent.h"
#include "OpenGlMultiQuad.h"

namespace Interface
{
	class InterfaceEngineLink;
	class BaseSlider;
	class TextSelector;

	struct PopupItems
	{
		std::vector<PopupItems> items_{};
		std::string name_{};
		int id_ = 0;
		bool selected_ = false;
		bool active_ = false;

		PopupItems() = default;
		PopupItems(std::string name) : name_(std::move(name)) { }

		PopupItems(int id, std::string name, bool selected = false, bool active = false)
		{
			name_ = std::move(name);
			id_ = id;
			selected_ = selected;
			active_ = active;
		}

		void addItem(int subId, std::string subName, bool subSelected = false, bool active = false)
		{
			items_.emplace_back(subId, std::move(subName), subSelected, active);
		}

		void addItem(const PopupItems &item) { items_.push_back(item); }
		void addItem(PopupItems &&item) { items_.emplace_back(std::move(item)); }
		int size() const { return (int)items_.size(); }
	};

	class SliderLabel : public Component
	{
	public:
		SliderLabel(BaseSlider *parent) : Component("Label")
		{
			setParent(parent);
			setInterceptsMouseClicks(false, false);
		}

		// call repaintBackground heading and move labelModifier and valueEntry
		void resized() override;
		void paint(Graphics &g) override;

		int getTotalWidth() const;
		auto getLabelText() const noexcept { return labelText_; }
		auto getLabelTextWidth() const noexcept { return usedFont_.getStringWidth(labelText_); }
		auto &getFont() const noexcept { return usedFont_; }

		void setParent(BaseSlider *parent) noexcept;
		void setLabelModifier(TextSelector *labelModifier) noexcept
		{
			labelModifier_ = labelModifier;
			if (valueEntry_)
				valueEntry_->setVisible(false);
		}
		void setValueEntry(OpenGlTextEditor *valueEntry) noexcept
		{
			valueEntry_ = valueEntry;
			if (!labelModifier_)
				valueEntry_->setVisible(true);
		}

	private:
		Font usedFont_{ Fonts::instance()->getInterVFont() };
		String labelText_{};
		BaseSlider *parent_ = nullptr;
		TextSelector *labelModifier_ = nullptr;
		OpenGlTextEditor *valueEntry_ = nullptr;
	};

	class BaseSlider : public Slider, public TextEditor::Listener, public ParameterUI
	{
	public:
		static constexpr float kRotaryAngle = 0.75f * kPi;

		static constexpr float kDefaultTextEntryWidthPercent = 0.9f;
		static constexpr float kTextEntryHeightPercent = 0.6f;

		static constexpr int kDefaultMaxTotalCharacters = 5;
		static constexpr int kDefaultMaxDecimalCharacters = 2;

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
			virtual void hoverStarted([[maybe_unused]] BaseSlider *slider) { }
			virtual void hoverEnded([[maybe_unused]] BaseSlider *slider) { }
			virtual void mouseDown([[maybe_unused]] BaseSlider *slider) { }
			virtual void mouseUp([[maybe_unused]] BaseSlider *slider) { }
			virtual void beginModulationEdit([[maybe_unused]] BaseSlider *slider) { }
			virtual void endModulationEdit([[maybe_unused]] BaseSlider *slider) { }
			virtual void menuFinished([[maybe_unused]] BaseSlider *slider) { }
			virtual void focusLost([[maybe_unused]] BaseSlider *slider) { }
			virtual void doubleClick([[maybe_unused]] BaseSlider *slider) { }
			virtual void modulationsChanged([[maybe_unused]] std::string_view name) { }
			virtual void modulationAmountChanged([[maybe_unused]] BaseSlider *slider) { }
			virtual void modulationRemoved([[maybe_unused]] BaseSlider *slider) { }
			virtual void guiChanged([[maybe_unused]] BaseSlider *slider) { }
		};

		BaseSlider(Framework::ParameterValue *parameter, String name = {});
		~BaseSlider() override = default;

		// Inherited from juce::Slider
		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseDoubleClick(const MouseEvent &e) override;
		double snapValue(double attemptedValue, DragMode dragMode) override;

		String getTextFromValue(double value) override { return getSliderTextFromValue(value); }
		double getValueFromText(const String &text) override;
		String getRawTextFromValue(double value);
		String getSliderTextFromValue(double value, bool retrieveSampleRate = true);

		// override this to define drawing logic for the image component
		void paint(Graphics &) override { }

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
		void parentHierarchyChanged() override;

		// Inherited from juce::TextEditor::Listener
		void textEditorTextChanged([[maybe_unused]] TextEditor &editor) override { textEntry_->redoImage(); }
		void textEditorReturnKeyPressed([[maybe_unused]] TextEditor &editor) override
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
			if (!hasParameterAssignment_)
				return false;

			auto oldValue = getValue();
			auto newValue = getValueSafe();

			bool needsUpdate = oldValue != newValue;
			if (needsUpdate)
				setValue(newValue, NotificationType::sendNotificationSync);

			return needsUpdate;
		}
		void endChange() override;

		virtual void redoImage() = 0;
		virtual void setSliderBounds() = 0;
		virtual void drawShadow(Graphics &) { }
		// override to set the colours of the entry box
		virtual void showTextEntry();

		void updateValueFromParameter() noexcept
		{
			auto value = parameterLink_->parameter->getNormalisedValue();
			setValue(value, sendNotificationSync);
			setValueSafe(value);
		}

		void snapToValue(bool snap, float value = 0.0)
		{
			snapToValue_ = snap;
			snapValue_ = value;
		}

		String formatValue(double value) const noexcept;
		float getNumericTextMaxWidth(const Font &usedFont) const;
		void setDefaultRange();

		void showPopup(bool primary);
		void hidePopup(bool primary);
		void handlePopupResult(int result);

		std::vector<Framework::ParameterBridge *> getMappedParameters() const noexcept;

		void overrideValue(Skin::ValueId valueId, float value) { valueLookup_[valueId] = value; }
		float findValue(Skin::ValueId valueId) const;

		void addSliderListener(SliderListener *listener) { sliderListeners_.push_back(listener); }

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

		float getSampleRate() const noexcept;

	public:
		bool isBipolar() const noexcept { return bipolar_; }
		bool isActive() const noexcept { return active_; }
		float isMouseHovering() const noexcept { return hovering_; }

		bool hasModulationArea() const noexcept { return modulationArea_.getWidth(); }
		bool isUsingImage() const noexcept { return useImage_; }
		bool isUsingQuad() const noexcept { return useQuad_; }
		bool isImageOnTop() const noexcept { return isImageOnTop_; }


		/*float getModulationAmount() const noexcept { return modulationAmount_; }*/
		auto getDrawBox() const noexcept { return drawBox_; }
		auto *getImageComponent() noexcept { return &imageComponent_; }
		auto *getQuadComponent() noexcept { return &sliderQuad_; }
		auto *getLabelComponent() noexcept { return &label_; }
		auto *getTextEditorComponent() noexcept { return textEntry_->getImageComponent(); }
		auto *getSectionParent() noexcept { return parent_; }
		double getSensitivity() const noexcept { return sensitivity_; }
		auto getPopupPlacement() const noexcept { return popupPlacement_; }
		auto getModulationPlacement() const noexcept { return modulationControlPlacement_; }
		auto *getExtraModulationTarget() { return extraModulationTarget_; }
		auto getModulationArea() const noexcept { return (modulationArea_.getWidth()) ? modulationArea_ : getLocalBounds(); }

		virtual Colour getSelectedColor() const { return findColour(Skin::kWidgetPrimary1, true); }
		virtual Colour getUnselectedColor() const { return Colours::transparentBlack; }
		virtual Colour getThumbColor() const { return Colours::white; }
		virtual Colour getBackgroundColor() const { return findColour(Skin::kWidgetBackground, true); }
		virtual Colour getModColor() const { return findColour(Skin::kModulationMeterControl, true); }
		virtual Rectangle<int> getModulationMeterBounds() const { return getLocalBounds(); }


		void setUseImage(bool useImage) noexcept { useImage_ = useImage; imageComponent_.setActive(useImage); }
		void setUseQuad(bool useQuad) noexcept { useQuad_ = useQuad; sliderQuad_.setActive(useQuad); }
		void setIsImageOnTop(bool isImageOnTop) noexcept { isImageOnTop_ = isImageOnTop; }

		void setMaxArc(float arc) { sliderQuad_.setMaxArc(arc); }
		void setAlpha(float alpha, bool reset = false) { sliderQuad_.setAlpha(alpha, reset); }
		void setDrawWhenNotVisible(bool draw) { sliderQuad_.setDrawWhenNotVisible(draw); }
		void setTextEntrySizePercent(float width_percent) { textEntryWidthPercent_ = width_percent; redoImage(); }
		/*void setModulationAmount(float modulation)
		{
			modulationAmount_ = modulation;
			if (modulationAmount_ != 0.0f)
			{
				sliderQuad_.setModColor(modColor_);
				sliderQuad_.setBackgroundColor(backgroundColor_);
			}
			else
			{
				sliderQuad_.setModColor(Colours::transparentBlack);
				sliderQuad_.setBackgroundColor(Colours::transparentBlack);
			}
		}*/

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
		void setShowPopupOnHover(bool show) noexcept { showPopupOnHover_ = show; }
		void setShouldUsePlusMinusPrefix(bool shouldUsePlusMinus) noexcept { shouldUsePlusMinusPrefix_ = shouldUsePlusMinus; }
		void setShouldRepaintOnHover(bool shouldRepaintOnHover) noexcept { shouldRepaintOnHover_ = shouldRepaintOnHover; }

		void setSensitivity(double sensitivity) noexcept { sensitivity_ = sensitivity; }
		void setPopupPlacement(BubbleComponent::BubblePlacement placement) { popupPlacement_ = placement; }
		void setModulationPlacement(BubbleComponent::BubblePlacement placement) { modulationControlPlacement_ = placement; }
		void setMaxTotalCharacters(int totalCharacters) noexcept
		{ COMPLEX_ASSERT(totalCharacters > 0); maxTotalCharacters_ = totalCharacters; }
		void setMaxDecimalCharacters(int decimalCharacters) noexcept
		{ COMPLEX_ASSERT(decimalCharacters > -1);maxDecimalCharacters_ = decimalCharacters; }
		void setPopupPrefix(String prefix) noexcept { popupPrefix_ = std::move(prefix); }


		void setModulationArea(Rectangle<int> area) noexcept { modulationArea_ = area; }
		void setExtraModulationTarget(Component *component) noexcept { extraModulationTarget_ = component; }
		void setValueForDrawingFunction(std::function<double(const BaseSlider *)> function)
		{ getValueForDrawing = std::move(function); }

		void setDrawBox(Rectangle<int> drawBox) { drawBox_ = drawBox; }

	protected:
		Colour thumbColor_;
		Colour selectedColor_;
		Colour unselectedColor_;
		Colour backgroundColor_;
		Colour modColor_;

		bool active_ = true;
		bool bipolar_ = false;
		float textEntryWidthPercent_ = kDefaultTextEntryWidthPercent;

		bool shouldShowPopup_ = false;
		bool showPopupOnHover_ = false;
		bool shouldUsePlusMinusPrefix_ = false;
		bool shouldRepaintOnHover_ = true;
		String popupPrefix_{};

		// TODO: completely remove when doing the modulation manager
		/*float modulationAmount_ = 0.0f;
		bool bipolarModulation_ = false;
		bool stereoModulation_ = false;
		bool bypassModulation_ = false;*/
		Rectangle<int> modulationArea_{};
		bool sensitiveMode_ = false;
		bool snapToValue_ = false;
		bool hovering_ = false;
		bool hasParameterAssignment_ = false;
		float snapValue_ = 0.0f;
		double sensitivity_ = kDefaultSensitivity;
		BubbleComponent::BubblePlacement popupPlacement_ = BubbleComponent::below;
		BubbleComponent::BubblePlacement modulationControlPlacement_ = BubbleComponent::below;
		int maxTotalCharacters_ = kDefaultMaxTotalCharacters;
		int maxDecimalCharacters_ = kDefaultMaxDecimalCharacters;
		std::function<double(BaseSlider *)> getValueForDrawing = [](const BaseSlider *self) { return self->getValue(); };

		Component *extraModulationTarget_ = nullptr;

		Rectangle<int> drawBox_{};
		bool useImage_ = false;
		bool useQuad_ = false;
		bool isImageOnTop_ = true;
		OpenGlQuad sliderQuad_{ Shaders::kRotarySliderFragment };
		OpenGlImageComponent imageComponent_;
		SliderLabel label_{ this };
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
		static constexpr int kDefaultWidthHeight = 36;
		static constexpr int kDefaultArcDimensions = 36;
		static constexpr int kDefaultBodyDimensions = 23;
		static constexpr int kLabelOffset = 6;
		static constexpr int kLabelVerticalPadding = 3;

		RotarySlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name))
		{
			setUseQuad(true);
			sliderQuad_.setMaxArc(kRotaryAngle);
			sliderQuad_.setFragmentShader(Shaders::kRotarySliderFragment);
			label_.setValueEntry(textEntry_.get());
			setRotaryParameters(2.0f * kPi - kRotaryAngle, 2.0f * kPi + kRotaryAngle, true);
		}

		void mouseDrag(const MouseEvent &e) override;

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

		float getKnobSizeScale() const noexcept { return knobSizeScale_; }
		void setKnobSizeScale(float scale) noexcept { knobSizeScale_ = scale; }

	protected:
		float knobSizeScale_ = 1.0f;
	};

	class LinearSlider : public BaseSlider
	{
	public:
		static constexpr float kLinearWidthPercent = 0.26f;
		static constexpr float kLinearModulationPercent = 0.1f;

		LinearSlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name))
		{
			setUseQuad(true);
			sliderQuad_.setFragmentShader(Shaders::kHorizontalSliderFragment);
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

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setSliderBounds() override;
		void showTextEntry() override;

		Rectangle<int> getModulationMeterBounds() const override
		{
			Rectangle<int> mod_bounds = getModulationArea();
			int buffer = (int)findValue(Skin::kWidgetMargin);

			if (getSliderStyle() == LinearBar)
			{
				return { mod_bounds.getX() + buffer, mod_bounds.getY(),
					mod_bounds.getWidth() - 2 * buffer, mod_bounds.getHeight() };
			}
			return { mod_bounds.getX(), mod_bounds.getY() + buffer,
				mod_bounds.getWidth(), mod_bounds.getHeight() - 2 * buffer };
		}

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
		ImageSlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name))
		{ setUseImage(true); }

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

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void paint(Graphics &g) override;
		void redoImage() override { imageComponent_.redrawImage(); }

	};

	class PinSlider : public BaseSlider
	{
	public:
		PinSlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name))
		{
			setUseQuad(true);
			sliderQuad_.setFragmentShader(Shaders::kPinSliderFragment);
			setUseImage(true);
			setShouldShowPopup(true);
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

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
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
			return (active_) ? findColour(Skin::kPinSlider, true) :
				findColour(Skin::kPinSliderDisabled, true);
		}

		Colour getThumbColor() const override
		{
			return (active_) ? findColour(Skin::kPinSliderThumb, true) :
				findColour(Skin::kPinSliderThumbDisabled, true);
		}

	};

	class TextSelector : public BaseSlider
	{
	public:
		static constexpr int kDefaultFontSize = 11;
		static constexpr int kArrowOffset = 4;
		static constexpr int kArrowWidth = 5;
		static constexpr float kArrowWidthHeightRatio = 0.5f;

		class TextSelectorListener
		{
		public:
			virtual ~TextSelectorListener() = default;
			virtual void resizeForText(TextSelector *textSelector, int requestedWidthChange) = 0;
		};

		TextSelector(Framework::ParameterValue *parameter, Font usedFont = {}, String name = {});

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;

		void paint(Graphics &g) override;
		void valueChanged() override;
		void redoImage() override;
		void showTextEntry() override;
		void setSliderBounds() override;

		Rectangle<int> getModulationMeterBounds() const override
		{
			static constexpr int kTextBarSize = 2;
			Rectangle<int> mod_bounds = getModulationArea();
			if (modulationBarRight_)
				return mod_bounds.removeFromRight(kTextBarSize);

			return mod_bounds.removeFromLeft(kTextBarSize);
		}

		// what is this for??
		/*String getTextFromValue(double value) override
		{
			if (hasParameterAssignment_ && popupPrefix_.isEmpty())
				return details_.displayName.data();
			if (!popupPrefix_.isEmpty())
				return popupPrefix_;
			return BaseSlider::getTextFromValue(value);
		}*/

		// if the stringLookup changes we need to resize and redraw to fit the new text
		void setParameterDetails(const Framework::ParameterDetails &details) override
		{
			ParameterUI::setParameterDetails(details);
			resizeForText();
			redoImage();
		}

		bool isModulationBarRight() const noexcept { return modulationBarRight_; }
		auto getTotalDrawWidth() const noexcept { return totalDrawWidth_; }

		void setModulationBarRight(bool right) noexcept { modulationBarRight_ = right; }
		void setUsedFont(Font usedFont) noexcept { usedFont_ = std::move(usedFont); }
		void setDrawArrow(bool drawArrow) noexcept { drawArrow_ = drawArrow; }
		void setTextSelectorListener(TextSelectorListener *listener) noexcept { textSelectorListener_ = listener; }

		// returns the resulting width from the new height
		// use it to set the width of the new bound
		[[nodiscard]] int setHeight(float height) noexcept;

	protected:
		void resizeForText() noexcept;

		Font usedFont_{};
		TextSelectorListener *textSelectorListener_ = nullptr;
		int textWidth_{};
		int totalDrawWidth_{};
		bool drawArrow_ = true;
		bool modulationBarRight_ = true;
	};

	class NumberBox : public BaseSlider
	{
	public:
		static constexpr float kTriangleWidthRatio = 0.5f;
		static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
		static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

		NumberBox(Framework::ParameterValue *parameter, String name = {});

		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;

		void paint(Graphics &g) override;
		void redoImage() override;

		void setSliderBounds() override;

		void showTextEntry() override;

		auto getTotalDrawWidth() const noexcept { return totalDrawWidth_; }

		// returns the resulting width from the new height
		// use it to set the width of the new bound
		[[nodiscard]] int setHeight(float height) noexcept;

		void setAlternativeMode(bool isAlternativeMode) noexcept
		{
			setUseQuad(isAlternativeMode);
			setShouldRepaintOnHover(isAlternativeMode);
			setUseImage(!isAlternativeMode);
			drawBackground_ = !isAlternativeMode;
		}

	protected:
		Font usedFont_{ Fonts::instance()->getDDinFont() };
		int totalDrawWidth_{};
		bool drawBackground_ = true;
	};

	class ModulationSlider : public BaseSlider
	{
	public:
		ModulationSlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name))
		{
			setUseQuad(true);
			sliderQuad_.setFragmentShader(Shaders::kModulationKnobFragment);
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

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setSliderSnapsToMousePosition(false);
			setMouseDragSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setSliderBounds() override
		{
			float radius = 1.0f - 1.0f / (float)drawBox_.getWidth();
			sliderQuad_.setQuad(0, -radius, -radius, 2.0f * radius, 2.0f * radius);
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
