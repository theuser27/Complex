/*
	==============================================================================

		Shaders.h
		Created: 16 Nov 2022 6:47:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"

namespace Interface
{
	class Shaders
	{
	public:
		enum VertexShader
		{
			kImageVertex,
			kPassthroughVertex,
			kScaleVertex,
			kRotaryModulationVertex,
			kLinearModulationVertex,
			kGainMeterVertex,
			kLineVertex,
			kFillVertex,
			kBarHorizontalVertex,
			kBarVerticalVertex,
			kNumVertexShaders
		};

		enum FragmentShader
		{
			kImageFragment,
			kTintedImageFragment,
			kGainMeterFragment,
			kColorFragment,
			kFadeSquareFragment,
			kCircleFragment,
			kRingFragment,
			kDiamondFragment,
			kRoundedCornerFragment,
			kRoundedRectangleFragment,
			kRoundedRectangleBorderFragment,
			kRotarySliderFragment,
			kRotaryModulationFragment,
			kHorizontalSliderFragment,
			kVerticalSliderFragment,
			kPinSliderFragment,
			kDotSliderFragment,
			kLinearModulationFragment,
			kModulationKnobFragment,
			kLineFragment,
			kFillFragment,
			kBarFragment,
			kNumFragmentShaders
		};

		Shaders(OpenGLContext &openGlContext) : openGlContext_(&openGlContext) { }

		GLuint getVertexShaderId(VertexShader shader) {
			if (vertexShaderIds_[shader] == 0)
				vertexShaderIds_[shader] = createVertexShader(shader);
			return vertexShaderIds_[shader];
		}

		GLuint getFragmentShaderId(FragmentShader shader) {
			if (fragmentShaderIds_[shader] == 0)
				fragmentShaderIds_[shader] = createFragmentShader(shader);
			return fragmentShaderIds_[shader];
		}

		OpenGLShaderProgram *getShaderProgram(VertexShader vertex_shader, FragmentShader fragment_shader,
			const GLchar **varyings = nullptr);

	private:
		static const char *getVertexShader(VertexShader shader);
		static const char *getFragmentShader(FragmentShader shader);

		bool checkShaderCorrect(GLuint shaderId) const;
		GLuint createVertexShader(VertexShader shader) const;
		GLuint createFragmentShader(FragmentShader shader) const;

		OpenGLContext *openGlContext_;
		GLuint vertexShaderIds_[kNumVertexShaders]{};
		GLuint fragmentShaderIds_[kNumFragmentShaders]{};

		std::map<int, std::unique_ptr<OpenGLShaderProgram>> shaderPrograms_;
	};

	struct OpenGlWrapper
	{
		OpenGlWrapper(OpenGLContext &c) : context(c), shaders(nullptr), display_scale(1.0f) { }

		OpenGLContext &context;
		Shaders *shaders;
		float display_scale;
	};
}
