/*
  ==============================================================================

    OpenGlBackground.h
    Created: 5 Dec 2022 11:56:28pm
    Author:  Lenovo

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "OpenGlComponent.h"

namespace Interface
{
  class OpenGlBackground
  {
  public:
    OpenGlBackground() : image_shader_(nullptr), vertices_()
    {
      new_background_ = false;
      vertex_buffer_ = 0;
      triangle_buffer_ = 0;
    }

    void updateBackgroundImage(Image background);

    void init(OpenGlWrapper &open_gl);
    void render(OpenGlWrapper &open_gl);
    void destroy(OpenGlWrapper &open_gl);

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }

    OpenGLShaderProgram *shader() { return image_shader_; }
    OpenGLShaderProgram::Uniform *texture_uniform() { return texture_uniform_.get(); }

    void bind();
    void enableAttributes();
    void disableAttributes();

  private:
    OpenGLShaderProgram *image_shader_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> texture_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> position_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> texture_coordinates_;

    float vertices_[16];

    std::mutex mutex_;
    OpenGLTexture background_;
    bool new_background_;
    Image background_image_;

    GLuint vertex_buffer_;
    GLuint triangle_buffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlBackground)
  };
}
