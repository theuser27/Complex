/*
  ==============================================================================

    open_gl_primitives.cpp
    Created: 9 Apr 2024 5:39:44pm
    Author:  theuser27

  ==============================================================================
*/

#include "open_gl_primitives.hpp"
#include <juce_opengl/juce_opengl.h>

#include "platform_definitions.hpp"
#include "utils.hpp"
#include "Interface/LookAndFeel/BaseComponent.hpp"

namespace
{
#if COMPLEX_DEBUG
  const char* getGLErrorMessage (const GLenum e) noexcept
  {
  #define CASE_GEN(x) case juce::gl::x: return #x;
    switch (e)
    {
      CASE_GEN(GL_INVALID_ENUM)
      CASE_GEN(GL_INVALID_VALUE)
      CASE_GEN(GL_INVALID_OPERATION)
      CASE_GEN(GL_OUT_OF_MEMORY)
    #ifdef GL_STACK_OVERFLOW
      CASE_GEN(GL_STACK_OVERFLOW)
    #endif
    #ifdef GL_STACK_UNDERFLOW
      CASE_GEN(GL_STACK_UNDERFLOW)
    #endif
    #ifdef GL_INVALID_FRAMEBUFFER_OPERATION
      CASE_GEN(GL_INVALID_FRAMEBUFFER_OPERATION)
    #endif
    default: break;
    }
  #undef CASE_GEN
    return "Unknown error";
  }
#endif
}

namespace Interface
{
#if COMPLEX_DEBUG
  void checkGLError(const char *file, const int line)
  {
    for (;;)
    {
      GLenum e = juce::gl::glGetError();

      if (e == juce::gl::GL_NO_ERROR)
        break;

      DBG("***** " << getGLErrorMessage(e) << "  at " << file << " : " << line);
      jassertfalse;
    }
  }
#endif
  using namespace juce::gl;

  void OpenGlUniform::set(GLfloat n1) const noexcept { glUniform1f(uniformId, n1); }
  void OpenGlUniform::set(GLint n1) const noexcept { glUniform1i(uniformId, n1); }
  void OpenGlUniform::set(GLfloat n1, GLfloat n2) const noexcept { glUniform2f(uniformId, n1, n2); }
  void OpenGlUniform::set(GLfloat n1, GLfloat n2, GLfloat n3) const noexcept { glUniform3f(uniformId, n1, n2, n3); }
  void OpenGlUniform::set(GLfloat n1, GLfloat n2, GLfloat n3, GLfloat n4) const noexcept { glUniform4f(uniformId, n1, n2, n3, n4); }
  void OpenGlUniform::set(GLint n1, GLint n2, GLint n3, GLint n4) const noexcept { glUniform4i(uniformId, n1, n2, n3, n4); }
  void OpenGlUniform::set(const GLfloat *values, GLsizei numValues) const noexcept { glUniform1fv(uniformId, numValues, values); }

  void OpenGlUniform::setMatrix2(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix2fv(uniformId, count, transpose, values); }
  void OpenGlUniform::setMatrix3(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix3fv(uniformId, count, transpose, values); }
  void OpenGlUniform::setMatrix4(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix4fv(uniformId, count, transpose, values); }

  static std::pair<int, int> createTexture(juce::OpenGLContext &context, GLuint &textureId,
    int desiredW, int desiredH, const void *pixels, GLenum type, bool topLeft, GLenum texMagFilter = GL_LINEAR)
  {
    if (textureId == 0)
    {
      COMPLEX_CHECK_OPENGL_ERROR;
      glGenTextures(1, &textureId);
      glBindTexture(GL_TEXTURE_2D, textureId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texMagFilter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      COMPLEX_CHECK_OPENGL_ERROR;
    }
    else
    {
      glBindTexture(GL_TEXTURE_2D, textureId);
      COMPLEX_CHECK_OPENGL_ERROR;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    COMPLEX_CHECK_OPENGL_ERROR;

    const auto textureNpotSupported = context.isTextureNpotSupported();
    const auto getAllowedTextureSize = [&](int n)
    {
      return textureNpotSupported ? n : juce::nextPowerOfTwo(n);
    };

    auto width = getAllowedTextureSize(desiredW);
    auto height = getAllowedTextureSize(desiredH);

    const GLint internalformat = type == GL_ALPHA ? GL_ALPHA : GL_RGBA;

    if (width != desiredW || height != desiredH)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, internalformat,
        width, height, 0, type, GL_UNSIGNED_BYTE, nullptr);

      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, topLeft ? (height - desiredH) : 0,
        desiredW, desiredH, type, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
      glTexImage2D(GL_TEXTURE_2D, 0, internalformat,
        desiredW, desiredH, 0, type, GL_UNSIGNED_BYTE, pixels);
    }

    COMPLEX_CHECK_OPENGL_ERROR;

    return { width, height };
  }

  template <class PixelType>
  static void flip(juce::HeapBlock<juce::PixelARGB> &dataCopy, const u8 *srcData, 
    const int lineStride, const int w, const int h)
  {
    dataCopy.malloc(w * h);

    for (int y = 0; y < h; ++y)
    {
      auto *src = (const PixelType *)srcData;
      auto *dst = (juce::PixelARGB *)(dataCopy + w * (h - 1 - y));

      for (int x = 0; x < w; ++x)
        dst[x].set(src[x]);

      srcData += lineStride;
    }
  }

  std::pair<int, int> loadImageAsTexture(juce::OpenGLContext &context,
    GLuint &textureId, const juce::Image &image, GLenum texMagFilter)
  {
    auto imageW = image.getWidth();
    auto imageH = image.getHeight();

    juce::HeapBlock<juce::PixelARGB> dataCopy;
    juce::Image::BitmapData srcData(image, juce::Image::BitmapData::readOnly);

    switch (srcData.pixelFormat)
    {
    case juce::Image::ARGB:           flip<juce::PixelARGB> (dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::RGB:            flip<juce::PixelRGB>  (dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::SingleChannel:  flip<juce::PixelAlpha>(dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::UnknownFormat:
    default: break;
    }

    return createTexture(context, textureId, imageW, imageH, dataCopy, GL_BGRA_EXT, true, texMagFilter);
  }

  std::pair<int, int> loadARGBAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter)
  {
    return createTexture(context, textureId, desiredW, desiredH, 
      pixels, GL_BGRA_EXT, false, texMagFilter);
  }

  std::pair<int, int> loadAlphaAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const u8 *pixels, int desiredW, int desiredH, GLenum texMagFilter)
  {
    return createTexture(context, textureId, desiredW, desiredH, 
      pixels, GL_ALPHA, false, texMagFilter);
  }

  std::pair<int, int> loadARGBFlippedAsTexture(juce::OpenGLContext &context, GLuint &textureId,
    const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter)
  {
    juce::HeapBlock<juce::PixelARGB> flippedCopy;
    flip<juce::PixelARGB>(flippedCopy, (const u8 *)pixels, 4 * desiredW, desiredW, desiredH);

    return createTexture(context, textureId, desiredW, desiredH, 
      flippedCopy, GL_BGRA_EXT, true, texMagFilter);
  }

  OpenGlShaderProgram::OpenGlShaderProgram(const juce::OpenGLContext &c) noexcept : context(c) { }

  OpenGlShaderProgram::~OpenGlShaderProgram() noexcept { release(); }

  void OpenGlShaderProgram::release() noexcept
  {
    if (programId != 0)
    {
      glDeleteProgram(programId);
      programId = 0;
    }
  }

  double OpenGlShaderProgram::getLanguageVersion()
  {
    return juce::String::fromUTF8((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION))
      .retainCharacters("1234567890.").getDoubleValue();
  }

  bool OpenGlShaderProgram::addShader(const juce::String &code, GLenum type)
  {
    GLuint shaderID = glCreateShader(type);

    const GLchar *c = code.toRawUTF8();
    glShaderSource(shaderID, 1, &c, nullptr);

    glCompileShader(shaderID);

    GLint status = GL_FALSE;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

    if (status == (GLint)GL_FALSE)
    {
      std::vector<GLchar> infoLog(16384);
      GLsizei infoLogLength = 0;
      glGetShaderInfoLog(shaderID, (GLsizei)infoLog.size(), &infoLogLength, infoLog.data());

    #if JUCE_DEBUG && ! JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR
      // Your GLSL code contained compile errors!
      // Hopefully this compile log should help to explain what went wrong.
      DBG(juce::String(infoLog.data(), (usize)infoLogLength));
      jassertfalse;
    #endif

      return false;
    }

    glAttachShader(getProgramId(), shaderID);
    glDeleteShader(shaderID);
    COMPLEX_CHECK_OPENGL_ERROR;
    return true;
  }

  bool OpenGlShaderProgram::addVertexShader(const juce::String &code) { return addShader(code, GL_VERTEX_SHADER); }
  bool OpenGlShaderProgram::addFragmentShader(const juce::String &code) { return addShader(code, GL_FRAGMENT_SHADER); }

  bool OpenGlShaderProgram::link() noexcept
  {
    // This method can only be used when the current thread has an active OpenGL context.
    jassert(juce::OpenGLHelpers::isContextActive());

    GLuint progID = getProgramId();

    glLinkProgram(progID);

    GLint status = GL_FALSE;
    glGetProgramiv(progID, GL_LINK_STATUS, &status);

    if (status == (GLint)GL_FALSE)
    {
      std::vector<GLchar> infoLog(16384);
      GLsizei infoLogLength = 0;
      glGetProgramInfoLog(progID, (GLsizei)infoLog.size(), &infoLogLength, infoLog.data());

    #if JUCE_DEBUG && ! JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR
      // Your GLSL code contained link errors!
      // Hopefully this compile log should help to explain what went wrong.
      DBG(juce::String(infoLog.data(), (usize)infoLogLength));
      jassertfalse;
    #endif
    }

    COMPLEX_CHECK_OPENGL_ERROR;
    return status != (GLint)GL_FALSE;
  }

  void OpenGlShaderProgram::use() const noexcept
  {
    // The shader program must have been successfully linked when this method is called!
    jassert(programId != 0);

    glUseProgram(programId);
  }

  GLuint OpenGlShaderProgram::getProgramId() const noexcept
  {
    if (programId == 0)
    {
      // This method should only be called when the current thread has an active OpenGL context.
      jassert(juce::OpenGLHelpers::isContextActive());

      programId = glCreateProgram();
    }

    return programId;
  }

  OpenGlWrapper::OpenGlWrapper(juce::OpenGLContext &c) noexcept : context(c) { }

  OpenGlWrapper::~OpenGlWrapper() noexcept = default;

}