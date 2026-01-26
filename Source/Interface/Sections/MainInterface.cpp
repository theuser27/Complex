
// Created: 2022-10-10 18:02:52

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Generation/SoundEngine.hpp"
#include "Generation/EffectsState.hpp"
#include "Plugin/Renderer.hpp"
#include "../LookAndFeel/Shaders.hpp"
//#include "../Components/BaseControl.hpp"
//#include "../Components/Spectrogram.hpp"
//#include "Popups.hpp"
//#include "EffectsStateSection.hpp"

namespace Interface
{
  MainInterface::MainInterface()
  {
    sizingFlags = (SizingFlags)(sizingFlags | IsVertical);
    arena = utils::bumpArena::create(COMPLEX_MB(256), COMPLEX_MB(1));
    reinstantiateUI();
  }

  MainInterface::~MainInterface()
  {
    deleteAllChildComponents();
    utils::bumpArena::destroy(arena);
  }

  bool MainInterface::render(OpenGlWrapper &openGl)
  {
    nvgFillColor(openGl.g, getColour(Skin::kBackground));
    nvgRect(openGl.g, 0.0f, 0.0f, (float)bounds.w, (float)bounds.h);


    //nvgFillColor(openGl.g, getColour(Skin::kBody));
    //nvgRect(openGl.g, (float)bottomBar.bounds.x, (float)bottomBar.bounds.y,
    //  (float)bottomBar.bounds.w, (float)bottomBar.bounds.h);

    return true;
  }

  //void MainInterface::controlValueChanged(BaseControl *control)
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
    deleteAllChildComponents();

    addChildComponent(&resizeCorner);

    //using namespace Framework;

    //controls_ = {};
    //removeAllChildren();
    //
    //auto &soundEngine = getPlugin(uiRelated.renderer).getSoundEngine();

    //mixNumberBox_ = utils::up<NumberBox>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::MasterMix::id().value()));
    //mixNumberBox_->setMaxTotalCharacters(5);
    //mixNumberBox_->setMaxDecimalCharacters(2);
    //mixNumberBox_->setCanUseScrollWheel(true);
    //mixNumberBox_->desiredPosition = { .placement = Placement::centerVertical | Placement::right };
    //topBar.addChildComponent(mixNumberBox_.get());

    //gainNumberBox_ = utils::up<NumberBox>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::OutGain::id().value()));
    //gainNumberBox_->setMaxTotalCharacters(5);
    //gainNumberBox_->setMaxDecimalCharacters(2);
    //gainNumberBox_->setShouldUsePlusMinusPrefix(true);
    //gainNumberBox_->setCanUseScrollWheel(true);
    //gainNumberBox_->desiredPosition = { .placement = Placement::centerVertical | Placement::right };
    //topBar.addChildComponent(gainNumberBox_.get());

    //topBar.desiredW =
    //{
    //  .type = Dimension::Grow,
    //  .maxSize = u32(-1),
    //  .margin = { kHeaderHorizontalEdgePadding, kHeaderHorizontalEdgePadding },
    //  .padding = { kHeaderNumberBoxMargin, 0 }
    //};
    //topBar.desiredH = { .type = Dimension::Fixed, .size = kHeaderHeight };
    //topBar.desiredPosition = { .placement = Placement::top };
    //addChildComponent(&topBar);


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


    //blockSizeNumberBox_ = utils::up<NumberBox>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::BlockSize::id().value()));
    //blockSizeNumberBox_->setMaxTotalCharacters(5);
    //blockSizeNumberBox_->setMaxDecimalCharacters(0);
    //blockSizeNumberBox_->setAlternativeMode(true);
    //blockSizeNumberBox_->setCanUseScrollWheel(true);
    //addControl(blockSizeNumberBox_.get(), false);

    //overlapNumberBox_ = utils::up<NumberBox>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::Overlap::id().value()));
    //overlapNumberBox_->setMaxTotalCharacters(4);
    //overlapNumberBox_->setMaxDecimalCharacters(2);
    //overlapNumberBox_->setAlternativeMode(true);
    //overlapNumberBox_->setCanUseScrollWheel(true);
    //addControl(overlapNumberBox_.get(), false);

    //windowTypeSelector_ = utils::up<TextSelector>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::WindowType::id().value()),
    //  Font{ .id = uiRelated.cache->DDinFontId, .height = Graphics::kDDinDefaultHeight,
    //  .kerning = Graphics::kDDinDefaultKerning });
    //windowTypeSelector_->setCanUseScrollWheel(true);
    //windowTypeSelector_->setPopupPlacement(Placement::top);
    //windowTypeSelector_->setLabelPlacement(Placement::left);
    //windowTypeSelector_->addLabel();
    //windowTypeSelector_->setTextSelectorListener(this);
    //addControl(windowTypeSelector_.get(), false);

    //windowAlphaNumberBox_ = utils::up<NumberBox>::create(
    //  soundEngine.getParameter(Processors::SoundEngine::WindowAlpha::id().value()));
    //windowAlphaNumberBox_->setMaxTotalCharacters(4);
    //windowAlphaNumberBox_->setMaxDecimalCharacters(2);
    //windowAlphaNumberBox_->setAlternativeMode(true);
    //windowAlphaNumberBox_->setCanUseScrollWheel(true);
    //windowAlphaNumberBox_->removeLabel();
    //controlValueChanged(windowTypeSelector_.get());
    //addControl(windowAlphaNumberBox_.get(), false);

    //windowTypeSelector_->setExtraNumberBox(windowAlphaNumberBox_.get());
    //windowTypeSelector_->setExtraNumberBoxPlacement(Placement::left);

    //bottomBar.desiredW =
    //{
    //  .type = Dimension::Grow,
    //  .maxSize = u32(-1),
    //  .margin = { kHeaderHorizontalEdgePadding, kHeaderHorizontalEdgePadding }
    //};
    //bottomBar.desiredH =
    //{
    //  .type = Dimension::Fixed,
    //  .size = kHeaderHorizontalEdgePadding,
    //  .margin = { kLaneToBottomSettingsMargin, 0 }
    //};
    //bottomBar.desiredPosition = { .placement = Placement::bottom };
    //addChildComponent(&bottomBar);


    //popupSelector_ = utils::up<PopupSelector>::create();
    //addChildComponent(popupSelector_.get());
    //popupSelector_->setVisible(false);
    //popupSelector_->layerIndex = 1;
    //popupSelector_->featureFlags.wantsFocus = true;

    //popupDisplay1_ = utils::up<PopupDisplay>::create();
    //popupDisplay1_->setVisible(false);
    //popupDisplay1_->layerIndex = 1;
    //addChildComponent(popupDisplay1_.get());

    //popupDisplay2_ = utils::up<PopupDisplay>::create();
    //popupDisplay2_->setVisible(false);
    //popupDisplay2_->layerIndex = 1;
    //addChildComponent(popupDisplay2_.get());
  }
}
