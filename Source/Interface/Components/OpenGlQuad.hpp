
// Created: 2022-12-14 02:05:11

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"
#include "../LookAndFeel/Shaders.hpp"
#include "../LookAndFeel/shader_types.hpp"

namespace Interface
{
	class OpenGlMultiQuad : public Component
	{
	public:
		static constexpr auto kVerticesPerQuad = 4;
		static constexpr auto kFloatsPerVertex = PassthroughVertex::kFloatsPerVertex;
		static constexpr auto kFloatsPerQuad = kVerticesPerQuad * kFloatsPerVertex;
		static constexpr auto kIndicesPerQuad = 6;

		OpenGlMultiQuad(utils::span<float> data, FragmentShader shader);

		bool render(OpenGlWrapper &openGl) override;

		void setCoordinates(usize i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads);
			usize index = i * kFloatsPerQuad;

			data[index + 4] = x;
			data[index + 5] = y;
			data[kFloatsPerVertex + index + 4] = x;
			data[kFloatsPerVertex + index + 5] = y + h;
			data[2 * kFloatsPerVertex + index + 4] = x + w;
			data[2 * kFloatsPerVertex + index + 5] = y + h;
			data[3 * kFloatsPerVertex + index + 4] = x + w;
			data[3 * kFloatsPerVertex + index + 5] = y;
		}
		void setShaderValue(usize i, float shaderValue, usize valueIndex = 0) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads);
			usize index = i * kFloatsPerQuad + 6 + valueIndex;

			data[index] = shaderValue;
			data[kFloatsPerVertex + index] = shaderValue;
			data[2 * kFloatsPerVertex + index] = shaderValue;
			data[3 * kFloatsPerVertex + index] = shaderValue;
		}
		void setQuadHorizontal(usize i, float x, float w) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads);
			usize index = i * kFloatsPerQuad;

			data[index] = x;
			data[kFloatsPerVertex + index] = x;
			data[2 * kFloatsPerVertex + index] = x + w;
			data[3 * kFloatsPerVertex + index] = x + w;
		}
		void setQuadVertical(usize i, float y, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads);
			usize index = i * kFloatsPerQuad;

			data[index + 1] = y;
			data[kFloatsPerVertex + index + 1] = y + h;
			data[2 * kFloatsPerVertex + index + 1] = y + h;
			data[3 * kFloatsPerVertex + index + 1] = y;
		}
		void setQuad(usize i, float x, float y, float w, float h) noexcept
		{
			COMPLEX_ASSERT(i < maxQuads);
			usize index = i * kFloatsPerQuad;

			data[index] = x;
			data[index + 1] = y;
			data[kFloatsPerVertex + index] = x;
			data[kFloatsPerVertex + index + 1] = y + h;
			data[2 * kFloatsPerVertex + index] = x + w;
			data[2 * kFloatsPerVertex + index + 1] = y + h;
			data[3 * kFloatsPerVertex + index] = x + w;
			data[3 * kFloatsPerVertex + index + 1] = y;
		}

		PassthroughVertex vertexShader{};
		FragmentShader fragmentShader{};
		OpenGlShaderProgram shaderProgram{};

		/*
		 *  data array indices per quad
		 *  0 - 1: vertex ndc position
		 *  2 - 3: scaled width and height for quad (acts like a uniform for individual quads)
		 *  4 - 5: coordinates inside the quad (ndc for most situations, normalised for OpenGLCorners)
		 *  6 - 7: shader values (doubles as left channel shader values)
		 *  8 - 9: right channel shader values (necessary for the modulation meters/indicators)
		 */
		utils::span<float> data;
		u32 maxQuads, numQuads;
		Component *ignoreClipIncluding = nullptr;

		bool isDirty = false;
		bool additiveBlending = false;
	};

	struct OpenGlQuad final : public OpenGlMultiQuad
	{
		float vertices[1 * kFloatsPerQuad]{};

		OpenGlQuad(FragmentShader shader) :
			OpenGlMultiQuad{ utils::span{ vertices }, COMPLEX_MOVE(shader) }
		{
			setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
		}
	};

	struct OpenGlCorners final : public OpenGlMultiQuad
	{
		float vertices[4 * kFloatsPerQuad]{};
		RoundedCornerFragment fragment{};

		OpenGlCorners() : OpenGlMultiQuad{ utils::span{ vertices }, FragmentShader{ fragment } }
		{
			setCoordinates(0, 1.0f, 1.0f, -1.0f, -1.0f);
			setCoordinates(1, 1.0f, 0.0f, -1.0f, 1.0f);
			setCoordinates(2, 0.0f, 0.0f, 1.0f, 1.0f);
			setCoordinates(3, 0.0f, 1.0f, 1.0f, -1.0f);
		}

		void setCorners(Rectangle<int> cornerBounds, float rounding)
		{
			float width = rounding / (float)cornerBounds.w * 2.0f;
			float height = rounding / (float)cornerBounds.h * 2.0f;

			setQuad(0, -1.0f, -1.0f, width, height);
			setQuad(1, -1.0f, 1.0f - height, width, height);
			setQuad(2, 1.0f - width, 1.0f - height, width, height);
			setQuad(3, 1.0f - width, -1.0f, width, height);
		}

		void setCorners(Rectangle<int> cornerBounds, float topRounding, float bottomRounding)
		{
			float topWidth = topRounding / (float)cornerBounds.w * 2.0f;
			float topHeight = topRounding / (float)cornerBounds.h * 2.0f;
			float bottomWidth = bottomRounding / (float)cornerBounds.w * 2.0f;
			float bottomHeight = bottomRounding / (float)cornerBounds.h * 2.0f;

			setQuad(0, -1.0f, -1.0f, bottomWidth, bottomHeight);
			setQuad(1, -1.0f, 1.0f - topHeight, topWidth, topHeight);
			setQuad(2, 1.0f - topWidth, 1.0f - topHeight, topWidth, topHeight);
			setQuad(3, 1.0f - bottomWidth, -1.0f, bottomWidth, bottomHeight);
		}

		void setTopCorners(Rectangle<int> cornerBounds, float topRounding)
		{
			float width = topRounding / (float)cornerBounds.w * 2.0f;
			float height = topRounding / (float)cornerBounds.h * 2.0f;

			setQuad(0, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(1, -1.0f, 1.0f - height, width, height);
			setQuad(2, 1.0f - width, 1.0f - height, width, height);
			setQuad(3, -2.0f, -2.0f, 0.0f, 0.0f);
		}

		void setBottomCorners(Rectangle<int> cornerBounds, float bottomRounding)
		{
			float width = bottomRounding / (float)cornerBounds.w * 2.0f;
			float height = bottomRounding / (float)cornerBounds.h * 2.0f;
			
			setQuad(0, -1.0f, -1.0f, width, height);
			setQuad(1, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(2, -2.0f, -2.0f, 0.0f, 0.0f);
			setQuad(3, 1.0f - width, -1.0f, width, height);
		}
	};
}
