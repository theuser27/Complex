
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/EffectsState.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Shaders.hpp"
//#include "../Components/Spectrogram.hpp"
//#include "EffectsStateSection.hpp"

namespace Interface
{
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
    nvgBeginPath(openGl);
    nvgRect(openGl, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h);
    nvgFillColor(openGl, getColour(Skin::kBody));
    nvgFill(openGl);

    return true;
  }


  MainInterface::MainInterface()
  {
    componentFlags.isVertical = true;
    arena = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(1));
    
    reinstantiateUI();

    popupDisplay1.initialise();
    popupDisplay2.initialise();
    popupSelector.initialise();
  }

  MainInterface::~MainInterface()
  {
    deleteAllChildComponents();
    utils::bumpArena::destroy(arena);
  }

  bool MainInterface::render(OpenGlWrapper &openGl)
  {
    nvgBeginPath(openGl);
    nvgRect(openGl, (float)bounds.x, (float)bounds.y,
      (float)bounds.w, (float)bounds.h);
    nvgFillColor(openGl, getColour(Skin::kBackground));
    nvgFill(openGl);

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


  void MainInterface::reinstantiateUI()
  {
    removeChildComponent(&resizeCorner);
    removeChildComponent(&topBar);
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
    //auto &soundEngine = getPlugin(uiRelated.renderer).getSoundEngine();

    topBar.placement = Placement::top;
    topBar.sizingFlags |= Component::GrowableX;
    topBar.desiredSize = { 0, kHeaderHeight, utils::max_limit<i32>, kHeaderHeight };
    topBar.margin = { 0, 0, 0, kHeaderNumberBoxMargin };
    topBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
    addChildComponent(&topBar);
    topBar.reinitialise();


    //spectrogram_ = utils::up<Spectrogram>::create("Main Spectrum");
    //spectrogram_->bufferView_ = soundEngine.getEffectsState().getOutputBuffer(kChannelsPerInOut, 0);
    //spectrogram_->desiredW = { .type = Dimension::Grow, .maxSize = u32(-1), .margin = { kHWindowEdgeMargin, kHWindowEdgeMargin } };
    //spectrogram_->desiredH = { .type = Dimension::Fixed, .size = kMainVisualiserHeight };
    //spectrogram_->desiredPosition = { .placement = Placement::top };
    //addChildComponent(spectrogram_.get());


    //effectsStateSection_ = utils::up<EffectsStateSection>::create(soundEngine.getEffectsState());
    //effectsStateSection_->desiredW = { .type = Dimension::Fit, .margin = { kHWindowEdgeMargin, kHWindowEdgeMargin } };
    //effectsStateSection_->desiredH = { .type = Dimension::Grow, .maxSize = u32(-1) };
    //effectsStateSection_->desiredPosition = { .placement = Placement::top };
    //addChildComponent(effectsStateSection_.get());


    bottomBar.placement = Placement::bottom;
    bottomBar.sizingFlags |= Component::GrowableX;
    bottomBar.desiredSize = { 0, kFooterHeight, utils::max_limit<i32>, kFooterHeight };
    bottomBar.margin = { 0, kLaneToBottomSettingsMargin, 0, 0 };
    bottomBar.padding = { ResizeCorner::kWidth, 0, ResizeCorner::kWidth, 0 };
    addChildComponent(&bottomBar);
    bottomBar.reinitialise();


    popupSelector.componentFlags.isVisible = false;
    popupSelector.layerIndex = 1;
    popupSelector.componentFlags.wantsFocus = true;
    addChildComponent(&popupSelector);

    popupDisplay1.componentFlags.isVisible = false;
    popupDisplay1.layerIndex = 1;
    addChildComponent(&popupDisplay1);

    popupDisplay2.componentFlags.isVisible = false;
    popupDisplay2.layerIndex = 1;
    addChildComponent(&popupDisplay2);
  }
}
