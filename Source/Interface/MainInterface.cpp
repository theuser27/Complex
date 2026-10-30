
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Third Party/pugl/pugl.h"
#include "Third Party/nanovg/nanosvg.h"

#include "Data/BinaryData.hpp"
#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/Effects.hpp"
#include "EffectsLaneSection.hpp"


bool
CommandMessages::handleProcessorInsertion(Processor *parentProcessor,
  Component *parentComponent, ProcessorInsertion *metadata, Placement placement,
  Component *(*getRelativeComponent)(Processor *))
{
  COMPLEX_ASSERT(parentProcessor);

  auto *processor = metadata->processor;

  // checking if child can be accepted by this processor
  {
    auto *acceptedChildren = parentProcessor->metadata->children;
    for (; acceptedChildren && processor->metadata->id != acceptedChildren->id;
      acceptedChildren = acceptedChildren->next) { }
    if (!acceptedChildren)
      return false;
  }

  Processor *child{};
  metadata->index = utils::min(metadata->index, parentProcessor->childrenCount);
  if (!metadata->useIndex)
  {
    // try to find the correct index for a given position

    auto point = parentComponent->getRelativePoint(getRelativeComponent(processor), metadata->position);

    auto Point<i32>:: *primary = (parentComponent->componentFlags.vertical) ?
      &Point<i32>::y : &Point<i32>::x;

    metadata->index = 0;
    for (child = parentProcessor->children; child;
      (++metadata->index), child = Processor::getChild(child, 1))
    {
      // skip if we encounter the processor we're moving
      if (child == metadata->processor)
      {
        // if we count ourselves as well we always move down 1 more than we have to
        --metadata->index;
        continue;
      }

      auto component = getRelativeComponent(child);
      auto position = Point{ (metadata->isMovingUpX) ? component->bounds.w : 0,
        (metadata->isMovingUpY) ? component->bounds.h : 0 };
      position = parentComponent->getRelativePoint(component, position);
      if (component && point.*primary < position.*primary)
        break;
    }
  }

  metadata->oldIndex = (metadata->processor->parent) ? (u32)metadata->processor->getIndex() : (u32)-1;
  // if the processor already exists at the correct index, we skip
  if (metadata->processor->parent != parentProcessor || metadata->oldIndex != metadata->index)
  {
    auto g = parentProcessor->state->plugin->acquireProcessingLock();
    if (processor->parent)
      processor->parent->removeChildProcessor(*processor);

    (void)parentProcessor->addChildProcessor(*processor, metadata->index);
  }

  if (metadata->useIndex)
    child = Processor::getChild(parentProcessor->children, metadata->index);

  Component *newComponent = getRelativeComponent(processor);
  auto *substituteInsert = metadata->placeholder;
  if (substituteInsert)
  {
    if (substituteInsert->parent)
      substituteInsert->parent->removeChildComponent(substituteInsert);
    newComponent = substituteInsert;
  }
  else if (newComponent->parent)
  {
    if (substituteInsert && substituteInsert->parent)
      substituteInsert->parent->removeChildComponent(substituteInsert);
    
    // FIXME:
    // edge-case
    // if the component is already in the correct place, 
    // re-adding it can succeed a component that is still getting deleted (i.e. delete 2nd module, delete 1st, undo)
    newComponent->parent->removeChildComponent(newComponent);
  }

  COMPLEX_ASSERT(!child || child->component);
  // if the processor was already inserted before we started
  // we might try to insert before itself,
  // therefore we need to the next element as insertBefore
  auto childComponent = child ? getRelativeComponent(child) : nullptr;
  if (child && newComponent == childComponent)
  {
    childComponent = child->next ? getRelativeComponent(child->next) : nullptr;
  }

  newComponent->placement = placement;
  parentComponent->addChildComponent(newComponent, childComponent);

  return true;
}

void CommandMessages::tryProcessorInsertion(Component *parentComponent, ProcessorInsertion info)
{
  bool success = preOrderTreeTraversal(parentComponent, [&info](Component *c)
  {
    auto copy = info;
    if (c->handleCommandMessage(CommandMessages::HandleProcessorInsertion, &copy))
    {
      info = copy;
      return true;
    }
    return false;
  }, false, true);

  // if it fails to insert, just add it to the immediate parent
  if (!success)
    parentComponent->addChildComponent(info.processor->component);
}

static void getLaneName(EffectsLaneSection *effectsLaneSection,
  EffectsSection *effectsSection)
{
  auto *effectsLane = effectsLaneSection->effectsLane;
  effectsLane->name = { effectsLane->pool, utils::string::minimumCapacity };
  usize i = effectsSection->laneNameOrdinal;
  do
  {
    char letter[] = { (char)('A' + (i % ('Z' - 'A' + 1))), '\0' };
    effectsLane->name.prepend(letter);
    i /= ('Z' - 'A' + 1);
  } while (i);

  effectsLaneSection->laneTitle.text = { effectsLaneSection->pool, effectsLane->name };
  ++effectsSection->laneNameOrdinal;
}

bool
EffectsSection::LaneSelector::AddMoreLanesButton::mouseDown(const MouseEvent &)
{
  // LaneSelector -> EffectsSection -> SoundEngineSection
  auto *effectsSection = (EffectsSection *)parent->parent;
  auto *soundEngine = ((SoundEngineSection *)effectsSection->parent)->soundEngine;
  auto *state = soundEngine->state;
  auto *effectsLane = (EffectsLane *)state->createProcessor(Processors::EffectsLane);

  auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
  state->plugin->undoManager.perform(anew(transactionArena, AddProcessorUpdate, { effectsLane,
    soundEngine->stateId, soundEngine->childrenCount }));

  return true;
}

bool
EffectsSection::LaneSelector::AddMoreLanesButton::render(Graphics &g)
{
  auto localBounds = getLocalBounds().toFloat();

  strokeRect(g, localBounds, scaleValue(1.0f),
    getColour(Skin::kBody, this), scaleValue(4.0f));

  static constexpr int kPlusSize = 8;
  auto plusSize = scaleValueRound(kPlusSize);

  auto plusBounds = Rectangle{ (float)localBounds.getCentreX(),
    (float)localBounds.getCentreY(), 0.0f, 0.0f }.withExpand(plusSize * 0.5f);

  strokePlus(g, plusBounds, scaleValue(1.0f), getColour(Skin::kNormalText, this));

  return true;
}

void EffectsSection::LaneSelector::reinitialise()
{
  removeAllChildComponents();

  componentFlags.clickable = true;
  sizingFlags = Component::SizingFlags(Component::GrowableX | Component::ScrollableSnapToMinX);
  padding = { kScrollOffset, (u16)(kScrollDimensions.y + kScrollOffset),
    kScrollOffset, (u16)(kScrollDimensions.h + kScrollOffset) };
  commandMessageHandler = &laneSelectorHandler;
  laneSelectorHandler.object = [](Component *c, u64 commandId, void *extraData)
  {
    auto *self = (LaneSelector *)c;
    switch (commandId)
    {
    case CommandMessages::HandleProcessorInsertion:
    {
      auto *soundEngineSection = (SoundEngineSection *)self->parent->parent;
      auto *metadata = (CommandMessages::ProcessorInsertion *)extraData;

      auto getMiniView = [](Processor *processor)
      {
        return (Component *)&((EffectsLaneSection *)processor->component)->miniView;
      };

      if (!CommandMessages::handleProcessorInsertion(soundEngineSection->soundEngine,
        self, metadata, Placement::centered, getMiniView))
        return false;

      self->removeChildComponent(&self->addMoreLanesButton);
      self->addChildComponent(&self->addMoreLanesButton);

      auto miniView = getMiniView(metadata->processor);
      miniView->previousPosition = invalidPosition;

      if (metadata->oldIndex != metadata->index)
      {
        auto *effectsState = metadata->processor->component->parent;
        effectsState->removeChildComponent(metadata->processor->component);
        effectsState->addChildComponent(metadata->processor->component, metadata->index);
      }

      return true;
    }
    case CommandMessages::HandleAutoscroll:
    {
      auto &data = *(CommandMessages::Autoscroll *)extraData;
      if (data.handleX)
      {
        static constexpr float kAutoscrollMultiplier = 5.0f;

        data.handleX = false;

        Point<i32> position = data.position;
        i32 offset = utils::max(0, kAutoScrollRegion - utils::min(position.x, self->bounds.w - position.x));
        offset = utils::min(offset, (i32)utils::int_max<i8>);
        auto xOffset = (i8)((position.x < self->bounds.w - position.x) ? offset : -offset);

        offsetScroll(self, xOffset * getThreadLocals()->deltaTime * kAutoscrollMultiplier, 0.0f, false);
      }

      return true;
    }
    default:
      break;
    }

    return false;
  };

  addChildComponent(&addMoreLanesButton);
  addMoreLanesButton.sizingFlags = Component::GrowableX;
  addMoreLanesButton.margin = { kScrollRectThickness / 2 + kScrollOffset, 0, kScrollRectThickness / 2 + kScrollOffset, 0 };
  addMoreLanesButton.desiredSize = { EffectsLaneSection::LaneMiniView::kMinWidth,
    EffectsLaneSection::LaneMiniView::kMinHeight, utils::int_max<i32>,
    EffectsLaneSection::LaneMiniView::kMinHeight };
}

static Rectangle<i32>
getSelectorBounds(EffectsSection::LaneSelector *laneSelector)
{
  auto *section = (EffectsSection *)laneSelector->parent;

  auto *start = laneSelector->children;
  for (u32 i = 0; i < section->startLaneIndex && start->next != &laneSelector->addMoreLanesButton; start = start->next)
    i += (u32)(start->placement != Placement::custom);

  Component *end = start;
  for (u32 i = 1; i < section->visibleLaneCount && end->next != &laneSelector->addMoreLanesButton; end = end->next)
    i += (u32)(start->placement != Placement::custom);

  return Rectangle<i32>::fromPoints(start->bounds.getPosition(),
    end->bounds.getPosition() + Point{ end->bounds.w, end->bounds.h });
}

bool
EffectsSection::LaneSelector::render(Graphics &g)
{
  auto effectsSection = (EffectsSection *)parent;
  effectsSection->setStartLaneIndex(effectsSection->startLaneIndex, effectsSection->visibleLaneCount);

  if (children && children != &addMoreLanesButton)
  {
    auto roundedOutline = getSelectorBounds(this).toFloat();
    auto colour = getColour(Skin::kWidgetPrimary1, this);
    roundedOutline = roundedOutline.withExpand(scaleValue(kScrollOffset + kScrollRectThickness));
    auto thickness = scaleValue(kScrollRectThickness);
    auto rounding = scaleValue(kLaneMiniViewRounding + kScrollOffset + kScrollRectThickness);
    strokeRect(g, roundedOutline, thickness, colour, rounding);

    auto y = roundedOutline.getBottom() - thickness;
    fillRect(g, { roundedOutline.x, y, roundedOutline.w, ::roundf(y + scaleValue((float)kScrollDimensions.h)) - y },
      colour, 0.0f, 0.0f, rounding, rounding);

    // yeah this is kinda ugly but it works
    fillRect(g, { roundedOutline.x, y - rounding, thickness, rounding + 1 }, colour);
    fillRect(g, { roundedOutline.getRight() - thickness, y - rounding, thickness, rounding + 1 }, colour);
  }

  return true;
}

void EffectsSection::LaneSelector::selectViewedLane(const MouseEvent &e)
{
  i32 i = 0;
  i32 closestPositionX = utils::int_max<i32>;
  for (auto *child = children; child; (++i), (child = child->next))
  {
    auto diff = utils::abs(child->bounds.getCentreX() - e.x);
    if (diff >= closestPositionX)
      break;
    closestPositionX = diff;
  }
  auto effectsSection = (EffectsSection *)parent;
  i = utils::max(0, i - 1 - (i32)::roundf((float)effectsSection->visibleLaneCount * 0.5f) / 2);
  effectsSection->setStartLaneIndex(i, effectsSection->visibleLaneCount);
}

bool
EffectsSection::LaneSelector::mouseDrag(const MouseEvent &e)
{
  selectViewedLane(e);
  return true;
}

bool
EffectsSection::LaneSelector::mouseWheelMove(const MouseEvent &e)
{
  if (e.mods.test(ModifierKeys::ctrlModifier))
    return false;

  auto multiplier = 30.0f * getThreadLocals()->scale;
  offsetScroll(this, ((utils::abs(e.wheelDeltaX) > utils::abs(e.wheelDeltaY)) ?
    e.wheelDeltaX : e.wheelDeltaY) * multiplier, 0.0f, false);

  return true;
}

bool
EffectsSection::LaneHolder::mouseWheelMove(const MouseEvent &e)
{
  if (!e.mods.test(ModifierKeys::shiftModifier) && e.wheelDeltaX == 0.0f)
    return false;

  auto delta = (e.mods.test(ModifierKeys::shiftModifier)) ? e.wheelDeltaY : e.wheelDeltaX;
  scrollThreshold += delta;
  i8 direction = (i8)utils::clamp((i32)::roundf(scrollThreshold), -1, 1);
  scrollThreshold -= direction;

  auto *section = (EffectsSection *)parent;
  section->setStartLaneIndex((u32)utils::max(0,
    (i32)section->startLaneIndex - direction), section->visibleLaneCount);

  return true;
}

bool
EffectsSection::LaneHolder::render(Graphics &g)
{
  for (auto *child = children; child; child = child->next)
  {
    auto section = (EffectsLaneSection *)child;
    section->renderStage = EffectsLaneSection::Background;
    renderChild(g, section);
  }

  for (auto *child = children; child; child = child->next)
  {
    auto section = (EffectsLaneSection *)child;
    section->renderStage = EffectsLaneSection::Modules;
    renderChild(g, section);
  }

  for (auto *child = children; child; child = child->next)
  {
    auto section = (EffectsLaneSection *)child;
    section->renderStage = EffectsLaneSection::Highlight;
    renderChild(g, section);
  }

  return false;
}

void EffectsSection::reinitialise()
{
  removeAllChildComponents();
  componentFlags.vertical = true;

  addChildComponent(&laneSelector);
  laneSelector.sizingFlags = Component::GrowableX;
  laneSelector.placement = Placement::top;
  laneSelector.desiredSize = { 0, kLaneSelectorHeight, 0, kLaneSelectorHeight };
  laneSelector.margin = { 0, 0, 0, kLaneSelectorToLanesMargin };
  laneSelector.pool = pool;
  laneSelector.reinitialise();

  laneHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY | Component::ScrollableX);
  laneHolder.placement = Placement::top;
  addChildComponent(&laneHolder);
}

void EffectsSection::removeLane(EffectsLaneSection *lane)
{
  auto laneIndex = utils::findIndexSll(laneSelector.children, (Component *)&lane->miniView);
  // remove mini view before the lane GUI
  lane->miniView.parent->removeChildComponent(&lane->miniView);

  if (laneIndex < startLaneIndex)
  {
    setStartLaneIndex(startLaneIndex - 1, visibleLaneCount);
    laneHolder.scrollOffset.x -= lane->next->bounds.x - lane->bounds.x;
    nextScrollPositionRatio = 1.0f;
  }
  else
    setStartLaneIndex(startLaneIndex, visibleLaneCount);

  auto &plugin = getThreadLocals()->plugin;
  auto transactionArena = plugin.undoManager.beginNewTransaction();
  plugin.undoManager.perform(anew(transactionArena, DeleteProcessorUpdate, { lane->effectsLane }));
}

void EffectsSection::setStartLaneIndex(u32 newStart, u32 newCount)
{
  auto oldStart = startLaneIndex;

  startLaneIndex = 0;
  visibleLaneCount = utils::max(1U, visibleLaneCount);

  if (laneHolder.children)
  {
    auto *start = laneHolder.children;
    u32 i = 0;
    for (; i < newStart && start->next; start = start->next)
      if (start->next->componentFlags.visible)
        ++i;

    Component *end = start;
    u32 j = 1;
    for (; end->next && j < newCount; end = end->next)
      if (end->next->componentFlags.visible)
        ++j;

    startLaneIndex = (u32)utils::max(0, (i32)(i - (visibleLaneCount - j)));
  }

  COMPLEX_ASSERT(visibleLaneCount > 0);

  if (oldStart != startLaneIndex)
  {
    laneHolder.previousOffsetX = laneHolder.scrollOffset.x;
    nextScrollPositionRatio = 0.0f;
  }

}

Area<u32>
EffectsSection::checkResizing(Area<u32> newScaledSize, bool force)
{
  if (newScaledSize.w == (u32)bounds.w && !force)
    return newScaledSize;

  float count = (float)newScaledSize.w / scaleValue((float)(kEffectsLaneWidth + kHLaneToLaneMargin));
  visibleLaneCount = utils::max(1U, (u32)::roundf(count));

  newScaledSize.w = scaleValueRoundInt((float)(visibleLaneCount * kEffectsLaneWidth +
    (visibleLaneCount - 1) * kHLaneToLaneMargin));

  return newScaledSize;
}

bool
EffectsSection::render(Graphics &)
{
  static constexpr float kMoveDelay = 0.15f; //s

  auto t0 = smoothstep(nextScrollPositionRatio);
  nextScrollPositionRatio = utils::min(nextScrollPositionRatio + getThreadLocals()->deltaTime * 1.0f / kMoveDelay, 1.0f);
  auto t1 = smoothstep(nextScrollPositionRatio);

  {
    auto *child = utils::indexSll(laneHolder.children, startLaneIndex);
    if (child)
    {
      auto nextOffsetX = child->bounds.x + laneHolder.scrollOffset.x - laneHolder.previousOffsetX;
      offsetScroll(&laneHolder, -nextOffsetX * (t1 - t0), 0.0f, false);
    }
    else
      scrollOffset.x = 0;
  }

  return true;
}

bool
SoundEngineSection::TopBar::UndoRedoButton::mouseUp(const MouseEvent &)
{
  if (componentFlags.isClicked && componentFlags.isHovered)
  {
    if (isRedo)
      getThreadLocals()->plugin.undoManager.redo();
    else
      getThreadLocals()->plugin.undoManager.undo();
  }

  return true;
}

bool
SoundEngineSection::TopBar::UndoRedoButton::render(Graphics &g)
{
  auto &undoManager = getThreadLocals()->plugin.undoManager;
  bool canDo = (isRedo && undoManager.canRedo()) || (!isRedo && undoManager.canUndo());
  auto colour = canDo ? getColour(Skin::kWidgetAccent1, this) :
    getColour(Skin::kWidgetPrimaryDisabled, this);

  tickAnimation(animationValues,
    { { componentFlags.isHovered || componentFlags.isClicked } },
    { { kHoverIncrement } });
  auto hoverAmount = animationValues[0];
  auto drawBounds = getLocalBounds().toFloat();

  float rounding = utils::min(drawBounds.w, drawBounds.h) * rectRoundingRatio;

  strokeRect(g, drawBounds, scaleValue(1.0f), colour.withAlpha(hoverAmount), rounding);

  drawBounds = drawBounds.withTrim(scaleValue(padding.toFloat()));

  if (isRedo)
  {
    drawBounds.x = drawBounds.getRight();
    drawBounds.w = -drawBounds.w;
  }

  drawSVG(g.getSVG({ (const byte *)BinaryData::Icon_Undo_svg, BinaryData::Icon_Undo_svgSize }),
    g, colour, drawBounds, 0.0f, scaleValue(1.0f));

  return true;
}

void SoundEngineSection::TopBar::reinitialise()
{
  removeAllChildComponents();
  undoRedoGroup.removeAllChildComponents();
  gainGroup.removeAllChildComponents();
  mixGroup.removeAllChildComponents();

  auto *mainSection = (SoundEngineSection *)parent;

  fpsGraph = {};
  fpsGraph.componentFlags.visible = mainSection->showFps;
  fpsGraph.placement = Placement::left;
  fpsGraph.margin = { 4, 0, 0, 0 };
  fpsGraph.desiredSize = { 24, 24, utils::int_max<i32>, utils::int_max<i32> };
  addChildComponent(&fpsGraph);

  undoRedoGroup.placement = Placement::right;
  undoRedoGroup.addChildComponent(&undoButton);
  redoButton.isRedo = true;
  undoRedoGroup.addChildComponent(&redoButton);
  addChildComponent(&undoRedoGroup);

  gainLabel.margin = { 0, 0, 4, 0 };
  gainLabel.control = &gain;
  gain.pool = pool;
  gain.maxDecimalCharacters = 2;
  gain.controlFlags.shouldUsePlusMinusPrefix = true;
  gain.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::OutGain));
  gainGroup.placement = Placement::right;
  gainGroup.margin = { 12, 0, 0, 0 };
  gainGroup.addChildComponent(&gainLabel);
  gainGroup.addChildComponent(&gain);
  addChildComponent(&gainGroup);

  mixLabel.margin = { 0, 0, 4, 0 };
  mixLabel.control = &mix;
  mix.pool = pool;
  mix.maxDecimalCharacters = 2;
  mix.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::Mix));
  mixGroup.placement = Placement::right;
  mixGroup.margin = { 12, 0, 0, 0 };
  mixGroup.addChildComponent(&mixLabel);
  mixGroup.addChildComponent(&mix);
  addChildComponent(&mixGroup);
}

void SoundEngineSection::BottomBar::reinitialise()
{
  removeAllChildComponents();
  blockSizeGroup.removeAllChildComponents();
  overlapGroup.removeAllChildComponents();
  windowGroup.removeAllChildComponents();

  auto *mainSection = (SoundEngineSection *)parent;

  addChildComponent(&blockSizeGroup);
  blockSizeGroup.placement = Placement::justifyX;
  blockSizeGroup.addChildComponent(&blockSizeLabel);
  blockSizeLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
  blockSizeLabel.control = &blockSize;
  blockSizeGroup.addChildComponent(&blockSize);
  blockSize.pool = pool;
  blockSize.drawBackgroundArrow = false;
  blockSize.maxDecimalCharacters = 0;
  blockSize.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::BlockSize));

  addChildComponent(&overlapGroup);
  overlapGroup.addChildComponent(&overlapLabel);
  overlapLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
  overlapLabel.control = &overlap;
  overlapGroup.addChildComponent(&overlap);
  overlap.pool = pool;
  overlap.drawBackgroundArrow = false;
  overlap.maxDecimalCharacters = 2;
  overlap.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::Overlap));
  overlapGroup.placement = Placement::justifyX;
  overlapGroup.componentFlags.animateMovement = true;

  addChildComponent(&windowGroup);
  windowGroup.placement = Placement::justifyX;
  windowGroup.addChildComponent(&windowLabel);
  windowLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
  windowLabel.control = &window;
  windowGroup.addChildComponent(&windowAlpha);
  windowAlpha.pool = pool;
  windowAlpha.margin = { 0, 0, Control::kLabelMargin, 0 };
  windowAlpha.drawBackgroundArrow = false;
  windowAlpha.maxDecimalCharacters = 1;
  windowAlpha.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::WindowAlpha));
  windowGroup.addChildComponent(&window);
  window.pool = pool;
  window.valueChangedCallback = [](Control *c, float newValue, float)
  {
    auto *bottomBar = (BottomBar *)c->parent->parent;
    bottomBar->windowAlpha.componentFlags.visible = getOptionFromValue(
      scaleValue(newValue, c->details), c->details).first->userFlags;
  };
  window.changeLinkedParameter(*mainSection->soundEngine->getParameter(SoundEngine::WindowType));

  window.valueChangedCallback(&window, window.getValue(), 0.0f);
}

bool
SoundEngineSection::BottomBar::render(Graphics &g)
{
  fillRect(g, bounds.withZeroOrigin().toFloat(), getColour(Skin::kBody, this));

  return true;
}

void SoundEngineSection::reinitialise()
{
  COMPLEX_ASSERT(soundEngine);

  removeAllChildComponents();
  componentFlags.vertical = true;
  sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  removeCommandMessageHandler(soundEngineHandler);
  addCommandMessageHandler(soundEngineHandler);
  soundEngineHandler.object = [](Component *c, u64 commandId, void *extraData)
  {
    auto self = (SoundEngineSection *)c;
    switch (commandId)
    {
    case CommandMessages::HandleProcessorInsertion:
    {
      auto *metadata = (CommandMessages::ProcessorInsertion *)extraData;

      if (!metadata->useIndex)
        return false;

      if (!CommandMessages::handleProcessorInsertion(self->soundEngine,
        &self->effectsSection.laneHolder, metadata, Placement::left))
        return false;

      auto section = (EffectsLaneSection *)metadata->processor->component;
      section->margin = { 0, 0, 4, 0 };

      // add mini view to lane selector
      section->miniView.margin = self->effectsSection.laneSelector.addMoreLanesButton.margin;
      section->miniView.surfaceToLiftTo = &self->effectsSection;
      if (!metadata->placeholder)
      {
        if (section->miniView.parent)
          section->miniView.parent->removeChildComponent(&section->miniView);

        self->effectsSection.laneSelector.addChildComponent(&section->miniView, metadata->index);
        if (metadata->processor->name.empty())
          getLaneName(section, &self->effectsSection);
      }

      return true;
    }
    default:
      break;
    }

    return false;
  };

  if (!pool)
    pool = utils::Pool::createNested(utils::Pool::fromAllocation(this), COMPLEX_KB(16));
  utils::Pool::clear(pool);

  topBar.placement = Placement::top;
  topBar.sizingFlags |= Component::GrowableX;
  topBar.desiredSize = { 0, kHeaderHeight, utils::int_max<i32>, kHeaderHeight };
  topBar.padding = { 0, 0, 16, 0 };
  topBar.pool = pool;
  addChildComponent(&topBar);
  topBar.reinitialise();

  spectrogram.sizingFlags = Component::GrowableX;
  spectrogram.placement = Placement::top;
  spectrogram.bufferView = soundEngine->getInterleavedOutputBuffer();
  spectrogram.desiredSize = { 0, kMainVisualiserHeight, 0, kMainVisualiserHeight };
  spectrogram.margin = { kHWindowEdgeMargin, 0, kHWindowEdgeMargin, kVGlobalMargin };
  addChildComponent(&spectrogram);
  spectrogram.reinitialise();

  effectsSection.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  effectsSection.placement = Placement::top;
  effectsSection.desiredSize = { kEffectsStateMinWidth, 0, kEffectsStateMinWidth, 0 };
  effectsSection.margin = { kHWindowEdgeMargin, 0, kHWindowEdgeMargin, 0 };
  effectsSection.pool = pool;
  addChildComponent(&effectsSection);
  effectsSection.reinitialise();

  bottomBar.placement = Placement::bottom;
  bottomBar.sizingFlags |= Component::GrowableX;
  bottomBar.desiredSize = { 0, kFooterHeight, utils::int_max<i32>, kFooterHeight };
  bottomBar.margin = { 0, kLaneToBottomSettingsMargin, 0, 0 };
  bottomBar.padding = { 16, 0, 16, 0 };
  bottomBar.pool = pool;
  addChildComponent(&bottomBar);
  bottomBar.reinitialise();
}

Area<u32>
SoundEngineSection::checkResizing(Area<u32> newScaledSize, bool force)
{
  auto leftRightPadding = scaleValueRoundInt(2 * kHWindowEdgeMargin);
  auto childNewScaledSize = effectsSection.checkResizing({
    newScaledSize.w - leftRightPadding, newScaledSize.h }, force);
  return { childNewScaledSize.w + leftRightPadding, childNewScaledSize.h };
}

bool
SoundEngineSection::render(Graphics &g)
{
  fillRect(g, bounds.withZeroOrigin().toFloat(),
    getColour(Skin::kBackground, this));

  //reinitialise();

  return true;
}

PopupSelector *getPopupSelector() { return &getThreadLocals()->renderer->gui->popupSelector; }
utils::Pool *getUIPool() { return getThreadLocals()->renderer->gui->pool; }
UndoManager &getUndoManager() { return getThreadLocals()->plugin.undoManager; }
PopupDisplay *getPopupDisplay(bool primary = true)
{
  auto *gui = getThreadLocals()->renderer->gui;
  return (primary) ? &gui->popupDisplay1 : &gui->popupDisplay2;
}

void MainInterface::restartUI(State *state)
{
  // clearing the pool will forget all GUI components
  utils::Pool::clear(state->guiStorage);

  state->gui = anew(state->guiStorage, MainInterface, {});
  state->gui->pool = state->guiStorage;
  state->plugin->renderer.resetGui(state->gui);

  auto recurseProcessors = [](const auto &self,
    Processor *processor, Component *parentComponent) -> void
  {
    // at this point processor->component will have a dangling reference
    // to the old component before the reset (if this is not the 1st time)
    processor->component = processor->createUI();
    if (!processor->component)
      return;

    CommandMessages::ProcessorInsertion data{ .processor = processor,
      .index = 0, .useIndex = true };
    if (processor->parent)
      data.index = (u32)processor->getIndex();

    CommandMessages::tryProcessorInsertion(parentComponent, data);

    auto *child = processor->children;
    for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
      self(self, child, processor->component);
  };

  recurseProcessors(recurseProcessors, state->soundEngine, state->gui);

  state->gui->popupSelector.componentFlags.visible = false;
  state->gui->addChildComponent(&state->gui->popupSelector);
  state->gui->popupSelector.reinitialise();

  state->gui->popupDisplay1.componentFlags.visible = false;
  state->gui->addChildComponent(&state->gui->popupDisplay1);
  state->gui->popupDisplay1.reinitialise();

  state->gui->popupDisplay2.componentFlags.visible = false;
  state->gui->addChildComponent(&state->gui->popupDisplay2);
  state->gui->popupDisplay2.reinitialise();
}


bool
MainInterface::keyPressed(const KeyPress &key)
{
  bool isUndo = key.mods.test(ModifierKeys::ctrlModifier) && key.keyCode == 'z';
  bool isRedo = key.mods.test(ModifierKeys::ctrlModifier) && key.keyCode == 'y';

  if (isUndo || isRedo)
  {
    if (isUndo)
      getThreadLocals()->plugin.undoManager.undo();
    else
      getThreadLocals()->plugin.undoManager.redo();

    return true;
  }

  return false;
}

Area<u32>
checkResizing(Area<u32> newScaledSize, bool force)
{
  auto minScaledArea = Area<u32>{ (u32)scaleValueRoundInt(kMinWidth), (u32)scaleValueRoundInt(kMinHeight) };

  newScaledSize.w = utils::max(newScaledSize.w, minScaledArea.w);
  newScaledSize.h = utils::max(newScaledSize.h, minScaledArea.h);

  auto state = getThreadLocals()->plugin.state_;
  if (state)
  {
    auto soundEngineSection = (SoundEngineSection *)state->soundEngine->component;
    newScaledSize = soundEngineSection->checkResizing(newScaledSize, force);
  }

  return newScaledSize;
}

Component *
createUISoundEngine(Processor *processor)
{
  auto *soundEngineSection = anew(getUIPool(), SoundEngineSection, {});
  soundEngineSection->soundEngine = (SoundEngine *)processor;
  soundEngineSection->reinitialise();
  return soundEngineSection;
}
