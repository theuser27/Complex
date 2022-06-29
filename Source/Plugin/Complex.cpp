/*
	==============================================================================

		Complex.cpp
		Created: 23 May 2021 12:20:15am
		Author:  Lenovo

	==============================================================================
*/

#include "Complex.h"

namespace Plugin
{
	ComplexPlugin::ComplexPlugin()
	{
		soundEngine = std::make_unique<Generation::SoundEngine>();
	}
	ComplexPlugin::~ComplexPlugin() = default;

	void ComplexPlugin::Initialise(double sampleRate, u32 samplesPerBlock)
	{
		soundEngine->Initialise(sampleRate, samplesPerBlock);
	}

	void ComplexPlugin::CheckGlobalParameters()
	{
		// temporary setters
		soundEngine->setMix(mix_->get());
		soundEngine->setWindowType(Framework::WindowTypes::Hann);
		soundEngine->setFFTOrder(order_->get());
		soundEngine->setOverlap(overlap_->get());

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

	void ComplexPlugin::Process(AudioBuffer<float> &buffer, int numSamples, int numInputs, int numOutputs)
	{
		soundEngine->MainProcess(buffer, numSamples, numInputs, numOutputs);
	}
}

