/*
  ==============================================================================

    OpenGlImage.hpp
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <variant>
#include "Interface/LookAndFeel/Miscellaneous.hpp"
#include "OpenGlComponent.hpp"

namespace Interface
{
  // Drawing class that acquires its logic from the paint method
  // of the currently linked component (i.e. the owner of the object)
  // and draws it on an owned image resource
  class OpenGlImage : public OpenGlComponent
  {
  public:
    OpenGlImage(String name = "OpenGlImage");
    ~OpenGlImage() override;

    void redrawImage(juce::Rectangle<int> redrawArea = {}, bool forceRedraw = true);
    virtual void paintToImage(Graphics &g, BaseComponent *target)
    {
      if (paintEntireComponent_)
        target->paintEntireComponent(g, false);
      else
        target->paint(g);
    }

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl) override;
    void destroy() override;

    void setTargetComponent(BaseComponent *targetComponent) { targetComponent_ = targetComponent; }
    // this is for responsible for render and also raster coords within targetComponent (either this or something else)
    void setCustomViewportBounds(juce::Rectangle<int> customViewportBounds) { customViewportBounds_ = customViewportBounds; }
    void setCustomScissorBounds(juce::Rectangle<int> customScissorBounds) { customScissorBounds_ = customScissorBounds; }
    void setAdditive(bool additive) { isAdditive_ = additive; }
    void setScissor(bool scissor) { useScissor_ = scissor; }
    void setUseAlpha(bool useAlpha) { useAlpha_ = useAlpha; }
    void setColor(Colour colour) { colour_ = colour; }
    void setActive(bool active) { isActive_ = active; }

    void setPaintFunction(std::function<void(Graphics &, juce::Rectangle<int>)> paintFunction) 
    { paintFunction_ = std::move(paintFunction); }
    void setShouldClearOnRedraw(bool clearOnRedraw) { clearOnRedraw_ = clearOnRedraw; }
    void paintEntireComponent(bool paintEntireComponent) { paintEntireComponent_ = paintEntireComponent; }

    void setVertexPosition(size_t index, float x, float y)
    {
      auto vertices = positionVertices_.lock();
      vertices[index] = x;
      vertices[index + 1] = y;
      positionVertices_.unlock();
      hasNewVertices_ = true;
    }

    void movePosition(float x, float y)
    {
      auto vertices = positionVertices_.lock();
      vertices[0]  = -1.0f + x;
      vertices[1]  =  1.0f + y;
      vertices[4]  = -1.0f + x;
      vertices[5]  = -1.0f + y;
      vertices[8]  =  1.0f + x;
      vertices[9]  = -1.0f + y;
      vertices[12] =  1.0f + x;
      vertices[13] =  1.0f + y;
      positionVertices_.unlock();
      hasNewVertices_ = true;
    }

  protected:
    utils::shared_value<Colour> colour_ = Colours::white;
    utils::shared_value<bool> isAdditive_ = false;
    utils::shared_value<bool> useAlpha_ = false;
    utils::shared_value<bool> useScissor_ = true;
    utils::shared_value<bool> isActive_ = true;

    utils::shared_value<bool> hasNewVertices_ = true;
    utils::shared_value<bool> shouldReloadImage_ = false;
    utils::shared_value<BaseComponent *> targetComponent_ = nullptr;
    utils::shared_value<juce::Rectangle<int>> customViewportBounds_{};
    utils::shared_value<juce::Rectangle<int>> customScissorBounds_{};

    utils::shared_value<std::unique_ptr<Image>> drawImage_;
    GLuint textureId_ = 0;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    OpenGlShaderProgram *imageShader_ = nullptr;
    OpenGlUniform imageColour_;
    OpenGlAttribute imagePosition_;
    OpenGlAttribute textureCoordinates_;

    utils::shared_value<std::unique_ptr<float[]>> positionVertices_;
    GLuint vertexBuffer_{};
    GLuint triangleBuffer_{};

    std::function<void(Graphics &, juce::Rectangle<int>)> paintFunction_{};
    bool paintEntireComponent_ = true;
    bool clearOnRedraw_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlImage)
  };

  class OpenGlBackground final : public OpenGlImage
  {
  public:
    OpenGlBackground() : OpenGlImage{ "OpenGlBackground" } { setShouldClearOnRedraw(false); }

    void paintToImage(Graphics &g, BaseComponent *target) override;
    void setComponentToRedraw(BaseSection *componentToRedraw)
    { componentToRedraw_ = componentToRedraw; }

  protected:
    BaseSection *componentToRedraw_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlBackground)
  };

  class PlainTextComponent final : public OpenGlImage
  {
  public:
    enum FontType
    {
      kTitle,
      kText,
      kValues
    };

    PlainTextComponent(String name, String text = {});

    void resized() override;
    void paintToImage(Graphics &g, BaseComponent *target) override;

    String getText() const { return text_; }
    int getTotalWidth() const { return font_.getStringWidth(text_); }
    int getTotalHeight() const { return (int)std::ceil(font_.getHeight()); }
    void updateState();

    void setText(String text) noexcept { text_ = std::move(text); redrawImage(); }
    void setTextHeight(float textSize) noexcept { textSize_ = textSize; }
    void setTextColour(Colour colour) noexcept { textColour_ = colour; }
    void setFontType(FontType type) noexcept { fontType_ = type; }
    void setJustification(Justification justification) noexcept { justification_ = justification; }

  private:
    String text_;
    float textSize_ = 11.0f;
    Colour textColour_ = Colours::white;
    Font font_;
    FontType fontType_ = kText;
    Justification justification_ = Justification::centred;
  };

  class PlainShapeComponent final : public OpenGlImage
  {
  public:
    PlainShapeComponent(String name) : OpenGlImage(std::move(name)) { }

    void resized() override { redrawImage(); }
    void paintToImage(Graphics &g, BaseComponent *target) override;

    void setShapes(Shape shape)
    {
      shape_ = std::move(shape);
      redrawImage();
    }

    void setJustification(Justification justification) { justification_ = justification; }

  private:
    Shape shape_;
    Justification justification_ = Justification::centred;
  };
}
