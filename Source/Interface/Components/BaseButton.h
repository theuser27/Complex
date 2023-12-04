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
			kOptionsButton,
			kShapeButton
		};

		ButtonComponent(BaseButton *button);

		void paint(Graphics &) override { }

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
		void renderOptionsButton(OpenGlWrapper &openGl, bool animate);
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
			else if (style_ == kOptionsButton)
				renderOptionsButton(openGl, animate);
			else if (style_ == kShapeButton || style_ == kPowerButton)
				renderShapeButton(openGl, animate);
		}

		void destroy() override
		{
			background_.destroy();
			text_.destroy();
			shape_.destroy();
		}

		void setParent(BaseSection *parent) noexcept override
		{
			parent_ = parent;
			background_.setParent(parent_);
			text_.setParent(parent_);
			shape_.setParent(parent_);
		}
		void setColors();
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

	class BaseButton : public BaseControl
	{
	public:
		static constexpr int kLabelOffset = 8;
		static constexpr float kHoverIncrement = 0.1f;

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
			virtual void buttonClicked(BaseButton *) = 0;
			virtual void guiChanged([[maybe_unused]] BaseButton *button) { }
		};

		BaseButton(Framework::ParameterValue *parameter);

		void parentHierarchyChanged() override { parent_ = findParentComponentOfClass<BaseSection>(); }
		//void resized() override;
		void setExtraElementsPositions([[maybe_unused]] Rectangle<int> anchorBounds) override { }
		void enablementChanged() override
		{
			updateState(isMouseOver(true), isMouseButtonDown());
			setColours();
		}

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &) override
		{
			updateState(false, true);
			for (auto &component : components_)
				component->getAnimator().setIsHovered(true);
		}
		void mouseExit(const MouseEvent &) override
		{
			updateState(false, false);
			for (auto &component : components_)
				component->getAnimator().setIsHovered(false);
		}

		bool getToggleState() const noexcept { return std::round(getValueSafe()) == 1.0; }
		void setToggleState(bool shouldBeOn, NotificationType notification);
		void valueChanged() override;

		bool isHeldDown() const noexcept { return isHeldDown_; }
		bool isHoveredOver() const noexcept { return isHoveredOver_; }

		//void setAccentedButton(bool isAccented) noexcept { buttonComponent_->setAccentedButton(isAccented); }
		//void setJustification(Justification justification) noexcept { buttonComponent_->setJustification(justification); }

		//void setNoBackground() noexcept { buttonComponent_->setStyle(ButtonComponent::kJustText); }
		//void setTextButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kTextButton); }
		//void setActionButton() noexcept { buttonComponent_->setStyle(ButtonComponent::kActionButton); }

		std::span<const std::string_view> getStringLookup() const { return details_.stringLookup; }
		void setStringLookup(std::span<const std::string_view> lookup) { details_.stringLookup = lookup; }

		String getTextFromValue(bool value) const noexcept;
		void handlePopupResult(int result);

		void addListener(BaseSection *listener) override;
		void removeListener(BaseSection *listener) override;

	protected:
		void updateState(bool isHeldDown, bool isHoveredOver) noexcept
		{
			if (isEnabled() && isVisible() && !isCurrentlyBlockedByAnotherModalComponent())
			{
				isHeldDown_ = isHeldDown;
				isHoveredOver_ = isHoveredOver;
			}
		}

		//gl_ptr<ButtonComponent> buttonComponent_;

		std::vector<Listener *> buttonListeners_{};

		bool isHeldDown_ = false;
		bool isHoveredOver_ = false;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
	};

	class PowerButton final : public BaseButton
	{
	public:
		static constexpr int kAddedMargin = 4;

		PowerButton(Framework::ParameterValue *parameter);

		void setColours() override;
		Rectangle<int> getBoundsForSizes(int height, int width = 0) override;
		void redoImage() override;
		void setComponentsBounds() override;

	private:
		gl_ptr<PlainShapeComponent> shapeComponent_ = nullptr;

		Colour onNormalColor_{};

		Colour activeColour_{};
		Colour hoverColour_{};
	};

	class RadioButton final : public BaseButton
	{
	public:
		static constexpr int kAddedMargin = 4;

		RadioButton(Framework::ParameterValue *parameter);

		void setColours() override;
		Rectangle<int> getBoundsForSizes(int height, int width = 0) override;
		void setExtraElementsPositions(Rectangle<int> anchorBounds) override;
		void redoImage() override;
		void setComponentsBounds() override;

		void setRounding(float rounding) noexcept { backgroundComponent_->setRounding(rounding); }

	private:
		gl_ptr<OpenGlQuad> backgroundComponent_ = nullptr;

		Colour onNormalColor_{};
		Colour offNormalColor_{};
		Colour backgroundColor_{};
	};

	class OptionsButton final : public BaseButton
	{
	public:
		static constexpr float kPlusRelativeSize = 7;
		static constexpr float kBorderRounding = 8.0f;

		OptionsButton(Framework::ParameterValue *parameter, String name = {}, String displayText = {});

		void setColours() override;
		Rectangle<int> getBoundsForSizes(int height, int width) override;
		void redoImage() override;
		void setComponentsBounds() override;

		void setText(String text) { text_ = std::move(text); }

	protected:
		gl_ptr<OpenGlQuad> plusComponent_ = nullptr;
		gl_ptr<OpenGlQuad> borderComponent_ = nullptr;
		gl_ptr<PlainTextComponent> textComponent_ = nullptr;

		String text_{};

		Colour borderColour_{};
	};

}
