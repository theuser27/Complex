
// Created: 2022-11-16 20:05:31

#pragma once

#include "Framework/stl_utils.hpp"
#include "Shaders.hpp"

namespace Interface
{
  #define CONSTRAIN_AXIS_FUNCTION																								\
    "float constrainAxis(float normAxis, float constraint, float offset) {\n"		\
    "    return clamp(ceil(-abs(normAxis + offset) + constraint), 0.0, 1.0);\n" \
    "}\n"

  #define CONSTRAIN_AXIS_VEC2_FUNCTION																					\
    "vec2 constrainAxis(vec2 normAxis, vec2 constraint, vec2 offset) {\n"				\
    "    return clamp(ceil(-abs(normAxis + offset) + constraint), 0.0, 1.0);\n"	\
    "}\n"

  COMPLEX_DEFINE_VERTEX_SHADER(PassthroughVertex,
    (
      static constexpr auto kFloatsPerVertex = 10;
      GLuint indicesBuffer = 0;
    ), 
    (), 
    ((vec2, position), (vec2, dimensions), (vec2, coordinates), (vec4, shaderValues)),
    ((vec2, dimensionsOut), (vec2, coordinatesOut), (vec4, shaderValuesOut)),
    R"(
      void main()
      {
        dimensionsOut = dimensions;
        coordinatesOut = coordinates;
        shaderValuesOut = shaderValues;
        gl_Position = vec4(position, 0.0, 1.0);
      }
    )"
  )

  COMPLEX_DEFINE_VERTEX_SHADER(ImageVertex, 
    (
      static constexpr int kNumPositions = 16;
      static constexpr int kNumTriangleIndices = 6;
      static constexpr int kPositionTriangles[kNumTriangleIndices] = 
      {
        0, 1, 2,
        2, 3, 0
      };
      static constexpr float KInitPositionVertices[kNumPositions] =
      {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        1.0f,  1.0f, 1.0f, 1.0f
      };
      GLuint indicesBuffer = 0;
    ), 
    (),
    ((vec3, position), (vec2, textureCoordIn)),
    ((vec2, textureCoordOut)),
    R"(
      void main()
      {
        textureCoordOut = textureCoordIn;
        gl_Position = vec4(position, 1.0);
      }
    )"
  )

  COMPLEX_DEFINE_VERTEX_SHADER(ScaleVertex, 
    (), 
    ((vec2, scale)),
    ((vec3, position)),
    (),
    R"(
      void main()
      {
        gl_Position.x = position.x * scale.x;
        gl_Position.y = position.y * scale.y;
        gl_Position.z = position.z;
        gl_Position.a = 1.0;
      }
    )"
  )

  COMPLEX_DEFINE_FRAGMENT_SHADER(RoundedCornerFragment,
    (),
    ((vec4, colour)),
    ((vec2, dimensionsOut), (vec2, coordinatesOut)),
    R"(
      void main()
      {
        float delta_center = length(coordinatesOut * dimensionsOut);
        float alpha = clamp(delta_center - dimensionsOut.x + 0.5, 0.0, 1.0);
        fragColor = vec4(color.r, color.g, color.b, color.a * alpha);
      }
    )"
  )

  COMPLEX_DEFINE_FRAGMENT_SHADER(ColourFragment,
    (),
    ((vec4, colour)),
    (),
    R"(
      void main()
      {
        fragColor = color;
      }
    )"
  )

  COMPLEX_DEFINE_FRAGMENT_SHADER(TintedImageFragment,
    (
      GLuint imageId = 0;
    ),
    ((vec4, colour)),
    ((vec2, texCoordOut)),
    R"(
      uniform sampler2D image;
      void main()
      {
        vec4 image_color = texture(image, texCoordOut);
        image_color.r *= color.r;
        image_color.g *= color.g;
        image_color.b *= color.b;
        image_color.a *= color.a;
        fragColor = image_color;
      }
    )"
  )
}
