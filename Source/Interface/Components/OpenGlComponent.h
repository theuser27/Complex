/*
	==============================================================================

		OpenGlComponent.h
		Created: 5 Dec 2022 11:55:29pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/Shaders.h"
#include "../LookAndFeel/Skin.h"

namespace Interface
{
	class BaseSection;
	class OpenGlCorners;
	class OpenGlComponent;

	class Animator
	{
	public:
		enum EventType { Hover, Click };

		void tick(bool isAnimating = true) noexcept
		{
			if (isAnimating)
			{
				if (isHovered_)
					hoverValue_ = std::min(1.0f, hoverValue_ + hoverIncrement_);
				else
					hoverValue_ = std::max(0.0f, hoverValue_ - hoverIncrement_);

				if (isClicked_)
					clickValue_ = std::min(1.0f, clickValue_ + clickIncrement_);
				else
					clickValue_ = std::max(0.0f, clickValue_ - clickIncrement_);
			}
			else
			{
				hoverValue_ = isHovered_;
				clickValue_ = isClicked_;
			}

			for (auto &callbackPair : tickCallbacks_)
			{
				switch (callbackPair.first)
				{
				case Hover:
					callbackPair.second(hoverValue_);
					break;
				case Click:
					callbackPair.second(clickValue_);
					break;
				default:
					break;
				}
			}
		}

		float getValue(EventType type, float min = 0.0f, float max = 1.0f) const noexcept
		{
			float value;
			switch (type)
			{
			case Animator::Hover:
				value = hoverValue_;
				break;
			case Animator::Click:
				value = clickValue_;
				break;
			default:
				value = 1.0f;
				break;
			}

			value *= max - min;
			return value + min;
		}

		void setHoverIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); hoverIncrement_ = increment; }

		void setClickIncrement(float increment) noexcept
		{ COMPLEX_ASSERT(increment > 0.0f); clickIncrement_ = increment; }

		void setIsHovered(bool isHovered) noexcept { isHovered_ = isHovered; }
		void setIsClicked(bool isClicked) noexcept { isClicked_ = isClicked; }

		void addTickCallback(EventType eventType, std::function<void(float)> function)
		{ tickCallbacks_.emplace_back(eventType, std::move(function)); }

		void removeTickCallback(size_t index)
		{ tickCallbacks_.erase(tickCallbacks_.begin() + index); }

	private:
		float hoverValue_ = 0.0f;
		float clickValue_ = 0.0f;

		float hoverIncrement_ = 1.0f;
		float clickIncrement_ = 1.0f;

		bool isHovered_ = false;
		bool isClicked_ = false;

		std::vector<std::pair<EventType, std::function<void(float)>>> tickCallbacks_{};
	};

	class OpenGlComponent : public Component
	{
	public:
		static bool setViewPort(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);
		static bool setViewPort(const Component *component, const OpenGlWrapper &openGl);
		static void setScissor(const Component *component, const OpenGlWrapper &openGl);
		static void setScissorBounds(const Component *component, Rectangle<int> bounds, const OpenGlWrapper &openGl);

		static std::unique_ptr<OpenGLShaderProgram::Uniform> getUniform(const OpenGLShaderProgram &program, const char *name)
		{
			if (juce::gl::glGetUniformLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Uniform>(program, name);
			return nullptr;
		}

		static std::unique_ptr<OpenGLShaderProgram::Attribute> getAttribute(const OpenGLShaderProgram &program, const char *name)
		{
			if (juce::gl::glGetAttribLocation(program.getProgramID(), name) >= 0)
				return std::make_unique<OpenGLShaderProgram::Attribute>(program, name);
			return nullptr;
		}

		static String translateFragmentShader(const String &code)
		{
		#if OPENGL_ES
			return String("#version 300 es\n") + "out mediump vec4 fragColor;\n" +
				code.replace("varying", "in").replace("texture2D", "texture").replace("gl_FragColor", "fragColor");
		#else
			return OpenGLHelpers::translateFragmentShaderToV3(code);
		#endif
		}

		static String translateVertexShader(const String &code)
		{
		#if OPENGL_ES
			return String("#version 300 es\n") + code.replace("attribute", "in").replace("varying", "out");
		#else
			return OpenGLHelpers::translateVertexShaderToV3(code);
		#endif
		}

		OpenGlComponent(String name = "");
		~OpenGlComponent() override;

		void resized() override;
		void paint(Graphics &g) override;

		virtual void init(OpenGlWrapper &openGl);
		virtual void render(OpenGlWrapper &openGl, bool animate) = 0;
		virtual void destroy(OpenGlWrapper &openGl);

		void renderCorners(OpenGlWrapper &openGl, bool animate, Colour color, float rounding);
		void renderCorners(OpenGlWrapper &openGl, bool animate);
		void addRoundedCorners();
		void addBottomRoundedCorners();
		void repaintBackground();

		Colour getBodyColor() const noexcept { return bodyColor_; }
		Animator &getAnimator() noexcept { return animator_; }
		float getValue(Skin::ValueId valueId) const;
		Colour getColour(Skin::ColorId colourId) const;

		//void setSkinValues(const Skin &skin) { skin.applyComponentColors(this, skinOverride_, false); }
		void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
		void setBackgroundColor(const Colour &color) noexcept { backgroundColor_ = color; }
		void setParent(const BaseSection *parent) noexcept { parent_ = parent; }

	protected:
		bool setViewPort(const OpenGlWrapper &openGl) const;

		Animator animator_{};
		std::unique_ptr<OpenGlCorners> corners_;
		bool onlyBottomCorners_ = false;
		Colour backgroundColor_ = Colours::transparentBlack;
		Colour bodyColor_;
		Skin::SectionOverride skinOverride_ = Skin::kNone;

		const BaseSection *parent_ = nullptr;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
	};
}
