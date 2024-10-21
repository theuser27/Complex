/*
  ==============================================================================

    OpenGlLineRenderer.hpp
    Created: 3 Feb 2023 10:14:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "OpenGlComponent.hpp"

namespace Interface
{
  class OpenGlCorners;
  class Renderer;

  class OpenGlLineRenderer
  {
  public:
    static constexpr int kLineFloatsPerVertex = 3;
    static constexpr int kFillFloatsPerVertex = 3;
    static constexpr int kLineVerticesPerPoint = 6;
    static constexpr int kFillVerticesPerPoint = 2;
    static constexpr int kLineFloatsPerPoint = kLineVerticesPerPoint * kLineFloatsPerVertex;
    static constexpr int kFillFloatsPerPoint = kFillVerticesPerPoint * kFillFloatsPerVertex;

    static constexpr float kDefaultLineWidth = 7.0f;

    OpenGlLineRenderer(int pointCount);
    ~OpenGlLineRenderer();

    void init(OpenGlWrapper &openGl);
    void render(const OpenGlWrapper &openGl, const OpenGlComponent &target, juce::Rectangle<int> bounds);
    void destroy(Renderer &renderer);

    strict_inline int getPointCount() const noexcept { return pointCount_; }

    void setPointCount(int pointCount);
    strict_inline void setColour(Colour colour) noexcept { colour_ = colour; }
    strict_inline void setLineWidth(float width) noexcept { lineWidth_ = width; }

    strict_inline void setFill(bool fill) noexcept { fill_ = fill; }
    strict_inline void setFillColour(Colour fillColour) noexcept { setFillColours(fillColour, fillColour); }
    strict_inline void setFillColours(Colour fillColourFrom, Colour fillColourTo) noexcept
    {
      fillColorFrom_ = fillColourFrom;
      fillColorTo_ = fillColourTo;
    }
    strict_inline void setFillCenter(float fillCenter) noexcept { fillCenter_ = fillCenter; }
    strict_inline void setFit(bool fit) noexcept { fit_ = fit; }

    strict_inline void setBoostAmount(float boostAmount) noexcept { boostAmount_ = boostAmount; }
    strict_inline void setFillBoostAmount(float fillBoostAmount) noexcept { fillBoostAmount_ = fillBoostAmount; }


    // thread safe ^^^ //         // not thread safe vvv //


    strict_inline float getBoostAt(int index) const noexcept { return boosts_[index]; }
    strict_inline float getYAt(int index) const noexcept { return y_[index]; }
    strict_inline float getXAt(int index) const noexcept { return x_[index]; }

    strict_inline void setBoost(int index, float val) noexcept
    {
      COMPLEX_ASSERT(pointCount_ > index);
      boosts_[index] = val;
      dirty_ = true;
    }
    strict_inline void setYAt(int index, float val) noexcept
    {
      COMPLEX_ASSERT(pointCount_ > index);
      y_[index] = val;
      dirty_ = true;
    }
    strict_inline void setXAt(int index, float val) noexcept
    {
      COMPLEX_ASSERT(pointCount_ > index);
      x_[index] = val;
      dirty_ = true;
    }

    void setFillVertices(float width, float height);
    void setLineVertices(float width, float height);

    void boostRange(float start, float end, int buffer_vertices, float min);
    void decayBoosts(float mult);

  private:
    utils::shared_value<Colour> colour_;
    utils::shared_value<Colour> fillColorFrom_;
    utils::shared_value<Colour> fillColorTo_;

    utils::shared_value<float> lineWidth_ = kDefaultLineWidth;
    utils::shared_value<bool> fill_ = false;
    utils::shared_value<float> fillCenter_ = 0.0f;
    utils::shared_value<bool> fit_ = false;

    utils::shared_value<float> boostAmount_ = 0.0f;
    utils::shared_value<float> fillBoostAmount_ = 0.0f;

    bool dirty_ = false;

    int pointCount_ = 0;
    int lineVerticesCount_;
    int fillVerticesCount_;
    int lineFloatsCount_;
    int fillFloatsCount_;
    bool shouldUpdateBufferSizes_ = true;

    OpenGlShaderProgram *lineShader_ = nullptr;
    OpenGlUniform lineScaleUniform_;
    OpenGlUniform lineColourUniform_;
    OpenGlUniform lineWidthUniform_;
    OpenGlAttribute linePosition_;

    OpenGlShaderProgram *fillShader_ = nullptr;
    OpenGlUniform fillScaleUniform_;
    OpenGlUniform fillColourFromUniform_;
    OpenGlUniform fillColourToUniform_;
    OpenGlUniform fillCenterUniform_;
    OpenGlUniform fillBoostAmountUniform_;
    OpenGlAttribute fillPosition_;

    GLuint lineBuffer_ = 0;
    GLuint fillBuffer_ = 0;
    GLuint indicesBuffer_ = 0;

    std::unique_ptr<float[]> x_;
    std::unique_ptr<float[]> y_;
    std::unique_ptr<float[]> boosts_;
    std::unique_ptr<float[]> lineData_;
    std::unique_ptr<float[]> fillData_;
    std::unique_ptr<int[]> indicesData_;

    mutable std::atomic<bool> buffersLock_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlLineRenderer)
  };
}
