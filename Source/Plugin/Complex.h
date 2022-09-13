/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "./Framework/common.h"
#include "./SoundEngine/SoundEngine.h"
#include "./Framework/parameter_value.h"

namespace Plugin
{
	class ComplexPlugin
	{
	public:

		ComplexPlugin();
		~ComplexPlugin() = default;

		// pointer to the main processing engine
		std::shared_ptr<Generation::SoundEngine> soundEngine;

		void Initialise(float sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(AudioBuffer<float> &buffer, u32 numSamples, u32 numInputs, u32 numOutputs);

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);
	};
}
