/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin editor.

	==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Interface/LookAndFeel/BorderBoundsConstrainer.h"

namespace Interface
{
	class Renderer;
}

class ComplexAudioProcessor;

class ComplexAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
	ComplexAudioProcessorEditor (ComplexAudioProcessor&);
	~ComplexAudioProcessorEditor() override;

	//==============================================================================
	void paint (juce::Graphics &) override { }
	void resized() override;
	void setScaleFactor(float newScale) override;

private:
	Interface::Renderer &renderer_;
	Interface::BorderBoundsConstrainer constrainer_{};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplexAudioProcessorEditor)
};
