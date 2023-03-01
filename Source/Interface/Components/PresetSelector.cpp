/*
  ==============================================================================

    PresetSelector.cpp
    Created: 31 Dec 2022 3:49:55am
    Author:  theuser27

  ==============================================================================
*/

#include "PresetSelector.h"

#include "../LookAndFeel/Skin.h"
#include "../LookAndFeel/DefaultLookAndFeel.h"
#include "../LookAndFeel/Fonts.h"

namespace Interface
{
  PresetSelector::PresetSelector() : BaseSection("preset_selector")
  {
    static const PathStrokeType arrow_stroke(0.05f, PathStrokeType::JointStyle::curved,
      PathStrokeType::EndCapStyle::rounded);

    text_ = std::make_unique<PlainTextComponent>("Text", "Init");
    text_->setFontType(PlainTextComponent::kTitle);
    text_->setInterceptsMouseClicks(false, false);
    addOpenGlComponent(text_.get());
    text_->setScissor(true);
    Path prev_line, prev_shape, next_line, next_shape;

    prev_preset_ = std::make_unique<ShapeButton>("Prev");
    addAndMakeVisible(prev_preset_.get());
    addOpenGlComponent(prev_preset_->getGlComponent());
    prev_preset_->addListener(this);
    prev_line.startNewSubPath(0.65f, 0.3f);
    prev_line.lineTo(0.35f, 0.5f);
    prev_line.lineTo(0.65f, 0.7f);

    arrow_stroke.createStrokedPath(prev_shape, prev_line);
    prev_shape.addLineSegment(Line<float>(0.0f, 0.0f, 0.0f, 0.0f), 0.2f);
    prev_shape.addLineSegment(Line<float>(1.0f, 1.0f, 1.0f, 1.0f), 0.2f);
    prev_preset_->setShape(prev_shape);

    next_preset_ = std::make_unique<ShapeButton>("Next");
    addAndMakeVisible(next_preset_.get());
    addOpenGlComponent(next_preset_->getGlComponent());
    next_preset_->addListener(this);
    next_line.startNewSubPath(0.35f, 0.3f);
    next_line.lineTo(0.65f, 0.5f);
    next_line.lineTo(0.35f, 0.7f);

    arrow_stroke.createStrokedPath(next_shape, next_line);
    next_shape.addLineSegment(Line<float>(0.0f, 0.0f, 0.0f, 0.0f), 0.2f);
    next_shape.addLineSegment(Line<float>(1.0f, 1.0f, 1.0f, 1.0f), 0.2f);
    next_preset_->setShape(next_shape);
  }

  void PresetSelector::paintBackground(Graphics &g)
  {
    float round_amount = findValue(Skin::kWidgetRoundedCorner);
    g.setColour(findColour(Skin::kPopupSelectorBackground, true));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), round_amount);
  }

  void PresetSelector::resized()
  {
    BaseSection::resized();

    if (text_component_)
    {
      BaseSection *parent = findParentComponentOfClass<BaseSection>();
      int button_height = parent->findValue(Skin::kTextComponentFontSize);
      int offset = parent->findValue(Skin::kTextComponentOffset);
      int button_y = (getHeight() - button_height) / 2 + offset;
      prev_preset_->setBounds(0, button_y, button_height, button_height);
      next_preset_->setBounds(getWidth() - button_height, button_y, button_height, button_height);
      text_->setBounds(getLocalBounds().translated(0, offset));
      text_->setTextSize(button_height);
    }
    else
    {
      int height = getHeight();
      text_->setBounds(Rectangle<int>(height, 0, getWidth() - 2 * height, height));
      text_->setTextSize(height * font_height_ratio_);
      prev_preset_->setBounds(0, 0, height, height);
      next_preset_->setBounds(getWidth() - height, 0, height, height);
      text_->setColor(findColour(Skin::kPresetText, true));
    }
  }
}
