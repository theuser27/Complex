/*
	==============================================================================

		PluginButton.h
		Created: 14 Dec 2022 7:01:32am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/parameter_value.h"
#include "OpenGlImageComponent.h"
#include "OpenGlMultiQuad.h"
#include "Interface/LookAndFeel/Paths.h"

namespace Interface
{
	class BaseButton;

	class ButtonLabel : public Component
	{
	public:
		ButtonLabel(BaseButton *parent) : Component("Button Label")
		{
			setParent(parent);
			setInterceptsMouseClicks(false, false);
		}

		void paint(Graphics &g) override;

		int getTotalWidth() const { return (int)std::round(usedFont_.getStringWidthFloat(labelText_)); }
		auto getLabelText() const noexcept { return labelText_; }
		auto getLabelTextWidth() const noexcept { return usedFont_.getStringWidth(labelText_); }
		auto &getFont() const noexcept { return usedFont_; }

		void setParent(BaseButton *parent) noexcept;
		void setFont(Font font) noexcept { usedFont_ = std::move(font); }

	private:
		Font usedFont_{ Fonts::instance()->getInterVFont() };
		String labelText_{};
		BaseButton *parent_ = nullptr;
	};

	class ButtonComponent : public OpenGlComponent
	{
	public:
		static constexpr float kHoverIncrement = 0.1f;

		enum ButtonStyle
		{
			kTextButton,
			kJustText,
			kPowerButton,
			kRadioButton,
			kActionButton,
			kLightenButton,
			kShapeButton
		};

		ButtonComponent(BaseButton *button);

		void paint([[maybe_unused]] Graphics &g) override { }

		void init(OpenGlWrapper &openGl) override
		{
			if (style_ == kRadioButton)
				background_.setFragmentShader(Shaders::kRoundedRectangleFragment);

			background_.init(openGl);
			text_.init(openGl);
			shape_.init(openGl);

			setColors();
		}

		void renderTextButton(OpenGlWrapper &openGl, bool animate);
		void renderRadioButton(OpenGlWrapper &openGl, bool animate);
		void renderActionButton(OpenGlWrapper &openGl, bool animate);
		void renderLightenButton(OpenGlWrapper &openGl, bool animate);
		void renderShapeButton(OpenGlWrapper &openGl, bool animate);

		void render(OpenGlWrapper &openGl, bool animate) override
		{
			animator_.tick(animate);

			if (style_ == kTextButton || style_ == kJustText)
				renderTextButton(openGl, animate);
			else if (style_ == kRadioButton)
				renderRadioButton(openGl, animate);
			else if (style_ == kActionButton)
				renderActionButton(openGl, animate);
			else if (style_ == kLightenButton)
				renderLightenButton(openGl, animate);
			else if (style_ == kShapeButton || style_ == kPowerButton)
				renderShapeButton(openGl, animate);
		}

		void destroy(OpenGlWrapper &openGl) override
		{
			background_.destroy(openGl);
			text_.destroy(openGl);
			shape_.destroy(openGl);
		}

		void setColors();
		void setText() noexcept;
		void setJustification(Justification justification) noexcept { text_.setJustification(justification); }
		void setShape(const Path &shape) { shape_.setShape(shape); }
		void setDown(bool down) noexcept { down_ = down; }
		void setStyle(ButtonStyle style) noexcept { style_ = style; }
		void setAccentedButton(bool isButtonAccented) noexcept { isButtonAccented_ = isButtonAccented; }

		auto &background() noexcept { return background_; }
		auto &shape() noexcept { return shape_; }
		auto &text() noexcept { return text_; }
		ButtonStyle style() const noexcept { return style_; }

	protected:
		ButtonStyle style_ = kTextButton;
		BaseButton *button_ = nullptr;
		bool isButtonAccented_ = false;
		bool down_ = false;
		OpenGlQuad background_{ Shaders::kRoundedRectangleFragment };
		PlainShapeComponent shape_{ "shape" };
		PlainTextComponent text_{ "text", "" };

		Colour onNormalColor_{};
		Colour onHoverColor_{};
		Colour onDownColor_{};
		Colour offNormalColor_{};
		Colour offHoverColor_{};
		Colour offDownColor_{};

		Colour backgroundColor_{};
		Colour bodyColor_{};

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonComponent)
	};

	class BaseButton : public ToggleButton, public ParameterUI
	{
	public:
		enum MenuId
		{
			kCancel = 0,
			kArmMidiLearn,
			kClearMidiLearn
		};

		class ButtonListener
		{
		public:
			virtual ~ButtonListener() = default;
			virtual void guiChanged([[maybe_unused]] BaseButton *button) { }
		};

		BaseButton(Framework::ParameterValue *parameter, String name = {});
		~BaseButton() override = default;

		// Inherited via ToggleButton
		void parentHierarchyChanged() override;
		void resized() override;
		void enablementChanged() override
		{
			ToggleButton::enablementChanged();
			buttonComponent_.setColors();
		}
		void clicked() override;

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override
		{
			ToggleButton::mouseEnter(e);
			buttonComponent_.getAnimator().setIsHovered(true);
		}
		void mouseExit(const MouseEvent &e) override
		{
			ToggleButton::mouseExit(e);
			buttonComponent_.getAnimator().setIsHovered(false);
		}

		// Inherited via ParameterUI
		bool updateValue() override
		{
			bool newValue = (bool)std::round(getValueSafe());
			bool needsUpdate = getToggleState() != newValue;
			if (needsUpdate)
				setToggleState(newValue, NotificationType::dontSendNotification);
			return needsUpdate;
		}

		void endChange() override;

		void updateValueFromParameter()
		{
			auto value = (bool)std::round(parameterLink_->parameter->getNormalisedValue());
			setToggleState(value, sendNotificationSync);
			setValueSafe(value);
		}

		auto *getGlComponent() noexcept { return &buttonComponent_; }
		auto *getLabelComponent() const noexcept { return label_.get(); }
		Colour getColour(Skin::ColorId colourId) const noexcept;

		void setText(const String &newText)
		{
			setButtonText(newText);
			buttonComponent_.setText();
		}

		void setShape(const Path &shape) { buttonComponent_.setShape(shape); }
		void setAccentedButton(bool isAccented) noexcept { buttonComponent_.setAccentedButton(isAccented); }
		void setJustification(Justification justification) noexcept { buttonComponent_.setJustification(justification); }

		void setPowerButton() { buttonComponent_.setStyle(ButtonComponent::kPowerButton); setShape(Paths::powerButtonIcon()); }
		void setRadioButton() noexcept { buttonComponent_.setStyle(ButtonComponent::kRadioButton); }
		void setNoBackground() noexcept { buttonComponent_.setStyle(ButtonComponent::kJustText); }
		void setTextButton() noexcept { buttonComponent_.setStyle(ButtonComponent::kTextButton); }
		void setLightenButton() noexcept { buttonComponent_.setStyle(ButtonComponent::kLightenButton); }
		void setUiButton() noexcept { buttonComponent_.setStyle(ButtonComponent::kActionButton); }
		void setShapeButton() noexcept { buttonComponent_.setStyle(ButtonComponent::kShapeButton); }

		std::span<const std::string_view> getStringLookup() const { return details_.stringLookup; }
		void setStringLookup(std::span<const std::string_view> lookup) { details_.stringLookup = lookup; }

		String getTextFromValue(bool value) const noexcept;
		void handlePopupResult(int result);
		void createLabel() { label_ = std::make_unique<ButtonLabel>(this); }

		void addButtonListener(ButtonListener *listener) { buttonListeners_.push_back(listener); }

	protected:
		void clicked(const ModifierKeys &modifiers) override;

		ButtonComponent buttonComponent_{ this };
		std::unique_ptr<ButtonLabel> label_ = nullptr;

		BaseSection *parent_ = nullptr;
		std::vector<ButtonListener *> buttonListeners_{};

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
	};

}
