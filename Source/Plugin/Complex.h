/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "AppConfig.h"
#include <juce_audio_basics/juce_audio_basics.h>
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
		void Process(juce::AudioBuffer<float> &buffer, u32 numSamples, float sampleRate, u32 numInputs, u32 numOutputs);

		u32 getProcessingDelay() const noexcept;
		
		void updateParameters(Framework::UpdateFlag flag, float sampleRate) noexcept;
		void initialiseModuleTree() noexcept;

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

		strict_inline auto &getParameterBridges() noexcept { return parameterBridges_; }
		strict_inline auto &getParameterModulators() noexcept { return parameterModulators_; }
		strict_inline auto &getSoundEngine() noexcept { return *soundEngine_; }

		Interface::Renderer &getRenderer();

	protected:
		Generation::BaseProcessor *deserialiseProcessor(ProcessorTree *processorTree, std::string_view processorType);

		// pointer to the main processing engine
		Generation::SoundEngine *soundEngine_;

		std::vector<Framework::ParameterBridge *> parameterBridges_{};
		std::vector<Framework::ParameterModulator *> parameterModulators_{};

		std::unique_ptr<Interface::Renderer> rendererInstance_;
	};
}
