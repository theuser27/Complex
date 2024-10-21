/*
  ==============================================================================

    Popups.cpp
    Created: 2 Feb 2023 7:49:54pm
    Author:  theuser27

  ==============================================================================
*/

#include "Popups.hpp"

#include "Framework/parameter_value.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/ScrollBar.hpp"
#include "../Components/BaseControl.hpp"

namespace
{
  int getLinesForText(const juce::Font &font, const juce::String &string, int widthAvailable)
  {
    float hintWidth = font.getStringWidthFloat(string);
    return (int)std::ceil(hintWidth / (float)widthAvailable);
  }
}

namespace Interface
{
  PopupDisplay::PopupDisplay() : OpenGlContainer{ "Popup Display" }
  {
    font_ = Fonts::instance()->getDDinFont();
    
    addOpenGlComponent(&body_);
    addOpenGlComponent(&border_);
    addOpenGlComponent(&text_);

    text_.setPaintFunction([this](Graphics &g, juce::Rectangle<int>)
      {
        g.setColour(getColour(Skin::kWidgetPrimary1));
        g.setFont(font_);
        g.drawText(string_, getLocalBounds(), Justification::centred); 
      });

    setSkinOverride(Skin::kPopupBrowser);
  }

  PopupDisplay::~PopupDisplay() = default;

  void PopupDisplay::redo()
  {
    juce::Rectangle<int> bounds = getLocalBounds();
    float rounding = getValue(Skin::kBodyRoundingTop);

    body_.setBounds(bounds);
    body_.setRounding(rounding);
    body_.setColor(getColour(Skin::kPopupDisplayBackground));

    border_.setBounds(bounds);
    border_.setRounding(rounding);
    border_.setThickness(1.0f);
    border_.setColor(getColour(Skin::kPopupDisplayBorder));

    text_.setColor(getColour(Skin::kNormalText));
    text_.setBounds(bounds);
    text_.redrawImage();
  }

  void PopupDisplay::setContent(String text, juce::Rectangle<int> sourceBounds, juce::Rectangle<int> screen,
    Placement placement, Skin::SectionOverride sectionOverride)
  {
    static constexpr int kHeight = 24;

    setSkinOverride(sectionOverride);

    string_ = COMPLEX_MOV(text);
    float floatHeight = std::round(scaleValue(kHeight));
    Fonts::instance()->setHeight(font_, floatHeight * 0.5f);
    int height = (int)floatHeight;
    int padding = height / 4;
    int buffer = padding * 2 + 2;
    int width = font_.getStringWidth(string_) + buffer;

    int middleX = sourceBounds.getX() + sourceBounds.getWidth() / 2;
    int middleY = sourceBounds.getY() + sourceBounds.getHeight() / 2;

    juce::Rectangle<int> bounds{};

    if (placement == Placement::above)
      bounds = { middleX - width / 2, sourceBounds.getY() - height, width, height };
    else if (placement == Placement::below)
      bounds = { middleX - width / 2, sourceBounds.getBottom(), width, height };
    else if (placement == Placement::left)
      bounds = { sourceBounds.getX() - width, middleY - height / 2, width, height };
    else if (placement == Placement::right)
      bounds = { sourceBounds.getRight(), middleY - height / 2, width, height };

    setBounds(bounds.constrainedWithin(screen));
    redo();
  }

  PopupList::PopupList() : OpenGlContainer{ "Popup List" }
  {
    background_.setTargetComponent(this);
    addOpenGlComponent(&background_);

    rows_.setColor(Colours::white);
    rows_.setPaintFunction([this](Graphics &g, juce::Rectangle<int> redrawArea) { paintList(g, redrawArea); });
    rows_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        float offset = 2.0f * (float)getViewPosition() / (float)getListHeight();
        auto *rows = utils::as<OpenGlImage>(&target);
        rows->movePosition(0.0f, offset);
        rows->render(openGl);
      });
    addOpenGlComponent(&rows_);

    hover_.setAdditive(true);
    hover_.setRenderFunction([this](OpenGlWrapper &openGl, OpenGlComponent &target)
      {
        auto bounds = hoveredBounds_.get();
        if (bounds.isEmpty())
          return;
        
        int y = bounds.getY() - getViewPosition();
        auto *hover = utils::as<OpenGlQuad>(&target);
        hover->setCustomViewportBounds(bounds.withY(y));
        hover->setRounding(hoveredRouding_);
        hover->render(openGl);
      });
    addOpenGlComponent(&hover_);

    scrollBar_ = utils::up<ScrollBar>::create(true);
    scrollBar_->setAlwaysOnTop(true);
    scrollBar_->addListener(this);
    addSubOpenGlContainer(scrollBar_.get());
  }

  PopupList::~PopupList() = default;

  int PopupList::getViewPosition() const
  {
    int max = std::max(0, getListHeight() - getVisibleHeight());
    return utils::clamp((int)viewPosition_.get(), 0, max);
  }

  void PopupList::visibilityChanged()
  {
    hoveredBounds_ = juce::Rectangle<int>{};
    childList_ = nullptr;
  }

  std::pair<PopupItems *, juce::Rectangle<int>> PopupList::getSelection(Point<int> position)
  {
    position.y -= (commonInfo_->listRounding - getViewPosition());
    for (auto [item, bounds] : itemBounds_)
    {
      if (!bounds.contains(position))
        continue;

      if (item->type == PopupItems::InlineGroup)
      {
        hoveredRouding_ = (float)commonInfo_->listRounding * 0.5f;
        int x = position.x - bounds.getX();
        int elementWidth = bounds.getWidth() / item->size();
        int hoveredElement = x / elementWidth;
        return { &item->items[hoveredElement], juce::Rectangle{ bounds.getX() + hoveredElement * elementWidth,
          bounds.getY(), elementWidth, bounds.getHeight() } };
      }
      
      hoveredRouding_ = 0.0f;
      return { item, bounds };
    }

    return { nullptr, {} };
  }

  void PopupList::mouseMove(const MouseEvent &e)
  {
    auto [list, bounds] = getSelection(e.position.toInt());

    // do we have a sublist open?
    if (childList_ && childList_ != list)
    {
      listener_->closeSubList(childList_);
      childList_ = nullptr;
    }
    // if we were hovering over a normal item and change to a delimiter/inactive item, turn off hover
    if (list && (list->type == PopupItems::Delimiter || !list->isActive))
    {
      hoveredBounds_ = juce::Rectangle<int>{};
      return;
    }
    // otherwise, we're changing to a normal item and we move hover
    else
      hoveredBounds_ = bounds + Point{ 0, commonInfo_->listRounding };

    // summoning child dropdown
    if (list && childList_ != list && !list->items.empty() && 
      list->type != PopupItems::InlineGroup)
    {
      childList_ = list;
      listener_->summonNewPopupList(getBounds(), list);
    }
  }

  void PopupList::mouseDrag(const MouseEvent &e)
  {
    auto [list, bounds] = getSelection(e.position.toInt());
    if (list && (list->type == PopupItems::Delimiter || !list->isActive))
    {
      hoveredBounds_ = juce::Rectangle<int>{};
      return;
    }

    if (e.position.x < 0.0f || e.position.x > (float)getWidth())
      bounds = {};
    hoveredBounds_ = bounds + Point{ 0, (int)commonInfo_->listRounding };
  }

  void PopupList::mouseExit(const MouseEvent &)
  {
    if (!childList_)
      hoveredBounds_ = juce::Rectangle<int>{};
  }

  void PopupList::mouseUp(const MouseEvent &e)
  {
    if (e.position.x < 0 || e.position.x > (float)getWidth())
      return;

    select(getSelection(e.position.toInt()));
  }

  void PopupList::select(std::pair<PopupItems *, juce::Rectangle<int>> selectedItem)
  {
    auto [item, bounds] = selectedItem;
    
    if (item == nullptr || !item->isActive ||
      item->type == PopupItems::Delimiter ||
      item->type == PopupItems::AutomationList ||
      item->type == PopupItems::InlineGroup)
      return;

    listener_->newSelection(this, item->id);

    // redraw list if upon change
    if (items_->type == PopupItems::AutomationList)
    {
      hoveredBounds_ = juce::Rectangle<int>{};
      rows_.redrawImage(bounds);
    }
  }

  void PopupList::recalculateSizes()
  {
    hoveredBounds_ = juce::Rectangle<int>{};
    childList_ = nullptr;
    itemBounds_.clear();
    
    int totalWidth = scaleValueRoundInt((float)commonInfo_->minWidth);
    int scrollBarWidth = scaleValueRoundInt(kScrollBarWidth);
    int vPadding = scaleValueRoundInt(kVPadding);
    int hEntryPadding = scaleValueRoundInt(kHEntryPadding);
    int inlineGroupHeight = scaleValueRoundInt(kInlineGroupHeight);
    int sideArrowWidth = scaleValueRoundInt(kSideArrowWidth);
    int crossWidth = scaleValueRoundInt(kCrossWidth);
    int automationListWidth = scaleValueRoundInt(kAutomationListWidth);
    int entryToSideArrowMinMargin = scaleValueRoundInt(kHEntryToSideArrowMinMargin);

    if (items_->type == PopupItems::AutomationList)
    {
      totalWidth = automationListWidth + crossWidth + scrollBarWidth;
    }
    else
    {
      std::optional<int> inlineGroupSize{};
      for (auto &[subItems, icon, name, hint, type, id, shortcut, isActive] : items_->items)
      {
        int elementWidth = 0;
        if (type == PopupItems::Entry || type == PopupItems::AutomationList)
        {
          elementWidth = commonInfo_->primaryFont.getStringWidth(name);
          elementWidth += hEntryPadding * 2;
          if (!subItems.empty() || type == PopupItems::AutomationList)
            elementWidth += entryToSideArrowMinMargin + sideArrowWidth;
        }
        else if (type == PopupItems::InlineGroup)
        {
          COMPLEX_ASSERT(!inlineGroupSize.has_value() && "Multiple inline groups cannot be handled");
          inlineGroupSize = (int)subItems.size();
          elementWidth = inlineGroupHeight * (int)subItems.size() + vPadding * 2;
        }
        else if (type == PopupItems::Delimiter)
        {
          elementWidth = commonInfo_->primaryFont.getStringWidth(name);
          elementWidth += hEntryPadding;
          elementWidth += commonInfo_->primaryFont.getStringWidth(hint);
          elementWidth += hEntryPadding * 2;
        }
        totalWidth = std::max(totalWidth, elementWidth);
      }

      if (inlineGroupSize)
      {
        // ceil to the nearest multiple of 2 * elementCount
        int width = totalWidth - vPadding * 2;
        int mod = 2 * inlineGroupSize.value();
        totalWidth = 2 * vPadding + width + mod - (width % mod);
      }
    }

    listWidth_ = totalWidth;

    int entryToHintMargin = scaleValueRoundInt(kVEntryToHintMargin);
    int delimiterHeight = scaleValueRoundInt(kDelimiterHeight);
    int primaryTextLineHeight = scaleValueRoundInt(kPrimaryTextLineHeight);
    int secondaryTextLineHeight = scaleValueRoundInt(kSecondaryTextLineHeight);
    int totalHeight = 0;
    if (items_->type == PopupItems::AutomationList)
    {
      auto lineWidth = totalWidth - crossWidth - scrollBarWidth;
      for (size_t i = 0; i < items_->items.size(); i += 2)
      {
        int elementHeight = 0;
        int lines = getLinesForText(commonInfo_->primaryFont, items_->items[i].name, lineWidth - hEntryPadding);
        elementHeight += std::max(lines, 1) * primaryTextLineHeight;
        elementHeight += 2 * vPadding;

        itemBounds_.emplace_back(&items_->items[i], juce::Rectangle{ 0, totalHeight, lineWidth, elementHeight });
        itemBounds_.emplace_back(&items_->items[i + 1], juce::Rectangle{ lineWidth, totalHeight +
           utils::centerAxis(crossWidth, elementHeight), crossWidth, crossWidth });

        totalHeight += elementHeight;
      }
    }
    else
    {
      for (int i = 0; i < items_->size(); ++i)
      {
        int elementHeight = 0;
        if (items_->items[i].type == PopupItems::Entry || items_->items[i].type == PopupItems::AutomationList)
        {
          elementHeight += primaryTextLineHeight;
          elementHeight += 2 * vPadding;

          if (!items_->items[i].hint.empty())
          {
            elementHeight += entryToHintMargin;
            int lines = getLinesForText(commonInfo_->secondaryFont, items_->items[i].hint, totalWidth - 2 * hEntryPadding);
            elementHeight += lines * secondaryTextLineHeight;
          }

          itemBounds_.emplace_back(&items_->items[i], juce::Rectangle{ 0, totalHeight, totalWidth, elementHeight });
        }
        else if (items_->items[i].type == PopupItems::InlineGroup)
        {
          elementHeight += vPadding + inlineGroupHeight;
          if (items_->size() > i + 1)
            elementHeight += vPadding;

          itemBounds_.emplace_back(&items_->items[i], 
            juce::Rectangle{ vPadding, totalHeight + vPadding, totalWidth - 2 * vPadding, inlineGroupHeight });
        }
        else if (items_->items[i].type == PopupItems::Delimiter)
        {
          elementHeight += delimiterHeight;
          itemBounds_.emplace_back(&items_->items[i], juce::Rectangle{ 0, totalHeight, totalWidth, delimiterHeight });
        }

        totalHeight += elementHeight;
      }
    }

    listHeight_ = totalHeight;
  }

  void PopupList::setComponentsBounds()
  {
    background_.setColor(getColour(commonInfo_->sectionOverride, Skin::kPopupSelectorBackground));
    background_.setRounding((float)commonInfo_->listRounding);

    int width = getWidth();
    int listRounding = commonInfo_->listRounding;
    int visibleHeight = getHeight() - 2 * listRounding;
    visibleHeight_ = visibleHeight;
    int listHeight = getListHeight();

    Colour lighten = getColour(commonInfo_->sectionOverride, Skin::kLightenScreen);
    scrollBar_->setColor(lighten);

    if (listHeight > visibleHeight)
    {
      int scrollBarWidth = scaleValueRoundInt(kScrollBarWidth);
      scrollBar_->setRenderInset({ 0, scrollBarWidth / 4, 0, scrollBarWidth / 4 });
      scrollBar_->setBounds(width - scrollBarWidth, listRounding, scrollBarWidth, visibleHeight);
      scrollBar_->setVisible(true);
      setScrollBarRange(visibleHeight, listHeight);
    }
    else
      scrollBar_->setVisible(false);

    hover_.setCustomScissorBounds({ 0, listRounding, width, visibleHeight });
    hover_.setColor(getColour(commonInfo_->sectionOverride, Skin::kWidgetPrimary1).darker(0.8f));

    rows_.setBounds(0, listRounding, getWidth(), std::max(listHeight, visibleHeight));
    rows_.setCustomScissorBounds({ width, visibleHeight });
    rows_.redrawImage();
  }

  void PopupList::mouseWheelMove(const MouseEvent &, const MouseWheelDetails &wheel)
  {
    viewPosition_ = viewPosition_.get() - wheel.deltaY * kScrollSensitivity;
    int visibleHeight = getVisibleHeight();
    int listHeight = getListHeight();
    viewPosition_ = utils::clamp(viewPosition_.get(), 0.0f, 
      (float)(std::max(listHeight, visibleHeight) - visibleHeight));
    setScrollBarRange(visibleHeight, listHeight);
  }

  void PopupList::setScrollBarRange(int visibleHeight, int listHeight)
  {
    static constexpr double kScrollStepRatio = 0.025f;

    scrollBar_->setRangeLimits(0.0f, std::max(listHeight, getHeight()));
    scrollBar_->setCurrentRange(getViewPosition(), visibleHeight, dontSendNotification);
    scrollBar_->setSingleStepSize(scrollBar_->getHeight() * kScrollStepRatio);
    scrollBar_->cancelPendingUpdate();
  }

  void PopupList::paintList(Graphics &g, juce::Rectangle<int> redrawArea)
  {
    static const Path sideArrow = []()
    {
      Path path;
      path.startNewSubPath(0.0f, 0.0f);
      path.lineTo(0.5f, 0.5f);
      path.lineTo(0.0f, 1.0f);
      return path;
    }();

    static const Path cross = []()
    {
      Path path;
      path.startNewSubPath(0.0f, 0.0f);
      path.lineTo(1.0f, 1.0f);
      path.startNewSubPath(1.0f, 0.0f);
      path.lineTo(0.0f, 1.0f);
      return path;
    }();

    int vPadding = scaleValueRoundInt(kVPadding);
    int hEntryPadding = scaleValueRoundInt(kHEntryPadding);
    int entryToHintMargin = scaleValueRoundInt(kVEntryToHintMargin);
    float sideArrowWidth = scaleValue(kSideArrowWidth);
    float iconSize = scaleValue(kIconSize);
    int primaryTextLineHeight = scaleValueRoundInt(kPrimaryTextLineHeight);
    int secondaryTextLineHeight = scaleValueRoundInt(kSecondaryTextLineHeight);
    int entryTextWidth = getWidth() - 2 * hEntryPadding;
    int delimiterTextWidth = entryTextWidth;
    Colour primaryTextColour = getColour(commonInfo_->sectionOverride, Skin::kTextComponentText1);
    Colour secondaryTextColour = getColour(commonInfo_->sectionOverride, Skin::kTextComponentText2);
    Colour delimiterBackground = getColour(commonInfo_->sectionOverride, Skin::kPopupSelectorDelimiter);
    Colour themeColour = getColour(commonInfo_->sectionOverride, Skin::kWidgetPrimary1);

    switch (items_->type)
    {
    case PopupItems::Entry:
      for (auto [item, bound] : itemBounds_)
      {
        switch (item->type)
        {
        case PopupItems::Entry:
        case PopupItems::AutomationList:
          g.setFont(commonInfo_->primaryFont);
          g.setColour(primaryTextColour);
          g.drawText(item->name, hEntryPadding, bound.getY() + vPadding,
            entryTextWidth, primaryTextLineHeight, Justification::centredLeft, true);

          if (item->size() || item->type == PopupItems::AutomationList)
          {
            g.setColour(secondaryTextColour);
            auto transform = sideArrow.getTransformToScaleToFit((float)(bound.getRight() - hEntryPadding) - sideArrowWidth,
              (float)(bound.getY() + vPadding), sideArrowWidth, (float)primaryTextLineHeight, true, Justification::centred);
            g.strokePath(sideArrow, PathStrokeType{ 1.0f }, transform);
          }

          if (!item->hint.empty())
          {
            g.setColour(secondaryTextColour);
            g.setFont(commonInfo_->secondaryFont);

            int secondaryLineBaselineY = bound.getY() + vPadding + primaryTextLineHeight + entryToHintMargin +
              secondaryTextLineHeight - (int)commonInfo_->secondaryFont.getDescent();

            g.drawMultiLineText(item->hint, hEntryPadding, secondaryLineBaselineY,
              entryTextWidth, Justification::centredLeft);
          }

          if (!item->isActive)
          {
            g.setColour(getColour(Skin::kShadow).withMultipliedAlpha(0.8f));
            g.fillRect(bound);
          }
          break;
        case PopupItems::InlineGroup:
        {
          float containerWidth = (float)bound.getWidth() / (float)item->size();
          float strokeWidth = scaleValue(1.0f);
          auto iconBounds = juce::Rectangle{ (float)bound.getX() + utils::centerAxis(iconSize, containerWidth),
            (float)bound.getY() + utils::centerAxis(iconSize, (float)bound.getHeight()), iconSize, iconSize };
          auto transform = AffineTransform::translation(iconBounds.getPosition()).scaled(uiRelated.scale);
          auto colours = std::vector{ secondaryTextColour, themeColour };

          for (int j = 0; j < item->size(); ++j)
          {
            item->items[j].icon.drawAll(g, PathStrokeType{ strokeWidth,
              PathStrokeType::curved, PathStrokeType::rounded }, transform, colours);
            transform = transform.translated(Point{ containerWidth, 0.0f });
          }
        }
        break;
        case PopupItems::Delimiter:
          g.setColour(delimiterBackground);
          g.fillRect(bound);
          g.setFont(commonInfo_->secondaryFont);
          g.setColour(secondaryTextColour);
          g.drawText(item->name, hEntryPadding, bound.getY(),
            delimiterTextWidth, bound.getHeight(), Justification::centredLeft, true);
          g.drawText(item->hint, hEntryPadding, bound.getY(),
            delimiterTextWidth, bound.getHeight(), Justification::centredRight, true);
          break;
        default:
          COMPLEX_ASSERT_FALSE("Unhandled case");
          break;
        }
      }
      break;
    case PopupItems::AutomationList:
      {
        int extraOffset = (int)std::round(((float)primaryTextLineHeight - commonInfo_->primaryFont.getHeight()) * 0.5f);
        auto drawEntry = [&](const String &name, const Framework::ParameterLink *mapping, 
          juce::Rectangle<int> itemBounds, juce::Rectangle<int> crossBounds)
        {
          g.setFont(commonInfo_->primaryFont);
          g.setColour((mapping) ? Colour{ mapping->parameter->getThemeColour() } : primaryTextColour);
          g.drawMultiLineText(name, hEntryPadding, itemBounds.getY() + vPadding + extraOffset +
            (int)commonInfo_->primaryFont.getAscent(), entryTextWidth, Justification::centredLeft);

          if (mapping)
          {
            g.setColour(primaryTextColour);
            auto transform = cross.getTransformToScaleToFit(crossBounds.toFloat(), true, Justification::centred);
            g.strokePath(cross, PathStrokeType{ 1.0f }, transform);
          }
        };

        auto bridges = uiRelated.renderer->getPlugin().getParameterBridges();

        if (g.getClipBounds() != redrawArea)
        {
          auto iter = std::ranges::find_if(itemBounds_, 
            [&](const auto &pair) { return pair.second == redrawArea; });

          COMPLEX_ASSERT(iter != itemBounds_.end());

          size_t index = (iter - itemBounds_.begin()) / 2;
          itemBounds_[index * 2].first->isActive = !itemBounds_[index * 2].first->isActive;
          itemBounds_[index * 2 + 1].first->isActive = !itemBounds_[index * 2 + 1].first->isActive;

          g.setColour(Colours::transparentBlack);
          g.fillRect({ 0, itemBounds_[index * 2].second.getY(), g.getClipBounds().getWidth(), 
            itemBounds_[index * 2].second.getHeight() }, true);

          drawEntry(bridges[index]->getName(), bridges[index]->getParameterLink(),
            itemBounds_[index * 2].second, itemBounds_[index * 2 + 1].second);

          return;
        }

        for (size_t i = 0; i < bridges.size(); ++i)
          drawEntry(bridges[i]->getName(), bridges[i]->getParameterLink(), 
            itemBounds_[i * 2].second, itemBounds_[i * 2 + 1].second);
      }
      break;
    case PopupItems::Delimiter:
    case PopupItems::InlineGroup:
    default:
      COMPLEX_ASSERT_FALSE("Invalid popup menu top level type");
      break;
    }
  }

  PopupSelector::PopupSelector() : BaseSection{ "Popup Selector" }
  {
    setInterceptsMouseClicks(false, true);
    addList();
  }

  PopupSelector::~PopupSelector() = default;

  void PopupSelector::resized()
  {
    commonInfo_.primaryFont = Fonts::instance()->getInterVFont();
    commonInfo_.secondaryFont = commonInfo_.primaryFont;
    Fonts::instance()->setHeight(commonInfo_.primaryFont, scaleValue(kPrimaryFontHeight));
    Fonts::instance()->setHeight(commonInfo_.secondaryFont, scaleValue(kSecondaryFontHeight));
    commonInfo_.listRounding = scaleValueRoundInt(getValue(Skin::kBodyRoundingTop));
  }

  bool PopupSelector::keyPressed(const KeyPress &key)
  {
    // get currently focused sublist
    PopupList *selectedList = nullptr;
    for (auto &list : lists_)
    {
      if (!list->getIsUsed())
        break;
      selectedList = list.get();
    }

    for (auto &item : selectedList->getItems()->items)
    {
      if (!key.isKeyCode(item.shortcut))
        continue;
      
      newSelection(selectedList, item.id);
      return true;
    }

    return false;
  }

  void PopupSelector::newSelection(PopupList *list, int id)
  {
    // we must make sure that the object that provided the callback still exists
    if (!checker_.wasObjectDeleted())
    {
      if (id >= 0)
      {
        callback_(id);
        if (list->getItems()->type == PopupItems::AutomationList 
          && BaseControl::isUnmappingParameter(id))
          return;

        setVisible(false);
      }
      else
        cancel_();
    }

    resetState();
  }

  void PopupSelector::summonNewPopupList(juce::Rectangle<int> sourceBounds, PopupItems *items)
  {
    PopupList *newList = nullptr;
    for (auto &list : lists_)
      if (!list->getIsUsed())
        newList = list.get();

    if (!newList)
    {
      addList();
      newList = lists_.back().get();
    }

    // we may have already used it with these items
    bool isCached = newList->getItems() == items && 
      items->type != PopupItems::AutomationList;
    if (!isCached)
    {
      newList->setItems(items);
      newList->recalculateSizes();
    }

    newList->setIsUsed(true);

    int width = newList->getListWidth();
    int height = newList->getListHeight() + 2 * commonInfo_.listRounding;
    int windowWidth = getWidth();
    int windowHeight = getHeight();

    int y = sourceBounds.getY();
    if (y + height >= getHeight())
    {
      y = std::max(sourceBounds.getBottom() - height, 0);
      height = std::min(height, windowHeight);
    }

    int x = sourceBounds.getRight();
    if ((lastPlacement_ == Placement::left || x + width >= windowWidth) 
      && sourceBounds.getX() - width > 0)
    {
      x = sourceBounds.getX() - width;
      lastPlacement_ = Placement::left;
    }
    else
    {
      x = std::min(x + width, windowWidth) - width;
      lastPlacement_ = Placement::right;
    }

    newList->setBounds(x, y, width, height);
    if (!isCached)
      newList->setComponentsBounds();
    newList->setVisible(true);
  }

  void PopupSelector::closeSubList(PopupItems *items)
  {
    size_t index = 0;
    for (; index < lists_.size(); ++index)
      if (lists_[index]->getItems() == items)
        break;

    if (index == lists_.size())
      return;

    COMPLEX_ASSERT(index > 0);

    lastPlacement_ = ((lists_[index]->getX() - lists_[index - 1]->getX()) > 0) ? 
      Placement::right : Placement::left;

    for (; index < lists_.size(); ++index)
    {
      lists_[index]->setVisible(false);
      lists_[index]->setIsUsed(false);
    }
  }

  void PopupSelector::focusLost(FocusChangeType)
  {
    // we must make sure that the object that provided the callback still exists
    if (!checker_.wasObjectDeleted())
    {
      setVisible(false);
      if (cancel_)
        cancel_();
    }

    resetState();
  }

  void PopupSelector::positionList(Point<int> sourcePosition)
  {
    PopupList *lastUsedList = nullptr;
    for (auto &list : lists_)
    {
      if (list->getIsUsed())
        lastUsedList = list.get();
      else
        break;
    }

    COMPLEX_ASSERT(lastUsedList);

    int width = lastUsedList->getListWidth();
    int height = lastUsedList->getListHeight() + 2 * commonInfo_.listRounding;
    auto [x, y] = sourcePosition;
    int windowWidth = getWidth();
    int windowHeight = getHeight();

    // do we have space to the right
    if (x + width > windowWidth)
    {
      // do we have enough space to the left
      if (x > width)
        x -= width;
      else
      {
        // neither have enough space, offset and shrink if necessary
        width = std::min(width, windowWidth);
        x = windowWidth - width;
      }
    }
    if (y + height > windowHeight)
    {
      if (y > height)
        y -= height;
      else
      {
        height = std::min(windowHeight, height);
        y = windowHeight - height;
      }
    }

    lastUsedList->setBounds(x, y, width, height);
    lastUsedList->setComponentsBounds();
    lastUsedList->setVisible(true);
  }

  void PopupSelector::positionList(juce::Rectangle<int> sourceBounds, Placement placement)
  {
    PopupList *lastUsedList = nullptr;
    for (auto &list : lists_)
    {
      if (list->getIsUsed())
        lastUsedList = list.get();
      else
        break;
    }

    COMPLEX_ASSERT(lastUsedList);

    auto position = sourceBounds.getPosition();
    int width = lastUsedList->getListWidth();
    int height = lastUsedList->getListHeight() + 2 * commonInfo_.listRounding;
    int windowWidth = getWidth();
    int windowHeight = getHeight();
    int popupToElement = scaleValueRoundInt(kPopupToElement);

    auto [finalX, finalY] = position;

    auto checkHorizontal = [&](int offset = 0)
    {
      if (finalX + width <= windowWidth)
        return;

      // popup cannot be fit horizontally, find the side with the most width
      if (position.x + sourceBounds.getWidth() >= windowWidth - position.x)
      {
        width = std::min(width, position.x + sourceBounds.getWidth() - offset);
        finalX = position.x + sourceBounds.getWidth() - offset - width;
      }
      else
      {
        finalX = position.x + offset;
        width = std::min(width, windowWidth - finalX);
      }
    };

    auto checkVertical = [&](int offset = 0)
    {
      if (finalY + height <= windowHeight)
        return;

      // popup cannot be fit vertically, find the side with the most height
      if (position.y + sourceBounds.getHeight() >= windowHeight - position.y)
      {
        height = std::min(height, position.y + sourceBounds.getHeight() - offset);
        finalY = position.y + sourceBounds.getHeight() - offset - height;
      }
      else
      {
        finalY = position.y + offset;
        height = std::min(height, windowHeight - finalY);
      }
    };

    if (placement == Placement::below || placement == Placement::above)
    {
      checkHorizontal();
      finalY = (placement == Placement::below) ? finalY + sourceBounds.getHeight() + popupToElement :
        finalY - height - popupToElement;
      checkVertical(sourceBounds.getHeight() + popupToElement);
    }
    else if (placement == Placement::left || placement == Placement::right)
    {
      checkVertical();
      finalX = (placement == Placement::left) ? finalX - width - popupToElement :
        finalX + sourceBounds.getWidth() + popupToElement;
      checkHorizontal(sourceBounds.getWidth() + popupToElement);
    }
    else
    {
      COMPLEX_ASSERT_FALSE("You need to choose a one of the placements for the popup");
      checkHorizontal();
      finalY = finalY + sourceBounds.getHeight();
      checkVertical(sourceBounds.getHeight());
    }

    lastUsedList->setBounds(finalX, finalY, width, height);
    lastUsedList->setComponentsBounds();
    lastUsedList->setVisible(true);
  }

  void PopupSelector::addList()
  {
    auto &list = lists_.emplace_back(utils::up<PopupList>::create());
    list->setListener(this);
    list->setCommonInfo(&commonInfo_);
    list->setAlwaysOnTop(true);
    list->setWantsKeyboardFocus(false);
    addSubOpenGlContainer(list.get());
  }

  void PopupSelector::fillAutomationListIfExists()
  {
    auto recurse = [this](auto &self, PopupItems *items) -> bool
    {
      for (auto &item : items->items)
      {
        if (item.type == PopupItems::AutomationList)
        {
          COMPLEX_ASSERT(item.items.empty());

          int id = item.id;
          auto bridges = uiRelated.renderer->getPlugin().getParameterBridges();
          item.items.reserve(bridges.size() * 2);
          for (auto *bridge : bridges)
          {
            auto *parameterLink = bridge->getParameterLink();
            item.items.emplace_back(PopupItems::Entry, id, bridge->getName().toStdString(),
              std::string{}, Shape{}, parameterLink == nullptr);

            item.items.emplace_back(PopupItems::Entry, id + 1, std::string{},
              std::string{}, Shape{}, parameterLink != nullptr);

            id += 2;
          }

          return true;
        }

        if (!item.items.empty() && self(self, &item))
          return true;
      }

      return false;
    };

    (void)recurse(recurse, &items_);
  }
}
