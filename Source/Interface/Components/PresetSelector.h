/*
  ==============================================================================

    PresetSelector.h
    Created: 31 Dec 2022 3:49:55am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"

namespace Interface
{
  /*class PresetSelector : public BaseSection
  {
  public:
    static constexpr float kDefaultFontHeightRatio = 0.63f;

    class Listener
    {
    public:
      virtual ~Listener() = default;

      virtual void prevClicked() = 0;
      virtual void nextClicked() = 0;
      virtual void textMouseUp(const MouseEvent &e) {}
      virtual void textMouseDown(const MouseEvent &e) {}
    };

    PresetSelector();
    ~PresetSelector() override = default;

    void paintBackground(Graphics &g) override;
    void resized() override;
    void buttonClicked(Button *clicked_button) override
    {
      if (clicked_button == prev_preset_.get())
        clickPrev();
      else if (clicked_button == next_preset_.get())
        clickNext();
    }
  	void mouseDown(const MouseEvent &e) override { textMouseDown(e); }
    void mouseUp(const MouseEvent &e) override { textMouseUp(e); }

    void mouseEnter(const MouseEvent &e) override { hover_ = true; }
    void mouseExit(const MouseEvent &e) override { hover_ = false; }

    String getText() { return text_->getText(); }
  	void setText(String text) { text_->setText(text); }
    void setText(String left, String center, String right)
  	{ text_->setText(left + "  " + center + "  " + right); }

    void setFontRatio(float ratio) { font_height_ratio_ = ratio; }
    void setRoundAmount(float round_amount) { round_amount_ = round_amount; }

    void addListener(Listener *listener) { listeners_.push_back(listener); }

    void clickPrev()
    {
      for (Listener *listener : listeners_)
        listener->prevClicked();
    }
    void clickNext()
    {
      for (Listener *listener : listeners_)
        listener->nextClicked();
    }

    void setTextComponent(bool text_component) { text_component_ = text_component; }

  private:
    void textMouseDown(const MouseEvent &e)
    {
      for (Listener *listener : listeners_)
        listener->textMouseDown(e);
    }
    void textMouseUp(const MouseEvent &e)
    {
      for (Listener *listener : listeners_)
        listener->textMouseUp(e);
    }

    float font_height_ratio_ = kDefaultFontHeightRatio;
    float round_amount_ = 0.0f;
    bool hover_ = false;
    bool text_component_ = false;

    std::unique_ptr<PlainTextComponent> text_ = nullptr;
    std::unique_ptr<ShapeButton> prev_preset_ = nullptr;
    std::unique_ptr<ShapeButton> next_preset_ = nullptr;

    std::vector<Listener *> listeners_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetSelector)
  };*/
}
