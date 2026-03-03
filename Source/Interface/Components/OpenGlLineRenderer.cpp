/*
  ==============================================================================

    OpenGlLineRenderer.cpp
    Created: 3 Feb 2023 10:14:36pm
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlLineRenderer.hpp"

#include "Framework/simd_values.hpp"

namespace
{
  strict_inline float inverseSqrt(float value)
  {
    float x2 = value * 0.5f;
    int i = utils::bit_cast<int>(value);
    i = 0x5f3759DF - (i >> 1);
    value = utils::bit_cast<float>(i);
    value = value * (1.5f - (x2 * value * value));
    value = value * (1.5f - (x2 * value * value));
    return value;
  }

  strict_inline Interface::Point<float> normalise(Interface::Point<float> point)
  { return point * inverseSqrt(point.x * point.x + point.y * point.y); }
}

namespace Interface
{
  void OpenGlLineRenderer::init(OpenGlWrapper &openGl)
  {
    glGenBuffers(1, &lineBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
    GLsizeiptr line_vert_size = static_cast<GLsizeiptr>(lineFloatsCount_ * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, line_vert_size, lineData_.get(), GL_STATIC_DRAW);

    glGenBuffers(1, &fillBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, fillBuffer_);
    GLsizeiptr fill_vert_size = static_cast<GLsizeiptr>(fillFloatsCount_ * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, fill_vert_size, fillData_.get(), GL_STATIC_DRAW);

    glGenBuffers(1, &indicesBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);
    GLsizeiptr line_size = static_cast<GLsizeiptr>(lineVerticesCount_ * sizeof(int));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, line_size, indicesData_.get(), GL_STATIC_DRAW);

    shouldUpdateBufferSizes_ = false;

    lineShader_ = openGl.shaders->getShaderProgram(Shaders::kLineVertex, Shaders::kLineFragment);
    lineShader_.use();
    lineColourUniform_ = getUniform<float[4]>(lineShader_, "color");
    lineScaleUniform_ = getUniform<float[2]>(lineShader_, "scale");
    lineWidthUniform_ = getUniform<float>(lineShader_, "line_width");
    linePosition_ = getAttribute(lineShader_, "position");

    fillShader_ = openGl.shaders->getShaderProgram(Shaders::kFillVertex, Shaders::kFillFragment);
    fillShader_.use();
    fillColourFromUniform_ = getUniform<float[4]>(fillShader_, "color_from");
    fillColourToUniform_ = getUniform<float[4]>(fillShader_, "color_to");
    fillCenterUniform_ = getUniform<float>(fillShader_, "center_position");
    fillBoostAmountUniform_ = getUniform<float>(fillShader_, "boost_amount");
    fillScaleUniform_ = getUniform<float[2]>(fillShader_, "scale");
    fillPosition_ = getAttribute(fillShader_, "position");
  }

  void OpenGlLineRenderer::destroy()
  {
    lineShader_ = {};
    linePosition_ = {};
    lineColourUniform_ = {};
    lineScaleUniform_ = {};
    lineWidthUniform_ = {};

    fillShader_ = {};
    fillColourFromUniform_ = {};
    fillColourToUniform_ = {};
    fillCenterUniform_ = {};
    fillBoostAmountUniform_ = {};
    fillScaleUniform_ = {};
    fillPosition_ = {};

    if (lineBuffer_)
      glDeleteBuffers(1, &lineBuffer_);
    if (fillBuffer_)
			glDeleteBuffers(1, &fillBuffer_);
    if (indicesBuffer_)
			glDeleteBuffers(1, &indicesBuffer_);

    lineBuffer_ = 0;
    fillBuffer_ = 0;
    indicesBuffer_ = 0;
  }

  void OpenGlLineRenderer::setPointCount(int pointCount)
  {
    if (pointCount == pointCount_)
      return;
    
    pointCount_ = pointCount;
    lineVerticesCount_ = kLineVerticesPerPoint * (pointCount_ + 2);
    fillVerticesCount_ = kFillVerticesPerPoint * (pointCount_ + 2);
    lineFloatsCount_ = kLineFloatsPerVertex * lineVerticesCount_;
    fillFloatsCount_ = kFillFloatsPerVertex * fillVerticesCount_;

    x_ = utils::up<float[]>::create(pointCount_);
    y_ = utils::up<float[]>::create(pointCount_);
    boosts_ = utils::up<float[]>::create(pointCount_);

    lineData_ = utils::up<float[]>::create(lineFloatsCount_);
    fillData_ = utils::up<float[]>::create(fillFloatsCount_);
    indicesData_ = utils::up<int[]>::create(lineVerticesCount_);

    for (int i = 0; i < lineVerticesCount_; ++i)
      indicesData_[i] = i;

    for (int i = 0; i < lineFloatsCount_; i += 2 * kLineFloatsPerVertex)
      lineData_[i + 2] = 1.0f;

    shouldUpdateBufferSizes_ = true;
  }

  void OpenGlLineRenderer::render(const OpenGlWrapper &openGl, const Component &target, Rectangle<int> bounds)
  {
    if (!setViewport(target.getPosition(), bounds, bounds, openGl, nullptr))
      return;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);

    if (dirty_)
    {
      setLineVertices((float)target.bounds.w, (float)target.bounds.h);
      setFillVertices((float)target.bounds.w, (float)target.bounds.h);

      if (shouldUpdateBufferSizes_)
      {
        glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
        GLsizeiptr line_vert_size = static_cast<GLsizeiptr>(lineFloatsCount_ * sizeof(float));
        glBufferData(GL_ARRAY_BUFFER, line_vert_size, lineData_.get(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, fillBuffer_);
        GLsizeiptr fill_vert_size = static_cast<GLsizeiptr>(fillFloatsCount_ * sizeof(float));
        glBufferData(GL_ARRAY_BUFFER, fill_vert_size, fillData_.get(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);
        GLsizeiptr line_size = static_cast<GLsizeiptr>(lineVerticesCount_ * sizeof(int));
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, line_size, indicesData_.get(), GL_STATIC_DRAW);

        shouldUpdateBufferSizes_ = false;
      }
      else
      {
        glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
        GLsizeiptr line_vert_size = static_cast<GLsizeiptr>(lineFloatsCount_ * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, line_vert_size, lineData_.get());

        glBindBuffer(GL_ARRAY_BUFFER, fillBuffer_);
        GLsizeiptr fill_vert_size = static_cast<GLsizeiptr>(fillFloatsCount_ * sizeof(float));
        glBufferSubData(GL_ARRAY_BUFFER, 0, fill_vert_size, fillData_.get());
      }

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      dirty_ = false;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesBuffer_);

    float x_shrink = 1.0f;
    float y_shrink = 1.0f;
    if (fit_)
    {
      x_shrink = 1.0f - 0.33f * lineWidth_ / (float)target.bounds.w;
      y_shrink = 1.0f - 0.33f * lineWidth_ / (float)target.bounds.h;
    }

    if (fill_)
    {
      glBindBuffer(GL_ARRAY_BUFFER, fillBuffer_);
      fillShader_.use();

      fillColourFromUniform_.set(fillColorFrom_.getFloatRed(), fillColorFrom_.getFloatGreen(),
        fillColorFrom_.getFloatBlue(), fillColorFrom_.getFloatAlpha());
      fillColourToUniform_.set(fillColorTo_.getFloatRed(), fillColorTo_.getFloatGreen(),
        fillColorTo_.getFloatBlue(), fillColorTo_.getFloatAlpha());

      fillCenterUniform_.set(fillCenter_);
      fillBoostAmountUniform_.set(fillBoostAmount_);
      fillScaleUniform_.set(x_shrink, y_shrink);

      glVertexAttribPointer(fillPosition_.attributeId, kFillFloatsPerVertex, GL_FLOAT,
        GL_FALSE, kFillFloatsPerVertex * sizeof(float), nullptr);
      glEnableVertexAttribArray(fillPosition_.attributeId);
      glDrawElements(GL_TRIANGLE_STRIP, fillVerticesCount_, GL_UNSIGNED_INT, nullptr);
    }

    glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
    lineShader_.use();
    glVertexAttribPointer(linePosition_.attributeId, kLineFloatsPerVertex, GL_FLOAT,
      GL_FALSE, kLineFloatsPerVertex * sizeof(float), nullptr);
    glEnableVertexAttribArray(linePosition_.attributeId);

    lineColourUniform_.set(colour_.getFloatRed(), colour_.getFloatGreen(), colour_.getFloatBlue(), colour_.getFloatAlpha());

    lineScaleUniform_.set(x_shrink, y_shrink);
    lineWidthUniform_.set();

    glDrawElements(GL_TRIANGLE_STRIP, lineVerticesCount_, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(linePosition_.attributeId);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
  }

  void OpenGlLineRenderer::boostRange(float start, float end, int buffer_vertices, float min)
  {
    dirty_ = true;
    float *boosts = boosts_.get();

    int active_points = pointCount_ - 2 * buffer_vertices;
    int start_index = utils::max((int)::ceilf(start * (float)(active_points - 1)), 0);
    float end_position = end * (float)(active_points - 1);
    int end_index = utils::max((int)::ceilf(end_position), 0);
    float progress = end_position - ::truncf(end_position);

    start_index %= active_points;
    end_index %= active_points;

    float delta = (1.0f - min) / (float)(end_index - start_index);
    float val = min;

    for (int i = start_index; i != end_index; i = (i + active_points + 1) % active_points)
    {
      val += delta;
      val = utils::min(1.0f, val);
      float last_value = boosts[i + buffer_vertices];
      boosts[i + buffer_vertices] = utils::max(last_value, val);
    }

    float end_value = boosts[end_index + buffer_vertices];
    boosts[end_index + buffer_vertices] = utils::max(end_value, progress * progress);
  }

  void OpenGlLineRenderer::decayBoosts(float mult)
  {
    bool anyBoost = false;
    float *boosts = boosts_.get();

    for (int i = 0; i < pointCount_; ++i)
    {
      boosts[i] *= mult;
      anyBoost = anyBoost || boosts[i] != 0.0f;
    }

    dirty_ = dirty_ || anyBoost;
  }

  void OpenGlLineRenderer::setFillVertices(float width, float height)
  {
    float *boosts = boosts_.get();

    float x_adjust = 2.0f / width;
    float y_adjust = 2.0f / height;

    for (int i = 0; i < pointCount_; ++i)
    {
      int index_top = (i + 1) * kFillFloatsPerPoint;
      int index_bottom = index_top + kFillFloatsPerVertex;
      float x = x_adjust * x_[i] - 1.0f;
      float y = 1.0f - y_adjust * y_[i];
      fillData_[index_top] = x;
      fillData_[index_top + 1] = y;
      fillData_[index_top + 2] = boosts[i];
      fillData_[index_bottom] = x;
      fillData_[index_bottom + 1] = fillCenter_;
      fillData_[index_bottom + 2] = boosts[i];
    }

    int padding_copy_size = kFillFloatsPerPoint * sizeof(float);
    int begin_copy_source = kFillFloatsPerPoint;
    int end_copy_source = pointCount_ * kFillFloatsPerPoint;

    int end_copy_dest = (pointCount_ + 1) * kFillFloatsPerPoint;
    memcpy(fillData_.get() + end_copy_dest, fillData_.get() + end_copy_source, padding_copy_size);
    memcpy(fillData_.get(), fillData_.get() + begin_copy_source, padding_copy_size);
  }

  void OpenGlLineRenderer::setLineVertices(float width, float height)
  {
    float *boosts = boosts_.get();

    Point<float> prev_normalized_delta;
    for (int i = 0; i < pointCount_ - 1; ++i)
    {
      if (x_[i] != x_[i + 1] || y_[i] != y_[i + 1])
      {
        prev_normalized_delta = normalise({ x_[i + 1] - x_[i], y_[i + 1] - y_[i] });
        break;
      }
    }

    // rotation of +90 degrees
    Point<float> prev_delta_normal{ -prev_normalized_delta.y, prev_normalized_delta.x };
    float line_radius = lineWidth_ * 0.5f + 0.5f;
    float magnitude = line_radius;

    float x_adjust = 2.0f / width;
    float y_adjust = 2.0f / height;

    for (int i = 0; i < pointCount_; ++i)
    {
      float radius = line_radius * (1.0f + boostAmount_ * boosts[i]);
      Point<float> point{ x_[i], y_[i] };
      
      int next_index = utils::min(i + 1, pointCount_ - 1);
      Point<float> delta{ x_[next_index] - point.x, y_[next_index] - point.y };
      if (delta == Point<float>{})
        delta = prev_normalized_delta;

      float next_magnitude = ::sqrtf(delta.x * delta.x + delta.y * delta.y);
      Point<float> normalized_delta{ delta.x / next_magnitude, delta.y / next_magnitude };
      Point<float> delta_normal{ -normalized_delta.y, normalized_delta.x };

      Point<float> angle_bisect_delta = normalized_delta - prev_normalized_delta;
      Point<float> bisect_line;
      bool straight = utils::abs(angle_bisect_delta.x) < 0.001f && utils::abs(angle_bisect_delta.y) < 0.001f;
      if (straight)
        bisect_line = delta_normal;
      else
        bisect_line = normalise(angle_bisect_delta);

      next_magnitude = utils::min(100'000.0f, next_magnitude);
      float max_inner_radius = utils::max(radius, 0.5f * (next_magnitude + magnitude));
      magnitude = next_magnitude;

      // dot product
      float bisect_delta_cos = bisect_line.x * delta_normal.x + bisect_line.y * delta_normal.y;
      float inner_mult = utils::min(10.0f, 1.0f / ::fabsf(bisect_delta_cos));
      Point<float> inner_point = point + bisect_line * utils::min(inner_mult * radius, max_inner_radius);
      Point<float> outer_point = point - bisect_line * radius;

      float x1, x2, x3, x4, x5, x6;
      float y1, y2, y3, y4, y5, y6;

      Point<float> outer_point_start = outer_point;
      Point<float> outer_point_end = outer_point;
      
      if (bisect_delta_cos < 0.0f)
      {
        if (!straight)
        {
          outer_point_start = point + prev_delta_normal * radius;
          outer_point_end = point + delta_normal * radius;
        }
        x1 = outer_point_start.x;
        y1 = outer_point_start.y;
        x3 = outer_point.x;
        y3 = outer_point.y;
        x5 = outer_point_end.x;
        y5 = outer_point_end.y;
        x2 = x4 = x6 = inner_point.x;
        y2 = y4 = y6 = inner_point.y;
      }
      else
      {
        if (!straight)
        {
          outer_point_start = point - prev_delta_normal * radius;
          outer_point_end = point - delta_normal * radius;
        }
        x2 = outer_point_start.x;
        y2 = outer_point_start.y;
        x4 = outer_point.x;
        y4 = outer_point.y;
        x6 = outer_point_end.x;
        y6 = outer_point_end.y;
        x1 = x3 = x5 = inner_point.x;
        y1 = y3 = y5 = inner_point.y;
      }

      int first = (i + 1) * kLineFloatsPerPoint;
      int second = first + kLineFloatsPerVertex;
      int third = second + kLineFloatsPerVertex;
      int fourth = third + kLineFloatsPerVertex;
      int fifth = fourth + kLineFloatsPerVertex;
      int sixth = fifth + kLineFloatsPerVertex;

      lineData_[first] = x_adjust * x1 - 1.0f;
      lineData_[first + 1] = 1.0f - y_adjust * y1;

      lineData_[second] = x_adjust * x2 - 1.0f;
      lineData_[second + 1] = 1.0f - y_adjust * y2;

      lineData_[third] = x_adjust * x3 - 1.0f;
      lineData_[third + 1] = 1.0f - y_adjust * y3;

      lineData_[fourth] = x_adjust * x4 - 1.0f;
      lineData_[fourth + 1] = 1.0f - y_adjust * y4;

      lineData_[fifth] = x_adjust * x5 - 1.0f;
      lineData_[fifth + 1] = 1.0f - y_adjust * y5;

      lineData_[sixth] = x_adjust * x6 - 1.0f;
      lineData_[sixth + 1] = 1.0f - y_adjust * y6;

      prev_delta_normal = delta_normal;
      prev_normalized_delta = normalized_delta;
    }

    Point<float> start{ x_[0], y_[0] };
    Point<float> end{ x_[pointCount_ - 1], y_[pointCount_ - 1] };

    Point<float> delta_start_offset = normalise({ x_[0] - x_[1], y_[0] - y_[1] }) * line_radius;
    Point<float> delta_end_offset = normalise({ x_[pointCount_ - 1] - x_[pointCount_ - 2], 
      y_[pointCount_ - 1] - y_[pointCount_ - 2] }) * line_radius;
    for (int i = 0; i < kLineVerticesPerPoint; ++i)
    {
      lineData_[i * kLineFloatsPerVertex] = (x_[0] + delta_start_offset.x) * x_adjust - 1.0f;
      lineData_[i * kLineFloatsPerVertex + 1] = 1.0f - (y_[0] + delta_start_offset.y) * y_adjust;
      lineData_[i * kLineFloatsPerVertex + 2] = boosts[0];

      int copy_index_start = (pointCount_ + 1) * kLineFloatsPerPoint + i * kLineFloatsPerVertex;
      lineData_[copy_index_start] = (end.x + delta_end_offset.x) * x_adjust - 1.0f;
      lineData_[copy_index_start + 1] = 1.0f - (end.y + delta_end_offset.y) * y_adjust;
      lineData_[copy_index_start + 2] = boosts[pointCount_ - 1];
    }
  }

}
