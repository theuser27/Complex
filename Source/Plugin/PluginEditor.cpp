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

//==============================================================================
ComplexAudioProcessorEditor::ComplexAudioProcessorEditor(ComplexAudioProcessor &p)
  : AudioProcessorEditor{ p }, renderer_{ p.getRenderer() }
{
  using namespace Interface;
  using namespace Framework::LoadSave;

  renderer_.startUI();
  auto *gui = renderer_.getGui();
  addAndMakeVisible(gui);

  constrainer_.setMinimumSize(kMinWidth, kMinHeight);
  constrainer_.setMaximumWidth(kMinWidth);
  constrainer_.setGui(gui);
  constrainer_.setRenderer(&renderer_);
  setConstrainer(&constrainer_);

  auto scale = getWindowScale();
  auto [windowWidth, windowHeight] = getWindowSize();
  renderer_.clampScaleWidthHeight(scale, windowWidth, windowHeight);

  saveWindowScale(scale);
  uiRelated.scale = (float)scale;

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
