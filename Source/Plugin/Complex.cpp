/*
	==============================================================================

		Complex.cpp
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#include "Complex.h"
#include "Interface/Components/BaseControl.h"
#include "Renderer.h"

namespace Plugin
{
	ComplexPlugin::ComplexPlugin()
	{
		// the object self-registers in the processor tree (this) to be a unique_ptr
		// don't worry this is not a memory leak
		soundEngine_ = new Generation::SoundEngine(this);
	}

	void ComplexPlugin::Initialise(float sampleRate, u32 samplesPerBlock)
	{
		if (sampleRate != getSampleRate())
			sampleRate_.store(sampleRate, std::memory_order_release);

		if (samplesPerBlock != getSamplesPerBlock())
		{
			samplesPerBlock_.store(samplesPerBlock, std::memory_order_release);
			soundEngine_->ResetBuffers();
		}
			
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

	void ComplexPlugin::initialiseModuleTree() noexcept
	{
		// TODO: make the module structure here instead of doing it in the constructors
	}

	void ComplexPlugin::parameterChangeMidi([[maybe_unused]] u64 parentModuleId,
		[[maybe_unused]] std::string_view parameterName, [[maybe_unused]] float value)
	{
		// TODO
	}

	Interface::Renderer &ComplexPlugin::getRenderer()
	{
		if (!rendererInstance_)
			rendererInstance_ = std::make_unique<Interface::Renderer>(*this);
		
		return *rendererInstance_;
	}

	Generation::BaseProcessor *ComplexPlugin::deserialiseProcessor(
		[[maybe_unused]] ProcessorTree *processorTree, [[maybe_unused]] std::string_view processorType)
	{
		return nullptr;
	}

	void ComplexPlugin::Process(AudioBuffer<float> &buffer, u32 numSamples, float sampleRate, u32 numInputs, u32 numOutputs)
	{
		utils::ScopedSpinLock lock(waitLock_);
		soundEngine_->MainProcess(buffer, numSamples, sampleRate, numInputs, numOutputs);
	}

}

