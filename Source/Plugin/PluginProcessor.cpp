/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.hpp"

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Framework/load_save.hpp"
#include "Generation/SoundEngine.hpp"
#include "PluginEditor.hpp"
#include "Renderer.hpp"

ComplexAudioProcessor::ComplexAudioProcessor(usize parameterMappings, u32 inSidechains, u32 outSidechains, usize undoSteps) :
  ComplexPlugin{ inSidechains, outSidechains, undoSteps }, AudioProcessor{ [&]()
    {
      BusesProperties buses{};

      buses.addBus(true, "Input", juce::AudioChannelSet::stereo(), true);
      buses.addBus(false, "Output", juce::AudioChannelSet::stereo(), true);

      if (!juce::JUCEApplicationBase::isStandaloneApp())
      {
        for (usize i = 0; i < inSidechains; ++i)
          buses.addBus(true, juce::String{ "Sidechain In " } + juce::String{ i + 1 }, juce::AudioChannelSet::stereo(), true);
        
        for (usize i = 0; i < outSidechains; ++i)
          buses.addBus(false, juce::String{ "Sidechain Out " } + juce::String{ i + 1 }, juce::AudioChannelSet::stereo(), true);
      }

      return buses;
    }()
  }

{
  parameterBridges_.reserve(parameterMappings);
  for (u64 i = 0; i < parameterMappings; ++i)
  {
    auto *bridge = new Framework::ParameterBridge{ this, i };
    parameterBridges_.push_back(bridge);
    addParameter(bridge);
  }
}

ComplexAudioProcessor::~ComplexAudioProcessor()
{
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
  if (!isLoaded_.load(std::memory_order_acquire))
    return;

  initialise((float)sampleRate, samplesPerBlock);
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
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

void ComplexAudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{
  juce::ScopedNoDenormals noDenormals;

  auto inputs = getTotalNumInputChannels();
  auto outputs = getTotalNumOutputChannels();
  auto numSamples = buffer.getNumSamples();

  float sampleRate = ComplexPlugin::getSampleRate();
  updateParameters(UpdateFlag::BeforeProcess, sampleRate);
  setLatencySamples((int)getProcessingDelay());

  process(buffer.getArrayOfWritePointers(), (u32)numSamples, sampleRate, (u32)inputs, (u32)outputs);

  updateParameters(UpdateFlag::AfterProcess, sampleRate);
}

//==============================================================================
bool ComplexAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ComplexAudioProcessor::createEditor()
{
  auto *editor = new ComplexAudioProcessorEditor{ *this };
  getRenderer().setEditor(editor);
  return editor;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  usize parameterMappings, inSidechains, outSidechains, undoSteps;
  Framework::LoadSave::getStartupParameters(parameterMappings, inSidechains, outSidechains, undoSteps);
  return new ComplexAudioProcessor{ parameterMappings, (u32)inSidechains, (u32)outSidechains, undoSteps };
}