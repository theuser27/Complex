/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.hpp"

#include "PluginProcessor.hpp"
#include "Renderer.hpp"
#include "Framework/load_save.hpp"
#include "Interface/Sections/MainInterface.hpp"
#include "Interface/LookAndFeel/Miscellaneous.hpp"

namespace Interface
{
  void BorderBoundsConstrainer::checkBounds(juce::Rectangle<int> &bounds, const juce::Rectangle<int> &previous,
    const juce::Rectangle<int> &limits, bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight)
  {
    ComponentBoundsConstrainer::checkBounds(bounds, previous, limits,
      isStretchingTop, isStretchingLeft, isStretchingBottom, isStretchingRight);

    juce::Rectangle<int> displayArea = juce::Desktop::getInstance().getDisplays().getTotalBounds(true);
    if (gui_)
      if (auto *peer = gui_->getPeer())
        peer->getFrameSize().subtractFrom(displayArea);

    // TODO: make resizing snap horizontally to the width of individual lanes 
  }

  void BorderBoundsConstrainer::resizeStart()
  {
    if (!renderer_)
      return;

    renderer_->setIsResizing(true);
  }

  void BorderBoundsConstrainer::resizeEnd()
  {
    if (!gui_ || !renderer_)
      return;

    auto [unscaledWidth, unscaledHeight] = unscaleDimensions(gui_->getWidth(), gui_->getHeight(), uiRelated.scale);
    Framework::LoadSave::saveWindowSize(unscaledWidth, unscaledHeight);

    renderer_->setIsResizing(false);
  }
}

ComplexAudioProcessorEditor::ComplexAudioProcessorEditor(ComplexAudioProcessor &p) : 
  AudioProcessorEditor{ p }, renderer_{ p.getRenderer() }
{
  renderer_.startUI();
  auto *gui = renderer_.getGui();
  addAndMakeVisible(gui);

  constrainer_.setMinimumSize(Interface::kMinWidth, Interface::kMinHeight);
  constrainer_.setMaximumWidth(Interface::kMinWidth);
  constrainer_.setGui(gui);
  constrainer_.setRenderer(&renderer_);
  setConstrainer(&constrainer_);

  auto scale = Framework::LoadSave::getWindowScale();
  auto [windowWidth, windowHeight] = Framework::LoadSave::getWindowSize();
  renderer_.clampScaleWidthHeight(scale, windowWidth, windowHeight);

  Framework::LoadSave::saveWindowScale(scale);
  Interface::uiRelated.scale = (float)scale;

  setResizable(false, true);
  setSize((int)std::round(windowWidth * scale), (int)std::round(windowHeight * scale));
}

ComplexAudioProcessorEditor::~ComplexAudioProcessorEditor()
{
  using namespace Interface;
  using namespace Framework::LoadSave;

  auto scaling = uiRelated.scale;
  saveWindowScale(scaling);

  auto unscaledWidth = (float)getWidth() / scaling;
  auto unscaledHeight = (float)getHeight() / scaling;
  saveWindowSize((int)std::round(unscaledWidth), (int)std::round(unscaledHeight));

  renderer_.stopUI();
  removeChildComponent(renderer_.getGui());
}

void ComplexAudioProcessorEditor::resized()
{
  utils::ScopedLock g{ renderer_.getRenderLock(), utils::WaitMechanism::WaitNotify };
  auto *gui = renderer_.getGui();
  gui->setBounds(getLocalBounds());
}

void ComplexAudioProcessorEditor::setScaleFactor(float)
{
  //AudioProcessorEditor::setScaleFactor(newScale);
  //interfaceLink_.getGui()->redoBackground();
}
