/*
  ==============================================================================

    OpenGlImageComponent.h
    Created: 14 Dec 2022 2:07:44am
    Author:  Lenovo

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/Fonts.h"
#include "OpenGlComponent.h"
#include "OpenGlImage.h"

namespace Interface
{
  // Drawing class that acquires its logic from the paint method
  // of the currently linked component (i.e. the owner of the object)
  // and draws it on an owned image resource
  class OpenGlImageComponent : public OpenGlComponent
  {
  public:
    OpenGlImageComponent(String name = "") : OpenGlComponent(std::move(name))
    {
      image_.setTopLeft(-1.0f, 1.0f);
      image_.setTopRight(1.0f, 1.0f);
      image_.setBottomLeft(-1.0f, -1.0f);
      image_.setBottomRight(1.0f, -1.0f);
      image_.setColor(Colours::white);

      if (getName() == "")
        setInterceptsMouseClicks(false, false);
    }

    void paint([[maybe_unused]] Graphics &g) override { redrawImage(false); }

    virtual void paintToImage(Graphics &g)
    {
      Component *component = component_ ? component_ : this;
      if (paintEntireComponent_)
        component->paintEntireComponent(g, false);
      else
        component->paint(g);
    }

    void init(OpenGlWrapper &openGl) override;
    void render(OpenGlWrapper &openGl, bool animate) override;
    void destroy(OpenGlWrapper &openGl) override;

    // the force argument is for when we KNOW we need to repaint
    // (i.e. isn't a hierarchy propaged paint call from a top level object that paints its children) 
  	void redrawImage(bool force = true);

    void setComponent(Component *component) { component_ = component; }
    void setScissor(bool scissor) { image_.setScissor(scissor); }
    void setUseAlpha(bool useAlpha) { image_.setUseAlpha(useAlpha); }
    void setColor(Colour color) { image_.setColor(color); }
    OpenGlImage &image() { return image_; }
    void setActive(bool active) { active_ = active; }
    void setStatic(bool staticImage) { staticImage_ = staticImage; }
    void paintEntireComponent(bool paintEntireComponent) { paintEntireComponent_ = paintEntireComponent; }
    bool isActive() const { return active_; }

  protected:
    Component *component_ = nullptr;
    bool active_ = true;
    bool staticImage_ = false;
    bool paintEntireComponent_ = true;
    std::unique_ptr<Image> drawImage_;
    OpenGlImage image_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlImageComponent)
  };

  template <typename ComponentType>
    requires std::derived_from<ComponentType, Component>
  class OpenGlAutoImageComponent : public ComponentType
  {
  public:
    using ComponentType::ComponentType;

    void mouseDown(const MouseEvent &e) override
    {
      ComponentType::mouseDown(e);
      redoImage();
    }

    void mouseUp(const MouseEvent &e) override
    {
      ComponentType::mouseUp(e);
      redoImage();
    }

    void mouseDoubleClick(const MouseEvent &e) override
    {
      ComponentType::mouseDoubleClick(e);
      redoImage();
    }

    void mouseEnter(const MouseEvent &e) override
    {
      ComponentType::mouseEnter(e);
      redoImage();
    }

    void mouseExit(const MouseEvent &e) override
    {
      ComponentType::mouseExit(e);
      redoImage();
    }

    void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override
    {
      ComponentType::mouseWheelMove(e, wheel);
      redoImage();
    }

    OpenGlImageComponent *getImageComponent() { return &imageComponent_; }
    void redoImage() { imageComponent_.redrawImage(); }

  protected:
    OpenGlImageComponent imageComponent_;
  };

  class OpenGlTextEditor : public OpenGlAutoImageComponent<TextEditor>, public TextEditor::Listener
  {
  public:
    OpenGlTextEditor(String name) : OpenGlAutoImageComponent(std::move(name))
    {
      imageComponent_.setComponent(this);
      addListener(this);
      //imageComponent_.image().setAdditive(true);
      //setJustification(Justification::centred);
      /*setBorder({ 1, 1, 1, 1 });
      setIndents(0, 0);*/
    }

    void textEditorTextChanged(TextEditor &) override { redoImage(); }
    void textEditorFocusLost(TextEditor &) override { redoImage(); }

    void mouseDrag(const MouseEvent &e) override
    {
      TextEditor::mouseDrag(e);
      redoImage();
    }

    void paint(Graphics &g) override
    {
      g.setFont(usedFont_);
    }

    void applyFont()
    {
      applyFontToAllText(usedFont_);
      redoImage();
    }

    void visibilityChanged() override
    {
      TextEditor::visibilityChanged();

      if (isVisible() && !isMultiLine())
        applyFont();
    }

    void resized() override
    {
      TextEditor::resized();
      if (isMultiLine())
      {
        auto indent = (int)imageComponent_.findValue(Skin::kLabelBackgroundRounding);
        setIndents(indent, indent);
        return;
      }

      if (isVisible())
        applyFont();

    }

    auto getUsedFont() const { return usedFont_; }
  	void setUsedFont(Font font) { usedFont_ = std::move(font); }

  private:
    Font usedFont_{ Fonts::instance()->getInterVFont() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlTextEditor)
  };

  class PlainTextComponent : public OpenGlImageComponent
  {
  public:
    enum FontType
    {
      kTitle,
      kText,
      kValues,
      kNumFontTypes
    };

    PlainTextComponent(String name, String text) : OpenGlImageComponent(name), text_(std::move(text))
    { setInterceptsMouseClicks(false, false); }

    void resized() override
    {
      OpenGlImageComponent::resized();
      redrawImage();
    }

    void paintToImage(Graphics &g) override
    {
      g.setColour(Colours::white);

      if (font_type_ == kTitle)
        g.setFont(Fonts::instance()->getInterVFont().withPointHeight(text_size_).boldened());
      else if (font_type_ == kText)
        g.setFont(Fonts::instance()->getInterVFont().withPointHeight(text_size_));
      else
        g.setFont(Fonts::instance()->getDDinFont().withPointHeight(text_size_));

      Component *component = component_ ? component_ : this;

      g.drawFittedText(text_, buffer_, 0, component->getWidth() - 2 * buffer_,
        component->getHeight(), justification_, false);
    }

    String getText() const { return text_; }

    void setText(String text)
    {
      if (text_ == text)
        return;

      text_ = text;
      redrawImage();
    }

    void setTextSize(float size)
    {
      text_size_ = size;
      redrawImage();
    }

    void setFontType(FontType font_type) { font_type_ = font_type; }
    void setJustification(Justification justification) { justification_ = justification; }
    void setBuffer(int buffer) { buffer_ = buffer; }

  private:
    String text_;
    float text_size_ = 1.0f;
    FontType font_type_ = kText;
    Justification justification_ = Justification::centred;
    int buffer_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlainTextComponent)
  };

  class PlainShapeComponent : public OpenGlImageComponent
  {
  public:
    PlainShapeComponent(String name) : OpenGlImageComponent(name)
    { setInterceptsMouseClicks(false, false); }

    void paintToImage(Graphics &g) override
    {
      Component *component = component_ ? component_ : this;
      Rectangle<float> bounds = component->getLocalBounds().toFloat();
      Path shape = shape_;
      shape.applyTransform(shape.getTransformToScaleToFit(bounds, true, justification_));

      g.setColour(Colours::white);
      g.fillPath(shape);
    }

    void setShape(Path shape)
    {
      shape_ = std::move(shape);
      redrawImage();
    }

    void setJustification(Justification justification) { justification_ = justification; }

  private:
    Path shape_;
    Justification justification_ = Justification::centred;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlainShapeComponent)
  };
}
