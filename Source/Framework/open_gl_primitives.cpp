/*
  ==============================================================================

    open_gl_primitives.cpp
    Created: 9 Apr 2024 5:39:44pm
    Author:  theuser27

  ==============================================================================
*/

#include "open_gl_primitives.h"
#include <juce_opengl/juce_opengl.h>

#include "platform_definitions.h"
#include "utils.h"

namespace
{
  #if JUCE_DEBUG && ! defined (JUCE_CHECK_OPENGL_ERROR)
  const char* getGLErrorMessage (const GLenum e) noexcept
  {
      switch (e)
      {
          case juce::gl::GL_INVALID_ENUM:         return "GL_INVALID_ENUM";
          case juce::gl::GL_INVALID_VALUE:        return "GL_INVALID_VALUE";
          case juce::gl::GL_INVALID_OPERATION:    return "GL_INVALID_OPERATION";
          case juce::gl::GL_OUT_OF_MEMORY:        return "GL_OUT_OF_MEMORY";
         #ifdef GL_STACK_OVERFLOW
          case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
         #endif
         #ifdef GL_STACK_UNDERFLOW
          case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
         #endif
         #ifdef GL_INVALID_FRAMEBUFFER_OPERATION
          case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
         #endif
          default: break;
      }

      return "Unknown error";
  }

  #if JUCE_MAC || JUCE_IOS

   #ifndef JUCE_IOS_MAC_VIEW
    #if JUCE_IOS
     #define JUCE_IOS_MAC_VIEW    UIView
     #define JUCE_IOS_MAC_WINDOW  UIWindow
    #else
     #define JUCE_IOS_MAC_VIEW    NSView
     #define JUCE_IOS_MAC_WINDOW  NSWindow
    #endif
   #endif

  #endif

  bool checkPeerIsValid (juce::OpenGLContext* context)
  {
      jassert (context != nullptr);

      if (context != nullptr)
      {
          if (auto* comp = context->getTargetComponent())
          {
              if (auto* peer [[maybe_unused]] = comp->getPeer())
              {
                 #if JUCE_MAC || JUCE_IOS
                  if (auto* nsView = (JUCE_IOS_MAC_VIEW*) peer->getNativeHandle())
                  {
                      if ([[maybe_unused]] auto nsWindow = [nsView window])
                      {
                         #if JUCE_MAC
                          return ([nsWindow isVisible]
                                    && (! [nsWindow hidesOnDeactivate] || [NSApp isActive]));
                         #else
                          return true;
                         #endif
                      }
                  }
                 #else
                  return true;
                 #endif
              }
          }
      }

      return false;
  }

  void checkGLError (const char* file, const int line)
  {
      for (;;)
      {
          const GLenum e = juce::gl::glGetError();

          if (e == juce::gl::GL_NO_ERROR)
              break;

          // if the peer is not valid then ignore errors
          if (! checkPeerIsValid (juce::OpenGLContext::getCurrentContext()))
              continue;

          DBG ("***** " << getGLErrorMessage (e) << "  at " << file << " : " << line);
          jassertfalse;
      }
  }

   #define JUCE_CHECK_OPENGL_ERROR checkGLError (__FILE__, __LINE__);
  #else
   #define JUCE_CHECK_OPENGL_ERROR ;
  #endif
}

namespace Interface
{
	using namespace juce::gl;

  OpenGlUniform::OpenGlUniform(const OpenGlShaderProgram &program, const char *const name)
		: uniformID(glGetUniformLocation(program.getProgramID(), name)) { COMPLEX_ASSERT(uniformID >= 0); }

	void OpenGlUniform::set(GLfloat n1) const noexcept { glUniform1f(uniformID, n1); }
	void OpenGlUniform::set(GLint n1) const noexcept { glUniform1i(uniformID, n1); }
	void OpenGlUniform::set(GLfloat n1, GLfloat n2) const noexcept { glUniform2f(uniformID, n1, n2); }
	void OpenGlUniform::set(GLfloat n1, GLfloat n2, GLfloat n3) const noexcept { glUniform3f(uniformID, n1, n2, n3); }
	void OpenGlUniform::set(GLfloat n1, GLfloat n2, GLfloat n3, GLfloat n4) const noexcept { glUniform4f(uniformID, n1, n2, n3, n4); }
	void OpenGlUniform::set(GLint n1, GLint n2, GLint n3, GLint n4) const noexcept { glUniform4i(uniformID, n1, n2, n3, n4); }
	void OpenGlUniform::set(const GLfloat *values, GLsizei numValues) const noexcept { glUniform1fv(uniformID, numValues, values); }

	void OpenGlUniform::setMatrix2(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix2fv(uniformID, count, transpose, values); }
	void OpenGlUniform::setMatrix3(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix3fv(uniformID, count, transpose, values); }
	void OpenGlUniform::setMatrix4(const GLfloat *values, GLint count, GLboolean transpose) const noexcept { glUniformMatrix4fv(uniformID, count, transpose, values); }

	OpenGlAttribute::OpenGlAttribute(const OpenGlShaderProgram &program, const char *name)
		: attributeID((GLuint)glGetAttribLocation(program.getProgramID(), name)) { COMPLEX_ASSERT((GLint)attributeID >= 0); }

  OpenGlTexture::~OpenGlTexture() { release(); }

  bool OpenGlTexture::isValidSize(int width, int height)
  {
    return utils::isPowerOfTwo(width) && utils::isPowerOfTwo(height);
  }

  void OpenGlTexture::create(const int w, const int h, const void *pixels, GLenum type, bool topLeft)
  {
    ownerContext = juce::OpenGLContext::getCurrentContext();

    // Texture objects can only be created when the current thread has an active OpenGL
    // context. You'll need to create this object in one of the OpenGLContext's callbacks.
    jassert(ownerContext != nullptr);

    if (textureID == 0)
    {
      JUCE_CHECK_OPENGL_ERROR
        glGenTextures(1, &textureID);
      glBindTexture(GL_TEXTURE_2D, textureID);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      auto glMagFilter = (GLint)(texMagFilter == linear ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glMagFilter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      JUCE_CHECK_OPENGL_ERROR
    }
    else
    {
      glBindTexture(GL_TEXTURE_2D, textureID);
      JUCE_CHECK_OPENGL_ERROR;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    JUCE_CHECK_OPENGL_ERROR

      const auto textureNpotSupported = ownerContext->isTextureNpotSupported();

    const auto getAllowedTextureSize = [&](int n)
    {
      return textureNpotSupported ? n : juce::nextPowerOfTwo(n);
    };

    width = getAllowedTextureSize(w);
    height = getAllowedTextureSize(h);

    const GLint internalformat = type == GL_ALPHA ? GL_ALPHA : GL_RGBA;

    if (width != w || height != h)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, internalformat,
        width, height, 0, type, GL_UNSIGNED_BYTE, nullptr);

      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, topLeft ? (height - h) : 0, w, h,
        type, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
      glTexImage2D(GL_TEXTURE_2D, 0, internalformat,
        w, h, 0, type, GL_UNSIGNED_BYTE, pixels);
    }

    JUCE_CHECK_OPENGL_ERROR
  }

  template <class PixelType>
  struct Flipper
  {
    static void flip(juce::HeapBlock<juce::PixelARGB> &dataCopy, const u8 *srcData, const int lineStride,
      const int w, const int h)
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
  };

  void OpenGlTexture::loadImage(const juce::Image &image)
  {
    const int imageW = image.getWidth();
    const int imageH = image.getHeight();

    juce::HeapBlock<juce::PixelARGB> dataCopy;
    juce::Image::BitmapData srcData(image, juce::Image::BitmapData::readOnly);

    switch (srcData.pixelFormat)
    {
    case juce::Image::ARGB:           Flipper<juce::PixelARGB> ::flip(dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::RGB:            Flipper<juce::PixelRGB>  ::flip(dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::SingleChannel:  Flipper<juce::PixelAlpha>::flip(dataCopy, srcData.data, srcData.lineStride, imageW, imageH); break;
    case juce::Image::UnknownFormat:
    default: break;
    }

    create(imageW, imageH, dataCopy, juce::JUCE_RGBA_FORMAT, true);
  }

  void OpenGlTexture::loadARGB(const juce::PixelARGB *pixels, const int w, const int h)
  {
    create(w, h, pixels, juce::JUCE_RGBA_FORMAT, false);
  }

  void OpenGlTexture::loadAlpha(const u8 *pixels, int w, int h)
  {
    create(w, h, pixels, GL_ALPHA, false);
  }

  void OpenGlTexture::loadARGBFlipped(const juce::PixelARGB *pixels, int w, int h)
  {
    juce::HeapBlock<juce::PixelARGB> flippedCopy;
    Flipper<juce::PixelARGB>::flip(flippedCopy, (const u8 *)pixels, 4 * w, w, h);

    create(w, h, flippedCopy, juce::JUCE_RGBA_FORMAT, true);
  }

  void OpenGlTexture::release()
  {
    if (textureID != 0)
    {
      // If the texture is deleted while the owner context is not active, it's
      // impossible to delete it, so this will be a leak until the context itself
      // is deleted.
      jassert(ownerContext == juce::OpenGLContext::getCurrentContext());

      if (ownerContext == juce::OpenGLContext::getCurrentContext())
      {
        glDeleteTextures(1, &textureID);

        textureID = 0;
        width = 0;
        height = 0;
      }
    }
  }

  void OpenGlTexture::bind() const
  {
    glBindTexture(GL_TEXTURE_2D, textureID);
  }

  void OpenGlTexture::unbind() const
  {
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  OpenGlShaderProgram::OpenGlShaderProgram(const juce::OpenGLContext &c) noexcept : context(c) { }

  OpenGlShaderProgram::~OpenGlShaderProgram() noexcept { release(); }

  void OpenGlShaderProgram::release() noexcept
  {
    if (programID != 0)
    {
      glDeleteProgram(programID);
      programID = 0;
    }
  }

  double OpenGlShaderProgram::getLanguageVersion()
  {
    return juce::String::fromUTF8((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION))
      .retainCharacters("1234567890.").getDoubleValue();
  }

  bool OpenGlShaderProgram::addShader(const juce::String &code, GLenum type)
  {
    GLuint shaderID = context.extensions.glCreateShader(type);

    const GLchar *c = code.toRawUTF8();
    context.extensions.glShaderSource(shaderID, 1, &c, nullptr);

    context.extensions.glCompileShader(shaderID);

    GLint status = GL_FALSE;
    context.extensions.glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

    if (status == (GLint)GL_FALSE)
    {
      std::vector<GLchar> infoLog(16384);
      GLsizei infoLogLength = 0;
      context.extensions.glGetShaderInfoLog(shaderID, (GLsizei)infoLog.size(), &infoLogLength, infoLog.data());

    #if JUCE_DEBUG && ! JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR
      // Your GLSL code contained compile errors!
      // Hopefully this compile log should help to explain what went wrong.
      DBG(juce::String(infoLog.data(), (size_t)infoLogLength));
      jassertfalse;
    #endif

      return false;
    }

    context.extensions.glAttachShader(getProgramID(), shaderID);
    context.extensions.glDeleteShader(shaderID);
    JUCE_CHECK_OPENGL_ERROR
      return true;
  }

  bool OpenGlShaderProgram::addVertexShader(const juce::String &code) { return addShader(code, GL_VERTEX_SHADER); }
  bool OpenGlShaderProgram::addFragmentShader(const juce::String &code) { return addShader(code, GL_FRAGMENT_SHADER); }

  bool OpenGlShaderProgram::link() noexcept
  {
    // This method can only be used when the current thread has an active OpenGL context.
    jassert(juce::OpenGLHelpers::isContextActive());

    GLuint progID = getProgramID();

    context.extensions.glLinkProgram(progID);

    GLint status = GL_FALSE;
    context.extensions.glGetProgramiv(progID, GL_LINK_STATUS, &status);

    if (status == (GLint)GL_FALSE)
    {
      std::vector<GLchar> infoLog(16384);
      GLsizei infoLogLength = 0;
      context.extensions.glGetProgramInfoLog(progID, (GLsizei)infoLog.size(), &infoLogLength, infoLog.data());

    #if JUCE_DEBUG && ! JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR
      // Your GLSL code contained link errors!
      // Hopefully this compile log should help to explain what went wrong.
      DBG(juce::String(infoLog.data(), (size_t)infoLogLength));
      jassertfalse;
    #endif
    }

    JUCE_CHECK_OPENGL_ERROR
      return status != (GLint)GL_FALSE;
  }

  void OpenGlShaderProgram::use() const noexcept
  {
    // The shader program must have been successfully linked when this method is called!
    jassert(programID != 0);

    context.extensions.glUseProgram(programID);
  }

  GLuint OpenGlShaderProgram::getProgramID() const noexcept
  {
    if (programID == 0)
    {
      // This method should only be called when the current thread has an active OpenGL context.
      jassert(juce::OpenGLHelpers::isContextActive());

      programID = glCreateProgram();
    }

    return programID;
  }
}