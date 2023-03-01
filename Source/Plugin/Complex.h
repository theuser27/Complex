/*
	==============================================================================

		Complex.h
		Created: 23 May 2021 12:20:15am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Generation/SoundEngine.h"
#include "Framework/parameter_bridge.h"

namespace Plugin
{
	class ComplexPlugin : public ProcessorTree
	{
	public:
		ComplexPlugin();
		~ComplexPlugin() override = default;

		void Initialise(float sampleRate, u32 samplesPerBlock);
		void CheckGlobalParameters();
		void Process(AudioBuffer<float> &buffer, u32 numSamples, u32 numInputs, u32 numOutputs);

		strict_inline u32 getProcessingDelay() const noexcept { return soundEngine->getProcessingDelay(); }
		
		void updateParameters(UpdateFlag flag) noexcept { soundEngine->UpdateParameters(flag); }
		void initialiseModuleTree() noexcept;

		virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

		strict_inline auto &getParameterBridges() noexcept { return parameterBridges_; }
		strict_inline auto &getParameterModulators() noexcept { return parameterModulators_; }

		strict_inline auto getSampleRate() const noexcept { return sampleRate_.load(std::memory_order_acquire); }
		strict_inline auto getSamplesPerBlock() const noexcept { return samplesPerBlock_.load(std::memory_order_acquire); }

	protected:
		// pointer to the main processing engine
		Generation::SoundEngine *soundEngine;

		std::vector<Framework::ParameterBridge *> parameterBridges_{};
		std::vector<Framework::ParameterModulator *> parameterModulators_{};
		juce::UndoManager undoManager{ 0, 100 };

		std::atomic<float> sampleRate_ = kDefaultSampleRate;
		std::atomic<u32> samplesPerBlock_ = 256;
	};
}
