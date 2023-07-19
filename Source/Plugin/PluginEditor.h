/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Interface/Sections/InterfaceEngineLink.h"
#include "Interface/LookAndFeel/BorderBoundsConstrainer.h"

//==============================================================================
/**
*/
class ComplexAudioProcessorEditor : public juce::AudioProcessorEditor, public Interface::InterfaceEngineLink
{
public:
	ComplexAudioProcessorEditor (ComplexAudioProcessor&);
	~ComplexAudioProcessorEditor() override;

	//==============================================================================
	void paint (juce::Graphics &g) override { }
	void resized() override;
	void setScaleFactor(float newScale) override;
	void updateFullGui() override;

private:
	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	ComplexAudioProcessor& audioProcessor_;
	Interface::BorderBoundsConstrainer constrainer_{};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplexAudioProcessorEditor)
};
