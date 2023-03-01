/*
  ==============================================================================

    OpenGlBackground.cpp
    Created: 5 Dec 2022 11:56:28pm
    Author:  Lenovo

  ==============================================================================
*/

#include "OpenGlBackground.h"

#include "OpenGlComponent.h"
#include "../LookAndFeel/Shaders.h"

namespace Interface
{
  using namespace juce::gl;

  void OpenGlBackground::init(OpenGlWrapper &openGl)
  {
    static const float vertices[] = {
      -1.0f, 1.0f, 0.0f, 1.0f,
      -1.0f, -1.0f, 0.0f, 0.0f,
      1.0f, -1.0f, 1.0f, 0.0f,
      1.0f, 1.0f, 1.0f, 1.0f
    };

    memcpy(vertices_, vertices, 16 * sizeof(float));

    static const int triangles[] = {
      0, 1, 2,
      2, 3, 0
    };

    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

    GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(sizeof(vertices)));
    glBufferData(GL_ARRAY_BUFFER, vert_size, vertices_, GL_STATIC_DRAW);

    glGenBuffers(1, &triangle_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);

    GLsizeiptr tri_size = static_cast<GLsizeiptr>(static_cast<size_t>(sizeof(triangles)));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tri_size, triangles, GL_STATIC_DRAW);

    image_shader_ = openGl.shaders->getShaderProgram(Shaders::kImageVertex, Shaders::kImageFragment);
    image_shader_->use();
    position_ = OpenGlComponent::getAttribute(*image_shader_, "position");
    texture_coordinates_ = OpenGlComponent::getAttribute(*image_shader_, "tex_coord_in");
    texture_uniform_ = OpenGlComponent::getUniform(*image_shader_, "image");
  }

  void OpenGlBackground::destroy(OpenGlWrapper &open_gl)
  {
    if (background_.getWidth())
      background_.release();

    image_shader_ = nullptr;
    position_ = nullptr;
    texture_coordinates_ = nullptr;
    texture_uniform_ = nullptr;

    glDeleteBuffers(1, &vertex_buffer_);
    glDeleteBuffers(1, &triangle_buffer_);
  }

  void OpenGlBackground::bind()
  {
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);
    background_.bind();
  }

  void OpenGlBackground::enableAttributes()
  {
    if (position_ != nullptr)
    {
      glVertexAttribPointer(position_->attributeID, 2, GL_FLOAT,
        GL_FALSE, 4 * sizeof(float), nullptr);
      glEnableVertexAttribArray(position_->attributeID);
    }
    if (texture_coordinates_ != nullptr)
    {
      glVertexAttribPointer(texture_coordinates_->attributeID, 2, GL_FLOAT,
        GL_FALSE, 4 * sizeof(float),
        (GLvoid *)(2 * sizeof(float)));
      glEnableVertexAttribArray(texture_coordinates_->attributeID);
    }
  }

  void OpenGlBackground::disableAttributes()
  {
    if (position_ != nullptr)
      glDisableVertexAttribArray(position_->attributeID);

    if (texture_coordinates_ != nullptr)
      glDisableVertexAttribArray(texture_coordinates_->attributeID);
  }

  void OpenGlBackground::render(OpenGlWrapper &open_gl)
  {
    mutex_.lock();
    if ((new_background_ || background_.getWidth() == 0) && background_image_.getWidth() > 0)
    {
      new_background_ = false;
      background_.loadImage(background_image_);

      float width_ratio = (1.0f * background_.getWidth()) / background_image_.getWidth();
      float height_ratio = (1.0f * background_.getHeight()) / background_image_.getHeight();
      float width_end = 2.0f * width_ratio - 1.0f;
      float height_end = 1.0f - 2.0f * height_ratio;

      vertices_[8] = vertices_[12] = width_end;
      vertices_[5] = vertices_[9] = height_end;

      glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
      GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(16 * sizeof(float)));
      glBufferData(GL_ARRAY_BUFFER, vert_size, vertices_, GL_STATIC_DRAW);
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    image_shader_->use();
    bind();
    glActiveTexture(GL_TEXTURE0);

    if (texture_uniform_ != nullptr && background_.getWidth())
      texture_uniform_->set(0);

    enableAttributes();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    disableAttributes();
    background_.unbind();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    mutex_.unlock();
  }

  void OpenGlBackground::updateBackgroundImage(Image background)
  {
    background_image_ = background;
    new_background_ = true;
  }
}
