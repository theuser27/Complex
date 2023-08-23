/*
  ==============================================================================

    OpenGlLineRenderer.h
    Created: 3 Feb 2023 10:14:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "OpenGlComponent.h"

namespace Interface
{
  class OpenGlLineRenderer : public OpenGlComponent
  {
  public:
    static constexpr int kLineFloatsPerVertex = 3;
    static constexpr int kFillFloatsPerVertex = 4;
    static constexpr int kLineVerticesPerPoint = 6;
    static constexpr int kFillVerticesPerPoint = 2;
    static constexpr int kLineFloatsPerPoint = kLineVerticesPerPoint * kLineFloatsPerVertex;
    static constexpr int kFillFloatsPerPoint = kFillVerticesPerPoint * kFillFloatsPerVertex;

    OpenGlLineRenderer(int num_points, bool loop = false);
    ~OpenGlLineRenderer() override = default;

    void init(OpenGlWrapper &open_gl) override;
    void render(OpenGlWrapper &open_gl, bool animate) override;
    void destroy() override;

    strict_inline float getBoostLeftAt(int index) const noexcept { return boost_left_[index]; }
    strict_inline float getBoostRightAt(int index) const noexcept { return boost_right_[index]; }
    strict_inline float getYAt(int index) const noexcept { return y_[index]; }
    strict_inline float getXAt(int index) const noexcept { return x_[index]; }

    strict_inline int getNumPoints() const noexcept { return num_points_; }
    strict_inline Colour getColor() const noexcept { return color_; }
    strict_inline bool getAnyBoostValue() const noexcept { return any_boost_value_; }

    strict_inline void setColor(Colour color) noexcept { color_ = color; }
    strict_inline void setLineWidth(float width) noexcept { line_width_ = width; }
    strict_inline void setBoost(float boost) noexcept { boost_ = boost; }

    strict_inline void setBoostLeft(int index, float val) noexcept
    {
      boost_left_[index] = val;
      dirty_ = true;
      COMPLEX_ASSERT(num_points_ > index);
    }
    strict_inline void setBoostRight(int index, float val) noexcept
    {
      boost_right_[index] = val;
      dirty_ = true;
      COMPLEX_ASSERT(num_points_ > index);
    }
    strict_inline void setYAt(int index, float val) noexcept
    {
      y_[index] = val;
      dirty_ = true;
      COMPLEX_ASSERT(num_points_ > index);
    }
    strict_inline void setXAt(int index, float val) noexcept
    {
      x_[index] = val;
      dirty_ = true;
      COMPLEX_ASSERT(num_points_ > index);
    }

    void setFillVertices(bool left);
    void setLineVertices(bool left);

    strict_inline void setFill(bool fill) noexcept { fill_ = fill; }
    strict_inline void setFillColor(Colour fill_color) noexcept { setFillColors(fill_color, fill_color); }
    strict_inline void setFillColors(Colour fill_color_from, Colour fill_color_to) noexcept
    {
      fill_color_from_ = fill_color_from;
      fill_color_to_ = fill_color_to;
    }
    strict_inline void setFillCenter(float fill_center) noexcept { fill_center_ = fill_center; }
    strict_inline void setFit(bool fit) noexcept { fit_ = fit; }

    strict_inline void setBoostAmount(float boost_amount) noexcept { boost_amount_ = boost_amount; }
    strict_inline void setFillBoostAmount(float boost_amount) noexcept { fill_boost_amount_ = boost_amount; }
    strict_inline void setIndex(int index) noexcept { index_ = index; }
    strict_inline void setEnableBackwardBoost(bool enable) noexcept { enable_backward_boost_ = enable; }

    void boostLeftRange(float start, float end, int buffer_vertices, float min);
    void boostRightRange(float start, float end, int buffer_vertices, float min);
    void boostRange(float *boosts, float start, float end, int buffer_vertices, float min);
    void boostRange(simd_float start, simd_float end, int buffer_vertices, simd_float min);
    void decayBoosts(simd_float mult);

    void drawLines(OpenGlWrapper &open_gl, bool left);


  private:
    Colour color_;
    Colour fill_color_from_;
    Colour fill_color_to_;

    int num_points_;
    float line_width_;
    float boost_ = 0.0f;
    bool fill_ = false;
    float fill_center_ = 0.0f;
    bool fit_ = false;

    float boost_amount_ = 0.0f;
    float fill_boost_amount_ = 0.0f;
    bool enable_backward_boost_ = true;
    int index_ = 0;

    bool dirty_ = false;
    bool last_drawn_left_ = false;
    bool last_negative_boost_;
    bool loop_;
    bool any_boost_value_ = false;
    int num_padding_;
    int num_line_vertices_;
    int num_fill_vertices_;
    int num_line_floats_;
    int num_fill_floats_;

    OpenGLShaderProgram *shader_ = nullptr;
    std::unique_ptr<OpenGLShaderProgram::Uniform> scale_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> color_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> boost_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> line_width_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> position_;

    OpenGLShaderProgram *fill_shader_ = nullptr;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fill_scale_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fill_color_from_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fill_color_to_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fill_center_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> fill_boost_amount_uniform_;
    std::unique_ptr<OpenGLShaderProgram::Attribute> fill_position_;

    GLuint vertex_array_object_;
    GLuint line_buffer_;
    GLuint fill_buffer_;
    GLuint indices_buffer_;

    std::unique_ptr<float[]> x_;
    std::unique_ptr<float[]> y_;
    std::unique_ptr<float[]> boost_left_;
    std::unique_ptr<float[]> boost_right_;
    std::unique_ptr<float[]> line_data_;
    std::unique_ptr<float[]> fill_data_;
    std::unique_ptr<int[]> indices_data_;

    // TODO: template the extents later
    //stdex::mdspan<float, stdex::extents<u32, stdex::dynamic_extent, stdex::dynamic_extent>> data2d{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlLineRenderer)
  };
}
