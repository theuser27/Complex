/*
	==============================================================================

		OpenGlMultiQuad.h
		Created: 14 Dec 2022 2:05:11am
		Author:  theuser27

	==============================================================================
*/

#pragma once

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

		OpenGlMultiQuad(int maxQuads, Shaders::FragmentShader shader = Shaders::kColorFragment, 
			String name = typeid(OpenGlMultiQuad).name());

		void resized() override
		{
			OpenGlComponent::resized();
			dirty_ = true;
		}

		void init(OpenGlWrapper &openGl) override;
		void render(OpenGlWrapper &openGl, bool animate) override;
		void destroy() override;

		Colour getColor() const noexcept { return color_; }
		float getMaxArc() const noexcept { return maxArc_; }
		float getQuadX(size_t i) const noexcept
		{
			auto quadX = data_.lock()[kNumFloatsPerQuad * i];
			data_.unlock();
			return quadX;
		}

		float getQuadY(size_t i) const noexcept
		{
			auto quadY = data_.lock()[kNumFloatsPerQuad * i + 1];
			data_.unlock();
			return quadY;
		}

		float getQuadWidth(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			auto *data = data_.lock();
			auto x0 = data[index];
			auto x1 = data[2 * kNumFloatsPerVertex + index];
			data_.unlock();
			return x1 - x0;
		}

		float getQuadHeight(size_t i) const noexcept
		{
			size_t index = kNumFloatsPerQuad * i;
			auto *data = data_.lock();
			auto y0 = data[index + 1];
			auto y1 = data[2 * kNumFloatsPerVertex + index + 1];
			data_.unlock();
			return y1 - y0;
		}

		void setFragmentShader(Shaders::FragmentShader shader) noexcept { fragmentShader_ = shader; }
		void setColor(Colour color) noexcept { color_ = color; }
		void setAltColor(Colour color) noexcept { altColor_ = color; }
		void setModColor(Colour color) noexcept { modColor_ = color; }
		void setBackgroundColor(Colour color) noexcept { backgroundColor_ = color; }
		void setThumbColor(Colour color) noexcept { thumbColor_ = color; }
		void setThumbAmount(float amount) noexcept { thumbAmount_ = amount; }
		void setStartPos(float position) noexcept { startPosition_ = position; }
		void setMaxArc(float maxArc) noexcept { maxArc_ = maxArc; }
		void setActive(bool active) noexcept { active_ = active; }
		void setThickness(float thickness) noexcept { thickness_ = thickness; }
		void setAdditive(bool additive) noexcept { additiveBlending_ = additive; }
		void setOverallAlpha(float alpha) noexcept { overallAlpha_ = alpha; }
		void setRounding(float rounding) noexcept
		{
			float adjusted = 2.0f * rounding;
			if (adjusted != rounding_)
				rounding_ = adjusted;
		}
		void setStaticValues(float value, size_t valueIndex = 0) noexcept
		{
			COMPLEX_ASSERT(valueIndex < 4);
			auto values = values_.get();
			values[valueIndex] = value;
			values_ = values;
		}
		void setNumQuads(size_t numQuads) noexcept
		{
			COMPLEX_ASSERT(numQuads <= maxQuads_);
			numQuads_ = numQuads;
			dirty_ = true;
		}

		void setTargetComponent(BaseComponent *targetComponent) noexcept { targetComponent_ = targetComponent; }
		void setScissorComponent(BaseComponent *scissorComponent) noexcept { scissorComponent_ = scissorComponent; }
		void setDrawWhenNotVisible(bool draw) noexcept { drawWhenNotVisible_ = draw; }
		void setCustomDrawBounds(Rectangle<int> bounds) noexcept { customDrawBounds_ = bounds; }

		void setCoordinates(size_t i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;

			auto data = data_.lock();
			data[index + 4] = x;
			data[index + 5] = y;
			data[kNumFloatsPerVertex + index + 4] = x;
			data[kNumFloatsPerVertex + index + 5] = y + h;
			data[2 * kNumFloatsPerVertex + index + 4] = x + w;
			data[2 * kNumFloatsPerVertex + index + 5] = y + h;
			data[3 * kNumFloatsPerVertex + index + 4] = x + w;
			data[3 * kNumFloatsPerVertex + index + 5] = y;
			data_.unlock();

			dirty_ = true;
		}

		void setShaderValue(size_t i, float shaderValue, int valueIndex = 0) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad + 6 + valueIndex;

			auto data = data_.lock();
			data[index] = shaderValue;
			data[kNumFloatsPerVertex + index] = shaderValue;
			data[2 * kNumFloatsPerVertex + index] = shaderValue;
			data[3 * kNumFloatsPerVertex + index] = shaderValue;
			data_.unlock();

			dirty_ = true;
		}

		void setDimensions(size_t i, float quadWidth, float quadHeight, float fullWidth, float fullHeight) noexcept
		{
			size_t index = i * kNumFloatsPerQuad;
			float w = quadWidth * fullWidth / 2.0f;
			float h = quadHeight * fullHeight / 2.0f;

			auto data = data_.lock();
			data[index + 2] = w;
			data[index + 3] = h;
			data[kNumFloatsPerVertex + index + 2] = w;
			data[kNumFloatsPerVertex + index + 3] = h;
			data[2 * kNumFloatsPerVertex + index + 2] = w;
			data[2 * kNumFloatsPerVertex + index + 3] = h;
			data[3 * kNumFloatsPerVertex + index + 2] = w;
			data[3 * kNumFloatsPerVertex + index + 3] = h;
			data_.unlock();

			dirty_ = true;
		}

		void setQuadHorizontal(size_t i, float x, float w) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;
			
			auto data = data_.lock();
			data[index] = x;
			data[kNumFloatsPerVertex + index] = x;
			data[2 * kNumFloatsPerVertex + index] = x + w;
			data[3 * kNumFloatsPerVertex + index] = x + w;
			data_.unlock();

			dirty_ = true;
		}

		void setQuadVertical(size_t i, float y, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;

			auto data = data_.lock();
			data[index + 1] = y;
			data[kNumFloatsPerVertex + index + 1] = y + h;
			data[2 * kNumFloatsPerVertex + index + 1] = y + h;
			data[3 * kNumFloatsPerVertex + index + 1] = y;
			data_.unlock();

			dirty_ = true;
		}

		void setQuad(size_t i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads_);
			size_t index = i * kNumFloatsPerQuad;

			auto data = data_.lock();
			data[index] = x;
			data[index + 1] = y;
			data[kNumFloatsPerVertex + index] = x;
			data[kNumFloatsPerVertex + index + 1] = y + h;
			data[2 * kNumFloatsPerVertex + index] = x + w;
			data[2 * kNumFloatsPerVertex + index + 1] = y + h;
			data[3 * kNumFloatsPerVertex + index] = x + w;
			data[3 * kNumFloatsPerVertex + index + 1] = y;
			data_.unlock();

			dirty_ = true;
		}

	protected:
		utils::shared_value<BaseComponent *> targetComponent_ = nullptr;
		utils::shared_value<Rectangle<int>> customDrawBounds_{};
		utils::shared_value<BaseComponent *> scissorComponent_ = nullptr;
		utils::shared_value<Shaders::FragmentShader> fragmentShader_;
		utils::shared_value<size_t> maxQuads_;
		utils::shared_value<size_t> numQuads_;

		utils::shared_value<bool> drawWhenNotVisible_ = false;
		utils::shared_value<bool> active_ = true;
		utils::shared_value<bool> dirty_ = false;
		utils::shared_value<Colour> color_;
		utils::shared_value<Colour> altColor_;
		utils::shared_value<Colour> modColor_;
		utils::shared_value<Colour> backgroundColor_;
		utils::shared_value<Colour> thumbColor_;
		utils::shared_value<float> maxArc_ = 2.0f;
		utils::shared_value<float> thumbAmount_ = 0.5f;
		utils::shared_value<float> startPosition_ = 0.0f;
		utils::shared_value<float> overallAlpha_ = 1.0f;
		utils::shared_value<bool> additiveBlending_ = false;
		utils::shared_value<float> thickness_ = 1.0f;
		utils::shared_value<float> rounding_ = 5.0f;
		utils::shared_value<std::array<float, 4>> values_{};

		/*
		 * data_ array indices per quad
		 * 0 - 1: vertex ndc position
		 * 2 - 3: scaled width and height for quad (acts like a uniform but idk why isn't one)
		 * 4 - 5: coordinates inside the quad (ndc for most situations, normalised for OpenGLCorners)
		 * 6 - 7: shader values (doubles as left channel shader values)
		 * 8 - 9: right channel shader values (necessary for the modulation meters/indicators)
		 */
		utils::shared_value<std::unique_ptr<float[]>> data_;
		std::unique_ptr<int[]> indices_;

		OpenGlShaderProgram *shader_ = nullptr;
		std::optional<OpenGlUniform> colorUniform_;
		std::optional<OpenGlUniform> altColorUniform_;
		std::optional<OpenGlUniform> modColorUniform_;
		std::optional<OpenGlUniform> backgroundColorUniform_;
		std::optional<OpenGlUniform> thumbColorUniform_;
		std::optional<OpenGlUniform> thicknessUniform_;
		std::optional<OpenGlUniform> roundingUniform_;
		std::optional<OpenGlUniform> maxArcUniform_;
		std::optional<OpenGlUniform> thumbAmountUniform_;
		std::optional<OpenGlUniform> startPositionUniform_;
		std::optional<OpenGlUniform> overallAlphaUniform_;
		std::optional<OpenGlUniform> valuesUniform_;
		std::optional<OpenGlAttribute> position_;
		std::optional<OpenGlAttribute> dimensions_;
		std::optional<OpenGlAttribute> coordinates_;
		std::optional<OpenGlAttribute> shader_values_;

		GLuint vertexBuffer_;
		GLuint indicesBuffer_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlMultiQuad)
	};

	class OpenGlQuad final : public OpenGlMultiQuad
	{
	public:
		OpenGlQuad(Shaders::FragmentShader shader, String name = typeid(OpenGlQuad).name()) :
			OpenGlMultiQuad(1, shader, std::move(name))
		{ setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f); }
	};

	class OpenGlCorners final : public OpenGlMultiQuad
	{
	public:
		OpenGlCorners() : OpenGlMultiQuad(4, Shaders::kRoundedCornerFragment, typeid(OpenGlCorners).name())
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
