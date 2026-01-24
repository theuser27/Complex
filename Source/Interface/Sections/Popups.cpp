
// Created: 2023-02-02 19:49:54

#include "Popups.hpp"

#include "Framework/parameter_value.hpp"
#include "Plugin/Renderer.hpp"
#include "Plugin/Complex.hpp"
#include "../LookAndFeel/Graphics.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/BaseControl.hpp"

namespace
{
  int getLinesForText(Interface::Graphics &g, utils::string_view string, int widthAvailable)
  {
    float hintWidth = g.getStringWidthFloat(string);
    return (int)::ceilf(hintWidth / (float)widthAvailable);
  }
}

namespace Interface
{
  static Range<i32>
  getPopupDisplayDimensions(Component *c, i32 *availablePrimarySize)
  {
    const int lineHeight = scaleValueRoundInt(PopupDisplay::kLineHeight);

    auto *display = (PopupDisplay *)c;
    if (!availablePrimarySize)
    {
      if (display->isControl)
      {
        auto *control = (BaseControl *)display->source;
        display->text = COMPLEX_MOVE(control->getScaledValueString(control->getValue()));
      }

      display->cachedFontWidth = (i32)::ceilf(uiRelated.cache->getStringWidthFloat(display->text));
      return Range<i32>{ 0, display->cachedFontWidth };
    }
    else
    {
      auto lineCount = (i32)::ceilf((float)display->cachedFontWidth / (float)(*availablePrimarySize));
      return Range<i32>{ lineHeight, lineCount *lineHeight };
    }
  }

  PopupDisplay::PopupDisplay()
  {
    utils::bumpArena::createNested(getUIArena(), COMPLEX_KB(8));
    skinOverride = Skin::kUseParentOverride;
    placement = Placement::custom;
    sizingFlags = Component::HasText;
    padding = { kLineHeight, kLineHeight, kLineHeight, kLineHeight };
    desiredSize.getTextDimensions = getPopupDisplayDimensions;

    textFontId = uiRelated.cache->InterFontId;
    numericFontId = uiRelated.cache->DDinFontId;
  }

  bool PopupDisplay::render(OpenGlWrapper &openGl)
  {
    float rounding = getValue(Skin::kBodyRoundingTop, true);
    nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.y, rounding);
    nvgFillColor(openGl.g, getColour(Skin::kPopupDisplayBackground));
    nvgFill(openGl.g);

    nvgRoundedRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.y, rounding);
    nvgStrokeColor(openGl.g, getColour(Skin::kPopupDisplayBorder));
    nvgStroke(openGl.g);

    auto usedFontId = (isControl) ? numericFontId : textFontId;
    auto fontHeight = uiRelated.cache->getFontHeightFromAscent(usedFontId, (float)bounds.h * 0.5f);
    auto fontKerning = uiRelated.cache->getKerningForHeight(usedFontId, (float)bounds.h * 0.5f);
    uiRelated.cache->setFont({ .id = usedFontId, .height = fontHeight, .kerning = fontKerning });

    auto textPadding = scaleValue(padding.toFloat());
    float width = scaleValue((float)bounds.w);

    nvgStrokeColor(openGl.g, getColour(Skin::kWidgetPrimary1));
    nvgTextAlign(openGl.g, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgTextBox(openGl.g, textPadding.x, textPadding.y, width, text.data(), text.data() + text.size());
    nvgText(openGl.g, textPadding.x, textPadding.y, text.data(), text.data() + text.size());

    return true;
  }

  void PopupDisplay::handleCommandMessage(u64 commandId, utils::whatever)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
    {
      Point<i32> position{};
      COMPLEX_ASSERT((placement & Placement::custom) == 0);
      i32 extraOffsetX = bounds.w / 2;
      i32 extraOffsetY = bounds.h / 2;

      switch (placement & Placement::justifyX)
      {
      case Placement::left:
        position.x -= bounds.w + extraOffsetX;
        break;

      case Placement::right:
        position.x += source->bounds.w + extraOffsetX;
        break;

      default:
      case Placement::justifyX:
        position.x += (source->bounds.w - bounds.w) / 2;
        break;
      }
      
      switch (placement & Placement::justifyY)
      {
      case Placement::top:
        position.y -= bounds.h + extraOffsetY;
        break;

      default:
      case Placement::bottom:
        position.y += source->bounds.h + extraOffsetY;
        break;

      case Placement::justifyY:
        position.y += (source->bounds.h - bounds.h) / 2;
        break;
      }

      bounds.setPosition(getRelativePoint(source, position));

    } break;
    }
  }

  class PopupList final : public Component
  {
  public:
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
    ~PopupList() noexcept override;

    bool render(OpenGlWrapper &openGl) override;

    bool mouseEnter(const MouseEvent &e) override { return mouseMove(e); }
    bool mouseMove(const MouseEvent &e) override;
    bool mouseDrag(const MouseEvent &e) override;
    bool mouseExit(const MouseEvent &) override;
    bool mouseUp(const MouseEvent &e) override;
    bool mouseWheelMove(const MouseEvent &) override;
    void visibilityChanged() override;

    utils::pair<PopupItem *, Rectangle<int>> getSelection(Point<int> position);

    void select(utils::pair<PopupItem *, Rectangle<int>> selectedItem);

    void recalculateSizes();
    void setComponentsBounds();

    void setScrollBarRange(int visibleHeight, int listHeight);
    void paintList(Graphics &g, Rectangle<int> redrawArea);
    int getViewPosition() const;

    PopupItem *items = nullptr;
    PopupItem *childList = nullptr;
    PopupSelector *selector = nullptr;
    utils::vector<utils::pair<PopupItem *, Rectangle<int>>> itemBounds_{};
    Rectangle<int> hoveredBounds_{};
    float hoveredRouding_ = 0.0f;
    float viewPosition_ = 0.0f;
    int listWidth_ = 0;
    int listHeight_ = 0;
    int visibleHeight_ = 0;
    bool isUsed = false;
  };

  /*PopupList::PopupList()
  {
    background_.setTargetComponent(this);
    addOpenGlComponent(&background_);

    rows_.setColor(Colours::white);
    rows_.setPaintFunction([this](Graphics &g, Rectangle<int> redrawArea) { paintList(g, redrawArea); });
    addOpenGlComponent(&rows_);

    hover_.setAdditive(true);
    addOpenGlComponent(&hover_);

    scrollBar_ = utils::up<ScrollBar>::create(true);
    scrollBar_->setAlwaysOnTop(true);
    scrollBar_->addListener(this);
    addSubOpenGlContainer(scrollBar_.get());
  }

  PopupList::~PopupList() = default;

  bool PopupList::render(OpenGlWrapper &openGl) override
  {
    float offset = 2.0f * (float)getViewPosition() / (float)getListHeight();
    rows_.movePosition(0.0f, offset);

    if (!hoveredBounds_.isEmpty())
    {
      int y = hoveredBounds_.y - getViewPosition();
      hover_.setCustomViewportBounds(hoveredBounds_.withY(y));
      hover_.setRounding(hoveredRouding_);
    }

    return true;
  }

  int PopupList::getViewPosition() const
  {
    int max = utils::max(0, getListHeight() - getVisibleHeight());
    return utils::clamp((int)viewPosition_.get(), 0, max);
  }

  void PopupList::visibilityChanged()
  {
    hoveredBounds_ = Rectangle<int>{};
    childList_ = nullptr;
  }

  utils::pair<PopupItem *, Rectangle<int>> PopupList::getSelection(Point<int> position)
  {
    position.y -= (commonInfo_->listRounding - getViewPosition());
    for (auto [item, bounds] : itemBounds_)
    {
      if (!bounds.contains(position))
        continue;

      if (item->type == PopupItem::InlineGroup)
      {
        hoveredRouding_ = (float)commonInfo_->listRounding * 0.5f;
        int x = position.x - bounds.x;
        int elementWidth = bounds.w / item->size();
        int hoveredElement = x / elementWidth;
        return { &item->items[hoveredElement], Rectangle{ bounds.x + hoveredElement * elementWidth,
          bounds.y, elementWidth, bounds.h } };
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
    if (list && (list->type == PopupItem::Delimiter || !list->isActive))
    {
      hoveredBounds_ = Rectangle<int>{};
      return;
    }
    // otherwise, we're changing to a normal item and we move hover
    else
      hoveredBounds_ = bounds + Point{ 0, commonInfo_->listRounding };

    // summoning child dropdown
    if (list && childList_ != list && !list->items.empty() && 
      list->type != PopupItem::InlineGroup)
    {
      childList_ = list;
      listener_->summonNewPopupList(getBounds(), list);
    }
  }

  void PopupList::mouseDrag(const MouseEvent &e)
  {
    auto [list, bounds] = getSelection(e.position.toInt());
    if (list && (list->type == PopupItem::Delimiter || !list->isActive))
    {
      hoveredBounds_ = Rectangle<int>{};
      return;
    }

    if (e.x < 0.0f || e.x > (float)getWidth())
      bounds = {};
    hoveredBounds_ = bounds + Point{ 0, (int)commonInfo_->listRounding };
  }

  void PopupList::mouseExit(const MouseEvent &)
  {
    if (!childList_)
      hoveredBounds_ = Rectangle<int>{};
  }

  void PopupList::mouseUp(const MouseEvent &e)
  {
    if (e.x < 0 || e.x > (float)getWidth())
      return;

    select(getSelection(e.position.toInt()));
  }

  void PopupList::select(utils::pair<PopupItem *, Rectangle<int>> selectedItem)
  {
    auto [item, bounds] = selectedItem;
    
    if (item == nullptr || !item->isActive ||
      item->type == PopupItem::Delimiter ||
      item->type == PopupItem::AutomationList ||
      item->type == PopupItem::InlineGroup)
      return;

    listener_->newSelection(this, item->id);

    // redraw list if upon change
    if (items_->type == PopupItem::AutomationList)
    {
      hoveredBounds_ = Rectangle<int>{};
      rows_.redrawImage(bounds);
    }
  }

  void PopupList::recalculateSizes()
  {
    hoveredBounds_ = Rectangle<int>{};
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

    if (items_->type == PopupItem::AutomationList)
    {
      totalWidth = automationListWidth + crossWidth + scrollBarWidth;
    }
    else
    {
      std::optional<int> inlineGroupSize{};
      for (auto &[subItems, icon, name, hint, type, id, shortcut, isActive] : items_->items)
      {
        int elementWidth = 0;
        if (type == PopupItem::Entry || type == PopupItem::AutomationList)
        {
          elementWidth = commonInfo_->primaryFont.getStringWidth(name);
          elementWidth += hEntryPadding * 2;
          if (!subItems.empty() || type == PopupItem::AutomationList)
            elementWidth += entryToSideArrowMinMargin + sideArrowWidth;
        }
        else if (type == PopupItem::InlineGroup)
        {
          COMPLEX_ASSERT(!inlineGroupSize.has_value() && "Multiple inline groups cannot be handled");
          inlineGroupSize = (int)subItems.size();
          elementWidth = inlineGroupHeight * (int)subItems.size() + vPadding * 2;
        }
        else if (type == PopupItem::Delimiter)
        {
          elementWidth = commonInfo_->primaryFont.getStringWidth(name);
          elementWidth += hEntryPadding;
          elementWidth += commonInfo_->primaryFont.getStringWidth(hint);
          elementWidth += hEntryPadding * 2;
        }
        totalWidth = utils::max(totalWidth, elementWidth);
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
    if (items_->type == PopupItem::AutomationList)
    {
      auto lineWidth = totalWidth - crossWidth - scrollBarWidth;
      for (usize i = 0; i < items_->items.size(); i += 2)
      {
        int elementHeight = 0;
        int lines = getLinesForText(commonInfo_->primaryFont, items_->items[i].name, lineWidth - hEntryPadding);
        elementHeight += utils::max(lines, 1) * primaryTextLineHeight;
        elementHeight += 2 * vPadding;

        itemBounds_.emplace_back(&items_->items[i], Rectangle{ 0, totalHeight, lineWidth, elementHeight });
        itemBounds_.emplace_back(&items_->items[i + 1], Rectangle{ lineWidth, totalHeight +
           utils::centerAxis(crossWidth, elementHeight), crossWidth, crossWidth });

        totalHeight += elementHeight;
      }
    }
    else
    {
      for (int i = 0; i < items_->size(); ++i)
      {
        int elementHeight = 0;
        if (items_->items[i].type == PopupItem::Entry || items_->items[i].type == PopupItem::AutomationList)
        {
          elementHeight += primaryTextLineHeight;
          elementHeight += 2 * vPadding;

          if (!items_->items[i].hint.empty())
          {
            elementHeight += entryToHintMargin;
            int lines = getLinesForText(commonInfo_->secondaryFont, items_->items[i].hint, totalWidth - 2 * hEntryPadding);
            elementHeight += lines * secondaryTextLineHeight;
          }

          itemBounds_.emplace_back(&items_->items[i], Rectangle{ 0, totalHeight, totalWidth, elementHeight });
        }
        else if (items_->items[i].type == PopupItem::InlineGroup)
        {
          elementHeight += vPadding + inlineGroupHeight;
          if (items_->size() > i + 1)
            elementHeight += vPadding;

          itemBounds_.emplace_back(&items_->items[i], 
            Rectangle{ vPadding, totalHeight + vPadding, totalWidth - 2 * vPadding, inlineGroupHeight });
        }
        else if (items_->items[i].type == PopupItem::Delimiter)
        {
          elementHeight += delimiterHeight;
          itemBounds_.emplace_back(&items_->items[i], Rectangle{ 0, totalHeight, totalWidth, delimiterHeight });
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

    rows_.setBounds(0, listRounding, getWidth(), utils::max(listHeight, visibleHeight));
    rows_.setCustomScissorBounds({ width, visibleHeight });
    rows_.redrawImage();
  }

  void PopupList::mouseWheelMove(const MouseEvent &e)
  {
    viewPosition_ = viewPosition_.get() - e.wheelDeltaY * kScrollSensitivity;
    int visibleHeight = getVisibleHeight();
    int listHeight = getListHeight();
    viewPosition_ = utils::clamp(viewPosition_.get(), 0.0f, 
      (float)(utils::max(listHeight, visibleHeight) - visibleHeight));
    setScrollBarRange(visibleHeight, listHeight);
  }

  void PopupList::setScrollBarRange(int visibleHeight, int listHeight)
  {
    static constexpr double kScrollStepRatio = 0.025f;

    scrollBar_->setRangeLimits(0.0f, utils::max(listHeight, getHeight()));
    scrollBar_->setCurrentRange(getViewPosition(), visibleHeight, dontSendNotification);
    scrollBar_->setSingleStepSize(scrollBar_->getHeight() * kScrollStepRatio);
    scrollBar_->cancelPendingUpdate();
  }

  void PopupList::paintList(Graphics &g, Rectangle<int> redrawArea)
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
    case PopupItem::Entry:
      for (auto [item, bound] : itemBounds_)
      {
        switch (item->type)
        {
        case PopupItem::Entry:
        case PopupItem::AutomationList:
          g.setFont(commonInfo_->primaryFont);
          g.setColour(primaryTextColour);
          g.drawText(item->name, hEntryPadding, bound.getY() + vPadding,
            entryTextWidth, primaryTextLineHeight, Justification::centredLeft, true);

          if (item->size() || item->type == PopupItem::AutomationList)
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
        case PopupItem::InlineGroup:
        {
          float containerWidth = (float)bound.w / (float)item->size();
          float strokeWidth = scaleValue(1.0f);
          auto iconBounds = Rectangle{ (float)bound.x + utils::centerAxis(iconSize, containerWidth),
            (float)bound.y + utils::centerAxis(iconSize, (float)bound.h), iconSize, iconSize };
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
        case PopupItem::Delimiter:
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
    case PopupItem::AutomationList:
      {
        int extraOffset = (int)::roundf(((float)primaryTextLineHeight - commonInfo_->primaryFont.getHeight()) * 0.5f);
        auto drawEntry = [&](const String &name, const Framework::ParameterLink *mapping, 
          Rectangle<int> itemBounds, Rectangle<int> crossBounds)
        {
          g.setFont(commonInfo_->primaryFont);
          g.setColour((mapping) ? Colour{ mapping->parameter->getThemeColour() } : primaryTextColour);
          g.drawMultiLineText(name, hEntryPadding, itemBounds.y + vPadding + extraOffset +
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
          auto iter = utils::find_if(itemBounds_, 
            [&](const auto &pair) { return pair.second == redrawArea; });

          COMPLEX_ASSERT(iter != itemBounds_.end());

          usize index = usize(iter - itemBounds_.begin()) / 2;
          itemBounds_[index * 2].first->isActive = !itemBounds_[index * 2].first->isActive;
          itemBounds_[index * 2 + 1].first->isActive = !itemBounds_[index * 2 + 1].first->isActive;

          g.setColour(Colours::transparentBlack);
          g.fillRect({ 0, itemBounds_[index * 2].second.y, g.getClipBounds().getWidth(), 
            itemBounds_[index * 2].second.h }, true);

          drawEntry(bridges[index]->getName(), bridges[index]->getParameterLink(),
            itemBounds_[index * 2].second, itemBounds_[index * 2 + 1].second);

          return;
        }

        for (usize i = 0; i < bridges.size(); ++i)
          drawEntry(bridges[i]->getName(), bridges[i]->getParameterLink(), 
            itemBounds_[i * 2].second, itemBounds_[i * 2 + 1].second);
      }
      break;
    case PopupItem::Delimiter:
    case PopupItem::InlineGroup:
    default:
      COMPLEX_ASSERT_FALSE("Invalid popup menu top level type");
      break;
    }
  }*/

  PopupSelector::PopupSelector()
  {
    arena = utils::bumpArena::createNested(getUIArena(), COMPLEX_KB(512));
    placement = Placement::custom;
  }

  bool PopupSelector::render(OpenGlWrapper &openGl)
  {
    // unclip children
    openGl.parentStack.back().isClipping = false;
    return true;
  }

  bool PopupSelector::keyPressed(const KeyPress &key)
  {
    // get currently focused sublist
    PopupList *selectedList = nullptr;
    for (auto &list : lists)
    {
      if (!list->isUsed)
        break;
      selectedList = list;
    }

    COMPLEX_ASSERT(selectedList);

    for (usize i = 0; i < selectedList->items->childComponents.size(); ++i)
    {
      // this is safe because only popup items can be childen
      auto *item = utils::as<PopupItem>(selectedList->items->childComponents[i]);
      if (key.keyCode != item->shortcutKeyCode)
        continue;
      
      newSelection(selectedList, item);
      return true;
    }

    return false;
  }

  void PopupSelector::newSelection(PopupList *list, PopupItem *entry)
  {
    if (entry->id >= 0)
    {
      if (callback)
      {
        callback(this, entry);
        if (!entry->closesPopup)
          return;
      }

      componentFlags.isVisible = false;
    }
    else if (cancel)
      cancel(this);

    resetState();
  }

  void PopupSelector::summonNewPopupList(Rectangle<int> sourceBounds, PopupItem *items)
  {
    PopupList *newList = nullptr;
    for (auto *list : lists)
      if (!list->isUsed)
        newList = list;

    if (!newList)
      newList = anew(arena, PopupList, {});

    // we may have already used it with these items
    bool isCached = newList->getItems() == items && 
      items->type != PopupItem::AutomationList;
    if (!isCached)
    {
      newList->setItems(items);
      newList->recalculateSizes();
    }

    newList->setIsUsed(true);

    int width = newList->getListWidth();
    int height = newList->getListHeight() + 2 * commonInfo_.listRounding;
    auto [_, __, windowWidth, windowHeight] = getLocalBounds();

    int y = sourceBounds.y;
    if (y + height >= getHeight())
    {
      y = utils::max(sourceBounds.getBottom() - height, 0);
      height = utils::min(height, windowHeight);
    }

    int x = sourceBounds.getRight();
    if ((lastPlacement_ == Placement::left || x + width >= windowWidth) 
      && sourceBounds.x - width > 0)
    {
      x = sourceBounds.x - width;
      lastPlacement_ = Placement::left;
    }
    else
    {
      x = utils::min(x + width, windowWidth) - width;
      lastPlacement_ = Placement::right;
    }

    newList->setBounds(x, y, width, height);
    if (!isCached)
      newList->setComponentsBounds();
    newList->setVisible(true);
  }

  void PopupSelector::closeSubList(PopupItem *items)
  {
    usize index = 0;
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

  bool PopupSelector::handleFocus(bool hasFocus, FocusChange)
  {
    if (!hasFocus)
    {
      componentFlags.isVisible = false;
      if (cancel)
        cancel(this);

      resetState();
    }

    return true;
  }

  void PopupSelector::handleCommandMessage(u64 commandId, utils::whatever)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:
    {


      break;
    }
    }
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
    auto [_, __, windowWidth, windowHeight] = getLocalBounds();

    // do we have space to the right
    if (x + width > windowWidth)
    {
      // do we have enough space to the left
      if (x > width)
        x -= width;
      else
      {
        // neither have enough space, offset and shrink if necessary
        width = utils::min(width, windowWidth);
        x = windowWidth - width;
      }
    }
    if (y + height > windowHeight)
    {
      if (y > height)
        y -= height;
      else
      {
        height = utils::min(windowHeight, height);
        y = windowHeight - height;
      }
    }

    lastUsedList->setBounds(x, y, width, height);
    lastUsedList->setComponentsBounds();
    lastUsedList->setVisible(true);
  }

  void PopupSelector::resetState()
  {
    callback = {};
    cancel = {};
    for (auto &list : lists)
    {
      list->componentFlags.isVisible = false;
      list->isUsed = false;
    }
    lastPlacement = Placement::right;
    utils::bumpArena::clear(arena);
    skinOverride = Skin::kNone;
    componentFlags.isVisible = false;
  }

  void PopupSelector::positionList(Rectangle<int> sourceBounds, Placement::Enum placement)
  {
    PopupList *lastUsedList = nullptr;
    for (auto &list : lists)
    {
      if (list->isUsed)
        lastUsedList = list.get();
      else
        break;
    }

    COMPLEX_ASSERT(lastUsedList);

    auto position = sourceBounds.getPosition();
    int width = lastUsedList->getListWidth();
    int height = lastUsedList->getListHeight() + 2 * commonInfo_.listRounding;
    auto [_, __, windowWidth, windowHeight] = getLocalBounds();
    int popupToElement = scaleValueRoundInt(kPopupToElement);

    auto [finalX, finalY] = position;

    auto checkHorizontal = [&](int offset = 0)
    {
      if (finalX + width <= windowWidth)
        return;

      // popup cannot be fit horizontally, find the side with the most width
      if (position.x + sourceBounds.w >= windowWidth - position.x)
      {
        width = utils::min(width, position.x + sourceBounds.w - offset);
        finalX = position.x + sourceBounds.w - offset - width;
      }
      else
      {
        finalX = position.x + offset;
        width = utils::min(width, windowWidth - finalX);
      }
    };

    auto checkVertical = [&](int offset = 0)
    {
      if (finalY + height <= windowHeight)
        return;

      // popup cannot be fit vertically, find the side with the most height
      if (position.y + sourceBounds.h >= windowHeight - position.y)
      {
        height = utils::min(height, position.y + sourceBounds.h - offset);
        finalY = position.y + sourceBounds.h - offset - height;
      }
      else
      {
        finalY = position.y + offset;
        height = utils::min(height, windowHeight - finalY);
      }
    };

    if (placement == Placement::below || placement == Placement::above)
    {
      checkHorizontal();
      finalY = (placement == Placement::below) ? finalY + sourceBounds.h + popupToElement :
        finalY - height - popupToElement;
      checkVertical(sourceBounds.h + popupToElement);
    }
    else if (placement == Placement::left || placement == Placement::right)
    {
      checkVertical();
      finalX = (placement == Placement::left) ? finalX - width - popupToElement :
        finalX + sourceBounds.w + popupToElement;
      checkHorizontal(sourceBounds.w + popupToElement);
    }
    else
    {
      COMPLEX_ASSERT_FALSE("You need to choose a one of the placements for the popup");
      checkHorizontal();
      finalY = finalY + sourceBounds.h;
      checkVertical(sourceBounds.h);
    }

    lastUsedList->setBounds(finalX, finalY, width, height);
    lastUsedList->setComponentsBounds();
    lastUsedList->setVisible(true);
  }

  
}
