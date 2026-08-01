
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/Effects.hpp"
#include "Plugin/Renderer.hpp"
#include "EffectsLaneSection.hpp"


namespace Interface
{
  bool
  CommandMessages::handleProcessorInsertion(Generation::Processor *parentProcessor,
    Component *parentComponent, ProcessorInsertion *metadata, Placement placement,
    Component *(*getRelativeComponent)(Generation::Processor *))
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

    Generation::Processor *child;
    metadata->index = utils::min(metadata->index, parentProcessor->childrenCount);
    if (!metadata->useIndex)
    {
      // try to find the correct index for a given position

      auto point = parentComponent->getRelativePoint(getRelativeComponent(processor), metadata->position);

      auto Point<i32>:: *primary = (parentComponent->componentFlags.vertical) ?
        &Point<i32>::y : &Point<i32>::x;

      //metadata->index = 0;
      //for (child = parentProcessor->children; child; (++metadata->index), child = Generation::Processor::getChild(child, 1))
      //{
      //  auto position = Point{ (metadata->isMovingUpX) ? child->component->bounds.w : 0,
      //    (metadata->isMovingUpY) ? child->component->bounds.h : 0 };
      //  position = parentComponent->getRelativePoint(child->component, position);
      //  const char *format = (child == metadata->processor) ?
      //    " > #%d: x: %d, y: %d, w: %d, h: %d\n" : "   #%d: x: %d, y: %d, w: %d, h: %d\n";
      //  COMPLEX_DEBUG_LOG(format, metadata->index, position.x, position.y,
      //    child->component->bounds.w, child->component->bounds.h);
      //}
      //COMPLEX_DEBUG_LOG("\n");

      metadata->index = 0;
      for (child = parentProcessor->children; child;
        (++metadata->index), child = Generation::Processor::getChild(child, 1))
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
    else
      child = Generation::Processor::getChild(parentProcessor->children, metadata->index);

    //COMPLEX_ASSERT(metadata->processor->parent == nullptr ||
    //  metadata->processor->parent == parentProcessor);

    metadata->oldIndex = (metadata->processor->parent) ? (u32)metadata->processor->getIndex() : (u32)-1;
    // if the processor already exists at the correct index, we skip
    if (metadata->processor->parent != parentProcessor || metadata->oldIndex != metadata->index)
    {
      auto g = parentProcessor->state->plugin->acquireProcessingLock();
      if (processor->parent)
        processor->parent->removeChildProcessor(*processor);

      (void)parentProcessor->addChildProcessor(*processor, metadata->index);
    }

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
      newComponent->parent->removeChildComponent(newComponent);
    }

    COMPLEX_ASSERT(!child || child->component);
    // if the processor was already inserted before we started
    // we might try to insert before itself,
    // therefore we need to the next element as insertBefore
    auto childComponent = child ? getRelativeComponent(child) : nullptr;
    if (child && newComponent == childComponent)
    {
      child = child->next;
      childComponent = child ? getRelativeComponent(child) : nullptr;
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

  bool
  EffectsSection::LaneSelector::AddMoreLanesButton::mouseDown(const MouseEvent &)
  {
    // LaneSelector -> EffectsSection -> SoundEngineSection
    auto *effectsSection = (EffectsSection *)parent->parent;
    auto *soundEngine = ((SoundEngineSection *)effectsSection->parent)->soundEngine;
    auto *state = soundEngine->state;
    auto *effectsLane = (Generation::EffectsLane *)state->createProcessor(Generation::Processors::EffectsLane);

    // calculating name
    effectsLane->name = { effectsLane->arena, utils::string::minimumCapacity };
    for (usize i = effectsSection->laneNameOrdinal; i; i /= ('Z' - 'A' + 1))
    {
      char letter[] = { (char)('A' + (i % ('Z' - 'A' + 1))), '\0' };
      effectsLane->name.prepend(letter);
    }
    if (effectsLane->name.empty())
      effectsLane->name.prepend("A");

    auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
    state->plugin->undoManager.perform(anew(transactionArena, Framework::AddProcessorUpdate, { effectsLane,
      soundEngine->stateId, soundEngine->childrenCount }));

    return true;
  }

  bool
  EffectsSection::LaneSelector::AddMoreLanesButton::render(OpenGlWrapper &openGl)
  {
    auto localBounds = getLocalBounds().toFloat();

    strokeRect(openGl, localBounds, scaleValue(1.0f),
      getColour(Skin::kBody, this), scaleValue(4.0f));

    static constexpr int kPlusSize = 8;
    auto plusSize = scaleValueRound(kPlusSize);

    auto plusBounds = Rectangle{ (float)localBounds.getCentreX(),
      (float)localBounds.getCentreY(), 0.0f, 0.0f }.withExpand(plusSize * 0.5f);

    strokePlus(openGl, plusBounds, scaleValue(1.0f), getColour(Skin::kNormalText, this));

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

        auto getMiniView = [](Generation::Processor *processor)
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

          offsetScroll(self, xOffset * uiRelated.deltaTime * kAutoscrollMultiplier, 0.0f, false);
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
  EffectsSection::LaneSelector::render(OpenGlWrapper &openGl)
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
      strokeRect(openGl, roundedOutline, thickness, colour, rounding);

      auto y = roundedOutline.getBottom() - thickness;
      fillRect(openGl, { roundedOutline.x, y, roundedOutline.w, ::roundf(y + scaleValue((float)kScrollDimensions.h)) - y },
        colour, 0.0f, 0.0f, rounding, rounding);

      // yeah this is kinda ugly but it works
      fillRect(openGl, { roundedOutline.x, y - rounding, thickness, rounding + 1 }, colour);
      fillRect(openGl, { roundedOutline.getRight() - thickness, y - rounding, thickness, rounding + 1 }, colour);
    }

    return true;
  }

  bool
  EffectsSection::LaneSelector::mouseDrag(const MouseEvent &e)
  {
    auto scrollThumbBounds = getSelectorBounds(this);
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

    return true;
  }

  bool
  EffectsSection::LaneSelector::mouseWheelMove(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::ctrlModifier))
      return false;

    auto multiplier = 30.0f * uiRelated.scale;
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

  void EffectsSection::reinitialise()
  {
    removeAllChildComponents();
    componentFlags.vertical = true;

    addChildComponent(&laneSelector);
    laneSelector.sizingFlags = Component::GrowableX;
    laneSelector.placement = Placement::top;
    laneSelector.desiredSize = { 0, kLaneSelectorHeight, 0, kLaneSelectorHeight };
    laneSelector.margin = { 0, 0, 0, kLaneSelectorToLanesMargin };
    laneSelector.arena = arena;
    laneSelector.reinitialise();

    laneHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY | Component::ScrollableX);
    laneHolder.placement = Placement::top;
    addChildComponent(&laneHolder);
  }

  void EffectsSection::removeLane(EffectsLaneSection *lane)
  {
    auto laneIndex = utils::findIndexSll(laneSelector.children, (Component *)&lane->miniView);
    // remove mini view before the lane UI
    lane->miniView.parent->removeChildComponent(&lane->miniView);

    if (laneIndex < startLaneIndex)
    {
      setStartLaneIndex(startLaneIndex - 1, visibleLaneCount);
      laneHolder.scrollOffset.x -= lane->next->bounds.x - lane->bounds.x;
      nextScrollPositionRatio = 1.0f;
    }
    else
      setStartLaneIndex(startLaneIndex, visibleLaneCount);

    auto &plugin = getPlugin(uiRelated.renderer);
    auto transactionArena = plugin.undoManager.beginNewTransaction();
    plugin.undoManager.perform(anew(transactionArena,
      Framework::DeleteProcessorUpdate, { lane->effectsLane }));
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
        if (start->next->componentFlags.isVisible)
          ++i;

      Component *end = start;
      u32 j = 1;
      for (; end->next && j < newCount; end = end->next)
        if (end->next->componentFlags.isVisible)
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
  EffectsSection::render(OpenGlWrapper &)
  {
    static constexpr float kMoveDelay = 0.15f; //s

    auto t0 = smoothstep(nextScrollPositionRatio);
    nextScrollPositionRatio = utils::min(nextScrollPositionRatio + uiRelated.deltaTime * 1.0f / kMoveDelay, 1.0f);
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

  void SoundEngineSection::TopBar::reinitialise()
  {
    removeAllChildComponents();
    gainGroup.removeAllChildComponents();
    mixGroup.removeAllChildComponents();

    gainLabel.margin = { 0, 0, 4, 0 };
    gainLabel.control = &gain;
    gain.arena = arena;
    gain.maxDecimalCharacters = 2;
    gain.controlFlags.shouldUsePlusMinusPrefix = true;
    gain.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::OutGain));
    gainGroup.placement = Placement::right;
    gainGroup.margin = { 12, 0, 0, 0 };
    gainGroup.addChildComponent(&gainLabel);
    gainGroup.addChildComponent(&gain);
    addChildComponent(&gainGroup);

    mixLabel.margin = { 0, 0, 4, 0 };
    mixLabel.control = &mix;
    mix.arena = arena;
    mix.maxDecimalCharacters = 2;
    mix.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::Mix));
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

    addChildComponent(&blockSizeGroup);
    blockSizeGroup.placement = Placement::justifyX;
    blockSizeGroup.componentFlags.animateMovement = true;
    blockSizeGroup.addChildComponent(&blockSizeLabel);
    blockSizeLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
    blockSizeLabel.control = &blockSize;
    blockSizeGroup.addChildComponent(&blockSize);
    blockSize.arena = arena;
    blockSize.drawBackgroundArrow = false;
    blockSize.maxDecimalCharacters = 0;
    blockSize.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::BlockSize));

    addChildComponent(&overlapGroup);
    overlapGroup.addChildComponent(&overlapLabel);
    overlapLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
    overlapLabel.control = &overlap;
    overlapGroup.addChildComponent(&overlap);
    overlap.arena = arena;
    overlap.drawBackgroundArrow = false;
    overlap.maxDecimalCharacters = 2;
    overlap.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::Overlap));
    overlapGroup.placement = Placement::justifyX;
    overlapGroup.componentFlags.animateMovement = true;

    addChildComponent(&windowGroup);
    windowGroup.placement = Placement::justifyX;
    //windowGroup.componentFlags.animateMovement = true;
    windowGroup.addChildComponent(&windowLabel);
    windowLabel.margin = { 0, 0, Control::kLabelMargin, 0 };
    windowLabel.control = &window;
    windowGroup.addChildComponent(&windowAlpha);
    windowAlpha.arena = arena;
    windowAlpha.margin = { 0, 0, Control::kLabelMargin, 0 };
    windowAlpha.drawBackgroundArrow = false;
    windowAlpha.maxDecimalCharacters = 1;
    windowAlpha.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::WindowAlpha));
    windowGroup.addChildComponent(&window);
    window.arena = arena;
    window.valueChangedCallback = [](Control *c, double newValue, double)
    {
      auto *bottomBar = (BottomBar *)c->parent->parent;
      bottomBar->windowAlpha.componentFlags.isVisible = Framework::getOptionFromValue(
        Framework::scaleValue(newValue, c->details), c->details).first->userFlags;
    };
    window.changeLinkedParameter(*mainSection->soundEngine->getParameter(Generation::SoundEngine::WindowType));

    window.valueChangedCallback(&window, window.getValue(), 0.0f);
  }

  bool
  SoundEngineSection::BottomBar::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, bounds.withZeroOrigin().toFloat(), getColour(Skin::kBody, this));

    //reinitialise();

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
        section->soundEngineSection = self;
        section->margin = { 0, 0, 4, 0 };

        // add mini view to lane selector
        section->miniView.componentFlags.animateMovement = true;
        section->miniView.previousPosition = invalidPosition;
        section->miniView.margin = self->effectsSection.laneSelector.addMoreLanesButton.margin;
        section->miniView.surfaceToLiftTo = &self->effectsSection;
        if (!metadata->placeholder)
        {
          self->effectsSection.laneSelector.addChildComponent(&section->miniView,
            &self->effectsSection.laneSelector.addMoreLanesButton);
          ++self->effectsSection.laneNameOrdinal;
        }

        return true;
      }
      default:
        break;
      }

      return false;
    };

    if (!arena)
      arena = utils::bumpArena::createNested(utils::bumpArena::fromAllocation(this), COMPLEX_KB(16));
    utils::bumpArena::clear(arena);

    topBar.placement = Placement::top;
    topBar.sizingFlags |= Component::GrowableX;
    topBar.desiredSize = { 0, kHeaderHeight, utils::int_max<i32>, kHeaderHeight };
    topBar.padding = { 16, 0, 16, 0 };
    topBar.arena = arena;
    topBar.mainSection = this;
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
    effectsSection.arena = arena;
    addChildComponent(&effectsSection);
    effectsSection.reinitialise();

    bottomBar.placement = Placement::bottom;
    bottomBar.sizingFlags |= Component::GrowableX;
    bottomBar.desiredSize = { 0, kFooterHeight, utils::int_max<i32>, kFooterHeight };
    bottomBar.margin = { 0, kLaneToBottomSettingsMargin, 0, 0 };
    bottomBar.padding = { 16, 0, 16, 0 };
    bottomBar.mainSection = this;
    bottomBar.arena = arena;
    addChildComponent(&bottomBar);
    bottomBar.reinitialise();
  }

  Area<u32>
  SoundEngineSection::checkResizing(Area<u32> newScaledSize, bool force)
  {
    auto childNewScaledSize = effectsSection.checkResizing({
      newScaledSize.w - (bounds.w - effectsSection.bounds.w), newScaledSize.h }, force);
    return { childNewScaledSize.w + bounds.w - effectsSection.bounds.w, childNewScaledSize.h };
  }

  bool
  SoundEngineSection::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, bounds.withZeroOrigin().toFloat(),
      getColour(Skin::kBackground, this));

    //reinitialise();

    return true;
  }

  PopupSelector *getPopupSelector() { return &getGui(uiRelated.renderer)->popupSelector; }
  utils::bumpArena *getUIArena() { return getGui(uiRelated.renderer)->arena; }
  PopupDisplay *getPopupDisplay(bool primary = true)
  {
    auto *gui = getGui(uiRelated.renderer);
    return (primary) ? &gui->popupDisplay1 : &gui->popupDisplay2;
  }

  void MainInterface::restartUI(Plugin::State *state)
  {
    removeChildComponent(&popupSelector);
    removeChildComponent(&popupDisplay1);
    removeChildComponent(&popupDisplay2);

    deleteAllChildComponents();

    auto recurseProcessors = [](const auto &self,
      Generation::Processor *processor, Component *parentComponent) -> void
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

    recurseProcessors(recurseProcessors, state->soundEngine, this);

    popupSelector.componentFlags.isVisible = false;
    popupSelector.componentFlags.alwaysOnTop = true;
    addChildComponent(&popupSelector);
    popupSelector.reinitialise();

    popupDisplay1.componentFlags.isVisible = false;
    popupDisplay1.componentFlags.alwaysOnTop = true;
    addChildComponent(&popupDisplay1);
    popupDisplay1.reinitialise();

    popupDisplay2.componentFlags.isVisible = false;
    popupDisplay2.componentFlags.alwaysOnTop = true;
    addChildComponent(&popupDisplay2);
    popupDisplay2.reinitialise();
  }

  Area<u32>
  checkResizing(Area<u32> newScaledSize, bool force)
  {
    auto minScaledArea = Area<u32>{ (u32)scaleValueRoundInt(kMinWidth), (u32)scaleValueRoundInt(kMinHeight) };

    newScaledSize.w = utils::max(newScaledSize.w, minScaledArea.w);
    newScaledSize.h = utils::max(newScaledSize.h, minScaledArea.h);

    auto state = getPlugin(uiRelated.renderer).state_;
    if (state)
    {
      auto soundEngineSection = (SoundEngineSection *)state->soundEngine->component;
      newScaledSize = soundEngineSection->checkResizing(newScaledSize, force);
    }

    return newScaledSize;
  }
}

Interface::Component *
Generation::SoundEngine::createUI()
{
  auto guiArena = Interface::getGui(Interface::uiRelated.renderer)->arena;
  auto *soundEngineSection = anew(guiArena, Interface::SoundEngineSection, {});
  soundEngineSection->soundEngine = this;
  soundEngineSection->reinitialise();
  return soundEngineSection;
}
