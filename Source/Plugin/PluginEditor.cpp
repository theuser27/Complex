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
	: AudioProcessorEditor (&p), audioProcessor_(p), renderer_(p, *this)
{
	using namespace Interface;
	using namespace Framework;

	setLookAndFeel(DefaultLookAndFeel::instance());
	addAndMakeVisible(dummyComponent_);

	renderer_.startUI(dummyComponent_);
	auto *gui = renderer_.getGui();
	addAndMakeVisible(gui);

	constrainer_.setMinimumSize(MainInterface::kMinWidth, MainInterface::kMinHeight);
	constrainer_.setGui(gui);
	setConstrainer(&constrainer_);

	auto scale = std::clamp(LoadSave::getWindowScale(), (double)kMinWindowScaleFactor, (double)kMaxWindowScaleFactor);
	auto [windowWidth, windowHeight] = LoadSave::getWindowSize();
	auto clampedScale = renderer_.clampScaleFactorToFit(scale, windowWidth, windowHeight);

	LoadSave::saveWindowScale(clampedScale);
	constrainer_.setScaleFactor(clampedScale);
	gui->setScaling((float)clampedScale);

	setResizable(false, true);
	setSize((int)std::round(windowWidth * clampedScale), (int)std::round(windowHeight * clampedScale));
}

ComplexAudioProcessorEditor::~ComplexAudioProcessorEditor()
{
	auto scaling = renderer_.getGui()->getScaling();
	Framework::LoadSave::saveWindowScale(scaling);

	auto unscaledWidth = (float)getWidth() / scaling;
	auto unscaledHeight = (float)getHeight() / scaling;
	Framework::LoadSave::saveWindowSize((int)std::round(unscaledWidth), (int)std::round(unscaledHeight));

	renderer_.stopUI();
}

void ComplexAudioProcessorEditor::resized()
{
	AudioProcessorEditor::resized();
	dummyComponent_.setSize(1, 1);
	renderer_.getGui()->setBounds(getLocalBounds());
}

void ComplexAudioProcessorEditor::setScaleFactor(float)
{
	//AudioProcessorEditor::setScaleFactor(newScale);
	//interfaceLink_.getGui()->redoBackground();
}
