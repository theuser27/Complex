/*
	==============================================================================

		Complex.cpp
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#include "Complex.h"

namespace Plugin
{
	ComplexPlugin::ComplexPlugin() : soundEngine(std::make_shared<Generation::SoundEngine>())
	{
		Generation::PluginModule::AllModules::addModule(soundEngine);
	}

	void ComplexPlugin::Initialise(float sampleRate, u32 samplesPerBlock)
	{
		if (sampleRate != RuntimeInfo::sampleRate.load(std::memory_order_acquire))
			RuntimeInfo::sampleRate.store(sampleRate, std::memory_order_release);

		if (samplesPerBlock != RuntimeInfo::samplesPerBlock.load(std::memory_order_acquire))
			RuntimeInfo::samplesPerBlock.store(samplesPerBlock, std::memory_order_release);

		soundEngine->Initialise(sampleRate, samplesPerBlock);
	}

	// IMPORTANT !!!
	// TODO: repurpose function for updating the plugin structure AFTER audio callback 
	// (i.e. adding new modules/new parameter maps, etc.)
	// because the audio callback is timed we have an eternity to set stuff up
	void ComplexPlugin::CheckGlobalParameters()
	{
		// TODO: do parameter checking
		// check for mix
			// getting info from parameter
			//// be careful with edge cases where the mix is at 0% but an FFT block is already underway
			//// in that case, just window the original input block and paste it in the output buffer
			//// TODO: add option in advanced settings to still FFT to display the spectrogram
		// check for FFT plan
			// getting info from parameter
		// check for window
			// get type
			// get alpha if necessary
		// check for overlap
			//// do a check in the gui section whether the control is between 0% and 98.45% or at 100%
			//// do a check in the gui if the window type is rectangular, if yes overlap = 0%
		// check for power matching
	}

	void ComplexPlugin::parameterChangeMidi([[maybe_unused]] u64 parentModuleId, 
		[[maybe_unused]] std::string_view parameterName, [[maybe_unused]] float value)
	{
		// TODO
	}

	void ComplexPlugin::Process(AudioBuffer<float> &buffer, u32 numSamples, u32 numInputs, u32 numOutputs)
	{
		soundEngine->MainProcess(buffer, numSamples, numInputs, numOutputs);
	}

}

