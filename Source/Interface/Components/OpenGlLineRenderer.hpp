/*
	==============================================================================

		OpenGlLineRenderer.hpp
		Created: 3 Feb 2023 10:14:36pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"

namespace Interface
{
	class OpenGlCorners;
	class Renderer;

	class OpenGlLineRenderer
	{
	public:
		static constexpr int kLineFloatsPerVertex = 3;
		static constexpr int kFillFloatsPerVertex = 3;
		static constexpr int kLineVerticesPerPoint = 6;
		static constexpr int kFillVerticesPerPoint = 2;
		static constexpr int kLineFloatsPerPoint = kLineVerticesPerPoint * kLineFloatsPerVertex;
		static constexpr int kFillFloatsPerPoint = kFillVerticesPerPoint * kFillFloatsPerVertex;

		static constexpr float kDefaultLineWidth = 7.0f;

		OpenGlLineRenderer(int pointCount);
		~OpenGlLineRenderer();

		void init(OpenGlWrapper &openGl);
		void render(const OpenGlWrapper &openGl, const Component &target, Rectangle<int> bounds);
		void destroy(Renderer &renderer);

		void setPointCount(int pointCount);

		strict_inline void setBoost(usize index, float val) noexcept
		{
			COMPLEX_ASSERT((usize)pointCount_ > index);
			boosts_[index] = val;
			dirty_ = true;
		}
		strict_inline void setYAt(usize index, float val) noexcept
		{
			COMPLEX_ASSERT((usize)pointCount_ > index);
			y_[index] = val;
			dirty_ = true;
		}
		strict_inline void setXAt(usize index, float val) noexcept
		{
			COMPLEX_ASSERT((usize)pointCount_ > index);
			x_[index] = val;
			dirty_ = true;
		}

		void setFillVertices(float width, float height);
		void setLineVertices(float width, float height);

		void boostRange(float start, float end, int buffer_vertices, float min);
		void decayBoosts(float mult);

		Colour colour_;
		Colour fillColorFrom_;
		Colour fillColorTo_;

		float lineWidth_ = kDefaultLineWidth;
		bool fill_ = false;
		float fillCenter_ = 0.0f;
		bool fit_ = false;

		float boostAmount_ = 0.0f;
		float fillBoostAmount_ = 0.0f;

		bool dirty_ = false;

		int pointCount_ = 0;
		int lineVerticesCount_;
		int fillVerticesCount_;
		int lineFloatsCount_;
		int fillFloatsCount_;
		bool shouldUpdateBufferSizes_ = true;

		OpenGlShaderProgram lineShader_;
		OpenGlUniform lineScaleUniform_;
		OpenGlUniform lineColourUniform_;
		OpenGlUniform lineWidthUniform_;
		OpenGlAttribute linePosition_;

		OpenGlShaderProgram fillShader_;
		OpenGlUniform fillScaleUniform_;
		OpenGlUniform fillColourFromUniform_;
		OpenGlUniform fillColourToUniform_;
		OpenGlUniform fillCenterUniform_;
		OpenGlUniform fillBoostAmountUniform_;
		OpenGlAttribute fillPosition_;

		GLuint lineBuffer_ = 0;
		GLuint fillBuffer_ = 0;
		GLuint indicesBuffer_ = 0;

		utils::up<float[]> x_;
		utils::up<float[]> y_;
		utils::up<float[]> boosts_;
		utils::up<float[]> lineData_;
		utils::up<float[]> fillData_;
		utils::up<int[]> indicesData_;
	};
}
