
// Created: 2023-02-03 18:42:55

#include "EffectsLaneSection.hpp"

#include <stddef.h>

#include "Generation/Effects.hpp"
#include "Plugin/Complex.hpp"
#include "Control.hpp"
#include "MainInterface.hpp"
#include "EffectModuleSection.hpp"

static bool
summonModulePopup(PopupSelector *selector, Component *summoner,
  EffectsLaneSection *laneSection, Placement placement, Point<i32> offset, usize insertionIndex)
{
  if (laneSection->isDropdownOpen)
  {
    selector->resetState();
    laneSection->isDropdownOpen = false;
    return false;
  }

  laneSection->isDropdownOpen = true;
  selector->resetState();

  // find the module options in the state
  IndexedData *indexedData{};
  ProcessorMetadata::visit(laneSection->effectsLane->state->pluginStructure.metadata,
    [&indexedData](ProcessorMetadata &metadata)
    {
      if (metadata.id != Processors::EffectModule)
        return false;

      ParameterMetadata *parameter = metadata.parameters;
      for (usize i = 0; i < metadata.parameterCount; (++i), (parameter = parameter->next))
        if (parameter->details.id == EffectModule::ModuleType)
          break;

      indexedData = parameter->details.options;

      return true;
    });

  PopupList *list = anew(selector->pool, PopupList, { selector });
  list->parentSelector = selector;

  iterateOverIndexedData(indexedData,
    [list](IndexedData &option)
    {
      if (option.childrenCount || option.canBeChosen())
        list->addChildComponent(OptionPopupItem::createOption(list, option));

      return false;
    });

  selector->list = list;
  selector->skinOverride = summoner->getSkinOverride();
  selector->callback = [laneSection, insertionIndex](PopupSelector *, PopupItem *selectedItem)
  {
    laneSection->isDropdownOpen = false;
    auto *option = (IndexedData *)selectedItem->extraData;
    COMPLEX_ASSERT(option->processorMetadata);

    auto *state = laneSection->effectsLane->state;
    auto *effectModule = (EffectModule *)state->createProcessor(
      Processors::EffectModule, nullptr, option);

    auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
    state->plugin->undoManager.perform(anew(transactionArena, AddProcessorUpdate, { effectModule,
      laneSection->effectsLane->stateId, insertionIndex }));
  };
  selector->cancel = [laneSection](PopupSelector *) { laneSection->isDropdownOpen = false; };
  selector->summon(summoner, placement, offset);

  summoner->componentFlags.isClicked = false;

  return true;
}

static bool
laneMessageHandler(Component *c, u64 commandId, void *extraData)
{
  auto *self = (EffectsLaneSection *)c;
  switch (commandId)
  {
  case CommandMessages::HandleAutoscroll:
  {
    auto &data = *(CommandMessages::Autoscroll *)extraData;
    if (data.handleY)
    {
      static constexpr float kAutoscrollMultiplier = 10.0f;

      data.handleY = false;

      Point<i32> position = data.position;
      i32 offset = utils::max(0, EffectsLaneSection::kAutoScrollRegion - utils::min(position.y, self->bounds.h - position.y));
      offset = utils::min(offset, (i32)utils::int_max<i8>);
      auto yOffset = (i8)((position.y < self->bounds.h - position.y) ? offset : -offset);

      offsetScroll(&self->moduleHolder, 0.0f, yOffset * getUiRelated()->deltaTime * kAutoscrollMultiplier, false);
    }

    //COMPLEX_DEBUG_LOG("Position: %d, %d\n", data.position.x, data.position.y);
    //COMPLEX_DEBUG_LOG("Autoscroll: %d\n", (int)self->moduleHolder.autoScrollIncrements.y);

    return true;
  }
  case CommandMessages::HandleProcessorInsertion:
  {
    auto *metadata = (CommandMessages::ProcessorInsertion *)extraData;

    if (!CommandMessages::handleProcessorInsertion(self->effectsLane,
      &self->moduleHolder, metadata, Placement::top))
      return false;

    auto section = (EffectModuleSection *)metadata->processor->component;
    section->laneSection = self;
    section->componentFlags.animateMovement = true;

    if (metadata->useIndex)
      section->effectHolder.header.draggableBox.surfaceToLiftTo =
        &((SoundEngineSection *)self->effectsLane->parent->component)->effectsSection;

    (metadata->placeholder ? metadata->placeholder : section)->margin = { 0, 0, 0, kVModuleToModuleMargin };

    return true;
  }
  }

  return false;
}

void EffectsLaneSection::reinitialise()
{
  static constexpr auto kHeaderLeftPadding = 12;
  static constexpr auto kHeaderRightPadding = 4;
  static constexpr auto kFooterPadding = 8;

  moduleHolder.removeAllChildComponents();
  header.removeAllChildComponents();
  footer.removeAllChildComponents();
  removeAllChildComponents();
  removeCommandMessageHandler(laneHandler);

  componentFlags.vertical = true;
  componentFlags.animateMovement = true;
  componentFlags.halfScissor = true;
  skinOverride = Skin::kEffectsLane;
  sizingFlags = Component::GrowableY;
  desiredSize = { kEffectsLaneWidth, 0, kEffectsLaneWidth, 0 };

  addCommandMessageHandler(laneHandler);
  laneHandler.object = laneMessageHandler;

  if (!pool)
    pool = utils::Pool::createNested(utils::Pool::fromAllocation(this), COMPLEX_KB(1));

  addChildComponent(&header);
  header.desiredSize = { 0, kEffectsLaneTopBarHeight, 0, kEffectsLaneTopBarHeight };
  header.sizingFlags = Component::GrowableX;
  header.placement = Placement::top;
  header.padding = { kHeaderLeftPadding, 0, kHeaderRightPadding, 0 };

  header.addChildComponent(&laneTitle);
  laneTitle.placement = Placement::left;
  laneTitle.text = { pool, effectsLane->name };
  laneTitle.textColour = Skin::kWidgetAccent1;
  header.addChildComponent(&inputSelector);
  inputSelector.placement = Placement::right;
  inputSelector.prefix = { pool, "From: " };
  inputSelector.pool = pool;
  inputSelector.text.font = FontId::InterType;
  inputSelector.changeLinkedParameter(*effectsLane->getParameter(EffectsLane::Input));
  header.addChildComponent(&laneActivator);
  laneActivator.placement = Placement::right;
  laneActivator.margin = { 4, 0, 0, 0 };
  laneActivator.desiredSize = { kDefaultActivatorSize, kDefaultActivatorSize, kDefaultActivatorSize, kDefaultActivatorSize };
  laneActivator.changeLinkedParameter(*effectsLane->getParameter(EffectsLane::LaneEnabled));


  addChildComponent(&moduleHolder);
  moduleHolder.placement = Placement::top;
  moduleHolder.margin = { kEffectsLaneOutlineThickness, 0, kEffectsLaneOutlineThickness, 0 };
  moduleHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY | Component::ScrollableWithBarY);
  moduleHolder.componentFlags.vertical = true;
  moduleHolder.componentFlags.clickable = true;
  moduleHolder.componentFlags.halfScissor = true;
  moduleHolder.padding = { kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin };

  addChildComponent(&footer);
  footer.desiredSize = { 0, kEffectsLaneBottomBarHeight, 0, kEffectsLaneBottomBarHeight };
  footer.padding = { kFooterPadding, 0, kFooterPadding, 0 };
  footer.sizingFlags = Component::GrowableX;
  footer.placement = Placement::bottom;

  footer.addChildComponent(&gainMatchingButton);
  gainMatchingButton.placement = Placement::left;
  gainMatchingButton.pool = pool;
  gainMatchingButton.changeLinkedParameter(*effectsLane->getParameter(EffectsLane::GainMatching));
  footer.addChildComponent(&gainMatchingButtonLabel);
  gainMatchingButtonLabel.placement = Placement::left;
  gainMatchingButtonLabel.control = &gainMatchingButton;
  gainMatchingButtonLabel.margin = { Control::kLabelMargin, 0, 0, 0 };
  footer.addChildComponent(&outputSelector);
  outputSelector.placement = Placement::right;
  outputSelector.popupPlacement = Placement::top;
  outputSelector.prefix = { pool, "To: " };
  outputSelector.pool = pool;
  outputSelector.text.font = FontId::InterType;
  outputSelector.changeLinkedParameter(*effectsLane->getParameter(EffectsLane::Output));

  miniView.processor = effectsLane;
  miniView.draggedComponent = &miniView;

  // add existing module components to the holder
  usize i = 0;
  for (auto *childModule = effectsLane->children;
    childModule && i < effectsLane->childrenCount; ++i)
    if (childModule->component)
      moduleHolder.addChildComponent(childModule->component);
}

bool
EffectsLaneSection::ModuleHolder::mouseEnter(const MouseEvent &)
{
  // this algorithm will ALMOST work in mouseMove
  // unfortunately if we add a module and the cursor happens to be where the next add module highlight appears
  // there is a pseudo race for where the next mouse hover refresh will happen:
  //  1. if it happens from the rendering procedure,
  //      the module's bounds will have been set and everything works or,
  //  2. the windowing system might send a bogus mouse event before that,
  //      so the module will have bounds of { 0, 0, 0, 0 },
  //      meaning that the next highlight bounds will be where they already were
  //      and if the user doesn't move the cursor, it will not recheck that

  auto *uiRelated = getUiRelated();

  COMPLEX_INVARIANT_ASSERT(
    (
      return uiRelated->renderer->callbacks.find(this) ==
        uiRelated->renderer->callbacks.data.end();
    )
  );

  uiRelated->renderer->callbacks.add(this, [](Component *c)
    {
      auto self = (EffectsLaneSection::ModuleHolder *)c;
      auto laneSection = (EffectsLaneSection *)self->parent;
      auto scaledPadding = scaleValueRoundInt(self->padding.toInt());
      auto localBounds = self->getLocalBounds().withTrimLeft(scaledPadding.x).withTrimRight(scaledPadding.w);

      if (auto *lastChild = Processor::getChild(
        laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
      {
        auto newBottom = lastChild->component->bounds.getBottom() +
          scaleValueRoundInt((float)lastChild->component->margin.h + kEffectModuleMinHeight);
        localBounds.h = utils::min(newBottom, localBounds.getBottom()) - localBounds.y;
      }
      else
      {
        localBounds.y = scaledPadding.y;
        localBounds.h = scaleValueRoundInt(kEffectModuleMinHeight);
      }

      if (localBounds.contains(Point{ self->lastMouseMove.x, self->lastMouseMove.y }))
      {
        if (!self->hasEnteredHover)
        {
          self->enterHoverTime = getUiRelated()->steadyTime;
          self->hasEnteredHover = true;
        }

        // get the module below cursor
        auto *laneSeciton = (EffectsLaneSection *)self->parent;
        self->hoveredBeforeModuleIndex = 0;
        self->hoveredBeforeModule = laneSeciton->effectsLane->children;
        for (; self->hoveredBeforeModule && self->hoveredBeforeModuleIndex < laneSeciton->effectsLane->childrenCount;
          (++self->hoveredBeforeModuleIndex), (self->hoveredBeforeModule = self->hoveredBeforeModule->next))
        {
          if (self->hoveredBeforeModule->component->bounds.y > self->lastMouseMove.y)
            break;
        }
      }
      else if (self->hasEnteredHover)
      {
        getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::Normal);
        self->hasEnteredHover = false;
      }

      return false;
    });

  return true;
}

bool
EffectsLaneSection::ModuleHolder::mouseExit(const MouseEvent &)
{
  getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::Normal);
  hasEnteredHover = false;

  getUiRelated()->renderer->callbacks.erase(this);

  COMPLEX_INVARIANT_ASSERT(
    (
      return getUiRelated()->renderer->callbacks.find(this) ==
        getUiRelated()->renderer->callbacks.data.end();
    )
  );

  return true;
}

bool
EffectsLaneSection::ModuleHolder::mouseMove(const MouseEvent &e)
{
  lastMouseMove = e;
  return true;
}

bool
EffectsLaneSection::ModuleHolder::mouseDown(const MouseEvent &e)
{
  if (Component::mouseDown(e))
    return true;

  auto laneSection = (EffectsLaneSection *)parent;
  if (laneSection->isDropdownOpen)
  {
    getPopupSelector()->resetState();
    return true;
  }

  double time = getUiRelated()->steadyTime;
  if (time - enterHoverTime < kTimeout)
    return true;

  if (e.mods.test(ModifierKeys::altModifier) || e.mods.test(ModifierKeys::popupMenuClickModifier))
    return true;

  summonModulePopup(getPopupSelector(), this, laneSection,
    Placement::custom, { e.x, e.y }, hoveredBeforeModuleIndex);

  return true;
}

static void renderInsertHint(EffectsLaneSection::ModuleHolder *holder, Graphics &g)
{
  static constexpr auto kHoverIncrement = 0.05f;

  auto laneSection = (EffectsLaneSection *)holder->parent;
  bool isEmptyAndHovered = holder->componentFlags.isHovered && laneSection->effectsLane->childrenCount == 0;
  tickAnimation(holder->animationValues,
    { { isEmptyAndHovered || laneSection->isDropdownOpen ||
      (holder->hasEnteredHover && getUiRelated()->steadyTime - holder->enterHoverTime >= EffectsLaneSection::kTimeout) } },
    { { kHoverIncrement } });

  if (isEmptyAndHovered || laneSection->isDropdownOpen ||
    (holder->hasEnteredHover && getUiRelated()->steadyTime - holder->enterHoverTime >= EffectsLaneSection::kTimeout))
  {
    if (holder->hasEnteredHover)
      getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::PointingHand);

    auto scaledPadding = scaleValueRoundInt(holder->padding.toInt());
    auto drawBounds = holder->getLocalBounds().withTrimLeft(scaledPadding.x).withTrimRight(scaledPadding.w);

    // the childrenCount check is necessary because we don't know if have outdated info
    // because callback hadn't run since we weren't hovered over
    if (holder->hoveredBeforeModule && laneSection->effectsLane->childrenCount)
    {
      if (holder->hoveredBeforeModule->previous->next)
        drawBounds.y = holder->hoveredBeforeModule->previous->component->bounds.getBottom();
      drawBounds.h = holder->hoveredBeforeModule->component->bounds.y - drawBounds.y;

      drawBounds = drawBounds.withY(drawBounds.getCentreY()).withHeight(0);
    }
    else
    {
      drawBounds = drawBounds.withTrimTop(scaledPadding.y).withTrimBottom(scaledPadding.h);
      if (auto *lastChild = Processor::getChild(
        laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
      {
        auto newY = lastChild->component->bounds.getBottom() + scaleValueRoundInt((float)holder->padding.h);
        drawBounds = drawBounds.withTop(newY);
      }

      drawBounds.h = utils::min(drawBounds.h, scaleValueRoundInt(kEffectModuleMinHeight));
    }

    auto colour = getColour(Skin::kBorder, holder).withMultipliedAlpha(holder->animationValues[0]);
    strokeRect(g, drawBounds.toFloat(), scaleValue(1.0f), colour, scaleValue(EffectsLaneSection::kBorderRounding));

    static constexpr int kPlusSize = 16;
    auto plusSize = scaleValueRound(kPlusSize);

    if (drawBounds.h >= 2 * plusSize)
    {
      auto plusBounds = Rectangle{ (float)drawBounds.getCentreX(),
        (float)drawBounds.getCentreY(), 0.0f, 0.0f }.withExpand(plusSize * 0.5f);

      strokePlus(g, plusBounds, scaleValue(2.0f), colour);
    }
  }
}

bool
EffectsLaneSection::ModuleHolder::render(Graphics &g)
{
  fillRect(g, getLocalBounds().toFloat(),
    getColour(Skin::kBackground, this), scaleValue(kInsideRouding));

  renderScrollbars(g, 0.2f);

  renderInsertHint(this, g);

  return true;
}

bool
EffectsLaneSection::render(Graphics &g)
{
  if (renderStage == All || renderStage == Background)
  {
    fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kBody, this), scaleValue(kInsideRouding));
    strokeRect(g, getLocalBounds().toFloat(), scaleValue(kEffectsLaneOutlineThickness),
      Colour{ 45, 45, 45 }, scaleValue(kInsideRouding));

    renderChild(g, &header);
    renderChild(g, &footer);

    renderChild(g, &moduleHolder, [](Graphics &g, Component *c) { c->render(g); });
  }

  if (renderStage == All || renderStage == Modules)
  {
    renderChild(g, &moduleHolder, [](Graphics &g, Component *c) { c->renderChildren(g); });
  }

  if (renderStage == All || renderStage == Highlight)
  {
    if (!laneActivator.isOn())
      fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kOverlayScreen, this));
  }

  return false;
}

EffectsLaneSection::LaneMiniView::LaneMiniView()
{
  sizingFlags = Component::GrowableX;
  padding = { 4, 0, 4, 0 };
  desiredSize = { kMinWidth, kMinHeight, utils::int_max<i32>, kMinHeight };
  componentFlags.animateMovement = true;
}

bool
EffectsLaneSection::LaneMiniView::mouseDown(const MouseEvent &e)
{
  if (e.mods.test(ModifierKeys::middleButtonModifier))
  {
    // LaneSelector -> EffectsSection
    auto *effectsSection = (EffectsSection *)parent->parent;
    effectsSection->removeLane((EffectsLaneSection *)processor->component);

    return true;
  }

  return true;
}

bool
EffectsLaneSection::LaneMiniView::mouseDrag(const MouseEvent &e)
{
  auto offset = e.getOffsetFromDragStart();
  auto constraint = scaleValueRoundInt(15);
  if (offset.x < constraint && offset.y < constraint && !isDragging)
  {
    auto event = e;
    event.x = event.mouseDownPosition.x;
    event.y = event.mouseDownPosition.y;

    (void)prepareToMove(event);
  }

  if (isDragging)
  {
    move(e);
    return true;
  }

  return true;
}

bool
EffectsLaneSection::LaneMiniView::mouseUp(const MouseEvent &e)
{
  auto offset = e.getOffsetFromDragStart();
  if (!isDragging)
  {
    // LaneSelector -> EffectsSection
    auto *effectsSection = (EffectsSection::LaneSelector *)parent;
    effectsSection->selectViewedLane(e.getEventRelativeTo(effectsSection));
    return true;
  }

  return DraggableComponent::mouseUp(e);
}

bool
EffectsLaneSection::LaneMiniView::render(Graphics &g)
{
  auto localBounds = getLocalBounds().toFloat();
  fillRect(g, localBounds, getColour(Skin::kBody, this), EffectsSection::LaneSelector::kLaneMiniViewRounding);

  auto textBounds = localBounds.withTrim(scaleValueRound(padding.toFloat()))
    .withY(localBounds.getCentreY()).withHeight(0.0f)
    .withExpand(0.0f, scaleValue(kPrimaryTextLineHeight / 2));

  renderText(((EffectsLaneSection *)processor->component)->laneTitle.text,
    FontId::DDinType, textBounds, g, getColour(Skin::kHeadingText, this));

  // remove the miniview as well if the lane is removed
  auto *laneSection = getParentStruct(EffectsLaneSection, miniView, this);
  if (!laneSection->parent)
    parent->removeChildComponent(this);

  return true;
}

Component *
createUIEffectsLane(Processor *processor)
{
  auto *effectsLaneSection = anew(getUIPool(), EffectsLaneSection, {});
  effectsLaneSection->effectsLane = (EffectsLane *)processor;
  effectsLaneSection->reinitialise();
  return effectsLaneSection;
}
