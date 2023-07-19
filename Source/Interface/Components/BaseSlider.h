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
	class BaseSlider;
	class TextSelector;

	class SliderLabel : public Component
	{
	public:
		SliderLabel() : Component("Slider Label") { setInterceptsMouseClicks(false, false); }

		// call repaintBackground heading and move labelModifier and valueEntry
		void resized() override;
		void paint(Graphics &g) override;

		int getTotalWidth() const;
		auto getLabelText() const noexcept { return labelText_; }
		auto getLabelTextWidth() const noexcept { return usedFont_.getStringWidth(labelText_); }
		auto &getFont() const noexcept { return usedFont_; }

		void setParent(BaseSlider *parent) noexcept;
		void setFont(Font font) noexcept { usedFont_ = std::move(font); }
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

		void setTextJustification(Justification justification) noexcept { textJustification_ = justification; }

	private:
		Font usedFont_{ Fonts::instance()->getInterVFont() };
		String labelText_{};
		Justification textJustification_ = Justification::centred;
		BaseSlider *parent_ = nullptr;
		TextSelector *labelModifier_ = nullptr;
		OpenGlTextEditor *valueEntry_ = nullptr;
	};

	class BaseSlider : public Slider, public TextEditor::Listener, public ParameterUI
	{
	public:
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

		BaseSlider(Framework::ParameterValue *parameter, String name = {}, SliderStyle style = LinearVertical);
		~BaseSlider() override = default;

		Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue *parameter, 
			bool getValueFromParameter = true);

		// Inherited from juce::Slider
		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseDoubleClick(const MouseEvent &e) override;
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;
		double snapValue(double attemptedValue, DragMode dragMode) override;

		String getTextFromValue(double value) override { return getSliderTextFromValue(value); }
		double getValueFromText(const String &text) override;
		String getRawTextFromValue(double value);
		String getSliderTextFromValue(double value, bool retrieveSampleRate = true);

		// override this to define drawing logic for the image component
		void paint(Graphics &) override { }

		void resized() override
		{
			areDrawBoundsSet_ = drawBounds_.getWidth() * drawBounds_.getHeight() != 0;
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
		void textEditorReturnKeyPressed([[maybe_unused]] TextEditor &editor) override;

		// Inherited from ParameterUI
		bool updateValue() override;
		void endChange() override;

		virtual void redoImage() = 0;
		virtual void setSliderBounds() = 0;
		virtual void drawShadow(Graphics &) { }
		// override to set the colours of the entry box
		virtual void showTextEntry();

		void updateValueFromParameter() noexcept
		{
			auto value = parameterLink_->parameter->getNormalisedValue();
			setValueSafe(value);
			setValue(value, sendNotificationSync);
		}

		void updateValueFromTextEntry()
		{
			if (!textEntry_ || textEntry_->getText().isEmpty())
				return;
			
			auto value = getValueFromText(textEntry_->getText());
			setValueSafe(value);
			setValue(value, NotificationType::sendNotificationSync);
			if (auto hostControl = parameterLink_->hostControl)
				hostControl->setValueFromUI((float)value);
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

		std::vector<Framework::ParameterBridge *> &getMappedParameters() const noexcept;

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
		bool isBipolar() const noexcept { return isBipolar_; }
		bool isActive() const noexcept { return isActive_; }

		bool hasModulationArea() const noexcept { return modulationArea_.getWidth(); }
		bool isUsingImage() const noexcept { return useImage_; }
		bool isUsingQuad() const noexcept { return useQuad_; }
		bool isImageOnTop() const noexcept { return isImageOnTop_; }


		/*float getModulationAmount() const noexcept { return modulationAmount_; }*/
		auto getDrawBox() const noexcept { return drawBounds_; }
		auto *getImageComponent() noexcept { return &imageComponent_; }
		auto *getQuadComponent() noexcept { return &sliderQuad_; }
		auto *getLabelComponent() noexcept { return &label_; }
		auto *getTextEditorComponent() noexcept { return textEntry_->getImageComponent(); }
		auto *getSectionParent() noexcept { return parent_; }
		double getSensitivity() const noexcept { return sensitivity_; }
		auto getPopupPlacement() const noexcept { return popupPlacement_; }
		auto getModulationPlacement() const noexcept { return modulationControlPlacement_; }
		auto getModulationArea() const noexcept { return (modulationArea_.getWidth()) ? modulationArea_ : getLocalBounds(); }

		Colour getColour(Skin::ColorId colourId) const noexcept;
		virtual Colour getSelectedColor() const { return getColour(Skin::kWidgetPrimary1); }
		virtual Colour getUnselectedColor() const { return Colours::transparentBlack; }
		virtual Colour getThumbColor() const { return Colours::white; }
		virtual Colour getBackgroundColor() const { return getColour(Skin::kWidgetBackground1); }
		virtual Colour getModColor() const { return getColour(Skin::kModulationMeterControl); }
		virtual Rectangle<int> getModulationMeterBounds() const { return getLocalBounds(); }


		void setUseImage(bool useImage) noexcept { useImage_ = useImage; imageComponent_.setActive(useImage); }
		void setUseQuad(bool useQuad) noexcept { useQuad_ = useQuad; sliderQuad_.setActive(useQuad); }
		void setIsImageOnTop(bool isImageOnTop) noexcept { isImageOnTop_ = isImageOnTop; }

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
			if (isBipolar_ == bipolar)
				return;

			isBipolar_ = bipolar;
			redoImage();
		}

		void setActive(bool active = true)
		{
			if (isActive_ == active)
				return;

			isActive_ = active;
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
		void setCanInputValue(bool canInputValue) noexcept { canInputValue_ = canInputValue; }

		void setSensitivity(double sensitivity) noexcept { sensitivity_ = sensitivity; }
		void setPopupPlacement(BubbleComponent::BubblePlacement placement) { popupPlacement_ = placement; }
		void setModulationPlacement(BubbleComponent::BubblePlacement placement) { modulationControlPlacement_ = placement; }
		void setMaxTotalCharacters(int totalCharacters) noexcept
		{ COMPLEX_ASSERT(totalCharacters > 0); maxTotalCharacters_ = totalCharacters; }
		void setMaxDecimalCharacters(int decimalCharacters) noexcept
		{ COMPLEX_ASSERT(decimalCharacters > -1);maxDecimalCharacters_ = decimalCharacters; }
		void setPopupPrefix(String prefix) noexcept { popupPrefix_ = std::move(prefix); }


		void setModulationArea(Rectangle<int> area) noexcept { modulationArea_ = area; }
		void setBoundsAndDrawBounds(Rectangle<int> bounds, Rectangle<int> drawBounds)
		{
			drawBounds_ = drawBounds;
			setBounds(bounds);
		}

	protected:
		Colour thumbColor_;
		Colour selectedColor_;
		Colour unselectedColor_;
		Colour backgroundColor_;
		Colour modColor_;

		bool isActive_ = true;
		bool isBipolar_ = false;
		bool shouldShowPopup_ = false;
		bool showPopupOnHover_ = false;
		bool shouldUsePlusMinusPrefix_ = false;
		bool shouldRepaintOnHover_ = true;
		bool canInputValue_ = true;
		bool areDrawBoundsSet_ = false;

		// TODO: completely remove when doing the modulation manager
		/*float modulationAmount_ = 0.0f;
		bool bipolarModulation_ = false;
		bool stereoModulation_ = false;
		bool bypassModulation_ = false;*/
		Rectangle<int> modulationArea_{};
		bool sensitiveMode_ = false;
		bool snapToValue_ = false;
		float snapValue_ = 0.0f;
		double sensitivity_ = kDefaultSensitivity;
		BubbleComponent::BubblePlacement popupPlacement_ = BubbleComponent::below;
		BubbleComponent::BubblePlacement modulationControlPlacement_ = BubbleComponent::below;
		int maxTotalCharacters_ = kDefaultMaxTotalCharacters;
		int maxDecimalCharacters_ = kDefaultMaxDecimalCharacters;
		String popupPrefix_{};

		Rectangle<int> drawBounds_{};
		bool useImage_ = false;
		bool useQuad_ = false;
		bool isImageOnTop_ = true;
		OpenGlQuad sliderQuad_{ Shaders::kRotarySliderFragment };
		OpenGlImageComponent imageComponent_;
		std::unique_ptr<OpenGlTextEditor> textEntry_ = nullptr;
		SliderLabel label_{};

		BaseSection *parent_ = nullptr;
		std::vector<SliderListener *> sliderListeners_{};

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseSlider)
	};

	class TextSelector;

	class RotarySlider : public BaseSlider
	{
	public:
		static constexpr float kRotaryAngle = 0.75f * kPi;
		static constexpr double kDefaultRotaryDragLength = 200.0;
		static constexpr int kDefaultWidthHeight = 36;
		static constexpr int kDefaultArcDimensions = 36;
		static constexpr int kDefaultBodyDimensions = 23;
		static constexpr int kLabelOffset = 6;
		static constexpr int kLabelVerticalPadding = 3;

		RotarySlider(Framework::ParameterValue *parameter, String name = {});

		void mouseDrag(const MouseEvent &e) override;

		void redoImage() override;
		void setSliderBounds() override;
		void drawShadow(Graphics &g) override;
		void showTextEntry() override;

		Colour getUnselectedColor() const override
		{
			if (isActive_)
				return getColour(Skin::kRotaryArcUnselected);
			return getColour(Skin::kRotaryArcUnselectedDisabled);
		}
		Colour getThumbColor() const override { return getColour(Skin::kRotaryHand); }

		float getKnobSizeScale() const noexcept { return knobSizeScale_; }
		void setKnobSizeScale(float scale) noexcept { knobSizeScale_ = scale; }
		void setMaxArc(float arc) { sliderQuad_.setMaxArc(arc); }

		void addModifier(TextSelector *modifier) noexcept
		{
			if (modifier)
			{
				isValueShown_ = false;
				label_.setLabelModifier(modifier);
			}
			else
			{
				isValueShown_ = true;
				label_.setLabelModifier(nullptr);
				label_.setValueEntry(textEntry_.get());
			}
		}

	protected:
		float knobSizeScale_ = 1.0f;
		bool isValueShown_ = true;
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
			if (isActive_)
				return getColour(Skin::kLinearSlider);
			return getColour(Skin::kLinearSliderDisabled);
		}
		Colour getUnselectedColor() const override { return getColour(Skin::kLinearSliderUnselected); }
		Colour getThumbColor() const override
		{
			if (isActive_)
				return getColour(Skin::kLinearSliderThumb);
			return getColour(Skin::kLinearSliderThumbDisabled);
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
		static constexpr int kDefaultPinSliderWidth = 10;

		PinSlider(Framework::ParameterValue *parameter, String name = {});

		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;

		void paint(Graphics &g) override;
		void redoImage() override;
		void setSliderBounds() override;

		Colour getThumbColor() const override { return getSelectedColor(); }

		void setTotalRange(double totalRange) noexcept { totalRange_ = totalRange; }

	private:
		double totalRange_ = 0.0;
		double lastDragPosition_ = 0.0;
		double runningTotal_ = 0.0;
	};

	class TextSelector : public BaseSlider
	{
	public:
		static constexpr int kDefaultTextSelectorHeight = 16;

		static constexpr float kArrowOffsetRatio = 0.25f;
		static constexpr float kHeightToArrowWidthRatio = 5.0f / 16.0f;
		static constexpr float kArrowWidthHeightRatio = 0.5f;

		class TextSelectorListener
		{
		public:
			virtual ~TextSelectorListener() = default;
			virtual void resizeForText(TextSelector *textSelector, int requestedWidthChange) = 0;
		};

		TextSelector(Framework::ParameterValue *parameter, std::optional<Font> usedFont = {}, String name = {});

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		void paint(Graphics &g) override;
		void valueChanged() override
		{
			resizeForText();
			BaseSlider::valueChanged();
		}
		void redoImage() override;
		void setSliderBounds() override;

		Colour getBackgroundColor() const override { return getColour(Skin::kWidgetBackground2); }

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

		void setModulationBarRight(bool right) noexcept { modulationBarRight_ = right; isDirty_ = true; }
		void setUsedFont(Font usedFont) noexcept { usedFont_ = std::move(usedFont); isDirty_ = true; }
		void setDrawArrow(bool drawArrow) noexcept { drawArrow_ = drawArrow; isDirty_ = true; }

		void setTextSelectorListener(TextSelectorListener *listener) noexcept { textSelectorListener_ = listener; }

		// returns the resulting width from the new height
		// use it to set the width of the new bound
		[[nodiscard]] int setHeight(int height) noexcept;

	protected:
		void resizeForText() noexcept;

		Font usedFont_{};
		TextSelectorListener *textSelectorListener_ = nullptr;
		int textWidth_{};
		int totalDrawWidth_{};
		bool drawArrow_ = true;
		bool modulationBarRight_ = true;
		bool isDirty_ = false;
	};

	class NumberBox : public BaseSlider
	{
	public:
		static constexpr int kDefaultNumberBoxHeight = 16;

		static constexpr float kTriangleWidthRatio = 0.5f;
		static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
		static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

		NumberBox(Framework::ParameterValue *parameter, String name = {});

		void mouseDrag(const MouseEvent &e) override;
		void paint(Graphics &g) override;
		void setVisible(bool shouldBeVisible) override;

		void textEditorReturnKeyPressed(TextEditor &editor) override;
		void textEditorEscapeKeyPressed(TextEditor &editor) override;
		void textEditorFocusLost(TextEditor &editor) override { textEditorEscapeKeyPressed(editor); }

		void redoImage() override;
		void setSliderBounds() override;
		void showTextEntry() override;

		Colour getBackgroundColor() const override { return (drawBackground_) ? 
			getColour(Skin::kWidgetBackground1) : getColour(Skin::kWidgetBackground2); }

		auto getTotalDrawWidth() const noexcept { return totalDrawWidth_; }

		// returns the resulting width from the new height
		// use it to set the width of the new bound
		[[nodiscard]] int setHeight(int height) noexcept;

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
		bool isEditing_ = false;
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
			float radius = 1.0f - 1.0f / (float)drawBounds_.getWidth();
			sliderQuad_.setQuad(0, -radius, -radius, 2.0f * radius, 2.0f * radius);
		}


		Colour getSelectedColor() const override
		{
			Colour background = getColour(Skin::kWidgetBackground1);
			if (isActive_)
				return getColour(Skin::kRotaryArc).interpolatedWith(background, 0.5f);
			return getColour(Skin::kRotaryArcDisabled).interpolatedWith(background, 0.5f);
		}
		Colour getUnselectedColor() const override { return getColour(Skin::kWidgetBackground1); }
		Colour getThumbColor() const override { return getColour(Skin::kRotaryArc); }

		void setAlpha(float alpha, bool reset = false) { sliderQuad_.setAlpha(alpha, reset); }
		void setDrawWhenNotVisible(bool draw) { sliderQuad_.setDrawWhenNotVisible(draw); }
	};

}
