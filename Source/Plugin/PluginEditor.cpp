/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Interface/LookAndFeel/DefaultLookAndFeel.h"

//==============================================================================
ComplexAudioProcessorEditor::ComplexAudioProcessorEditor (ComplexAudioProcessor& p)
	: AudioProcessorEditor (&p), InterfaceEngineLink(p), audioProcessor_(p)
{
	using namespace Interface;
	using namespace Framework;

	setLookAndFeel(DefaultLookAndFeel::instance());

	constrainer_.setMinimumSize(kMinWindowWidth, kMinWindowHeight);
	constrainer_.setGui(gui_.get());
	setConstrainer(&constrainer_);
	addAndMakeVisible(gui_.get());

	auto scale = std::clamp(LoadSave::getWindowScale(), (double)kMinWindowScaleFactor, (double)kMaxWindowScaleFactor);
	auto [windowWidth, windowHeight] = LoadSave::getWindowSize();
	auto clampedScale = clampScaleFactorToFit(scale, windowWidth, windowHeight);

	LoadSave::saveWindowScale(clampedScale);
	constrainer_.setScaleFactor(clampedScale);
	gui_->setScaling((float)clampedScale);

	setResizable(true, true);
	setSize((int)std::round(windowWidth * clampedScale), (int)std::round(windowHeight * clampedScale));
}

void ComplexAudioProcessorEditor::resized()
{
	AudioProcessorEditor::resized();
	gui_->setBounds(getLocalBounds());
}

void ComplexAudioProcessorEditor::setScaleFactor(float newScale)
{
	AudioProcessorEditor::setScaleFactor(newScale);
	gui_->redoBackground();
}

void ComplexAudioProcessorEditor::updateFullGui()
{
	InterfaceEngineLink::updateFullGui();
	audioProcessor_.updateHostDisplay();
}
