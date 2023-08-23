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
#include "BaseControl.h"

namespace Interface
{
	class BaseButton;

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

		void paint(Graphics &) override { }

		void init(OpenGlWrapper &openGl) override
		{
			if (style_ == kRadioButton)
				background_.setFragmentShader(Shaders::kRoundedRectangleFragment);

			background_.setParent(parent_);
			background_.init(openGl);
			text_.setParent(parent_);
			text_.init(openGl);
			shape_.setParent(parent_);
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

		void destroy() override
		{
			background_.destroy();
			text_.destroy();
			shape_.destroy();
		}

		void setColors();
		void setText() noexcept;
		void setJustification(Justification justification) noexcept { text_.setJustification(justification); }
		void setShape(std::pair<Path, Path> strokeFillShapes) { shape_.setShapes(std::move(strokeFillShapes)); }
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

	class BaseButton : public ToggleButton, public BaseControl
	{
	public:
		static constexpr int kLabelOffset = 8;

		enum MenuId
		{
			kCancel = 0,
			kArmMidiLearn,
			kClearMidiLearn
		};

		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void guiChanged([[maybe_unused]] BaseButton *button) { }
		};

		BaseButton(Framework::ParameterValue *parameter, String name = {});
		~BaseButton() override = default;

		// Inherited via ToggleButton
		void parentHierarchyChanged() override
		{
			parent_ = findParentComponentOfClass<BaseSection>();
			ToggleButton::parentHierarchyChanged();
		}
		void resized() override;
		void enablementChanged() override
		{
			ToggleButton::enablementChanged();
			buttonComponent_->setColors();
		}
		void clicked() override;

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override
		{
			ToggleButton::mouseEnter(e);
			buttonComponent_->getAnimator().setIsHovered(true);
		}
		void mouseExit(const MouseEvent &e) override
		{
			ToggleButton::mouseExit(e);
			buttonComponent_->getAnimator().setIsHovered(false);
		}

		// Inherited via BaseControl
		double getValueInternal() const noexcept override { return getToggleState(); }
		void setValueInternal(double value, NotificationType notification) noexcept override 
		{ setToggleState(std::round(value) == 1.0f, notification); }
		void setBoundsInternal(Rectangle<int> bounds) override { setBounds(bounds); }
		[[nodiscard]] Rectangle<int> getOverallBoundsForHeight(int height) override;
		void positionExtraElements(Rectangle<int> anchorBounds) override;
		void refresh() override { resized(); }

		auto getGlComponent() noexcept { return buttonComponent_; }

		void setText(const String &newText)
		{
			setButtonText(newText);
			buttonComponent_->setText();
		}

		void setShape(std::pair<Path, Path> strokeFillShapes) { buttonComponent_->setShape(std::move(strokeFillShapes)); }
		void setAccentedButton(bool isAccented) noexcept { buttonComponent_->setAccentedButton(isAccented); }
		void setJustification(Justification justification) noexcept { buttonComponent_->setJustification(justification); }

		void setPowerButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kPowerButton); setShape(Paths::powerButtonIcon()); }
		void setRadioButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kRadioButton); addLabel(); }
		void setNoBackground() noexcept { buttonComponent_->setStyle(ButtonComponent::kJustText); }
		void setTextButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kTextButton); }
		void setLightenButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kLightenButton); }
		void setUiButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kActionButton); }
		void setShapeButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kShapeButton); }

		std::span<const std::string_view> getStringLookup() const { return details_.stringLookup; }
		void setStringLookup(std::span<const std::string_view> lookup) { details_.stringLookup = lookup; }

		String getTextFromValue(bool value) const noexcept;
		void handlePopupResult(int result);

		void addListener(BaseSection *listener) override;
		void removeListener(BaseSection *listener) override;

	protected:
		void clicked(const ModifierKeys &modifiers) override;

		gl_ptr<ButtonComponent> buttonComponent_;

		std::vector<Listener *> buttonListeners_{};

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
	};

}
