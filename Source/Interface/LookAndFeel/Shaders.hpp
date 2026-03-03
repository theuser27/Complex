
// Created: 2022-11-16 06:47:15

#pragma once

#include "glad/glad.h"

#include "Framework/memory.hpp"
#include "Framework/macro_helpers.hpp"
#include "gui_utils.hpp"
#include "ui_constants.hpp"

extern "C"
{
  typedef struct NVGcontext NVGcontext;
}

namespace Interface
{
  template<typename T>
  struct OpenGlUniform
  {
    static constexpr auto size = sizeof(T) / sizeof(utils::remove_extent_t<T>);
    static constexpr bool isFloat = utils::is_floating_point_v<utils::remove_extent_t<T>>;
    static constexpr bool isArray = utils::is_array_v<T>;

    OpenGlUniform &operator=(const OpenGlUniform &) = default;

    template<typename U>
    OpenGlUniform &
    operator=(U rhs)
    {
      if constexpr (size == 1)
        data = rhs;
      else if constexpr (size == 2)
      {
        auto [one, two] = rhs;
        data[0] = one;
        data[1] = two;
      }
      else if constexpr (size == 3)
      {
        auto [one, two, three] = rhs;
        data[0] = one;
        data[1] = two;
        data[2] = three;
      }
      else if constexpr (size == 4)
      {
        auto [one, two, three, four] = rhs;
        data[0] = one;
        data[1] = two;
        data[2] = three;
        data[3] = four;
      }

      return *this;
    }

    void set() const
    {
      COMPLEX_ASSERT(uniformId >= 0);

      if constexpr (!isArray)
      {
        if constexpr (isFloat)
          glUniform1f(uniformId, data);
        else
          glUniform1i(uniformId, data);
      }
      else
      {
        if constexpr (size == 2)
        {
          if constexpr (isFloat)
            glUniform2f(uniformId, data[0], data[1]);
          else
            glUniform2i(uniformId, data[0], data[1]);
        }
        else if constexpr (size == 3)
        {
          if constexpr (isFloat)
            glUniform3f(uniformId, data[0], data[1], data[2]);
          else
            glUniform3i(uniformId, data[0], data[1], data[2]);
        }
        else if constexpr (size == 4)
        {
          if constexpr (isFloat)
            glUniform4f(uniformId, data[0], data[1], data[2], data[3]);
          else
            glUniform4i(uniformId, data[0], data[1], data[2], data[3]);
        }
      }
    }

    explicit operator bool() const { return uniformId >= 0; }

    // If the uniform couldn't be found, this value will be < 0.
    GLint uniformId = -1;
    T data{};
  };

  struct OpenGlAttribute
  {
    explicit operator bool() { return (GLint)attributeId >= 0; }

    // If the uniform couldn't be found, this value will be < 0.
    GLuint attributeId = (GLuint)-1;
  };

  struct OpenGlBuffer
  {
    ~OpenGlBuffer() noexcept { if (id) glDeleteBuffers(1, &id); }
    GLuint id = 0;
  };

  struct OpenGlTexture
  {
    ~OpenGlTexture() noexcept { if (id) glDeleteTextures(1, &id); }
    GLuint id = 0;
  };

  struct OpenGlShaderProgram
  {
    void use() const noexcept
    {
      COMPLEX_ASSERT(id != 0);
      glUseProgram(id);
    }

    GLuint id = 0;
  };


  #define COMPLEX_INTERNAL_UNIFORM_DECLARATION(x, y) <x> y{}
  #define COMPLEX_INTERNAL_IN_DECLARATION(x, y) y
  #define COMPLEX_INTERNAL_SHADER_DECLARATION(x, y) #x #y
  #define COMPLEX_INTERNAL_NAME_DECLARATION(x, y) #y
  #define COMPLEX_INTERNAL_IN_PARAMETER(x, y) utils::pair{ utils::type_identity<x>{}, ins.y }
  
  #define COMPLEX_INTERNAL_DEFINE_GL_TYPES                            \
    using uint = u32;                                                 \
    using vec2 = float[2]; using ivec2 = i32[2]; using uvec2 = u32[2];\
    using vec3 = float[3]; using ivec3 = i32[3]; using uvec3 = u32[3];\
    using vec4 = float[4]; using ivec4 = i32[4]; using uvec4 = u32[4];



  #define COMPLEX_DEFINE_VERTEX_SHADER(name, extraVariables, uniformsPack, insPack, outsPack, ...)\
    struct name                                                                                   \
    {                                                                                             \
      COMPLEX_INTERNAL_DEFINE_GL_TYPES                                                            \
                                                                                                  \
      static constexpr utils::typeInfo key = typeId(name);                                        \
      static constexpr const char *uniformNames[] = { COMPLEX_FOR_EACH(                           \
        COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_NAME_DECLARATION, (, (,)),    \
        COMPLEX_DEPAREN(uniformsPack)) "" };                                                      \
      static constexpr const char *insNames[] = { COMPLEX_FOR_EACH(                               \
        COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_NAME_DECLARATION, (, (,)),    \
        COMPLEX_DEPAREN(insPack)) "" };                                                           \
      static constexpr char code[] =                                                              \
        "#version 150\n"                                                                          \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_SHADER_DECLARATION,    \
          ("uniform ", ";\n"), COMPLEX_DEPAREN(uniformsPack))                                     \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_SHADER_DECLARATION,    \
          ("in ", ";\n"), COMPLEX_DEPAREN(insPack))                                               \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_SHADER_DECLARATION,    \
          ("out ", ";\n"), COMPLEX_DEPAREN(outsPack))                                             \
        __VA_ARGS__;                                                                              \
                                                                                                  \
      struct                                                                                      \
      {                                                                                           \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_UNIFORM_DECLARATION,   \
          (OpenGlUniform, ;), COMPLEX_DEPAREN(uniformsPack))                                      \
      } uniforms{};                                                                               \
      struct                                                                                      \
      {                                                                                           \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION,        \
          (OpenGlAttribute, ;), COMPLEX_DEPAREN(insPack))                                         \
      } ins{};                                                                                    \
                                                                                                  \
      void getVariables(OpenGlShaderProgram program)                                              \
      {                                                                                           \
        [[maybe_unused]] usize i = 0;                                                             \
                                                                                                  \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION,        \
          (uniforms., .uniformId = glGetUniformLocation(program.id, uniformNames[i++]);),         \
          COMPLEX_DEPAREN(uniformsPack))                                                          \
                                                                                                  \
        i = 0;                                                                                    \
                                                                                                  \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION, (ins., \
          .attributeId = (GLuint)glGetAttribLocation(program.id, insNames[i++]));,                \
          COMPLEX_DEPAREN(insPack))                                                               \
      }                                                                                           \
                                                                                                  \
      void setUniforms()                                                                          \
      {                                                                                           \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION,        \
          (uniforms., .set());, COMPLEX_DEPAREN(uniformsPack))                                    \
      }                                                                                           \
                                                                                                  \
      void enableAttributes()                                                                     \
      {                                                                                           \
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);                                              \
        auto iterate = []<typename ... Ts>(utils::pair<utils::type_identity<Ts>,                  \
          OpenGlAttribute> ... args)                                                              \
        {                                                                                         \
          static constexpr auto cycle = (sizeof(Ts) + ...);                                       \
          usize offset = 0;                                                                       \
          auto function = [&offset]<typename T>(auto arg)                                         \
          {                                                                                       \
            using underlying = utils::remove_extent_t<T>;                                         \
            constexpr auto size = sizeof(T) / sizeof(underlying);                                 \
            GLenum type = utils::is_floating_point_v<underlying> ?                                \
              GL_FLOAT : (underlying(-1) < underlying(0)) ? GL_INT : GL_UNSIGNED_INT;             \
                                                                                                  \
            if (arg)                                                                              \
            {                                                                                     \
              glVertexAttribPointer(arg.attributeId, size, type, GL_FALSE,                        \
                cycle, (GLvoid *)offset);                                                         \
              glEnableVertexAttribArray(arg.attributeId);                                         \
            }                                                                                     \
            offset += sizeof(T);                                                                  \
          };                                                                                      \
                                                                                                  \
          (function.template operator()<Ts>(args.second), ...);                                   \
        };                                                                                        \
                                                                                                  \
        iterate(COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE_EXCLUSIVE,                       \
          COMPLEX_INTERNAL_IN_PARAMETER, (, (,)), COMPLEX_DEPAREN(insPack)));                     \
      }                                                                                           \
                                                                                                  \
      void disableAttributes()                                                                    \
      {                                                                                           \
        OpenGlAttribute attributes[] = { COMPLEX_FOR_EACH(                                        \
          COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_IN_DECLARATION,             \
          (ins., (,)), COMPLEX_DEPAREN(insPack)) };                                               \
        for (usize i = 0; i < sizeof(attributes) / sizeof(attributes[0]); ++i)                    \
          if (attributes[i])                                                                      \
            glDisableVertexAttribArray(attributes[i].attributeId);                                \
                                                                                                  \
        glBindBuffer(GL_ARRAY_BUFFER, 0);                                                         \
      }                                                                                           \
                                                                                                  \
      GLuint vertexBuffer = 0;                                                                    \
      GLuint shaderId = 0;                                                                        \
                                                                                                  \
      COMPLEX_DEPAREN(extraVariables)                                                             \
    };

  #define COMPLEX_DEFINE_FRAGMENT_SHADER(name, extraVariables, uniformsPack, insPack, ...)        \
    struct name                                                                                   \
    {                                                                                             \
      COMPLEX_INTERNAL_DEFINE_GL_TYPES                                                            \
                                                                                                  \
      static constexpr utils::typeInfo key = typeId(name);                                        \
      static constexpr const char *uniformNames[] = { COMPLEX_FOR_EACH(                           \
        COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_NAME_DECLARATION,             \
        (, (,)), COMPLEX_DEPAREN(uniformsPack)) "" };                                             \
      static constexpr char code[] =                                                              \
        "#version 150\n"                                                                          \
        "out vec4 fragColor;\n"                                                                   \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_SHADER_DECLARATION,    \
          ("uniform ", ";\n"), COMPLEX_DEPAREN(uniformsPack))                                     \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_SHADER_DECLARATION,    \
          ("in ", ";\n"), COMPLEX_DEPAREN(insPack))                                               \
        __VA_ARGS__;                                                                              \
                                                                                                  \
      struct                                                                                      \
      {                                                                                           \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_UNIFORM_DECLARATION,   \
          (OpenGlUniform, ;), COMPLEX_DEPAREN(uniformsPack))                                      \
      } uniforms{};                                                                               \
                                                                                                  \
      void getVariables(OpenGlShaderProgram program)                                              \
      {                                                                                           \
        [[maybe_unused]] usize i = 0;                                                             \
                                                                                                  \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION,        \
          (uniforms., .uniformId = glGetUniformLocation(program.id, uniformNames[i++]);),         \
          COMPLEX_DEPAREN(uniformsPack))                                                          \
      }                                                                                           \
                                                                                                  \
      void setUniforms()                                                                          \
      {                                                                                           \
        COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_IN_DECLARATION,        \
          (uniforms., .set());, COMPLEX_DEPAREN(uniformsPack))                                    \
      }                                                                                           \
                                                                                                  \
      GLuint shaderId = 0;                                                                        \
                                                                                                  \
      COMPLEX_DEPAREN(extraVariables)                                                             \
    };

  struct FragmentShader
  {
    void *object = nullptr;
    utils::typeInfo key{};
    const char *code = nullptr;
    GLuint *shaderId = nullptr;
    void (*getVariablesPointer)(void *, OpenGlShaderProgram program) = nullptr;
    void (*setUniformsPointer)(void *) = nullptr;

    constexpr FragmentShader() = default;

    template<typename T>
    constexpr FragmentShader(T &shaderObject)
    {
      object = &shaderObject;
      key = shaderObject.key;
      code = shaderObject.code;
      shaderId = &shaderObject.shaderId;
      getVariablesPointer = [](void *object, OpenGlShaderProgram program) 
      { ((T *)object)->getVariables(program); };
      setUniformsPointer = [](void *object) { ((T *)object)->setUniforms(); };
    }
    
    void getVariables(OpenGlShaderProgram program) { getVariablesPointer(object, program); }
    void setUniforms() { setUniformsPointer(object); }
  };

  GLuint createShader(const char *shader, bool isFragment = true);

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

    Shaders(utils::bumpArena *parentArena)
    {
      arena = utils::bumpArena::createNested(parentArena, COMPLEX_KB(8));
      vertexShaders.data.reserve(arena, 32);
      fragmentShaders.data.reserve(arena, 32);
      shaderPrograms.data.reserve(arena, 32);
    }

    OpenGlShaderProgram getShaderProgram(GLuint vertexShaderId,
      GLuint fragmentShaderId, const GLchar **varyings = nullptr);

    void releaseAll();

    GLuint addVertexShader(utils::typeInfo key, const char *shader)
    {
      if (auto iter = vertexShaders.find(key); iter != vertexShaders.data.end())
        return iter->second;

      auto id = createShader(shader, false);
      vertexShaders.add(key, id);
      return id;
    }

    GLuint getVertexShader(utils::typeInfo key)
    {
      if (auto iter = vertexShaders.find(key); iter != vertexShaders.data.end())
        return iter->second;

      return 0;
    }

    GLuint addFragmentShader(utils::typeInfo key, const char *shader)
    {
      if (auto iter = fragmentShaders.find(key); iter != fragmentShaders.data.end())
        return iter->second;

      auto id = createShader(shader, true);
      fragmentShaders.add(key, id);
      return id;
    }

    GLuint getFragmentShader(utils::typeInfo key)
    {
      if (auto iter = fragmentShaders.find(key); iter != fragmentShaders.data.end())
        return iter->second;

      return 0;
    }

    utils::bumpArena *arena{};
    utils::vector_map<u64, OpenGlShaderProgram> shaderPrograms;
    utils::vector_map<utils::typeInfo, GLuint> vertexShaders;
    utils::vector_map<utils::typeInfo, GLuint> fragmentShaders;
  };
  
  struct OpenGlWrapper;

  void clearWithColour(OpenGlWrapper &openGl, Colour colour, Rectangle<int> bounds);

  //auto loadImageAsTexture(juce::OpenGLContext &context, GLuint &textureId,
  //  const juce::Image &image, GLenum texMagFilter = GL_LINEAR)
  //  -> utils::pair<int, int>;
  //auto loadARGBAsTexture(juce::OpenGLContext &context, GLuint &textureId,
  //  const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = GL_LINEAR)
  //  -> utils::pair<int, int>;
  //auto loadAlphaAsTexture(juce::OpenGLContext &context, GLuint &textureId,
  //  const u8 *pixels, int desiredW, int desiredH, GLenum texMagFilter = GL_LINEAR)
  //  -> utils::pair<int, int>;
  //auto loadARGBFlippedAsTexture(juce::OpenGLContext &context, GLuint &textureId,
  //  const juce::PixelARGB *pixels, int desiredW, int desiredH, GLenum texMagFilter = GL_LINEAR)
  //  -> utils::pair<int, int>;

  template<typename T>
  inline OpenGlUniform<T> getUniform(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlUniform<T>{ glGetUniformLocation(program.id, name) }; }
  inline OpenGlAttribute getAttribute(const OpenGlShaderProgram &program, const char *name)
  { return OpenGlAttribute{ (GLuint)glGetAttribLocation(program.id, name) }; }
}
