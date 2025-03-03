/*
  ==============================================================================

    Shaders.hpp
    Created: 16 Nov 2022 6:47:15am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "juce_opengl/opengl/juce_gl.h"

#include "Framework/vector_map.hpp"

namespace juce
{
  template<typename T>
  class Rectangle;
  class OpenGLContext;
  class OpenGLShaderProgram;
  class Image;
  class PixelARGB;
}

namespace Interface
{
#if COMPLEX_DEBUG
  void checkGLError(const char *file, const int line);
  #define COMPLEX_CHECK_OPENGL_ERROR checkGLError(__FILE__, __LINE__)
#else
  #define COMPLEX_CHECK_OPENGL_ERROR
#endif

  enum class OpenGlAllocatedResource : u8 { Buffer, Texture };

  struct OpenGlUniform
  {
    /** Sets a vecn float uniform. */
    void set(GLfloat n1) const noexcept { juce::gl::glUniform1f(uniformId, n1); }
    void set(GLfloat n1, GLfloat n2) const noexcept { juce::gl::glUniform2f(uniformId, n1, n2); }
    void set(GLfloat n1, GLfloat n2, GLfloat n3) const noexcept { juce::gl::glUniform3f(uniformId, n1, n2, n3); }
    void set(GLfloat n1, GLfloat n2, GLfloat n3, GLfloat n4) const noexcept { juce::gl::glUniform4f(uniformId, n1, n2, n3, n4); }
    /** Sets a vector float uniform. */
    void set(const GLfloat *values, GLsizei numValues) const noexcept { juce::gl::glUniform1fv(uniformId, numValues, values); }
    /** Sets an vecn int uniform. */
    void set(GLint n1) const noexcept { juce::gl::glUniform1i(uniformId, n1); }
    void set(GLint n1, GLint n2, GLint n3, GLint n4) const noexcept { juce::gl::glUniform4i(uniformId, n1, n2, n3, n4); }
    /** Sets a nxn matrix float uniform. */
    void setMatrix2(const GLfloat *values, GLint count, GLboolean transpose) const noexcept
    { juce::gl::glUniformMatrix2fv(uniformId, count, transpose, values); }
    void setMatrix3(const GLfloat *values, GLint count, GLboolean transpose) const noexcept
    { juce::gl::glUniformMatrix3fv(uniformId, count, transpose, values); }
    void setMatrix4(const GLfloat *values, GLint count, GLboolean transpose) const noexcept
    { juce::gl::glUniformMatrix4fv(uniformId, count, transpose, values); }

    explicit operator bool() { return uniformId >= 0; }

    // If the uniform couldn't be found, this value will be < 0.
    GLint uniformId = -1;
  };

  struct OpenGlAttribute
  {
    explicit operator bool() { return attributeId >= 0; }

    // If the uniform couldn't be found, this value will be < 0.
    GLint attributeId = -1;
  };

  struct OpenGlShaderProgram
  {
    void use() const noexcept
    {
      COMPLEX_ASSERT(id != 0);
      juce::gl::glUseProgram(id);
    }

    GLuint id = 0;
  };


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

    OpenGlShaderProgram getShaderProgram(VertexShader vertexShader, 
      FragmentShader fragmentShader, const GLchar **varyings = nullptr);

    void releaseAll();

  private:
    GLuint createVertexShader(VertexShader shader) const;
    GLuint createFragmentShader(FragmentShader shader) const;

    utils::VectorMap<int, OpenGlShaderProgram> shaderPrograms_;

    GLuint vertexShaderIds_[kVertexShaderCount]{};
    GLuint fragmentShaderIds_[kFragmentShaderCount]{};
  };


  struct ViewportChange;
  class OpenGlComponent;
  class BaseComponent;

  struct OpenGlWrapper
  {
    OpenGlWrapper(juce::OpenGLContext &c) noexcept;
    ~OpenGlWrapper() noexcept;

    std::vector<ViewportChange> parentStack;
    juce::OpenGLContext &context;
    Shaders *shaders = nullptr;
    int topLevelHeight = 0;
    bool animate = true;
  };

  bool setViewPort(const BaseComponent &target, const OpenGlComponent &renderSource, juce::Rectangle<int> viewportBounds,
    juce::Rectangle<int> scissorBounds, const OpenGlWrapper &openGl, const BaseComponent *ignoreClipIncluding);

  auto loadImageAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::Image &image, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> utils::pair<int, int>;
  auto loadARGBAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> utils::pair<int, int>;
  auto loadAlphaAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const u8 *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> utils::pair<int, int>;
  auto loadARGBFlippedAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> utils::pair<int, int>;

  inline OpenGlUniform getUniform(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlUniform{ juce::gl::glGetUniformLocation(program.id, name) }; }
  inline OpenGlAttribute getAttribute(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlAttribute{ juce::gl::glGetAttribLocation(program.id, name) }; }
}
