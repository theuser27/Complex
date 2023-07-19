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
		static constexpr size_t kNumVertices = 4;
		static constexpr size_t kNumFloatsPerVertex = 10;
		static constexpr size_t kNumFloatsPerQuad = kNumVertices * kNumFloatsPerVertex;
		static constexpr size_t kNumIndicesPerQuad = 6;
		static constexpr float kThicknessDecay = 0.4f;
		static constexpr float kAlphaInc = 0.2f;

		OpenGlMultiQuad(int maxQuads, Shaders::FragmentShader shader = Shaders::kColorFragment);

		void paint(Graphics &) override { }
		void resized() override
		{
			OpenGlComponent::resized();
			dirty_ = true;
		}

		void init(OpenGlWrapper &openGl) override;
		void render(OpenGlWrapper &openGl, bool animate) override;
		void destroy(OpenGlWrapper &openGl) override;

		void markDirty() noexcept { dirty_ = true; }

		Colour getColor() const noexcept { return color_; }
		float getMaxArc() const noexcept { return maxArc_; }
		float getQuadX(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			return data_[index];
		}

		float getQuadY(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			return data_[index + 1];
		}

		float getQuadWidth(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			return data_[2 * kNumFloatsPerVertex + index] - data_[index];
		}

		float getQuadHeight(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			return data_[kNumFloatsPerVertex + index + 1] - data_[index + 1];
		}

		float *getVerticesData(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
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
		void setNumQuads(size_t numQuads) noexcept
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

		void setTargetComponent(Component *targetComponent) noexcept { targetComponent_ = targetComponent; }
		void setScissorComponent(Component *scissorComponent) noexcept { scissorComponent_ = scissorComponent; }
		void setAdditive(bool additive) noexcept { additiveBlending_ = additive; }
		void setAlpha(float alpha, bool reset = false) noexcept
		{
			alphaMult_ = alpha;
			if (reset)
				currentAlphaMult_ = alpha;
		}

		void setDrawWhenNotVisible(bool draw) noexcept { drawWhenNotVisible_ = draw; }
		void setCustomDrawBounds(Rectangle<int> bounds) noexcept { customDrawBounds_ = bounds; }

		void setRotatedCoordinates(size_t i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;

			data_[index + 4] = x;
			data_[index + 5] = y + h;
			data_[kNumFloatsPerVertex + index + 4] = x + w;
			data_[kNumFloatsPerVertex + index + 5] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[2 * kNumFloatsPerVertex + index + 5] = y;
			data_[3 * kNumFloatsPerVertex + index + 4] = x;
			data_[3 * kNumFloatsPerVertex + index + 5] = y;
		}

		void setCoordinates(size_t i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;

			data_[index + 4] = x;
			data_[index + 5] = y;
			data_[kNumFloatsPerVertex + index + 4] = x;
			data_[kNumFloatsPerVertex + index + 5] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[2 * kNumFloatsPerVertex + index + 5] = y + h;
			data_[3 * kNumFloatsPerVertex + index + 4] = x + w;
			data_[3 * kNumFloatsPerVertex + index + 5] = y;
		}

		void setShaderValue(size_t i, float shaderValue, int valueIndex = 0) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad + 6 + valueIndex;
			data_[index] = shaderValue;
			data_[kNumFloatsPerVertex + index] = shaderValue;
			data_[2 * kNumFloatsPerVertex + index] = shaderValue;
			data_[3 * kNumFloatsPerVertex + index] = shaderValue;
			dirty_ = true;
		}

		void setDimensions(size_t i, float quadWidth, float quadHeight, float fullWidth, float fullHeight) noexcept
		{
			size_t index = i * kNumFloatsPerQuad;
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

		void setQuadHorizontal(size_t i, float x, float w) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;
			data_[index] = x;
			data_[kNumFloatsPerVertex + index] = x;
			data_[2 * kNumFloatsPerVertex + index] = x + w;
			data_[3 * kNumFloatsPerVertex + index] = x + w;

			dirty_ = true;
		}

		void setQuadVertical(size_t i, float y, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;
			data_[index + 1] = y;
			data_[kNumFloatsPerVertex + index + 1] = y + h;
			data_[2 * kNumFloatsPerVertex + index + 1] = y + h;
			data_[3 * kNumFloatsPerVertex + index + 1] = y;

			dirty_ = true;
		}

		void setQuad(size_t i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;
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
		Rectangle<int> customDrawBounds_{};
		Component *scissorComponent_ = nullptr;
		Shaders::FragmentShader fragmentShader_;
		size_t maxQuads_;
		size_t numQuads_;

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
		 * 0 - 1: vertex ndc position
		 * 2 - 3: scaled width and height for quad (acts like a uniform but idk why isn't one)
		 * 4 - 5: coordinates inside the quad (ndc for most situations, normalised for OpenGLCorners)
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
		static constexpr float kHoverChange = 0.2f;

		OpenGlScrollQuad() : OpenGlQuad(Shaders::kRoundedRectangleFragment)
		{
			animator_.setHoverIncrement(kHoverChange);
		}

		void render(OpenGlWrapper &openGl, bool animate) override
		{
			float lastHover = hoverAmount_;
			hoverAmount_ = animator_.getValue(Animator::Hover);

			if (lastHover != hoverAmount_)
			{
				if (shrinkLeft_)
					setQuadHorizontal(0, -1.0f, 1.0f + hoverAmount_);
				else
					setQuadHorizontal(0, 0.0f - hoverAmount_, 1.0f + hoverAmount_);
			}

			Range<double> range = scrollBar_->getCurrentRange();
			Range<double> totalRange = scrollBar_->getRangeLimit();
			double startRatio = (range.getStart() - totalRange.getStart()) / totalRange.getLength();
			double endRatio = (range.getEnd() - totalRange.getStart()) / totalRange.getLength();
			setQuadVertical(0, 1.0f - 2.0f * (float)endRatio, 2.0f * (float)(endRatio - startRatio));

			OpenGlQuad::render(openGl, animate);
		}

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
			bar_.getAnimator().setIsHovered(true);
		}

		void mouseExit(const MouseEvent &e) override
		{
			ScrollBar::mouseExit(e);
			bar_.getAnimator().setIsHovered(false);
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

		void setCorners(Rectangle<int> bounds, float rounding)
		{
			float width = rounding / bounds.getWidth() * 2.0f;
			float height = rounding / bounds.getHeight() * 2.0f;

			setQuad(0, -1.0f, -1.0f, width, height);
			setQuad(1, -1.0f, 1.0f - height, width, height);
			setQuad(2, 1.0f - width, 1.0f - height, width, height);
			setQuad(3, 1.0f - width, -1.0f, width, height);
		}

		void setCorners(Rectangle<int> bounds, float topRounding, float bottomRounding)
		{
			float topWidth = topRounding / bounds.getWidth() * 2.0f;
			float topHeight = topRounding / bounds.getHeight() * 2.0f;
			float bottomWidth = bottomRounding / bounds.getWidth() * 2.0f;
			float bottomHeight = bottomRounding / bounds.getHeight() * 2.0f;

			setQuad(0, -1.0f, -1.0f, bottomWidth, bottomHeight);
			setQuad(1, -1.0f, 1.0f - topHeight, topWidth, topHeight);
			setQuad(2, 1.0f - topWidth, 1.0f - topHeight, topWidth, topHeight);
			setQuad(3, 1.0f - bottomWidth, -1.0f, bottomWidth, bottomHeight);
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
