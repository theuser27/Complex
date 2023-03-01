/*
  ==============================================================================

    OpenGlComponent.h
    Created: 5 Dec 2022 11:55:29pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/Shaders.h"
#include "../LookAndFeel/Skin.h"

namespace Interface
{
  class BaseSection;
  class OpenGlCorners;
  class MainInterface;

  class OpenGlComponent : public Component
  {
  public:
    static bool setViewPort(Component *component, Rectangle<int> bounds, OpenGlWrapper &openGl);
    static bool setViewPort(Component *component, OpenGlWrapper &openGl);
    static void setScissor(Component *component, OpenGlWrapper &openGl);
    static void setScissorBounds(Component *component, Rectangle<int> bounds, OpenGlWrapper &openGl);

    static std::unique_ptr<OpenGLShaderProgram::Uniform> getUniform(const OpenGLShaderProgram &program, const char *name)
    {
      if (juce::gl::glGetUniformLocation(program.getProgramID(), name) >= 0)
        return std::make_unique<OpenGLShaderProgram::Uniform>(program, name);
      return nullptr;
    }

    static std::unique_ptr<OpenGLShaderProgram::Attribute> getAttribute(const OpenGLShaderProgram &program, const char *name)
    {
      if (juce::gl::glGetAttribLocation(program.getProgramID(), name) >= 0)
        return std::make_unique<OpenGLShaderProgram::Attribute>(program, name);
      return nullptr;
    }

    static String translateFragmentShader(const String &code)
    {
    #if OPENGL_ES
      return String("#version 300 es\n") + "out mediump vec4 fragColor;\n" +
        code.replace("varying", "in").replace("texture2D", "texture").replace("gl_FragColor", "fragColor");
    #else
      return OpenGLHelpers::translateFragmentShaderToV3(code);
    #endif
    }

    static String translateVertexShader(const String &code)
    {
    #if OPENGL_ES
      return String("#version 300 es\n") + code.replace("attribute", "in").replace("varying", "out");
    #else
      return OpenGLHelpers::translateVertexShaderToV3(code);
    #endif
    }

    OpenGlComponent(String name = "") : Component(name) { }
    ~OpenGlComponent() override = default;

    void resized() override;
    void paint(Graphics &g) override;

    virtual void init(OpenGlWrapper &openGl);
    virtual void render(OpenGlWrapper &openGl, bool animate) = 0;
    virtual void destroy(OpenGlWrapper &openGl);

    void renderCorners(OpenGlWrapper &openGl, bool animate, Colour color, float rounding);
    void renderCorners(OpenGlWrapper &openGl, bool animate);
    void addRoundedCorners();
    void addBottomRoundedCorners();
    void repaintBackground();

    Colour getBodyColor() const noexcept { return bodyColor_; }
    float findValue(Skin::ValueId valueId) const;

    void setSkinValues(const Skin &skin) { skin.applyComponentColors(this, skinOverride_, false); }
    void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
    void setBackgroundColor(const Colour &color) noexcept { backgroundColor_ = color; }
    void setParent(const BaseSection *parent) noexcept { parent_ = parent; }

  protected:
    bool setViewPort(OpenGlWrapper &openGl);

    std::unique_ptr<OpenGlCorners> corners_;
    bool onlyBottomCorners_ = false;
    Colour backgroundColor_ = Colours::transparentBlack;
    Colour bodyColor_;
    Skin::SectionOverride skinOverride_ = Skin::kNone;

    const BaseSection *parent_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
  };
}
