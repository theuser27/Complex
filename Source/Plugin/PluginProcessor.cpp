/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ComplexAudioProcessor::ComplexAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
		 : AudioProcessor (BusesProperties()
										 #if ! JucePlugin_IsMidiEffect
											#if ! JucePlugin_IsSynth
											 .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
											#endif
											 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
										 #endif
											 ),
#endif
				ComplexPlugin()
{
	using namespace Framework;

	parameterBridges_.reserve(kMaxParameterMappings + pluginParameterList.size());

	for (auto &parameterDetails : pluginParameterList)
	{
		auto parameter = getProcessorParameter(soundEngine->getProcessorId(), parameterDetails.name);
		if (auto parameterPointer = parameter.lock())
		{
			Framework::ParameterBridge *bridge = new Framework::ParameterBridge((u32)(-1), parameterPointer->getParameterLink());
			parameterBridges_.push_back(bridge);
			addParameter(bridge);
		}
	}

	// temporary, used for testing
	size_t index = 0;
	for (auto &parameterDetails : effectModuleParameterList)
	{
		auto parameter = getProcessorParameter(3, parameterDetails.name);
		if (auto parameterPointer = parameter.lock())
		{
			Framework::ParameterBridge *bridge = new Framework::ParameterBridge(index, parameterPointer->getParameterLink());
			parameterBridges_.push_back(bridge);
			addParameter(bridge);
		}
		index++;
	}

	for (auto &parameterDetails : baseEffectParameterList)
	{
		auto parameter = getProcessorParameter(4, parameterDetails.name);
		if (auto parameterPointer = parameter.lock())
		{
			Framework::ParameterBridge *bridge = new Framework::ParameterBridge(index, parameterPointer->getParameterLink());
			parameterBridges_.push_back(bridge);
			addParameter(bridge);
		}
		index++;
	}

	for (auto &parameterDetails : contrastEffectParameterList)
	{
		auto parameter = getProcessorParameter(4, parameterDetails[0].name);
		if (auto parameterPointer = parameter.lock())
		{
			Framework::ParameterBridge *bridge = new Framework::ParameterBridge(index, parameterPointer->getParameterLink());
			parameterBridges_.push_back(bridge);
			addParameter(bridge);
		}
		index++;
	}

	/*for (size_t i = 0; i < kMaxParameterMappings; i++)
	{
		Framework::ParameterBridge *bridge = new Framework::ParameterBridge(i);
		parameterBridges_.push_back(bridge);
		addParameter(bridge);
	}*/
}
ComplexAudioProcessor::~ComplexAudioProcessor() {}

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
void ComplexAudioProcessor::changeProgramName([[maybe_unused]] int index, [[maybe_unused]] const juce::String &newName) {}

//==============================================================================
void ComplexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	Initialise(sampleRate, samplesPerBlock);
	setLatencySamples(getProcessingDelay());
}

void ComplexAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ComplexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	juce::ignoreUnused (layouts);
	return true;
#else
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
#endif
}
#endif

void ComplexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	auto inputs = getTotalNumInputChannels();
	auto outputs = getTotalNumOutputChannels();
	auto numSamples = buffer.getNumSamples();
	//DBG(numSamples);

	//CheckGlobalParameters();
	updateParameters(UpdateFlag::BeforeProcess);
	setLatencySamples(getProcessingDelay());

	Process(buffer, (u32)numSamples, (u32)inputs, (u32)outputs);

	updateParameters(UpdateFlag::AfterProcess);
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