/*
  ==============================================================================

    Popups.hpp
    Created: 2 Feb 2023 7:49:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/parameter_bridge.hpp"
#include "BaseSection.hpp"
#include "../Components/OpenGlQuad.hpp"
#include "../Components/OpenGlImage.hpp"

namespace Interface
{
  class OpenGlQuad;
  class OpenGlImage;

  class PopupDisplay final : public OpenGlContainer
  {
  public:
    PopupDisplay();
    ~PopupDisplay() override;

    void resized() override { }

    void setContent(juce::String text, juce::Rectangle<int> sourceBounds, 
      juce::Rectangle<int> screen, Placement placement, Skin::SectionOverride sectionOverride);

  private:
    void redo();

    OpenGlQuad body_{ Shaders::kRoundedRectangleFragment };
    OpenGlQuad border_{ Shaders::kRoundedRectangleBorderFragment };
    OpenGlImage text_{ "Popup Text" };

    juce::String string_;
    juce::Font font_;
  };

  class PopupList final : public OpenGlContainer, OpenGlScrollBarListener
  {
  public:
    class Listener
    {
    public:
      virtual ~Listener() = default;
      virtual void newSelection(PopupList *list, int id) = 0;
      virtual void summonNewPopupList([[maybe_unused]] juce::Rectangle<int> sourceBounds,
        [[maybe_unused]] PopupItems *items) { }
      virtual void closeSubList([[maybe_unused]] PopupItems *items) { }
    };

    struct CommonListInfo
    {
      juce::Font primaryFont;
      juce::Font secondaryFont;
      int minWidth = 0;
      int listRounding = 0;
      Skin::SectionOverride sectionOverride;
    };

    static constexpr float kScrollSensitivity = 150.0f;
    static constexpr float kAutomationListWidth = 150.0f;

    static constexpr float kIconSize = 16.0f;
    static constexpr float kScrollBarWidth = 8.0f;
    static constexpr float kSideArrowWidth = 4.0f;
    static constexpr float kCrossWidth = 8.0f;

    static constexpr float kPrimaryTextLineHeight = 16.0f;
    static constexpr float kSecondaryTextLineHeight = 12.0f;
    static constexpr float kDelimiterHeight = 20.0f;
    static constexpr float kInlineGroupHeight = 28.0f;	// serves also as minimum width for the elements

    static constexpr float kVPadding = 4.0f;
    static constexpr float kHEntryPadding = 12.0f;
    static constexpr float kHEntryToSideArrowMinMargin = 16.0f;
    static constexpr float kVEntryToHintMargin = 3.0f;

    PopupList();
    ~PopupList() override;

    void mouseEnter(const juce::MouseEvent &e) override { mouseMove(e); }
    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &wheel) override;
    void resized() override { }
    void visibilityChanged() override;
    
    std::pair<PopupItems *, juce::Rectangle<int>> getSelection(juce::Point<int> position);
    PopupItems *getItems() const noexcept { return items_; }
    int getListWidth() const noexcept { return listWidth_; }
    int getListHeight() const noexcept { return listHeight_; }
    int getVisibleHeight() const noexcept { return visibleHeight_; }
    bool getIsUsed() const noexcept { return isUsed_; }

    void scrollBarMoved(ScrollBar *, double rangeStart) override 
    { viewPosition_ = (float)rangeStart; }

    void setItems(PopupItems *items) { items_ = items; }
    void setCommonInfo(CommonListInfo *commonInfo) { commonInfo_ = commonInfo; }
    void setListener(Listener *listener) { listener_ = listener; }
    void setIsUsed(bool isUsed) { isUsed_ = isUsed; }
    void select(std::pair<PopupItems *, juce::Rectangle<int>> selectedItem);

    void recalculateSizes();
    void setComponentsBounds();
  private:
    void setScrollBarRange(int visibleHeight, int listHeight);
    void paintList(juce::Graphics &g, juce::Rectangle<int> redrawArea);
    int getViewPosition() const;

    Listener *listener_ = nullptr;
    PopupItems *items_ = nullptr;
    PopupItems *childList_ = nullptr;
    CommonListInfo *commonInfo_ = nullptr;
    std::vector<std::pair<PopupItems *, juce::Rectangle<int>>> itemBounds_{};
    utils::shared_value<juce::Rectangle<int>> hoveredBounds_{};
    utils::shared_value<float> hoveredRouding_ = 0.0f;
    utils::shared_value<float> viewPosition_ = 0.0f;
    utils::shared_value<int> listWidth_ = 0;
    utils::shared_value<int> listHeight_ = 0;
    utils::shared_value<int> visibleHeight_ = 0;
    bool isUsed_ = false;

    utils::up<ScrollBar> scrollBar_;
    OpenGlQuad background_{ Shaders::kRoundedRectangleFragment };
    OpenGlImage rows_{ "Popup List Items" };
    OpenGlQuad hover_{ Shaders::kRoundedRectangleFragment };
  };

  class PopupSelector final : public BaseSection, public PopupList::Listener
  {
  public:
    static constexpr float kPrimaryFontHeight = 13.0f;
    static constexpr float kSecondaryFontHeight = 11.0f;

    PopupSelector();
    ~PopupSelector() override;

    void renderOpenGlComponents(OpenGlWrapper &openGl) override
    {
      openGl.parentStack.emplace_back(ScopedBoundsEmplace::doNotClipFlag);
      BaseSection::renderOpenGlComponents(openGl);
    }

    void resized() override;
    void visibilityChanged() override
    {
      if (isShowing())
        grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress &key) override;

    void newSelection(PopupList *list, int id) override;
    void summonNewPopupList(juce::Rectangle<int> sourceBounds, PopupItems *items) override;
    void closeSubList(PopupItems *items) override;
    void focusLost(FocusChangeType) override;

    void positionList(juce::Point<int> sourcePosition);
    void positionList(juce::Rectangle<int> sourceBounds, Placement placement);

    // unfortunately due to the way juce::WeakReference was designed
    // it's pretty much impossible to get a reference to const object
    // without doing lots of internal hacky casts
    // no mutation, only check to see if the object still exists when a selection is made
    void setComponent(const BaseComponent *component) { checker_ = const_cast<BaseComponent *>(component); }
    void setCallback(std::function<void(int)> callback) { callback_ = COMPLEX_MOV(callback); }
    void setCancelCallback(std::function<void()> cancel) { cancel_ = COMPLEX_MOV(cancel); }
    void setPopupSkinOverride(Skin::SectionOverride skinOverride) { commonInfo_.sectionOverride = skinOverride; }
    void setItems(PopupItems selections, int minWidth)
    {
      items_ = COMPLEX_MOV(selections);
      fillAutomationListIfExists();
      commonInfo_.minWidth = minWidth;
      lists_[0]->setItems(&items_);
      lists_[0]->setIsUsed(true);
      lists_[0]->recalculateSizes();
    }

    void resetState()
    {
      callback_ = {};
      cancel_ = {};
      for (auto &list : lists_)
      {
        list->setVisible(false);
        list->setIsUsed(false);
      }
      lastPlacement_ = Placement::right;
    }
  private:
    void addList();
    void fillAutomationListIfExists();

    PopupList::CommonListInfo commonInfo_{};
    std::function<void(int)> callback_{};
    std::function<void()> cancel_{};
    std::vector<utils::up<PopupList>> lists_;
    juce::WeakReference<BaseComponent> checker_;
    PopupItems items_{};

    Placement lastPlacement_ = Placement::right;
  };

}
