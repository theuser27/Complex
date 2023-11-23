/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ComplexAudioProcessor::ComplexAudioProcessor()
		 : AudioProcessor (BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
																				.withOutput("Output", juce::AudioChannelSet::stereo(), true))

{
	using namespace Framework;

	constexpr auto pluginParametersEnum = BaseProcessors::SoundEngine::enum_values<nested_enum::OuterNodes>();
	parameterBridges_.reserve(kMaxParameterMappings + pluginParametersEnum.size());

	for (BaseProcessors::SoundEngine::type parameterDetails : pluginParametersEnum)
	{
		if (auto parameter = getProcessorParameter(soundEngine_->getProcessorId(), parameterDetails))
		{
			auto *bridge = new ParameterBridge(this, (u32)(-1), parameter->getParameterLink());
			parameterBridges_.push_back(bridge);
			addParameter(bridge);
		}
	}

	for (u32 i = 0; i < kMaxParameterMappings; i++)
	{
		auto *bridge = new ParameterBridge(this, i);
		parameterBridges_.push_back(bridge);
		addParameter(bridge);
	}
}

//==============================================================================
const juce::String ComplexAudioProcessor::getName() const { return JucePlugin_Name; }
bool ComplexAudioProcessor::acceptsMidi() const {	return false; /* TODO: change when midi implementation is done */ }
bool ComplexAudioProcessor::producesMidi() const { return false; }
bool ComplexAudioProcessor::isMidiEffect() const { return false; }
double ComplexAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int ComplexAudioProcessor::getNumPrograms() { return 1; }
int ComplexAudioProcessor::getCurrentProgram() { return 0; }
void ComplexAudioProcessor::setCurrentProgram([[maybe_unused]] int index) {}
const juce::String ComplexAudioProcessor::getProgramName([[maybe_unused]] int index) { return {}; }
void ComplexAudioProcessor::changeProgramName([[maybe_unused]] int index, [[maybe_unused]] const juce::String &newName) { }

//==============================================================================
void ComplexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	Initialise((float)sampleRate, samplesPerBlock);
	setLatencySamples((int)getProcessingDelay());
}

void ComplexAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

bool ComplexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
	// This is the place where you check if the layout is supported.
	// In this template code we only support mono or stereo.
	// Some plugin hosts, such as certain GarageBand versions, will only
	// load plugins that support stereo bus layouts.
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
		&& layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
#endif

	return true;
}

void ComplexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	auto inputs = getTotalNumInputChannels();
	auto outputs = getTotalNumOutputChannels();
	auto numSamples = buffer.getNumSamples();
	//DBG(numSamples);

	//CheckGlobalParameters();
	auto sampleRate = ComplexPlugin::getSampleRate();
	updateParameters(Framework::UpdateFlag::BeforeProcess, sampleRate);
	setLatencySamples((int)getProcessingDelay());

	Process(buffer, (u32)numSamples, sampleRate, (u32)inputs, (u32)outputs);

	updateParameters(Framework::UpdateFlag::AfterProcess, sampleRate);
}

//==============================================================================
bool ComplexAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ComplexAudioProcessor::createEditor()
{ return new ComplexAudioProcessorEditor (*this); }

//==============================================================================
void ComplexAudioProcessor::getStateInformation ([[maybe_unused]] juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void ComplexAudioProcessor::setStateInformation ([[maybe_unused]] const void* data, [[maybe_unused]] int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ComplexAudioProcessor(); }