/*
	==============================================================================

		Shaders.h
		Created: 16 Nov 2022 6:47:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "AppConfig.h"
#include <juce_opengl/juce_opengl.h>

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

			kVertexShaderCount
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
			kPlusFragment,
			kHighlightFragment,
			kDotSliderFragment,
			kLinearModulationFragment,
			kModulationKnobFragment,
			kLineFragment,
			kFillFragment,
			kBarFragment,

			kFragmentShaderCount
		};

		Shaders(juce::OpenGLContext &openGlContext) : openGlContext_(openGlContext) { }

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

		juce::OpenGLShaderProgram *getShaderProgram(VertexShader vertex_shader, 
		                                            FragmentShader fragment_shader, const GLchar **varyings = nullptr);

	private:
		static const char *getVertexShader(VertexShader shader);
		static const char *getFragmentShader(FragmentShader shader);

		bool checkShaderCorrect(GLuint shaderId) const;
		GLuint createVertexShader(VertexShader shader) const;
		GLuint createFragmentShader(FragmentShader shader) const;

		juce::OpenGLContext &openGlContext_;
		std::array<GLuint, kVertexShaderCount> vertexShaderIds_{};
		std::array<GLuint, kFragmentShaderCount> fragmentShaderIds_{};

		std::map<int, std::unique_ptr<juce::OpenGLShaderProgram>> shaderPrograms_;
	};

	struct OpenGlWrapper
	{
		OpenGlWrapper(juce::OpenGLContext &c) : context(c) { }

		juce::OpenGLContext &context;
		Shaders *shaders = nullptr;
	};
}
