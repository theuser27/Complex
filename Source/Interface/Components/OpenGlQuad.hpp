/*
  ==============================================================================

    OpenGlQuad.hpp
    Created: 14 Dec 2022 2:05:11am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "OpenGlComponent.hpp"

namespace Interface
{
  class OpenGlMultiQuad : public OpenGlComponent
  {
  public:
    static constexpr usize kNumVertices = 4;
    static constexpr usize kNumFloatsPerVertex = 10;
    static constexpr usize kNumFloatsPerQuad = kNumVertices * kNumFloatsPerVertex;
    static constexpr usize kNumIndicesPerQuad = 6;

    struct QuadData
    {
      float getQuadX(usize i) const noexcept { return data[kNumFloatsPerQuad * i]; }
      float getQuadY(usize i) const noexcept { return data[kNumFloatsPerQuad * i + 1]; }
      float getQuadWidth(usize i) const noexcept
      {
        usize index = kNumFloatsPerQuad * i;
        return data[2 * kNumFloatsPerVertex + index] - data[index];
      }
      float getQuadHeight(usize i) const noexcept
      {
        usize index = kNumFloatsPerQuad * i;
        return data[2 * kNumFloatsPerVertex + index + 1] - data[index + 1];
      }
      void setCoordinates(usize i, float x, float y, float w, float h) noexcept
      {
        COMPLEX_ASSERT(i < maxQuads);
        usize index = i * kNumFloatsPerQuad;

        data[index + 4] = x;
        data[index + 5] = y;
        data[kNumFloatsPerVertex + index + 4] = x;
        data[kNumFloatsPerVertex + index + 5] = y + h;
        data[2 * kNumFloatsPerVertex + index + 4] = x + w;
        data[2 * kNumFloatsPerVertex + index + 5] = y + h;
        data[3 * kNumFloatsPerVertex + index + 4] = x + w;
        data[3 * kNumFloatsPerVertex + index + 5] = y;
      }
      void setShaderValue(usize i, float shaderValue, int valueIndex = 0) noexcept
      {
        COMPLEX_ASSERT(i < maxQuads);
        usize index = i * kNumFloatsPerQuad + 6 + valueIndex;

        data[index] = shaderValue;
        data[kNumFloatsPerVertex + index] = shaderValue;
        data[2 * kNumFloatsPerVertex + index] = shaderValue;
        data[3 * kNumFloatsPerVertex + index] = shaderValue;
      }
      void setQuadHorizontal(usize i, float x, float w) noexcept
      {
        COMPLEX_ASSERT(i < maxQuads);
        usize index = i * kNumFloatsPerQuad;

        data[index] = x;
        data[kNumFloatsPerVertex + index] = x;
        data[2 * kNumFloatsPerVertex + index] = x + w;
        data[3 * kNumFloatsPerVertex + index] = x + w;
      }
      void setQuadVertical(usize i, float y, float h) noexcept
      {
        COMPLEX_ASSERT(i < maxQuads);
        usize index = i * kNumFloatsPerQuad;

        data[index + 1] = y;
        data[kNumFloatsPerVertex + index + 1] = y + h;
        data[2 * kNumFloatsPerVertex + index + 1] = y + h;
        data[3 * kNumFloatsPerVertex + index + 1] = y;
      }
      void setQuad(usize i, float x, float y, float w, float h) noexcept
      {
        COMPLEX_ASSERT(i < maxQuads);
        usize index = i * kNumFloatsPerQuad;

        data[index] = x;
        data[index + 1] = y;
        data[kNumFloatsPerVertex + index] = x;
        data[kNumFloatsPerVertex + index + 1] = y + h;
        data[2 * kNumFloatsPerVertex + index] = x + w;
        data[2 * kNumFloatsPerVertex + index + 1] = y + h;
        data[3 * kNumFloatsPerVertex + index] = x + w;
        data[3 * kNumFloatsPerVertex + index + 1] = y;
      }

      float &operator[](usize i) noexcept { return data[i]; }

      utils::shared_value<float[]>::span data;
      usize maxQuads;
    };

    OpenGlMultiQuad(usize maxQuads, Shaders::FragmentShader shader = Shaders::kColorFragment, 
      String name = "OpenGlMultiQuad");
    ~OpenGlMultiQuad() override;

    void resized() override
    {
      OpenGlComponent::resized();
      data_.update();
    }

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl) override;
    void destroy() override;

    Colour getColor() const noexcept { return color_; }
    float getMaxArc() const noexcept { return maxArc_; }
    auto getQuadData() noexcept { return QuadData{ data_.write(), maxQuads_ }; }

    void setFragmentShader(Shaders::FragmentShader shader) noexcept { fragmentShader_ = shader; }
    void setColor(Colour color) noexcept { color_ = color; }
    void setAltColor(Colour color) noexcept { altColor_ = color; }
    void setModColor(Colour color) noexcept { modColor_ = color; }
    void setBackgroundColor(Colour color) noexcept { backgroundColor_ = color; }
    void setThumbColor(Colour color) noexcept { thumbColor_ = color; }
    void setThumbAmount(float amount) noexcept { thumbAmount_ = amount; }
    void setStartPos(float position) noexcept { startPosition_ = position; }
    void setMaxArc(float maxArc) noexcept { maxArc_ = maxArc; }
    void setActive(bool active) noexcept { active_ = active; }
    void setThickness(float thickness) noexcept { thickness_ = thickness; }
    void setAdditive(bool additive) noexcept { additiveBlending_ = additive; }
    void setOverallAlpha(float alpha) noexcept { overallAlpha_ = alpha; }
    void setRounding(float rounding) noexcept
    {
      float adjusted = 2.0f * rounding;
      if (adjusted != rounding_)
        rounding_ = adjusted;
    }

    void setDrawWhenNotVisible(bool draw) noexcept { drawWhenNotVisible_ = draw; }
    void setTargetComponent(BaseComponent *targetComponent) noexcept { targetComponent_ = targetComponent; }
    void setCustomViewportBounds(juce::Rectangle<int> bounds) noexcept
    {
      auto oldBounds = customViewportBounds_.get();
      customViewportBounds_ = bounds;
      if (bounds.withZeroOrigin() != oldBounds.withZeroOrigin())
        data_.update();
    }
    void setCustomScissorBounds(juce::Rectangle<int> bounds) noexcept { customScissorBounds_ = bounds; }

    void setNumQuads(usize newNumQuads) noexcept
    {
      COMPLEX_ASSERT(newNumQuads <= maxQuads_);
      numQuads_ = newNumQuads;
      data_.update();
    }

  protected:
    utils::shared_value<BaseComponent *> targetComponent_ = nullptr;
    utils::shared_value<juce::Rectangle<int>> customViewportBounds_{};
    utils::shared_value<juce::Rectangle<int>> customScissorBounds_{};
    utils::shared_value<Shaders::FragmentShader> fragmentShader_;

    utils::shared_value<bool> drawWhenNotVisible_ = false;
    utils::shared_value<bool> active_ = true;
    utils::shared_value<Colour> color_;
    utils::shared_value<Colour> altColor_;
    utils::shared_value<Colour> modColor_ = Colours::transparentBlack;
    utils::shared_value<Colour> backgroundColor_;
    utils::shared_value<Colour> thumbColor_;
    utils::shared_value<float> maxArc_ = 2.0f;
    utils::shared_value<float> thumbAmount_ = 0.5f;
    utils::shared_value<float> startPosition_ = 0.0f;
    utils::shared_value<float> overallAlpha_ = 1.0f;
    utils::shared_value<bool> additiveBlending_ = false;
    utils::shared_value<float> thickness_ = 1.0f;
    utils::shared_value<float> rounding_ = 5.0f;

    /*/
     *  data_ array indices per quad
     *  0 - 1: vertex ndc position
     *  2 - 3: scaled width and height for quad (acts like a uniform for individual quads)
     *  4 - 5: coordinates inside the quad (ndc for most situations, normalised for OpenGLCorners)
     *  6 - 7: shader values (doubles as left channel shader values)
     *  8 - 9: right channel shader values (necessary for the modulation meters/indicators)
    /*/
    utils::shared_value<float[]> data_;
    utils::shared_value<usize> maxQuads_, numQuads_;

    OpenGlShaderProgram shader_;
    OpenGlUniform colorUniform_;
    OpenGlUniform altColorUniform_;
    OpenGlUniform modColorUniform_;
    OpenGlUniform backgroundColorUniform_;
    OpenGlUniform thumbColorUniform_;
    OpenGlUniform thicknessUniform_;
    OpenGlUniform roundingUniform_;
    OpenGlUniform maxArcUniform_;
    OpenGlUniform thumbAmountUniform_;
    OpenGlUniform startPositionUniform_;
    OpenGlUniform overallAlphaUniform_;
    OpenGlAttribute position_;
    OpenGlAttribute dimensions_;
    OpenGlAttribute coordinates_;
    OpenGlAttribute shader_values_;

    GLuint vertexBuffer_ = 0;
    GLuint indicesBuffer_ = 0;
  };

  class OpenGlQuad final : public OpenGlMultiQuad
  {
  public:
    OpenGlQuad(Shaders::FragmentShader shader, String name = "OpenGlQuad") :
      OpenGlMultiQuad(1, shader, COMPLEX_MOVE(name))
    {
      getQuadData().setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
    }
  };

  class OpenGlCorners final : public OpenGlMultiQuad
  {
  public:
    OpenGlCorners() : OpenGlMultiQuad(4, Shaders::kRoundedCornerFragment, "OpenGlCorners")
    {
      auto quadData = getQuadData();
      quadData.setCoordinates(0, 1.0f, 1.0f, -1.0f, -1.0f);
      quadData.setCoordinates(1, 1.0f, 0.0f, -1.0f, 1.0f);
      quadData.setCoordinates(2, 0.0f, 0.0f, 1.0f, 1.0f);
      quadData.setCoordinates(3, 0.0f, 1.0f, 1.0f, -1.0f);
    }

    void setCorners(juce::Rectangle<int> bounds, float rounding)
    {
      float width = rounding / bounds.getWidth() * 2.0f;
      float height = rounding / bounds.getHeight() * 2.0f;

      auto quadData = getQuadData();

      quadData.setQuad(0, -1.0f, -1.0f, width, height);
      quadData.setQuad(1, -1.0f, 1.0f - height, width, height);
      quadData.setQuad(2, 1.0f - width, 1.0f - height, width, height);
      quadData.setQuad(3, 1.0f - width, -1.0f, width, height);
    }

    void setCorners(juce::Rectangle<int> bounds, float topRounding, float bottomRounding)
    {
      float topWidth = topRounding / bounds.getWidth() * 2.0f;
      float topHeight = topRounding / bounds.getHeight() * 2.0f;
      float bottomWidth = bottomRounding / bounds.getWidth() * 2.0f;
      float bottomHeight = bottomRounding / bounds.getHeight() * 2.0f;

      auto quadData = getQuadData();

      quadData.setQuad(0, -1.0f, -1.0f, bottomWidth, bottomHeight);
      quadData.setQuad(1, -1.0f, 1.0f - topHeight, topWidth, topHeight);
      quadData.setQuad(2, 1.0f - topWidth, 1.0f - topHeight, topWidth, topHeight);
      quadData.setQuad(3, 1.0f - bottomWidth, -1.0f, bottomWidth, bottomHeight);
    }

    void setTopCorners(juce::Rectangle<int> bounds, float topRounding)
    {
      float width = topRounding / bounds.getWidth() * 2.0f;
      float height = topRounding / bounds.getHeight() * 2.0f;

      auto quadData = getQuadData();

      quadData.setQuad(0, -2.0f, -2.0f, 0.0f, 0.0f);
      quadData.setQuad(1, -1.0f, 1.0f - height, width, height);
      quadData.setQuad(2, 1.0f - width, 1.0f - height, width, height);
      quadData.setQuad(3, -2.0f, -2.0f, 0.0f, 0.0f);
    }

    void setBottomCorners(juce::Rectangle<int> bounds, float bottomRounding)
    {
      float width = bottomRounding / bounds.getWidth() * 2.0f;
      float height = bottomRounding / bounds.getHeight() * 2.0f;

      auto quadData = getQuadData();
      
      quadData.setQuad(0, -1.0f, -1.0f, width, height);
      quadData.setQuad(1, -2.0f, -2.0f, 0.0f, 0.0f);
      quadData.setQuad(2, -2.0f, -2.0f, 0.0f, 0.0f);
      quadData.setQuad(3, 1.0f - width, -1.0f, width, height);
    }
  };
}
