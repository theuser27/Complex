/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "./Framework/common.h"
#include "./SoundEngine/SoundEngine.h"

namespace Plugin
{
	class ComplexPlugin
	{
	public:

		ComplexPlugin();
		~ComplexPlugin();


		// pointer to the main processing engine
		std::unique_ptr<Generation::SoundEngine> soundEngine;


	public:
		void Initialise(double sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(AudioBuffer<float> &buffer, int numSamples, int numInputs, int numOutputs);

	protected:
		AudioParameterFloat *mix_;
		AudioParameterInt *order_;
		AudioParameterFloat *overlap_;

		double sampleRate_;
		u32 samplesPerBlock_;
	};
}
