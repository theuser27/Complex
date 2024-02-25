/*
	==============================================================================

		BaseSlider.h
		Created: 14 Dec 2022 6:59:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/Miscellaneous.h"
#include "BaseControl.h"

namespace Framework
{
	class ParameterBridge;
}

namespace Interface
{
	class SliderListener;
	class TextSelectorListener;
	class OpenGlQuad;
	class OpenGlImageComponent;
	class PlainShapeComponent;

	class BaseSlider : public BaseControl, public OpenGlTextEditorListener
	{
	public:
		static constexpr int kDefaultMaxTotalCharacters = 5;
		static constexpr int kDefaultMaxDecimalCharacters = 2;

		static constexpr float kSlowDragMultiplier = 0.1f;
		static constexpr float kDefaultSensitivity = 1.0f;

		enum SliderType : u8
		{
			IsHorizontalDrag = 1,
			Bipolar = 1 << 1,
			CanLoopAround = 1 << 2,
			ShouldMoveOnValueChange = 1 << 3,
		};

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

		BaseSlider(Framework::ParameterValue *parameter);
		~BaseSlider() override;
		
		// ========================================================== Mouse related
		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseDoubleClick(const MouseEvent &e) override;
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		// ========================================================== Value related
		auto getValue() const noexcept { return getValueSafe(); }
		template<CommonConcepts::ParameterRepresentation T>
		T getInternalValue(float sampleRate = kDefaultSampleRate, bool isNormalised = false) const noexcept 
		{ return parameterLink_->parameter->getInternalValue<T>(sampleRate, isNormalised); }
		void setValue(double newValue, NotificationType notification = sendNotificationSync);
		void valueChanged() override
		{
			redoImage();
			notifyGuis();
		}
		Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
			bool getValueFromParameter = true) override;
		void updateValueFromTextEntry();
		String getRawTextFromValue(double value) const;
		double getRawValueFromText(const String &text) const;
		String getSliderTextFromValue(double value, bool retrieveSampleRate = true) const;
		double getSliderValueFromText(const String &text) const;
		double snapValue(double attemptedValue, Slider::DragMode dragMode);
		void snapToValue(bool shouldSnap, float snapValue = 0.0f)
		{
			shouldSnapToValue_ = shouldSnap;
			snapValue_ = snapValue;
		}
		String formatValue(double value) const noexcept;
		float getNumericTextMaxWidth(const Font &usedFont) const;
		void setValueInterval();
		void setResetValue(double resetValue, bool resetWithDoubleClick = true, 
			ModifierKeys mods = ModifierKeys::ctrlAltCommandModifiers) noexcept
		{
			resetValueOnDoubleClick_ = resetWithDoubleClick;
			resetValue_ = resetValue;
			resetValueModifiers_ = mods;
		}

		// ===================================================== Text Entry related
		void addTextEntry();
		void removeTextEntry();
		void changeTextEntryFont(Font font);
		// override to set the colours of the entry box
		virtual void showTextEntry();
		void textEditorReturnKeyPressed([[maybe_unused]] OpenGlTextEditor &editor) override;

		// ========================================================== Popup related
		void showPopup(bool primary);
		void hidePopup(bool primary);
		void handlePopupResult(int result);
	protected:
		PopupItems createPopupMenu() const noexcept;
	public:

		// ======================================================== Drawing related
		bool isVertical() const noexcept { return !isHorizontal(); }
		bool isHorizontal() const noexcept { return type_ & SliderType::IsHorizontalDrag; }
		bool isBipolar() const noexcept { return type_ & SliderType::Bipolar; }
		bool hasModulationArea() const noexcept { return modulationArea_.getWidth(); }

		auto getPopupPlacement() const noexcept { return popupPlacement_; }
		auto getModulationPlacement() const noexcept { return modulationControlPlacement_; }
		auto getModulationArea() const noexcept { return (modulationArea_.getWidth()) ? modulationArea_ : getLocalBounds(); }

		virtual Colour getSelectedColor() const { return getColour(Skin::kWidgetPrimary1); }
		virtual Colour getUnselectedColor() const { return Colours::transparentBlack; }
		virtual Colour getThumbColor() const { return Colours::white; }
		virtual Colour getBackgroundColor() const { return getColour(Skin::kWidgetBackground1); }
		virtual Colour getModColor() const { return getColour(Skin::kModulationMeterControl); }
		virtual Rectangle<int> getModulationMeterBounds() const { return getLocalBounds(); }

		void setBipolar(bool bipolar = true)
		{
			if (isBipolar() == bipolar)
				return;

			type_ = (SliderType)(type_ ^ SliderType::Bipolar);
			redoImage();
		}

		void setActive(bool active = true)
		{
			if (isActive() == active)
				return;

			isActive_ = active;
			setColours();
			redoImage();
		}

		void setColours() override
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
		void setCanInputValue(bool canInputValue) noexcept { canInputValue_ = canInputValue; }
		void setCanUseScrollWheel(bool canUseScrollWheel) noexcept { canUseScrollWheel_ = canUseScrollWheel; }

		void setSensitivity(double sensitivity) noexcept { sensitivity_ = sensitivity; }
		void setPopupPlacement(BubbleComponent::BubblePlacement placement) { popupPlacement_ = placement; }
		void setModulationPlacement(BubbleComponent::BubblePlacement placement) { modulationControlPlacement_ = placement; }
		void setModulationArea(Rectangle<int> area) noexcept { modulationArea_ = area; }
		void setMaxTotalCharacters(int totalCharacters) noexcept
		{ COMPLEX_ASSERT(totalCharacters > 0); maxTotalCharacters_ = totalCharacters; }
		void setMaxDecimalCharacters(int decimalCharacters) noexcept
		{ COMPLEX_ASSERT(decimalCharacters >= 0); maxDecimalCharacters_ = decimalCharacters; }
		void setPopupPrefix(String prefix) noexcept { popupPrefix_ = std::move(prefix); }

		// ========================================================== Miscellaneous
		using BaseControl::getValue;
		void modifierKeysChanged(const ModifierKeys &) override { }
		void addListener(BaseSection *listener) override;
		void removeListener(BaseSection *listener) override;
	protected:
		void notifyModulationAmountChanged();
		void notifyModulationRemoved();
		void notifyModulationsChanged();
		void notifyGuis() noexcept;
		float getSampleRate() const noexcept;
		std::vector<Framework::ParameterBridge *> &getMappedParameters() const noexcept;
		void resetValue() noexcept;
		void setImmediateSensitivity(double immediateSensitivity) noexcept { immediateSensitivity_ = immediateSensitivity; }

		// ============================================================== Variables
		shared_value<Colour> thumbColor_;
		shared_value<Colour> selectedColor_;
		shared_value<Colour> unselectedColor_;
		shared_value<Colour> backgroundColor_;
		shared_value<Colour> modColor_;

		SliderType type_{};
		bool shouldShowPopup_ = false;
		bool showPopupOnHover_ = false;
		bool shouldUsePlusMinusPrefix_ = false;
		bool canInputValue_ = false;
		bool canUseScrollWheel_ = false;
		bool shouldSnapToValue_ = false;
		bool sensitiveMode_ = false;

		double snapValue_ = 0.0;
		double sensitivity_ = kDefaultSensitivity;
		int maxTotalCharacters_ = kDefaultMaxTotalCharacters;
		int maxDecimalCharacters_ = kDefaultMaxDecimalCharacters;

		BubbleComponent::BubblePlacement popupPlacement_ = BubbleComponent::below;
		String popupPrefix_{};

		Rectangle<int> modulationArea_{};
		BubbleComponent::BubblePlacement modulationControlPlacement_ = BubbleComponent::below;

		gl_ptr<OpenGlQuad> quadComponent_ = nullptr;
		gl_ptr<OpenGlImageComponent> imageComponent_ = nullptr;
		std::unique_ptr<OpenGlTextEditor> textEntry_ = nullptr;

		std::vector<SliderListener *> sliderListeners_{};

		// ============================================================== Internals
	private:
		double immediateSensitivity_ = 250.0;
		double valueInterval_ = 0.0;
		double valueOnMouseDown_ = 0.0;
		Point<float> mouseDragStartPosition_{};
		Time lastMouseWheelTime_;
		double resetValue_ = 0.0;
		ModifierKeys resetValueModifiers_;
		bool resetValueOnDoubleClick_ = true;
		bool useDragEvents_ = false;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseSlider)
	};

	class TextSelector;

	class RotarySlider final : public BaseSlider
	{
	public:
		static constexpr float kRotaryAngle = 0.75f * kPi;
		static constexpr double kDefaultRotaryDragLength = 200.0;
		static constexpr int kDefaultWidthHeight = 36;
		static constexpr int kDefaultArcDimensions = 36;
		static constexpr int kDefaultBodyDimensions = 23;
		static constexpr int kLabelOffset = 6;
		static constexpr int kLabelVerticalPadding = 3;

		RotarySlider(Framework::ParameterValue *parameter);
		~RotarySlider() override;

		void mouseDrag(const MouseEvent &e) override;

		void redoImage() override;
		void setComponentsBounds() override;
		void drawShadow(Graphics &g) const;
		void showTextEntry() override;
		void setExtraElementsPositions(Rectangle<int> anchorBounds) override;
		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;

		Colour getUnselectedColor() const override
		{
			if (isActive())
				return getColour(Skin::kRotaryArcUnselected);
			return getColour(Skin::kRotaryArcUnselectedDisabled);
		}
		Colour getThumbColor() const override { return getColour(Skin::kRotaryHand); }

		float getKnobSizeScale() const noexcept { return knobSizeScale_; }
		void setKnobSizeScale(float scale) noexcept { knobSizeScale_ = scale; }
		void setMaxArc(float arc) noexcept;

		void setModifier(TextSelector *modifier) noexcept;

	protected:
		shared_value<float> knobArcThickness_{};
		float knobSizeScale_ = 1.0f;
		TextSelector *modifier_ = nullptr;
	};

	class LinearSlider final : public BaseSlider
	{
	public:
		static constexpr float kLinearWidthPercent = 0.26f;
		static constexpr float kLinearModulationPercent = 0.1f;

		LinearSlider(Framework::ParameterValue *parameter);

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setComponentsBounds() override;
		void showTextEntry() override;

		Rectangle<int> getModulationMeterBounds() const override
		{
			Rectangle<int> mod_bounds = getModulationArea();
			int buffer = (int)getValue(Skin::kWidgetMargin);

			if (isHorizontal())
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

		void setHorizontal() { type_ = (SliderType)(type_ | SliderType::IsHorizontalDrag); }
		void setVertical() { type_ = (SliderType)(type_ ^ (type_ & SliderType::IsHorizontalDrag)); }
	};

	class ImageSlider final : public BaseSlider
	{
	public:
		ImageSlider(Framework::ParameterValue *parameter);

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
	};

	class PinSlider : public BaseSlider
	{
	public:
		static constexpr int kDefaultPinSliderWidth = 10;

		PinSlider(Framework::ParameterValue *parameter);

		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;

		void redoImage() override;
		void setComponentsBounds() override;
		void setExtraElementsPositions([[maybe_unused]] Rectangle<int> anchorBounds) override { }
		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;

		Colour getThumbColor() const override { return getSelectedColor(); }

		void setTotalRange(double totalRange) noexcept { totalRange_ = totalRange; }

	private:
		double totalRange_ = 0.0;
		Point<double> lastDragPosition_{ 0.0, 0.0 };
		double runningTotal_ = 0.0;
	};

	class TextSelector final : public BaseSlider
	{
	public:
		static constexpr int kDefaultTextSelectorHeight = 16;
		static constexpr int kLabelOffset = 8;

		static constexpr float kMarginsHeightRatio = 0.25f;
		static constexpr float kHeightToArrowWidthRatio = 5.0f / 16.0f;
		static constexpr float kArrowWidthHeightRatio = 0.5f;

		TextSelector(Framework::ParameterValue *parameter, std::optional<Font> usedFont = {});

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;

		void valueChanged() override
		{
			resizeForText();
			BaseSlider::valueChanged();
		}
		void redoImage() override;
		void setComponentsBounds() override;
		void setExtraElementsPositions(Rectangle<int> anchorBounds) override;
		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;

		void addListener(BaseSection *listener) override;
		void removeListener(BaseSection *listener) override;

		Colour getBackgroundColor() const override { return getColour(Skin::kWidgetBackground2); }

		// if the stringLookup changes we need to resize and redraw to fit the new text
		void setParameterDetails(const Framework::ParameterDetails &details) override
		{
			BaseControl::setParameterDetails(details);
			resizeForText();
			redoImage();
		}

		void setUsedFont(Font usedFont) noexcept { usedFont_ = std::move(usedFont); isDirty_ = true; }
		void setDrawArrow(bool drawArrow) noexcept { drawArrow_ = drawArrow; isDirty_ = true; }

		void setExtraIcon(PlainShapeComponent *icon) noexcept;
		void setTextSelectorListener(TextSelectorListener *listener) noexcept { textSelectorListener_ = listener; }

	protected:
		void resizeForText() noexcept;

		Font usedFont_{};
		int textWidth_{};
		bool drawArrow_ = true;
		bool isDirty_ = false;

		PlainShapeComponent *extraIcon_ = nullptr;
		TextSelectorListener *textSelectorListener_ = nullptr;
	};

	class NumberBox final : public BaseSlider
	{
	public:
		static constexpr int kDefaultNumberBoxHeight = 16;
		static constexpr int kLabelOffset = 4;

		static constexpr float kTriangleWidthRatio = 0.5f;
		static constexpr float kTriangleToValueMarginRatio = 2.0f / 16.0f;
		static constexpr float kValueToEndMarginRatio = 5.0f / 16.0f;

		NumberBox(Framework::ParameterValue *parameter);

		void mouseDrag(const MouseEvent &e) override;
		void setVisible(bool shouldBeVisible) override;

		void textEditorReturnKeyPressed(OpenGlTextEditor &editor) override;
		void textEditorEscapeKeyPressed(OpenGlTextEditor &editor) override;
		void textEditorFocusLost(OpenGlTextEditor &editor) override { textEditorEscapeKeyPressed(editor); }

		void redoImage() override;
		void setComponentsBounds() override;
		void showTextEntry() override;
		void setExtraElementsPositions(Rectangle<int> anchorBounds) override;

		Colour getBackgroundColor() const override { return (drawBackground_) ? 
			getColour(Skin::kWidgetBackground1) : getColour(Skin::kWidgetBackground2); }

		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;

		void setAlternativeMode(bool isAlternativeMode) noexcept;

	protected:
		bool drawBackground_ = true;
		bool isEditing_ = false;
	};

	class ModulationSlider final : public BaseSlider
	{
	public:
		ModulationSlider(Framework::ParameterValue *parameter);

		void mouseDown(const MouseEvent &e) override
		{
			if (!e.mods.isAltDown() && !e.mods.isPopupMenu())
			{
				setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / sensitivity_));
			}

			BaseSlider::mouseDown(e);
		}

		void mouseDrag(const MouseEvent &e) override
		{
			float multiply = 1.0f;

			sensitiveMode_ = e.mods.isCommandDown();
			if (sensitiveMode_)
				multiply *= kSlowDragMultiplier;

			setImmediateSensitivity((int)((double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply)));

			BaseSlider::mouseDrag(e);
		}

		void redoImage() override;
		void setComponentsBounds() override;


		Colour getSelectedColor() const override
		{
			Colour background = getColour(Skin::kWidgetBackground1);
			if (isActive_)
				return getColour(Skin::kRotaryArc).interpolatedWith(background, 0.5f);
			return getColour(Skin::kRotaryArcDisabled).interpolatedWith(background, 0.5f);
		}
		Colour getUnselectedColor() const override { return getColour(Skin::kWidgetBackground1); }
		Colour getThumbColor() const override { return getColour(Skin::kRotaryArc); }

		void setDrawWhenNotVisible(bool draw) noexcept;
	};

}
