/*
	==============================================================================

		EffectsState.h
		Created: 2 Oct 2021 8:53:05pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <thread>
#include "EffectModules.h"
#include "./Framework/simd_buffer.h"

namespace Generation
{
	class EffectsState;

	struct EffectsChainData
	{
		EffectsChainData() noexcept
		{
			// currently buffers will only process a single complex input
			// size is half the max because a single SIMD package stores both real and imaginary parts
			sourceBuffer.reserve(kNumChannels, kMaxFFTBufferLength);
			workBuffer.reserve(kNumChannels, kMaxFFTBufferLength);
		}

		// is the work buffer in cartesian or polar representation
		bool isCartesian = true;
		// flag for keeping track in which buffer the latest data is
		bool dataIsInWork = false;

		// intermediate buffer used for dry/wet mixing per effect
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer;
		// main buffer for processing
		Framework::SimdBuffer<std::complex<float>, simd_float> workBuffer;
	};

	class EffectsChain : public PluginModule
	{
	public:
		EffectsChain() = delete;
		EffectsChain(const EffectsChain &) = delete;
		EffectsChain(EffectsChain &&) = delete;

		EffectsChain(AllModules *globalModulesState, u64 parentModuleId) noexcept : 
			PluginModule(globalModulesState, parentModuleId, Framework::kPluginModules[2])
		{
			chainData = std::make_unique<EffectsChainData>();

			subModules_.reserve(kInitialNumEffects);
			insertSubModule(0, Framework::kEffectModuleNames[1]);

			moduleParameters_.data.reserve(Framework::effectChainParameterList.size());
			createModuleParameters(Framework::effectChainParameterList.data(), Framework::effectChainParameterList.size());
		}

		~EffectsChain() noexcept { PluginModule::~PluginModule(); }

		EffectsChain(const EffectsChain &other, u64 parentModuleId) noexcept : PluginModule(other, parentModuleId)
		{
			COMPLEX_ASSERT(moduleType_ == Framework::kPluginModules[2]);
			chainData = std::make_unique<EffectsChainData>();
		}

		EffectsChain &operator=(const EffectsChain &other) noexcept
		{ PluginModule::operator=(other); return *this; }

		void initialise() noexcept override
		{
			PluginModule::initialise();
			isFinished_.store(true, std::memory_order_release);
			isStopped_.store(false, std::memory_order_release);
			currentEffectIndex_.store(0, std::memory_order_release);
		}

		// Inherited via PluginModule
		bool insertSubModule(u32 index, std::string_view moduleType) noexcept override;
		bool deleteSubModule([[maybe_unused]] u32 index) noexcept override;
		bool copySubModule(const std::shared_ptr<PluginModule> &newSubModule, u32 index) noexcept override;
		bool moveSubModule(std::shared_ptr<PluginModule> newSubModule, u32 index) noexcept override;

	private:
		std::unique_ptr<EffectsChainData> chainData;

		//// Parameters
		// 
		// 1. chain enabled
		// 2. input index
		// 3. output index
		// 4. gain match

		simd_float inputVolume_, outputVolume_;
		std::atomic<u32> currentEffectIndex_ = 0;

		// has this chain been stopped temporarily?
		std::atomic<bool> isStopped_ = false;
		// has this chain finished all processing?
		std::atomic<bool> isFinished_ = true;

		friend class EffectsState;
	};

	class EffectsState : public PluginModule
	{
	public:
		// data link between modules in different chains
		struct EffectsModuleLink
		{
			bool checkForFeedback();

			std::pair<u32, u32> sourceIndex, destinationIndex;
		};

		EffectsState() = delete;
		EffectsState(const EffectsState &) = delete;
		EffectsState(EffectsState &&) = delete;
		EffectsState &operator=(const EffectsState &other) = delete;
		EffectsState &operator=(EffectsState &&other) = delete;

		EffectsState(AllModules *globalModulesState, u64 parentModuleId) noexcept : 
			PluginModule(globalModulesState, parentModuleId, Framework::kPluginModules[1])
		{
			subModules_.reserve(kMaxNumChains);
			// size is half the max because a single SIMD package stores both real and imaginary parts
			sourceBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
			chainThreads_.reserve(kMaxNumChains);
			
			insertSubModule(subModules_.size(), Framework::kPluginModules[2]);
		}

		~EffectsState() noexcept
		{
			for (size_t i = 0; i < chainThreads_.size(); i++)
				chainThreads_[i].~jthread();
			PluginModule::~PluginModule();
		}

		void writeInputData(const AudioBuffer<float> &inputBuffer) noexcept;
		void distributeData() noexcept;
		void processChains() noexcept;
		void sumChains() noexcept;
		void writeOutputData(AudioBuffer<float> &outputBuffer) noexcept;


		// Inherited via PluginModule
		bool insertSubModule([[maybe_unused]] u32 index, [[maybe_unused]] std::string_view moduleType) noexcept override;
		bool deleteSubModule(u32 index) noexcept override;
		bool copySubModule(const std::shared_ptr<PluginModule> &newSubModule, [[maybe_unused]] u32 index) noexcept override;

		auto getUsedInputChannels() noexcept
		{
			for (size_t i = 0; i < subModules_.size(); i++)
			{
				auto chain = static_cast<EffectsChain *>(subModules_[i].get());

				// if the input is not another chain's output and the chain is enabled
				if (auto chainInput = chain->moduleParameters_[1]->getInternalValue<u32>();
					((chainInput & kChainInputMask) == 0) && chain->moduleParameters_[0]->getInternalValue<u32>())
					usedInputs_[chainInput] = true;
			}

			std::array<bool, kNumTotalChannels> usedInputChannels{};
			for (size_t i = 0; i < usedInputChannels.size(); i++)
				usedInputChannels[i] = usedInputs_[i / kComplexSimdRatio];

			return usedInputChannels;
		}

		auto getUsedOutputChannels() noexcept
		{
			for (size_t i = 0; i < subModules_.size(); i++)
			{
				auto chain = static_cast<EffectsChain *>(subModules_[i].get());

				// if the output is not defaulted and the chain is enabled
				if (auto chainOutput = chain->moduleParameters_[2]->getInternalValue<u32>();
					(chainOutput != kDefaultOutput) && chain->moduleParameters_[0]->getInternalValue<u32>())
					usedOutputs_[chainOutput] = true;
			}

			std::array<bool, kNumTotalChannels> usedOutputChannels{};
			for (size_t i = 0; i < usedOutputChannels.size(); i++)
				usedOutputChannels[i] = usedOutputs_[i / kComplexSimdRatio];

			return usedOutputChannels;
		}

		size_t getNumChains() const noexcept { return subModules_.size(); }
		u32 getFFTSize() const noexcept { return FFTSize_; }
		float getSampleRate() const noexcept { return sampleRate_; }

		void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }
		void setSampleRate(float newSampleRate) noexcept { sampleRate_ = newSampleRate; }

	private:
		static void processIndividualChains(std::stop_token stoken, EffectsState &state, u32 chainIndex) noexcept;

		// main buffer to store every FFT-ed input
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer_;
		// current FFT process size
		u32 FFTSize_ = 1 << kDefaultFFTOrder;
		float sampleRate_ = kDefaultSampleRate;

		// if an input/output isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedInputs_{};
		std::array<bool, kNumInputsOutputs> usedOutputs_{};

		std::vector<std::jthread> chainThreads_{};

		static constexpr u32 kChainInputMask = kSignMask;
		static constexpr u32 kDefaultOutput = (u32)(-1);
	};
}