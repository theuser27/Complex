
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/Effects.hpp"
#include "Plugin/Renderer.hpp"


namespace Interface
{
  InvisibleHoverComponent::InvisibleHoverComponent()
  {
    overrideSize = [](Component *c, bool isCalculatingVertical)
    {
      auto *self = (InvisibleHoverComponent *)c;
      if (isCalculatingVertical)
        return Range<i32>{ self->lastBounds.h, self->lastBounds.h };
      else
        return Range<i32>{ self->lastBounds.w, self->lastBounds.w };
    };
  }

  bool
  InvisibleHoverComponent::render(OpenGlWrapper &openGl)
  {
    auto offset = scaleValueRoundInt(4);
    fillRect(openGl, bounds.withZeroOrigin()
      .withTrimmedTop(offset).withTrimmedLeft(offset).toFloat(),
      getColour(Skin::kLightenScreen, this));

    return true;
  }

  bool
  SoundEngineSection::EffectsSection::LaneSelector::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
    //fillRect(openGl, getLocalBounds().toFloat());

    return true;
  }

  void SoundEngineSection::EffectsSection::reinitialise()
  {
    removeAllChildComponents();
    componentFlags.vertical = true;

    laneSelector.sizingFlags = Component::GrowableX;
    laneSelector.placement = Placement::top;
    laneSelector.desiredSize = { 0, kLaneSelectorHeight, 0, kLaneSelectorHeight };
    laneSelector.margin = { 0, 0, 0, kLaneSelectorToLanesMargin };
    addChildComponent(&laneSelector);

    laneHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
    laneHolder.placement = Placement::top;
    addChildComponent(&laneHolder);
  }

  bool
  SoundEngineSection::EffectsSection::render(OpenGlWrapper &openGl)
  {
    (void)openGl;
    //fillRect(openGl, getLocalBounds().toFloat(), Colours::black);

    if (laneSelector.firstVisibleLaneIndex != laneSelector.lastFirstVisibleLaneIndex)
    {
      // TODO:
    }

    return true;
  }

  bool
  CommandMessages::handleProcessorInsertion(Generation::Processor *parentProcessor,
    Component *parentComponent, ProcessorInsertion *metadata, Component *substituteInsert)
  {
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
      auto point = parentComponent->getRelativePoint(processor->component, metadata->position);
      point.x -= (i32)::roundf(parentComponent->scrollOffset.x);
      point.y -= (i32)::roundf(parentComponent->scrollOffset.y);

      auto Point<i32>:: *primary = (parentComponent->componentFlags.vertical) ?
        &Point<i32>::y : &Point<i32>::x;

      metadata->index = 0;
      child = parentProcessor->children;
      for (; child; (++metadata->index), child = Generation::Processor::getChild(child, 1))
        if (child->component && point.*primary < parentComponent->getRelativePoint(
          child->component, child->component->bounds.getPosition()).*primary)
          break;
    }
    else
      child = Generation::Processor::getChild(parentProcessor->children, metadata->index);

    {
      auto g = parentProcessor->state->plugin->acquireProcessingLock();
      if (processor->parent)
        processor->parent->removeChildProcessor(*processor);

      (void)parentProcessor->addChildProcessor(*processor, metadata->index);
    }

    if (processor->component->parent)
      processor->component->parent->removeChildComponent(processor->component);
    if (substituteInsert)
      parentComponent->removeChildComponent(substituteInsert);
    auto *newComponent = (metadata->insertPlaceholder && substituteInsert) ?
      substituteInsert : processor->component;

    COMPLEX_ASSERT(!child || child->component);
    parentComponent->addChildComponent(newComponent, (child) ? child->component : nullptr);

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
        auto *invisibleHover = &getGui(uiRelated.renderer)->invisibleHover;

        if (!CommandMessages::handleProcessorInsertion(self->soundEngine,
          &self->effectsSection.laneHolder, metadata, 
          metadata->insertPlaceholder ? invisibleHover : nullptr))
          return false;

        invisibleHover->source = metadata->processor->component;
        return true;
      }
      default:
        break;
      }

      return false;
    };

    if (!arena)
      arena = utils::bumpArena::createNested(utils::bumpArena::fromAllocation(this), COMPLEX_KB(16));

    topBar.placement = Placement::top;
    topBar.sizingFlags |= Component::GrowableX;
    topBar.desiredSize = { 0, kHeaderHeight, utils::max_limit<i32>, kHeaderHeight };
    topBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
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

    effectsSection.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
    effectsSection.placement = Placement::top;
    effectsSection.desiredSize = { kEffectsStateMinWidth, 0, kEffectsStateMinWidth, 0 };
    effectsSection.margin = { kHWindowEdgeMargin, 0, kHWindowEdgeMargin, 0 };
    effectsSection.arena = arena;
    addChildComponent(&effectsSection);
    effectsSection.reinitialise();

    bottomBar.placement = Placement::bottom;
    bottomBar.sizingFlags |= Component::GrowableX;
    bottomBar.desiredSize = { 0, kFooterHeight, utils::max_limit<i32>, kFooterHeight };
    bottomBar.margin = { 0, kLaneToBottomSettingsMargin, 0, 0 };
    bottomBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
    bottomBar.mainSection = this;
    bottomBar.arena = arena;
    addChildComponent(&bottomBar);
    bottomBar.reinitialise();
  }

  bool
  SoundEngineSection::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, bounds.withZeroOrigin().toFloat(),
      getColour(Skin::kBackground, this));

    //reinitialise();

    return true;
  }

  ResizeCorner::ResizeCorner()
  {
    componentFlags.clickable = true;
    desiredSize = { kWidth, kHeight, kWidth, kHeight };

    placement = Placement::custom;
  }

  //bool
  //ResizeCorner::handleCommandMessage(u64 commandId, void *)
  //{
  //  switch (commandId)
  //  {
  //  case CommandMessages::HandleCustomPosition:
  //    bounds = bounds.withPosition(parent->bounds.w - bounds.w, parent->bounds.h - bounds.h);
  //    return true;
  //  }
  //
  //  return false;
  //}

  bool
  ResizeCorner::mouseEnter(const MouseEvent &)
  {
    Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::UpLeftDownRightResize);
    return true;
  }

  bool
  ResizeCorner::mouseExit(const MouseEvent &)
  {
    Interface::setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
    return true;
  }

  bool
  ResizeCorner::mouseDown(const MouseEvent &)
  {
    areaAtMouseDown = getUISize(uiRelated.renderer);
    return true;
  }

  extern "C" void cplug_checkSize(void *userGUI, u32 *width, u32 *height);

  bool
  ResizeCorner::mouseDrag(const MouseEvent &event)
  {
    auto offset = event.getOffsetFromDragStart();
    u32 w = areaAtMouseDown.w + offset.x;
    u32 h = areaAtMouseDown.h + offset.y;
    cplug_checkSize(uiRelated.renderer, &w, &h);
    setUISize(uiRelated.renderer, w, h);
    return true;
  }


  PopupSelector *getPopupSelector() { return &getGui(uiRelated.renderer)->popupSelector; }
  utils::bumpArena *getUIArena() { return getGui(uiRelated.renderer)->arena; }
  PopupDisplay *getPopupDisplay(bool primary = true)
  {
    auto *gui = getGui(uiRelated.renderer);
    return (primary) ? &gui->popupDisplay1 : &gui->popupDisplay2;
  }

  MainInterface::MainInterface()
  {
    arena = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(4));
  }

  MainInterface::~MainInterface()
  {
    removeChildComponent(&popupSelector);
    removeChildComponent(&popupDisplay1);
    removeChildComponent(&popupDisplay2);

    deleteAllChildComponents();
    utils::bumpArena::destroy(arena);
  }

  static constexpr int kHeaderHorizontalEdgePadding = 10;
  static constexpr int kHeaderNumberBoxMargin = 12;

  static constexpr int kFooterHPadding = 16;

  static constexpr int kLabelToControlMargin = 4;


  void MainInterface::restartUI()
  {
    //removeChildComponent(&resizeCorner);
    removeChildComponent(&popupSelector);
    removeChildComponent(&popupDisplay1);
    removeChildComponent(&popupDisplay2);

    deleteAllChildComponents();

    auto state = getPlugin(uiRelated.renderer).state_;

    auto recurseProcessors = [](const auto &self,
      Generation::Processor *processor, Component *parentComponent) -> void
    {
      // at this point processor->component will have a dangling reference
      // to the old component before the reset (if this is not the 1st time)
      processor->component = processor->createUI();
      if (!processor->component)
        return;

      CommandMessages::ProcessorInsertion data{ .processor = processor,
        .index = u32(-1), .useIndex = true };

      CommandMessages::tryProcessorInsertion(parentComponent, data);

      auto *child = processor->children;
      for (usize i = 0; i < processor->childrenCount; (++i), (child = child->next))
        self(self, child, processor->component);
    };

    recurseProcessors(recurseProcessors, state->soundEngine, this);

    //addChildComponent(&resizeCorner);

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
}

namespace Generation
{
  Interface::Component *
  SoundEngine::createUI()
  {
    auto guiArena = Interface::getGui(Interface::uiRelated.renderer)->arena;
    auto *soundEngineSection = anew(guiArena, Interface::SoundEngineSection, {});
    soundEngineSection->soundEngine = this;
    soundEngineSection->reinitialise();
    return soundEngineSection;
  }
}
