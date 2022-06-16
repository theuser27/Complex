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
  addParameter(mix_ = new AudioParameterFloat("MIX", "Mix", 0.0f, 1.0f, 1.0f));
  addParameter(order_ = new AudioParameterInt("ORDER", "Order", kMinFFTOrder, kMaxFFTOrder, kDefaultFFTOrder));
  addParameter(overlap_ = new AudioParameterFloat("OVERLAP", "Overlap", kMinWindowOverlap, kMaxWindowOverlap, kDefaultWindowOverlap));
}

ComplexAudioProcessor::~ComplexAudioProcessor()
{
}

//==============================================================================
const juce::String ComplexAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ComplexAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ComplexAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ComplexAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ComplexAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ComplexAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ComplexAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ComplexAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ComplexAudioProcessor::getProgramName (int index)
{
    return {};
}

void ComplexAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ComplexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    Initialise(sampleRate, samplesPerBlock);
    setLatencySamples(soundEngine->getProcessingDelay());
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

void ComplexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    int inputs = getTotalNumInputChannels();
    int outputs = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();
    //DBG(numSamples);

    CheckGlobalParameters();
    setLatencySamples(soundEngine->getProcessingDelay());
    Process(buffer, numSamples, inputs, outputs);
}

//==============================================================================
bool ComplexAudioProcessor::hasEditor() const
{
    // TODO: change this when you get to the editor
    return false; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ComplexAudioProcessor::createEditor()
{
    return new ComplexAudioProcessorEditor (*this);
}

//==============================================================================
void ComplexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ComplexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ComplexAudioProcessor();
}
