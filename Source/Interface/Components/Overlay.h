/*
  ==============================================================================

    Overlay.h
    Created: 14 Dec 2022 5:11:59am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"

namespace Interface
{
  class OverlayBackgroundRenderer : public OpenGlComponent
  {
  public:
    static constexpr int kNumVertices = 4;
    static constexpr int kNumFloatsPerVertex = 2;
    static constexpr int kTotalFloats = kNumVertices * kNumFloatsPerVertex;
    static constexpr int kIndices = 6;

    OverlayBackgroundRenderer();
    ~OverlayBackgroundRenderer() override = default;

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl, bool animate) override;
    void destroy(OpenGlWrapper &openGl) override;
    void paint(Graphics &) override {}

    void setColor(const Colour &color) { color_ = color; }
    void setAdditiveBlending(bool additiveBlending) { additiveBlending_ = additiveBlending; }

  protected:
    void drawOverlay(OpenGlWrapper &openGl);

    OpenGLShaderProgram *shader_ = nullptr;
    std::unique_ptr<OpenGLShaderProgram::Uniform> colorUniform_ = nullptr;
    std::unique_ptr<OpenGLShaderProgram::Attribute> position_ = nullptr;

    Colour color_ = Colours::black;
    bool additiveBlending_ = false;

    float data_[kTotalFloats];
    int indices_[kIndices];
    GLuint dataBuffer_ = 0;
    GLuint indicesBuffer_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayBackgroundRenderer)
  };

  class Overlay : public BaseSection
  {
  public:

    Overlay(std::string_view name) : BaseSection(name), size_ratio_(1.0f)
    {
      setSkinOverride(Skin::kOverlay);
      addOpenGlComponent(&background_);
    }

    void resized() override
    {
      background_.setColor(getColour(Skin::kOverlayScreen));
      background_.setBounds(getLocalBounds());
    }

    void paintBackground(Graphics &g) override { paintOpenGlChildrenBackgrounds(g); }
    void setScaling(float ratio) override { size_ratio_ = ratio; }

  protected:
    float size_ratio_;
    OverlayBackgroundRenderer background_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Overlay)
  };
}
