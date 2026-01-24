/*
	==============================================================================

		OpenGlQuad.cpp
		Created: 14 Dec 2022 2:05:11am
		Author:  theuser27

	==============================================================================
*/

#include "OpenGlQuad.hpp"

namespace Interface
{
	OpenGlMultiQuad::OpenGlMultiQuad(utils::span<float> data, FragmentShader shader) :
		fragmentShader{ COMPLEX_MOVE(shader) }, data{ data }, maxQuads{ (u32)data.size() / kFloatsPerQuad }, numQuads{ maxQuads }
	{
		for (usize i = 0; i < maxQuads; i++)
		{
			setCoordinates(i, -1.0f, -1.0f, 2.0f, 2.0f);
			setShaderValue(i, 1.0f);
		}
	}

	static void destroyQuad(OpenGlMultiQuad *quad)
	{
		if (quad->vertexShader.indicesBuffer)
			glDeleteBuffers(1, &quad->vertexShader.indicesBuffer);
		quad->vertexShader = {};

		quad->componentFlags.isOpenGlInitialised = false;
		quad->componentFlags.destroyOpenGl = false;
	}

	static void updateDimensions(utils::span<float> data,
		Rectangle<int> viewportBounds, usize numQuads)
	{
		float fullWidth = (float)viewportBounds.w;
		float fullHeight = (float)viewportBounds.h;

		for (usize i = 0; i < numQuads; ++i)
		{
			usize index = i * OpenGlMultiQuad::kFloatsPerQuad;
			float quadWidth = data[2 * OpenGlMultiQuad::kFloatsPerVertex + index] - data[index];
			float quadHeight = data[2 * OpenGlMultiQuad::kFloatsPerVertex + index + 1] - data[index + 1];
			float w = quadWidth * fullWidth / 2.0f;
			float h = quadHeight * fullHeight / 2.0f;
			data[index + 2] = w;
			data[index + 3] = h;
			data[OpenGlMultiQuad::kFloatsPerVertex + index + 2] = w;
			data[OpenGlMultiQuad::kFloatsPerVertex + index + 3] = h;
			data[2 * OpenGlMultiQuad::kFloatsPerVertex + index + 2] = w;
			data[2 * OpenGlMultiQuad::kFloatsPerVertex + index + 3] = h;
			data[3 * OpenGlMultiQuad::kFloatsPerVertex + index + 2] = w;
			data[3 * OpenGlMultiQuad::kFloatsPerVertex + index + 3] = h;
		}
	}

	bool OpenGlMultiQuad::render(OpenGlWrapper &openGl)
	{
		if (componentFlags.isOpenGlInitialised && componentFlags.destroyOpenGl)
		{
			destroyQuad(this);
			return true;
		}

		auto viewportBounds = getLocalBounds();

		if (!componentFlags.isOpenGlInitialised)
		{
			{
				glGenBuffers(1, &vertexShader.shaderId);
				glBindBuffer(GL_ARRAY_BUFFER, vertexShader.shaderId);

				updateDimensions(data, viewportBounds, numQuads);
				glBufferData(GL_ARRAY_BUFFER, data.sizeBytes(), data.data(), GL_STATIC_DRAW);
			}

			{
				static constexpr int triangles[] =
				{
					0, 1, 2,
					2, 3, 0
				};

				glGenBuffers(1, &vertexShader.indicesBuffer);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexShader.indicesBuffer);

				auto indices = utils::up<int[]>::create(maxQuads * kIndicesPerQuad);
				for (usize i = 0; i < maxQuads; i++)
					for (usize j = 0; j < kIndicesPerQuad; j++)
						indices[i * kIndicesPerQuad + j] = triangles[j] + (int)i * (int)kIndicesPerQuad;

				glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxQuads * kIndicesPerQuad * sizeof(int), 
					indices.get(), GL_STATIC_DRAW);
			}

			vertexShader.shaderId = openGl.shaders->addVertexShader(vertexShader.key, vertexShader.code);
			*fragmentShader.shaderId = openGl.shaders->addFragmentShader(fragmentShader.key, fragmentShader.code);

			shaderProgram = openGl.shaders->getShaderProgram(vertexShader.shaderId, *fragmentShader.shaderId);
			shaderProgram.use();
			vertexShader.getVariables(shaderProgram);
			fragmentShader.getVariables(shaderProgram);

			componentFlags.isOpenGlInitialised = true;
		}

		if (!setViewport(getPosition(), viewportBounds, viewportBounds, openGl, ignoreClipIncluding))
			return false;

		// setup
		glEnable(GL_BLEND);
		glEnable(GL_SCISSOR_TEST);
		if (additiveBlending)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		COMPLEX_ASSERT(numQuads <= maxQuads);

		shaderProgram.use();

		if (isDirty)
		{
			updateDimensions(data, viewportBounds, numQuads);
			glBindBuffer(GL_ARRAY_BUFFER, vertexShader.vertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, data.sizeBytes(), data.data(), GL_STATIC_DRAW);
		}

		fragmentShader.setUniforms();

		COMPLEX_CHECK_OPENGL_ERROR();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexShader.indicesBuffer);
		vertexShader.enableAttributes();

		// render
		glDrawElements(GL_TRIANGLES, (GLsizei)(numQuads * kIndicesPerQuad), GL_UNSIGNED_INT, nullptr);

		// clean-up
		vertexShader.disableAttributes();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);

		return true;
	}
}
