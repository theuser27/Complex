/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Renderer.h"
#include "Interface/LookAndFeel/BorderBoundsConstrainer.h"

//==============================================================================
/**
*/
class ComplexAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
	ComplexAudioProcessorEditor (ComplexAudioProcessor&);
	~ComplexAudioProcessorEditor() override;

	//==============================================================================
	void paint (Graphics &) override { }
	void resized() override;
	void setScaleFactor(float newScale) override;

private:
	Interface::Renderer &renderer_;
	Interface::BorderBoundsConstrainer constrainer_{};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplexAudioProcessorEditor)
};
