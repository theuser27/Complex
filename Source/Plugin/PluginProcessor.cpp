/*
	==============================================================================

		This file contains the basic framework code for a JUCE plugin processor.

	==============================================================================
*/

#include "Framework/parameter_value.h"
#include "Framework/parameter_bridge.h"
#include "Generation/SoundEngine.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Renderer.h"

//==============================================================================
ComplexAudioProcessor::ComplexAudioProcessor() :
	AudioProcessor (BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
																	 .withOutput("Output", juce::AudioChannelSet::stereo(), true))

{
	using namespace Framework;

	static_assert(kMaxParameterMappings >= 64, "If you want to lower the number of available parameter mappings, \
		this might break compatibility with save files inside projects and presets. Comment out this assert if you're aware of the risk.");

	constexpr auto pluginParameterStrings = BaseProcessors::SoundEngine::enum_names<nested_enum::OuterNodes>(false);
	parameterBridges_.reserve(kMaxParameterMappings + pluginParameterStrings.size());

	for (std::string_view parameterName : pluginParameterStrings)
	{
		if (auto *parameter = getProcessorParameter(soundEngine_->getProcessorId(), parameterName))
		{
			auto *bridge = new ParameterBridge(this, UINT64_MAX, parameter->getParameterLink());
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

	utils::loadLibraries();
}

ComplexAudioProcessor::~ComplexAudioProcessor()
{
	utils::unloadLibraries();
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

void ComplexAudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	auto inputs = getTotalNumInputChannels();
	auto outputs = getTotalNumOutputChannels();
	auto numSamples = buffer.getNumSamples();

	auto sampleRate = ComplexPlugin::getSampleRate();
	updateParameters(UpdateFlag::BeforeProcess, sampleRate);
	setLatencySamples((int)getProcessingDelay());

	Process(buffer.getArrayOfWritePointers(), (u32)numSamples, sampleRate, (u32)inputs, (u32)outputs);

	updateParameters(UpdateFlag::AfterProcess, sampleRate);
}

//==============================================================================
bool ComplexAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ComplexAudioProcessor::createEditor()
{
	auto *editor = new ComplexAudioProcessorEditor(*this);
	rendererInstance_->setEditor(editor);
	return editor;
}

//==============================================================================


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ComplexAudioProcessor(); }