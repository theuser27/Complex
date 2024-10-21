/*
  ==============================================================================

    HeaderFooterSections.cpp
    Created: 23 May 2023 6:00:44am
    Author:  theuser27

  ==============================================================================
*/

#include "HeaderFooterSections.hpp"

#include "Framework/windows.hpp"
#include "Generation/EffectsState.hpp"
#include "Generation/SoundEngine.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "../Components/OpenGlQuad.hpp"
#include "../Components/BaseButton.hpp"
#include "../Components/BaseSlider.hpp"
#include "../Components/Spectrogram.hpp"
#include "Plugin/Complex.hpp"

namespace Interface
{
  static constexpr float kDynamicWindowsStart = (float)Framework::Processors::SoundEngine::WindowType::Exp /
    (float)Framework::Processors::SoundEngine::WindowType::enum_count();

  HeaderFooterSections::HeaderFooterSections(Generation::SoundEngine &soundEngine) :
    BaseSection{ "Header Footer Section" }
  {
    using namespace Framework;

    setInterceptsMouseClicks(false, true);

    bottomBarColour_ = utils::up<OpenGlQuad>::create(Shaders::kColorFragment);
    bottomBarColour_->setTargetComponent(this);
    addOpenGlComponent(bottomBarColour_.get());

    spectrogram_ = utils::up<Spectrogram>::create("Main Spectrum");
    spectrogram_->setSpectrumData(soundEngine.getEffectsState().getOutputBuffer(kChannelsPerInOut, 0), false);
    addOpenGlComponent(spectrogram_.get());
    
    mixNumberBox_ = utils::up<NumberBox>::create(
      soundEngine.getParameter(Processors::SoundEngine::MasterMix::id().value()));
    mixNumberBox_->setMaxTotalCharacters(5);
    mixNumberBox_->setMaxDecimalCharacters(2);
    mixNumberBox_->setCanUseScrollWheel(true);
    addControl(mixNumberBox_.get());

    gainNumberBox_ = utils::up<NumberBox>::create(
      soundEngine.getParameter(Processors::SoundEngine::OutGain::id().value()));
    gainNumberBox_->setMaxTotalCharacters(5);
    gainNumberBox_->setMaxDecimalCharacters(2);
    gainNumberBox_->setShouldUsePlusMinusPrefix(true);
    gainNumberBox_->setCanUseScrollWheel(true);
    addControl(gainNumberBox_.get());

    topContainer_.addControl(mixNumberBox_.get());
    topContainer_.addControl(gainNumberBox_.get());
    topContainer_.setAnchor(Placement::right);

    blockSizeNumberBox_ = utils::up<NumberBox>::create(
      soundEngine.getParameter(Processors::SoundEngine::BlockSize::id().value()));
    blockSizeNumberBox_->setMaxTotalCharacters(5);
    blockSizeNumberBox_->setMaxDecimalCharacters(0);
    blockSizeNumberBox_->setAlternativeMode(true);
    blockSizeNumberBox_->setCanUseScrollWheel(true);
    addControl(blockSizeNumberBox_.get());

    overlapNumberBox_ = utils::up<NumberBox>::create(
      soundEngine.getParameter(Processors::SoundEngine::Overlap::id().value()));
    overlapNumberBox_->setMaxTotalCharacters(4);
    overlapNumberBox_->setMaxDecimalCharacters(2);
    overlapNumberBox_->setAlternativeMode(true);
    overlapNumberBox_->setCanUseScrollWheel(true);
    addControl(overlapNumberBox_.get());

    windowTypeSelector_ = utils::up<TextSelector>::create(
      soundEngine.getParameter(Processors::SoundEngine::WindowType::id().value()),
      Fonts::instance()->getDDinFont());
    windowTypeSelector_->setCanUseScrollWheel(true);
    windowTypeSelector_->setPopupPlacement(Placement::above);
    windowTypeSelector_->setLabelPlacement(Placement::left);
    windowTypeSelector_->addLabel();
    windowTypeSelector_->setTextSelectorListener(this);
    addControl(windowTypeSelector_.get());

    windowAlphaNumberBox_ = utils::up<NumberBox>::create(
      soundEngine.getParameter(Processors::SoundEngine::WindowAlpha::id().value()));
    windowAlphaNumberBox_->setMaxTotalCharacters(4);
    windowAlphaNumberBox_->setMaxDecimalCharacters(2);
    windowAlphaNumberBox_->setAlternativeMode(true);
    windowAlphaNumberBox_->setCanUseScrollWheel(true);
    windowAlphaNumberBox_->removeLabel();
    addControl(windowAlphaNumberBox_.get());
    if (windowTypeSelector_->getValue() < kDynamicWindowsStart)
      windowAlphaNumberBox_->setVisible(false);

    windowTypeSelector_->setExtraNumberBox(windowAlphaNumberBox_.get());
    windowTypeSelector_->setExtraNumberBoxPlacement(Placement::left);

    bottomContainer_.addControl(blockSizeNumberBox_.get());
    bottomContainer_.addControl(overlapNumberBox_.get());
    bottomContainer_.addControl(windowTypeSelector_.get());
    bottomContainer_.setAnchor(Placement::centerHorizontal);
  }

  HeaderFooterSections::~HeaderFooterSections() = default;

  void HeaderFooterSections::resized()
  {
    auto bounds = getLocalBounds();
    int footerHeight = scaleValueRoundInt(kFooterHeight);

    bottomBarColour_->setColor(getColour(Skin::kBody));
    bottomBarColour_->setCustomViewportBounds(bounds.withTop(bounds.getBottom() - footerHeight));
    bottomBarColour_->setBounds(bounds.withTop(bounds.getBottom() - footerHeight));

    auto spectrumBounds = juce::Rectangle
    {
      scaleValueRoundInt(kHWindowEdgeMargin),
      scaleValueRoundInt(kHeaderHeight),
      getWidth() - scaleValueRoundInt(2 * kHWindowEdgeMargin),
      scaleValueRoundInt(kMainVisualiserHeight)
    };
    spectrogram_->setBounds(spectrumBounds);
    spectrogram_->setCornerColour(getColour(Skin::kBackground));


    int numberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
    int headerNumberBoxMargin = scaleValueRoundInt(kHeaderNumberBoxMargin);
    int hPadding = scaleValueRoundInt(kHeaderHorizontalEdgePadding);
    int headerHeight = scaleValueRoundInt(kHeaderHeight);

    topContainer_.setControlSpacing(headerNumberBoxMargin);
    topContainer_.setParentAndBounds(this, { hPadding, utils::centerAxis(numberBoxHeight, headerHeight),
      getWidth() - 2 * hPadding, numberBoxHeight });
    topContainer_.setControlSizes(mixNumberBox_.get(), numberBoxHeight);
    topContainer_.setControlSizes(gainNumberBox_.get(), numberBoxHeight);
    topContainer_.repositionControls();


    int textSelectorHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);
    int footerHPadding = scaleValueRoundInt(kFooterHPadding);

    bottomContainer_.setParentAndBounds(this, { footerHPadding, bounds.getBottom() - footerHeight,
      getWidth() - 2 * footerHPadding, footerHeight });
    bottomContainer_.setControlSizes(blockSizeNumberBox_.get(), numberBoxHeight);
    bottomContainer_.setControlSizes(overlapNumberBox_.get(), numberBoxHeight);
    bottomContainer_.setControlSizes(windowTypeSelector_.get(), textSelectorHeight);
    bottomContainer_.repositionControls();

    // dumb workaround, otherwise the parameter name appears blank in the automation list
    windowAlphaNumberBox_->setColours();
  }

  void HeaderFooterSections::resizeForText(TextSelector *textSelector, int, Placement)
  {
    if (textSelector != windowTypeSelector_.get())
      return;

    if (windowTypeSelector_->getValue() >= kDynamicWindowsStart)
      windowAlphaNumberBox_->setVisible(true);
    else
      windowAlphaNumberBox_->setVisible(false);

    windowTypeSelector_->setBounds(windowTypeSelector_->getDrawBounds());
  }
}
