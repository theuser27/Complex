/*
  ==============================================================================

    open_gl_primitives.hpp
    Created: 9 Apr 2024 5:39:44pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <vector>

#include "juce_opengl/opengl/juce_gl.h"

#include "platform_definitions.hpp"

namespace juce
{
  class String;
  template<typename T>
  class Rectangle;
  class PixelARGB;
  class Image;
  class OpenGLContext;
  class OpenGLShaderProgram;
}

namespace Interface
{
  class OpenGlShaderProgram;
  class OpenGlComponent;
  class Shaders;
  struct ViewportChange;
  class BaseComponent;

#if COMPLEX_DEBUG
  void checkGLError(const char *file, const int line);
  #define COMPLEX_CHECK_OPENGL_ERROR checkGLError(__FILE__, __LINE__)
#else
  #define COMPLEX_CHECK_OPENGL_ERROR
#endif

  enum class OpenGlAllocatedResource { Buffer, Texture };

  struct OpenGlUniform
  {
    /** Sets a vecn float uniform. */
    void set(GLfloat n1) const noexcept;
    void set(GLfloat n1, GLfloat n2) const noexcept;
    void set(GLfloat n1, GLfloat n2, GLfloat n3) const noexcept;
    void set(GLfloat n1, GLfloat n2, GLfloat n3, GLfloat n4) const noexcept;
    /** Sets a vector float uniform. */
    void set(const GLfloat *values, GLsizei numValues) const noexcept;
    /** Sets an vecn int uniform. */
    void set(GLint n1) const noexcept;
    void set(GLint n1, GLint n2, GLint n3, GLint n4) const noexcept;
    /** Sets a nxn matrix float uniform. */
    void setMatrix2(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;
    void setMatrix3(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;
    void setMatrix4(const GLfloat *values, GLint count, GLboolean transpose) const noexcept;

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

  class OpenGlShaderProgram
  {
  public:
    /** Creates a shader for use in a particular GL context. */
    OpenGlShaderProgram(const juce::OpenGLContext &) noexcept;

    /** Destructor. */
    ~OpenGlShaderProgram() noexcept;

    /** Returns the version of GLSL that the current context supports.
        E.g.
        @code
        if (OpenGLShaderProgram::getLanguageVersion() > 1.199)
        {
            // ..do something that requires GLSL 1.2 or above..
        }
        @endcode
    */
    static double getLanguageVersion();

    /** Compiles and adds a shader to this program.

        After adding all your shaders, remember to call link() to link them into
        a usable program.

        If your app is built in debug mode, this method will assert if the program
        fails to compile correctly.

        The shaderType parameter could be GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, etc.

        @returns  true if the shader compiled successfully. If not, you can call
                  getLastError() to find out what happened.
    */
    bool addShader(const juce::String &shaderSourceCode, GLenum shaderType);

    /** Compiles and adds a fragment shader to this program.
        This is equivalent to calling addShader() with a type of GL_VERTEX_SHADER.
    */
    bool addVertexShader(const juce::String &shaderSourceCode);

    /** Compiles and adds a fragment shader to this program.
        This is equivalent to calling addShader() with a type of GL_FRAGMENT_SHADER.
    */
    bool addFragmentShader(const juce::String &shaderSourceCode);

    /** Links all the compiled shaders into a usable program.
        If your app is built in debug mode, this method will assert if the program
        fails to link correctly.
        @returns  true if the program linked successfully. If not, you can call
                  getLastError() to find out what happened.
    */
    bool link() noexcept;

    /** Selects this program into the current context. */
    void use() const noexcept;

    /** Deletes the program. */
    void release() noexcept;

    /** The ID number of the compiled program. */
    GLuint getProgramId() const noexcept;

  private:
    const juce::OpenGLContext &context;
    mutable GLuint programId = 0;
  };

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

  bool setViewPort(const BaseComponent &target, const OpenGlComponent &renderSource, const juce::Rectangle<int> &viewportBounds,
    const juce::Rectangle<int> &scissorBounds, const OpenGlWrapper &openGl, const BaseComponent *ignoreClipIncluding);

  auto loadImageAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::Image &image, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> std::pair<int, int>;
  auto loadARGBAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> std::pair<int, int>;
  auto loadAlphaAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const u8 *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> std::pair<int, int>;
  auto loadARGBFlippedAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = juce::gl::GL_LINEAR)
    -> std::pair<int, int>;

  inline OpenGlUniform getUniform(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlUniform{ juce::gl::glGetUniformLocation(program.getProgramId(), name) }; }
  inline OpenGlAttribute getAttribute(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlAttribute{ juce::gl::glGetAttribLocation(program.getProgramId(), name) }; }
}