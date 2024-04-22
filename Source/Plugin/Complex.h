/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "ProcessorTree.h"

namespace Generation
{
	class SoundEngine;
}

namespace Interface
{
	class Renderer;
}

namespace Plugin
{
	class ComplexPlugin : public ProcessorTree
	{
	public:
		ComplexPlugin();

		void Initialise(float sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(float *const *buffer, u32 numSamples, float sampleRate, u32 numInputs, u32 numOutputs);

		u32 getProcessingDelay() const noexcept;
		
		void updateParameters(UpdateFlag flag, float sampleRate) noexcept;
		void initialiseModuleTree() noexcept;

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

		auto &getSoundEngine() noexcept { return *soundEngine_; }
		u32 getFFTSize() noexcept;

		Interface::Renderer &getRenderer();

	protected:
		// pointer to the main processing engine
		Generation::SoundEngine *soundEngine_;

		std::unique_ptr<Interface::Renderer> rendererInstance_;
	};
}
