/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Framework/parameter_bridge.h"
#include "Generation/SoundEngine.h"
#include "ProcessorTree.h"

namespace Plugin
{
	class ComplexPlugin : public ProcessorTree
	{
	public:
		ComplexPlugin();

		void Initialise(float sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(AudioBuffer<float> &buffer, u32 numSamples, float sampleRate, u32 numInputs, u32 numOutputs);

		strict_inline u32 getProcessingDelay() const noexcept { return soundEngine_->getProcessingDelay(); }
		
		strict_inline void updateParameters(Framework::UpdateFlag flag, float sampleRate) noexcept
		{ soundEngine_->UpdateParameters(flag, sampleRate); }
		void initialiseModuleTree() noexcept;

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

		strict_inline auto &getParameterBridges() noexcept { return parameterBridges_; }
		strict_inline auto &getParameterModulators() noexcept { return parameterModulators_; }
		strict_inline auto &getSoundEngine() noexcept { return *soundEngine_; }

	protected:
		Generation::BaseProcessor *deserialiseProcessor(ProcessorTree *processorTree, std::string_view processorType);

		// pointer to the main processing engine
		Generation::SoundEngine *soundEngine_;

		std::vector<Framework::ParameterBridge *> parameterBridges_{};
		std::vector<Framework::ParameterModulator *> parameterModulators_{};
	};
}
