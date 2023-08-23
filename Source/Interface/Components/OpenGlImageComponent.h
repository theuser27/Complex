/*
  ==============================================================================

    OpenGlImageComponent.h
    Created: 14 Dec 2022 2:07:44am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <variant>

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

      setInterceptsMouseClicks(false, false);
    }

    void paintBackground([[maybe_unused]] Graphics &g) override { }
    
    void redrawImage(bool clearOnRedraw = true);
    virtual void paintToImage(Graphics &g, Component *target)
    {
      if (paintEntireComponent_)
        target->paintEntireComponent(g, false);
      else
        target->paint(g);
    }

    void init(OpenGlWrapper &openGl) override { image_.init(openGl); }
    void render(OpenGlWrapper &openGl, bool animate) override;
    void destroy() override { image_.destroy(); }

    void setTargetComponent(Component *targetComponent) { targetComponent_ = targetComponent; }
    void setScissor(bool scissor) { image_.setScissor(scissor); }
    void setUseAlpha(bool useAlpha) { image_.setUseAlpha(useAlpha); }
    void setColor(Colour color) { image_.setColor(color); }
    void setActive(bool active) { active_ = active; }
    void setStatic(bool staticImage) { staticImage_ = staticImage; }
    void paintEntireComponent(bool paintEntireComponent) { paintEntireComponent_ = paintEntireComponent; }
    bool isActive() const { return active_; }

  protected:
    Component *targetComponent_ = nullptr;
    bool active_ = true;
    bool staticImage_ = false;
    bool paintEntireComponent_ = true;
    bool isDirty_ = true;
    std::unique_ptr<Image> drawImage_;
    OpenGlImage image_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlImageComponent)
  };

  class OpenGlBackground : public OpenGlImageComponent
  {
  public:
    OpenGlBackground() : OpenGlImageComponent(typeid(OpenGlBackground).name()) { }

    void paintToImage(Graphics &g, Component *target) override;
    void setComponentToRedraw(std::variant<BaseSection *, OpenGlComponent *> componentToRedraw)
    { componentToRedraw_ = componentToRedraw; }

  protected:
    std::variant<BaseSection *, OpenGlComponent *> componentToRedraw_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlBackground)
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

    gl_ptr<OpenGlImageComponent> getImageComponent() { return imageComponent_; }
    void redoImage() { imageComponent_->redrawImage(); }

  protected:
    gl_ptr<OpenGlImageComponent> imageComponent_;
  };

  class OpenGlTextEditor : public OpenGlAutoImageComponent<TextEditor>, public TextEditor::Listener
  {
  public:
    OpenGlTextEditor(String name) : OpenGlAutoImageComponent(std::move(name))
    {
      imageComponent_ = makeOpenGlComponent<OpenGlImageComponent>("Text Editor Image");
      imageComponent_->setTargetComponent(this);
      addListener(this);
    }

    void colourChanged() override { redoImage(); }
    void textEditorTextChanged(TextEditor &) override { redoImage(); }
    void textEditorFocusLost(TextEditor &) override { redoImage(); }

    void mouseDrag(const MouseEvent &e) override
    {
      TextEditor::mouseDrag(e);
      redoImage();
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

    bool keyPressed(const KeyPress &key) override
    {
      bool result = TextEditor::keyPressed(key);
      redoImage();
      return result;
    }

    void resized() override
    {
      TextEditor::resized();
      if (isMultiLine())
      {
        auto indent = (int)imageComponent_->getValue(Skin::kLabelBackgroundRounding);
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
      kValues
    };

    PlainTextComponent(String name, String text = {}) :
  		OpenGlImageComponent(std::move(name)), text_(std::move(text)) { }

    void resized() override
    {
      OpenGlImageComponent::resized();
      redrawImage();
    }

    void paintToImage(Graphics &g, Component *target) override;

    String getText() const { return text_; }
    int getTotalWidth() const { return font_.getStringWidth(text_); }
    void updateState();

    void setText(String text) noexcept { isDirty_ = isDirty_ || text_ != text; text_ = std::move(text); redrawImage(); }
    void setTextHeight(float textSize) noexcept { isDirty_ = isDirty_ || textSize_ != textSize; textSize_ = textSize; }
    void setFontType(FontType type) noexcept { isDirty_ = isDirty_ || fontType_ != type; fontType_ = type; }
    void setJustification(Justification justification) noexcept
  	{ isDirty_ = isDirty_ || justification_ != justification; justification_ = justification; }

  private:
    String text_;
    float textSize_ = 11.0f;
    Colour textColour_ = Colours::white;
    Font font_{ Fonts::instance()->getInterVFont() };
    FontType fontType_ = kText;
    Justification justification_ = Justification::centred;
  };

  class PlainShapeComponent : public OpenGlImageComponent
  {
  public:
    PlainShapeComponent(String name) : OpenGlImageComponent(std::move(name)) { }

    void resized() override { redrawImage(); }
    void paintToImage(Graphics &g, Component *target) override;

    void setShapes(std::pair<Path, Path> strokeFillShapes)
    {
      strokeShape_ = std::move(strokeFillShapes.first);
      fillShape_ = std::move(strokeFillShapes.second);
      redrawImage();
    }

    void setJustification(Justification justification) { justification_ = justification; }

  private:
    Path strokeShape_;
    Path fillShape_;
    Justification justification_ = Justification::centred;
  };
}
