/*
	==============================================================================

		OpenGlMultiQuad.h
		Created: 14 Dec 2022 2:05:11am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "OpenGlComponent.h"

namespace Interface
{
	class OpenGlMultiQuad : public OpenGlComponent
	{
	public:
		static constexpr u32 kNumVertices = 4;
		static constexpr u32 kNumFloatsPerVertex = 10;
		static constexpr u32 kNumFloatsPerQuad = kNumVertices * kNumFloatsPerVertex;
		static constexpr u32 kNumIndicesPerQuad = 6;
		static constexpr float kThicknessDecay = 0.4f;
		static constexpr float kAlphaInc = 0.2f;

		OpenGlMultiQuad(int max_quads, Shaders::FragmentShader shader = Shaders::kColorFragment);
		~OpenGlMultiQuad() override = default;

		void paint(Graphics &g) override { }
		void resized() override
		{
			OpenGlComponent::resized();
			dirty_ = true;
		}

		void init(OpenGlWrapper &open_gl) override;
		void render(OpenGlWrapper &open_gl, bool animate) override;
		void destroy(OpenGlWrapper &open_gl) override;

		void markDirty() noexcept { dirty_ = true; }

		Colour getColor() const noexcept { return color_; }
		float getMaxArc() const noexcept { return maxArc_; }
		float getQuadX(u32 i) const noexcept
		{
			u32 index = kNumFloatsPerQuad * i;
			return data_[index];
		}

		float getQuadY(u32 i) const noexcept
		{
			u32 index = kNumFloatsPerQuad * i;
			return data_[index + 1];
		}

		float getQuadWidth(u32 i) const noexcept
		{
			u32 index = kNumFloatsPerQuad * i;
			return data_[2 * kNumFloatsPerVertex + index] - data_[index];
		}

		float getQuadHeight(u32 i) const noexcept
		{
			u32 index = kNumFloatsPerQuad * i;
			return data_[kNumFloatsPerVertex + index + 1] - data_[index + 1];
		}

		float *getVerticesData(u32 i) const noexcept
		{
			u32 index = kNumFloatsPerQuad * i;
			return data_.get() + index;
		}

		void setFragmentShader(Shaders::FragmentShader shader) noexcept { fragmentShader_ = shader; }
		void setColor(Colour color) noexcept { color_ = color; }
		void setAltColor(Colour color) noexcept { altColor_ = color; }
		void setModColor(Colour color) noexcept { modColor_ = color; }
		void setThumbColor(Colour color) noexcept { thumbColor_ = color; }
		void setThumbAmount(float amount) noexcept { thumbAmount_ = amount; }
		void setStartPos(float position) noexcept { startPosition_ = position; }
		void setMaxArc(float maxArc) noexcept { maxArc_ = maxArc; }
		void setActive(bool active) noexcept { active_ = active; }
		void setNumQuads(u32 numQuads) noexcept
		{
			COMPLEX_ASSERT(numQuads <= maxQuads_);
			numQuads_ = numQuads;
			dirty_ = true;
		}

		void setThickness(float thickness, bool reset = false) noexcept
		{
			thickness_ = thickness;
			if (reset)
				currentThickness_ = thickness_;
		}

		void setRounding(float rounding) noexcept
		{
			float adjusted = 2.0f * rounding;
			if (adjusted != rounding_)
			{
				dirty_ = true;
				rounding_ = adjusted;
			}
		}

		void setTargetComponent(Component *targetComponent) noexcept
		{ targetComponent_ = targetComponent; }

		void setScissorComponent(Component *scissorComponent) noexcept
		{ scissorComponent_ = scissorComponent; }

		void setAdditive(bool additive) noexcept { additiveBlending_ = additive; }
		void setAlpha(float alpha, bool reset = false) noexcept
		{
			alphaMult_ = alpha;
			if (reset)
				currentAlphaMult_ = alpha;
		}

		void setDrawWhenNotVisible(bool draw) noexcept { drawWhenNotVisible_ = draw; }

		void setRotatedCoordinates(u32 i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad;

			data_[index + 4] = x;
			data_[index + 5] = y + h;
			data_[kNumFloatsPerVertex + index + 4] = x + w;
			data_[kNumFloatsPerVertex + index + 5] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[2 * kNumFloatsPerVertex + index + 5] = y;
			data_[3 * kNumFloatsPerVertex + index + 4] = x;
			data_[3 * kNumFloatsPerVertex + index + 5] = y;
		}

		void setCoordinates(u32 i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad;

			data_[index + 4] = x;
			data_[index + 5] = y;
			data_[kNumFloatsPerVertex + index + 4] = x;
			data_[kNumFloatsPerVertex + index + 5] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[2 * kNumFloatsPerVertex + index + 5] = y + h;
			data_[3 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[3 * kNumFloatsPerVertex + index + 5] = y;
		}

		void setShaderValue(u32 i, float shaderValue, int valueIndex = 0) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad + 6 + valueIndex;
			data_[index] = shaderValue;
			data_[kNumFloatsPerVertex + index] = shaderValue;
			data_[2 * kNumFloatsPerVertex + index] = shaderValue;
			data_[3 * kNumFloatsPerVertex + index] = shaderValue;
			dirty_ = true;
		}

		void setDimensions(u32 i, float quadWidth, float quadHeight, float fullWidth, float fullHeight) noexcept
		{
			u32 index = i * kNumFloatsPerQuad;
			float w = quadWidth * fullWidth / 2.0f;
			float h = quadHeight * fullHeight / 2.0f;

			data_[index + 2] = w;
			data_[index + 3] = h;
			data_[kNumFloatsPerVertex + index + 2] = w;
			data_[kNumFloatsPerVertex + index + 3] = h;
			data_[2 * kNumFloatsPerVertex + index + 2] = w;
			data_[2 * kNumFloatsPerVertex + index + 3] = h;
			data_[3 * kNumFloatsPerVertex + index + 2] = w;
			data_[3 * kNumFloatsPerVertex + index + 3] = h;
		}

		void setQuadHorizontal(u32 i, float x, float w) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad;
			data_[index] = x;
			data_[kNumFloatsPerVertex + index] = x;
			data_[2 * kNumFloatsPerVertex + index] = x + w;
			data_[3 * kNumFloatsPerVertex + index] = x + w;

			dirty_ = true;
		}

		void setQuadVertical(u32 i, float y, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad;
			data_[index + 1] = y;
			data_[kNumFloatsPerVertex + index + 1] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 1] = y + h;
			data_[3 * kNumFloatsPerVertex + index + 1] = y;

			dirty_ = true;
		}

		void setQuad(u32 i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			u32 index = i * kNumFloatsPerQuad;
			data_[index] = x;
			data_[index + 1] = y;
			data_[kNumFloatsPerVertex + index] = x;
			data_[kNumFloatsPerVertex + index + 1] = y + h;
			data_[2 * kNumFloatsPerVertex + index] = x + w;
			data_[2 * kNumFloatsPerVertex + index + 1] = y + h;
			data_[3 * kNumFloatsPerVertex + index] = x + w;
			data_[3 * kNumFloatsPerVertex + index + 1] = y;

			dirty_ = true;
		}

	protected:
		Component *targetComponent_ = nullptr;
		Component *scissorComponent_ = nullptr;
		Shaders::FragmentShader fragmentShader_;
		u32 maxQuads_;
		u32 numQuads_;

		bool drawWhenNotVisible_ = false;
		bool active_ = true;
		bool dirty_ = false;
		Colour color_;
		Colour altColor_;
		Colour modColor_;
		Colour thumbColor_;
		float maxArc_ = 2.0f;
		float thumbAmount_ = 0.5f;
		float startPosition_ = 0.0f;
		float currentAlphaMult_ = 1.0f;
		float alphaMult_ = 1.0f;
		bool additiveBlending_ = false;
		float currentThickness_ = 1.0f;
		float thickness_ = 1.0f;
		float rounding_ = 5.0f;

		/*
		 * data_ array indices per quad
		 * 0 - 1: quad xy-expansion when hovered over (necessary for knobs/sliders)
		 * 2 - 3: scaled width and height
		 * 4 - 5: xy-positions
		 * 6 - 7: shader values (doubles as left channel shader values)
		 * 6 - 9: right channel shader values (necessary for the modulation meters/indicators)
		 */
		std::unique_ptr<float[]> data_;
		std::unique_ptr<int[]> indices_;

		OpenGLShaderProgram *shader_ = nullptr;
		std::unique_ptr<OpenGLShaderProgram::Uniform> colorUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> altColorUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> modColorUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> backgroundColorUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> thumbColorUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> thicknessUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> roundingUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> maxArcUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> thumbAmountUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> startPositionUniform_;
		std::unique_ptr<OpenGLShaderProgram::Uniform> alphaMultUniform_;
		std::unique_ptr<OpenGLShaderProgram::Attribute> position_;
		std::unique_ptr<OpenGLShaderProgram::Attribute> dimensions_;
		std::unique_ptr<OpenGLShaderProgram::Attribute> coordinates_;
		std::unique_ptr<OpenGLShaderProgram::Attribute> shader_values_;

		GLuint vertexBuffer_;
		GLuint indicesBuffer_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlMultiQuad)
	};

	class OpenGlQuad : public OpenGlMultiQuad
	{
	public:
		OpenGlQuad(Shaders::FragmentShader shader) : OpenGlMultiQuad(1, shader)
		{ setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f); }
	};

	class OpenGlScrollQuad : public OpenGlQuad
	{
	public:
		OpenGlScrollQuad() : OpenGlQuad(Shaders::kRoundedRectangleFragment) { }
		~OpenGlScrollQuad() override = default;

		void render(OpenGlWrapper &open_gl, bool animate) override
		{
			static constexpr float kHoverChange = 0.2f;
			float last_hover = hoverAmount_;
			if (hover_)
				hoverAmount_ = std::min(1.0f, hoverAmount_ + kHoverChange);
			else
				hoverAmount_ = std::max(0.0f, hoverAmount_ - kHoverChange);

			if (last_hover != hoverAmount_)
			{
				if (shrinkLeft_)
					setQuadHorizontal(0, -1.0f, 1.0f + hoverAmount_);
				else
					setQuadHorizontal(0, 0.0f - hoverAmount_, 1.0f + hoverAmount_);
			}

			Range<double> range = scrollBar_->getCurrentRange();
			Range<double> total_range = scrollBar_->getRangeLimit();
			float start_ratio = (range.getStart() - total_range.getStart()) / total_range.getLength();
			float end_ratio = (range.getEnd() - total_range.getStart()) / total_range.getLength();
			setQuadVertical(0, 1.0f - 2.0f * end_ratio, 2.0f * (end_ratio - start_ratio));

			OpenGlQuad::render(open_gl, animate);
		}

		void setHover(bool hover) { hover_ = hover; }
		void setShrinkLeft(bool shrinkLeft) { shrinkLeft_ = shrinkLeft; }
		void setScrollBar(ScrollBar *scrollBar) { scrollBar_ = scrollBar; }

	private:
		ScrollBar *scrollBar_ = nullptr;
		bool hover_ = false;
		bool shrinkLeft_ = false;
		float hoverAmount_ = -1.0f;
	};

	class OpenGlScrollBar : public ScrollBar
	{
	public:
		OpenGlScrollBar() : ScrollBar(true)
		{
			bar_.setTargetComponent(this);
			addAndMakeVisible(bar_);
			bar_.setScrollBar(this);
		}
		~OpenGlScrollBar() override = default;

		OpenGlQuad *getGlComponent() { return &bar_; }

		void resized() override
		{
			ScrollBar::resized();
			bar_.setBounds(getLocalBounds());
			bar_.setRounding(getWidth() * 0.25f);
		}

		void mouseEnter(const MouseEvent &e) override
		{
			ScrollBar::mouseEnter(e);
			bar_.setHover(true);
		}

		void mouseExit(const MouseEvent &e) override
		{
			ScrollBar::mouseExit(e);
			bar_.setHover(false);
		}

		void mouseDown(const MouseEvent &e) override
		{
			ScrollBar::mouseDown(e);
			bar_.setColor(color_.overlaidWith(color_));
		}

		void mouseUp(const MouseEvent &e) override
		{
			ScrollBar::mouseDown(e);
			bar_.setColor(color_);
		}

		void setColor(Colour color) { color_ = color; bar_.setColor(color); }
		void setShrinkLeft(bool shrink_left) { bar_.setShrinkLeft(shrink_left); }

	private:
		Colour color_;
		OpenGlScrollQuad bar_;
	};

	class OpenGlCorners : public OpenGlMultiQuad
	{
	public:
		OpenGlCorners() : OpenGlMultiQuad(4, Shaders::kRoundedCornerFragment)
		{
			setCoordinates(0, 1.0f, 1.0f, -1.0f, -1.0f);
			setCoordinates(1, 1.0f, 0.0f, -1.0f, 1.0f);
			setCoordinates(2, 0.0f, 0.0f, 1.0f, 1.0f);
			setCoordinates(3, 0.0f, 1.0f, 1.0f, -1.0f);
		}
		~OpenGlCorners() override = default;

		void setCorners(Rectangle<int> bounds, float rounding)
		{
			float width = rounding / bounds.getWidth() * 2.0f;
			float height = rounding / bounds.getHeight() * 2.0f;

			setQuad(0, -1.0f, -1.0f, width, height);
			setQuad(1, -1.0f, 1.0f - height, width, height);
			setQuad(2, 1.0f - width, 1.0f - height, width, height);
			setQuad(3, 1.0f - width, -1.0f, width, height);
		}

		void setTopCorners(Rectangle<int> bounds, float topRounding)
		{
			float width = topRounding / bounds.getWidth() * 2.0f;
			float height = topRounding / bounds.getHeight() * 2.0f;

			setQuad(0, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(1, -1.0f, 1.0f - height, width, height);
			setQuad(2, 1.0f - width, 1.0f - height, width, height);
			setQuad(3, -2.0f, -2.0f, 0.0f, 0.0f);
		}

		void setBottomCorners(Rectangle<int> bounds, float bottomRounding)
		{
			float width = bottomRounding / bounds.getWidth() * 2.0f;
			float height = bottomRounding / bounds.getHeight() * 2.0f;

			setQuad(0, -1.0f, -1.0f, width, height);
			setQuad(1, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(2, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(3, 1.0f - width, -1.0f, width, height);
		}
	};
}
