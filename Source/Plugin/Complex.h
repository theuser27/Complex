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
	class ComplexPlugin : public Generation::AllModules
	{
	public:
		ComplexPlugin();
		~ComplexPlugin() = default;

		void Initialise(float sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(AudioBuffer<float> &buffer, u32 numSamples, u32 numInputs, u32 numOutputs);

		u32 getProcessingDelay() { return soundEngine->getProcessingDelay(); }
		void updateMainParameters() { soundEngine->UpdateParameters(UpdateFlag::BeforeProcess); }

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

	protected:
		// pointer to the main processing engine
		std::shared_ptr<Generation::SoundEngine> soundEngine;
	};
}
