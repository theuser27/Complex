/*
	==============================================================================

		OpenGlImageComponent.h
		Created: 14 Dec 2022 2:07:44am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <variant>
#include "OpenGlComponent.h"

namespace Interface
{
	// Drawing class that acquires its logic from the paint method
	// of the currently linked component (i.e. the owner of the object)
	// and draws it on an owned image resource
	class OpenGlImageComponent : public OpenGlComponent
	{
	public:
		OpenGlImageComponent(String name = "");

		void redrawImage(bool forceRedraw = true);
		virtual void paintToImage(Graphics &g, BaseComponent *target)
		{
			if (paintEntireComponent_)
				target->paintEntireComponent(g, false);
			else
				target->paint(g);
		}

		void init(OpenGlWrapper &openGl) override;
		void render(OpenGlWrapper &openGl, bool animate) override;
		void destroy() override;

		void setTargetComponent(BaseComponent *targetComponent) { targetComponent_ = targetComponent; }
		void setCustomDrawBounds(Rectangle<int> customDrawBounds) { customDrawBounds_ = customDrawBounds; }
		void setAdditive(bool additive) { additive_ = additive; }
		void setScissor(bool scissor) { scissor_ = scissor; }
		void setUseAlpha(bool useAlpha) { useAlpha_ = useAlpha; }
		void setColor(Colour color) { color_ = color; }
		void setActive(bool active) { active_ = active; }

		void setPaintFunction(std::function<void(Graphics &)> paintFunction) { paintFunction_ = std::move(paintFunction); }
		void setShouldClearOnRedraw(bool clearOnRedraw) { clearOnRedraw_ = clearOnRedraw; }
		void paintEntireComponent(bool paintEntireComponent) { paintEntireComponent_ = paintEntireComponent; }

	protected:
		shared_value<Colour> color_ = Colours::white;
		shared_value<bool> additive_ = false;
		shared_value<bool> useAlpha_ = false;
		shared_value<bool> scissor_ = true;
		shared_value<bool> active_ = true;

		shared_value<bool> hasNewVertices_ = true;
		shared_value<bool> shouldReloadImage_ = false;
		shared_value<BaseComponent *> targetComponent_ = nullptr;
		shared_value<Rectangle<int>> customDrawBounds_{};

		shared_value<std::unique_ptr<Image>> drawImage_;
		OpenGlTexture texture_;
		OpenGlShaderProgram *imageShader_ = nullptr;
		OpenGlUniform imageColor_;
		OpenGlAttribute imagePosition_;
		OpenGlAttribute textureCoordinates_;

		std::unique_ptr<float[]> positionVertices_;
		std::unique_ptr<int[]> positionTriangles_;
		GLuint vertexBuffer_{};
		GLuint triangleBuffer_{};

		std::function<void(Graphics &)> paintFunction_{};
		bool paintEntireComponent_ = true;
		bool clearOnRedraw_ = true;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlImageComponent)
	};

	class OpenGlBackground final : public OpenGlImageComponent
	{
	public:
		OpenGlBackground() : OpenGlImageComponent(typeid(OpenGlBackground).name()) { setShouldClearOnRedraw(false); }

		void paintToImage(Graphics &g, BaseComponent *target) override;
		void setComponentToRedraw(BaseSection *componentToRedraw)
		{ componentToRedraw_ = componentToRedraw; }

	protected:
		BaseSection *componentToRedraw_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlBackground)
	};

	class PlainTextComponent final : public OpenGlImageComponent
	{
	public:
		enum FontType
		{
			kTitle,
			kText,
			kValues
		};

		PlainTextComponent(String name, String text = {});

		void resized() override;
		void paintToImage(Graphics &g, BaseComponent *target) override;

		String getText() const { return text_; }
		int getTotalWidth() const { return font_.getStringWidth(text_); }
		int getTotalHeight() const { return (int)std::ceil(font_.getHeight()); }
		void updateState();

		void setText(String text) noexcept { text_ = std::move(text); redrawImage(); }
		void setTextHeight(float textSize) noexcept { textSize_ = textSize; }
		void setTextColour(Colour colour) noexcept { textColour_ = colour; }
		void setFontType(FontType type) noexcept { fontType_ = type; }
		void setJustification(Justification justification) noexcept { justification_ = justification; }

	private:
		String text_;
		float textSize_ = 11.0f;
		Colour textColour_ = Colours::white;
		Font font_;
		FontType fontType_ = kText;
		Justification justification_ = Justification::centred;
	};

	class PlainShapeComponent final : public OpenGlImageComponent
	{
	public:
		PlainShapeComponent(String name) : OpenGlImageComponent(std::move(name)) { }

		void resized() override { redrawImage(); }
		void paintToImage(Graphics &g, BaseComponent *target) override;

		void setShapes(std::pair<Path, Path> strokeFillShapes)
		{
			strokeShape_ = std::move(strokeFillShapes.first);
			fillShape_ = std::move(strokeFillShapes.second);
			redrawImage();
		}

		void setJustification(Justification justification) { justification_ = justification; }

	private:
		Path strokeShape_;
		Path fillShape_;
		Justification justification_ = Justification::centred;
	};
}
