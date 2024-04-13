/*
	==============================================================================

		PluginButton.h
		Created: 14 Dec 2022 7:01:32am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "BaseControl.h"

namespace Interface
{
	class BaseButton;
	class PlainShapeComponent;
	class OpenGlQuad;

	/*class ButtonComponent : public OpenGlComponent
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

		void setParentSection(BaseSection *parent) noexcept override
		{
			parentSection_ = parent;
			background_.setParentSection(parentSection_);
			text_.setParentSection(parentSection_);
			shape_.setParentSection(parentSection_);
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
	};*/

	class ButtonListener;

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

		BaseButton(Framework::ParameterValue *parameter);
		~BaseButton() override;

		//void resized() override;
		void setExtraElementsPositions([[maybe_unused]] Rectangle<int> anchorBounds) override { }
		void enablementChanged() override
		{
			updateState(isMouseOver(true), isMouseButtonDown());
			setColours();
		}

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseEnter(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;

		bool getToggleState() const noexcept { return std::round(getValueSafe()) == 1.0; }
		void setToggleState(bool shouldBeOn, NotificationType notification);
		void valueChanged() override;

		bool isHeldDown() const noexcept { return isHeldDown_; }
		bool isHoveredOver() const noexcept { return isHoveredOver_; }

		std::span<const std::string_view> getStringLookup() const { return details_.stringLookup; }
		void setStringLookup(std::span<const std::string_view> lookup) { details_.stringLookup = lookup; }

		String getTextFromValue(bool value) const noexcept;
		void handlePopupResult(int result);

		void addListener(BaseSection *listener) override;
		void removeListener(BaseSection *listener) override;

	protected:
		void updateState(bool isHeldDown, bool isHoveredOver) noexcept;

		std::vector<ButtonListener *> buttonListeners_{};

		utils::shared_value<bool> isHeldDown_ = false;
		utils::shared_value<bool> isHoveredOver_ = false;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseButton)
	};

	class PowerButton final : public BaseButton
	{
	public:
		static constexpr int kAddedMargin = 4;

		PowerButton(Framework::ParameterValue *parameter);
		~PowerButton() override;

		void setColours() override;
		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;
		void redoImage() override;
		void setComponentsBounds() override;

	private:
		gl_ptr<PlainShapeComponent> shapeComponent_ = nullptr;

		Colour onNormalColor_{};

		utils::shared_value<Colour> activeColour_{};
		utils::shared_value<Colour> hoverColour_{};
	};

	class RadioButton final : public BaseButton
	{
	public:
		static constexpr int kAddedMargin = 4;

		RadioButton(Framework::ParameterValue *parameter);
		~RadioButton() override;

		void setColours() override;
		Rectangle<int> setBoundsForSizes(int height, int width = 0) override;
		void setExtraElementsPositions(Rectangle<int> anchorBounds) override;
		void redoImage() override;
		void setComponentsBounds() override;

		void setRounding(float rounding) noexcept;

	private:
		gl_ptr<OpenGlQuad> backgroundComponent_ = nullptr;

		utils::shared_value<Colour> onNormalColor_{};
		utils::shared_value<Colour> offNormalColor_{};
		utils::shared_value<Colour> backgroundColor_{};
	};

	class OptionsButton final : public BaseButton
	{
	public:
		static constexpr float kPlusRelativeSize = 7;
		static constexpr float kBorderRounding = 8.0f;

		OptionsButton(Framework::ParameterValue *parameter, String name = {}, String displayText = {});
		~OptionsButton() override;

		void setColours() override;
		Rectangle<int> setBoundsForSizes(int height, int width) override;
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

	class ActionButton final : public BaseButton
	{
	public:
		static constexpr float kBorderRounding = 8.0f;

		ActionButton(String name = {}, String displayText = {});
		~ActionButton() override;

		void mouseDown(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void setColours() override;
		Rectangle<int> setBoundsForSizes(int height, int width) override;
		void redoImage() override;
		void setComponentsBounds() override;

		void setText(String text) { text_ = std::move(text); }
		void setAction(std::function<void()> action) { action_ = std::move(action); }
	protected:
		gl_ptr<OpenGlQuad> fillComponent_ = nullptr;
		gl_ptr<PlainTextComponent> textComponent_ = nullptr;

		std::function<void()> action_{};

		String text_{};
		Colour fillColour_{};
		Colour textColour_{};
	};

}
