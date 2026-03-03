
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/EffectsState.hpp"
#include "Plugin/Renderer.hpp"


namespace Interface
{
  InvisibleHoverComponent::InvisibleHoverComponent()
  {
    overrideDimensions = [](Component *c, bool isCalculatingVertical)
    {
      auto *self = (InvisibleHoverComponent *)c;
      if (isCalculatingVertical)
        return Range<i32>{ self->bounds.h, self->bounds.h };
      else
        return Range<i32>{ self->bounds.w, self->bounds.w };
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

  void EffectsStateSection::reinitialise()
  {
    componentFlags.vertical = true;

    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_MB(1));
    utils::bumpArena::clear(arena);

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
  EffectsStateSection::render(OpenGlWrapper &openGl)
  {
    if (laneSelector.firstVisibleLaneIndex != laneSelector.lastFirstVisibleLaneIndex)
    {
      // TODO:
    }

    return true;
  }

  struct ComponentInsertionData
  {
    Component *component{};
    utils::typeInfo typeInfo{};
  };

  bool 
  EffectsStateSection::handleCommandMessage(u64 commandId, utils::whatever<64> extraData)
  {
    switch (commandId)
    {
    case Component::HandleComponentInsertion:
    {
      ComponentInsertionData metadata{};
      if (!extraData.tryExtract(metadata))
        break;

      if (metadata.typeInfo != typeId(ProcessorSection))
        break;

      auto *processorSection = (ProcessorSection *)metadata.component;

      if (processorSection->processor->metadata->id != Generation::Processors::EffectsLane)
        break;

      auto centrePoint = getRelativeArea(processorSection).getCentre() - scrollOffset;
      auto *child = children;
      for (; child; child = child->next)
        if (centrePoint.x < child->bounds.getCentre().x)
          break;

      invisibleHover.source = processorSection;
      removeChildComponent(&invisibleHover);
      addChildComponent(&invisibleHover, child);

      return true;
    }
    default:
      break;
    }

    return false;
  }

  void TopBar::reinitialise()
  {
    removeAllChildComponents();

    auto &soundEngine = getPlugin(uiRelated.renderer).state_->getSoundEngine();

    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(16));
    utils::bumpArena::clear(arena);

    gainLabel.margin = { 0, 0, 4, 0 };
    gainLabel.control = &gain;
    gain.arena = arena;
    gain.maxDecimalCharacters = 2;
    gain.controlFlags.shouldUsePlusMinusPrefix = true;
    gain.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::OutGain));
    gainGroup.placement = Placement::right;
    gainGroup.margin = { 12, 0, 0, 0 };
    gainGroup.addChildComponent(&gainLabel);
    gainGroup.addChildComponent(&gain);
    addChildComponent(&gainGroup);

    mixLabel.margin = { 0, 0, 4, 0 };
    mixLabel.control = &mix;
    mix.arena = arena;
    mix.maxDecimalCharacters = 2;
    mix.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::Mix));
    mixGroup.placement = Placement::right;
    mixGroup.margin = { 12, 0, 0, 0 };
    mixGroup.addChildComponent(&mixLabel);
    mixGroup.addChildComponent(&mix);
    addChildComponent(&mixGroup);
  }

  void BottomBar::reinitialise()
  {
    removeAllChildComponents();

    auto &soundEngine = getPlugin(uiRelated.renderer).state_->getSoundEngine();

    if (!arena)
      arena = utils::bumpArena::createNested(parent->arena, COMPLEX_KB(16));
    utils::bumpArena::clear(arena);

    blockSizeLabel.margin = { 0, 0, 4, 0 };
    blockSizeLabel.control = &blockSize;
    blockSize.arena = arena;
    blockSize.drawBackgroundArrow = false;
    blockSize.maxDecimalCharacters = 0;
    blockSize.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::BlockSize));
    blockSizeGroup.placement = Placement::justifyX;
    blockSizeGroup.componentFlags.animateMovement = true;
    blockSizeGroup.addChildComponent(&blockSizeLabel);
    blockSizeGroup.addChildComponent(&blockSize);
    addChildComponent(&blockSizeGroup);

    overlapLabel.margin = { 0, 0, 4, 0 };
    overlapLabel.control = &overlap;
    overlap.arena = arena;
    overlap.drawBackgroundArrow = false;
    overlap.maxDecimalCharacters = 2;
    overlap.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::Overlap));
    overlapGroup.placement = Placement::justifyX;
    overlapGroup.componentFlags.animateMovement = true;
    overlapGroup.addChildComponent(&overlapLabel);
    overlapGroup.addChildComponent(&overlap);
    addChildComponent(&overlapGroup);

    windowLabel.margin = { 0, 0, 4, 0 };
    windowLabel.control = &window;
    windowAlpha.arena = arena;
    windowAlpha.margin = { 0, 0, 4, 0 };
    windowAlpha.drawBackgroundArrow = false;
    windowAlpha.maxDecimalCharacters = 1;
    windowAlpha.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::WindowAlpha));
    window.arena = arena;
    window.changeLinkedParameter(*soundEngine.getParameter(Generation::SoundEngine::WindowType));
    windowGroup.placement = Placement::justifyX;
    overlapGroup.componentFlags.animateMovement = true;
    windowGroup.addChildComponent(&windowLabel);
    windowGroup.addChildComponent(&windowAlpha);
    windowGroup.addChildComponent(&window);
    addChildComponent(&windowGroup);

    window.valueChangedCallback = [](Control *c, double newValue, double)
    {
      auto *bottomBar = (BottomBar *)c->parent->parent;
      bottomBar->windowAlpha.componentFlags.isVisible = Framework::getIndexedData(
        Framework::scaleValue(newValue, c->details), c->details).first->userFlags;
    };
    window.valueChangedCallback(&window, window.getValue(), 0.0f);
  }

  bool 
  BottomBar::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, bounds.withZeroOrigin().toFloat(), getColour(Skin::kBody, this));

    return true;
  }

  ResizeCorner::ResizeCorner()
  {
    componentFlags.clickable = true;
    desiredSize = { kWidth, kHeight, kWidth, kHeight };

    placement = Placement::custom;
  }

  bool
  ResizeCorner::handleCommandMessage(u64 commandId, utils::whatever<64>)
  {
    switch (commandId)
    {
    case HandleCustomPosition:
      bounds = bounds.withPosition(parent->bounds.w - bounds.w, parent->bounds.h - bounds.h);
      return true;
    }

    return false;
  }

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


  MainInterface::MainInterface()
  {
    componentFlags.vertical = true;
    arena = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(4));
    
    reinitialise();

    popupDisplay1.initialise();
    popupDisplay2.initialise();
    popupSelector.initialise();
  }

  MainInterface::~MainInterface()
  {
    deleteAllChildComponents();
    utils::bumpArena::destroy(arena);
  }

  bool 
  MainInterface::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, bounds.withZeroOrigin().toFloat(), 
      getColour(Skin::kBackground, this));

    return true;
  }

  //void MainInterface::controlValueChanged(Control *control)
  //{
  //  if (control == windowTypeSelector_.get())
  //    windowAlphaNumberBox_->setVisible(windowTypeSelector_->getValue() >= kDynamicWindowsStart);
  //}

  static constexpr int kHeaderHorizontalEdgePadding = 10;
  static constexpr int kHeaderNumberBoxMargin = 12;

  static constexpr int kFooterHPadding = 16;

  static constexpr int kLabelToControlMargin = 4;
  static constexpr float kDynamicWindowsStart = (float)utils::find_index(Framework::Window::kTypesValues, 
    Framework::Window::Exponential) / (float)Framework::Window::kTypesValues.size();


  void MainInterface::reinitialise()
  {
    removeChildComponent(&resizeCorner);
    removeChildComponent(&topBar);
    removeChildComponent(&spectrogram);
    removeChildComponent(&bottomBar);
    removeChildComponent(&popupSelector);
    removeChildComponent(&popupDisplay1);
    removeChildComponent(&popupDisplay2);
    deleteAllChildComponents();

    //addChildComponent(&resizeCorner);

    //using namespace Framework;

    //controls_ = {};
    //removeAllChildren();
    //
    auto &soundEngine = getPlugin(uiRelated.renderer).state_->getSoundEngine();

    topBar.placement = Placement::top;
    topBar.sizingFlags |= Component::GrowableX;
    topBar.desiredSize = { 0, kHeaderHeight, utils::max_limit<i32>, kHeaderHeight };
    topBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
    addChildComponent(&topBar);
    topBar.reinitialise();


    spectrogram.sizingFlags = Component::GrowableX;
    spectrogram.placement = Placement::top;
    spectrogram.bufferView_ = soundEngine.getEffectsState().getOutputBuffer();
    spectrogram.desiredSize = { 0, kMainVisualiserHeight, 0, kMainVisualiserHeight };
    spectrogram.margin = { kHWindowEdgeMargin, 0, kHWindowEdgeMargin, kVGlobalMargin };
    addChildComponent(&spectrogram);


    effectsStateSection.sizingFlags = GrowableY;
    effectsStateSection.placement = Placement::top;
    effectsStateSection.desiredSize = { kEffectsStateMinWidth, 0, kEffectsStateMinWidth, 0 };
    effectsStateSection.margin = { kHWindowEdgeMargin, 0, kHWindowEdgeMargin, 0 };
    effectsStateSection.processor = &soundEngine.getEffectsState();
    addChildComponent(&effectsStateSection);
    effectsStateSection.reinitialise();


    bottomBar.placement = Placement::bottom;
    bottomBar.sizingFlags |= Component::GrowableX;
    bottomBar.desiredSize = { 0, kFooterHeight, utils::max_limit<i32>, kFooterHeight };
    bottomBar.margin = { 0, kLaneToBottomSettingsMargin, 0, 0 };
    bottomBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
    addChildComponent(&bottomBar);
    bottomBar.reinitialise();


    popupSelector.componentFlags.isVisible = false;
    popupSelector.componentFlags.alwaysOnTop = true;
    popupSelector.componentFlags.wantsFocus = true;
    addChildComponent(&popupSelector);

    popupDisplay1.componentFlags.isVisible = false;
    popupDisplay1.componentFlags.alwaysOnTop = true;
    addChildComponent(&popupDisplay1);

    popupDisplay2.componentFlags.isVisible = false;
    popupDisplay2.componentFlags.alwaysOnTop = true;
    addChildComponent(&popupDisplay2);
  }
}
