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
#include "Framework/simd_buffer.h"

namespace Generation
{
	class EffectsState;

	class EffectsLane : public BaseProcessor
	{
	public:
		EffectsLane() = delete;
		EffectsLane(const EffectsLane &) = delete;
		EffectsLane(EffectsLane &&) = delete;

		EffectsLane(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept;

		EffectsLane(const EffectsLane &other, u64 parentModuleId) noexcept;
		EffectsLane &operator=(const EffectsLane &other) noexcept
		{
			BaseProcessor::operator=(other);
			return *this;
		}

		~EffectsLane() noexcept override { BaseProcessor::~BaseProcessor(); }

		void initialise() noexcept override
		{
			BaseProcessor::initialise();
			isFinished_.store(true, std::memory_order_release);
			isStopped_.store(false, std::memory_order_release);
			currentEffectIndex_.store(0, std::memory_order_release);
		}

		// Inherited via BaseProcessor
		void insertSubProcessor(u32 index, std::string_view newModuleType = {}, BaseProcessor *newSubModule = nullptr) noexcept override;
		BaseProcessor *deleteSubProcessor(u32 index) noexcept override;
		BaseProcessor *updateSubProcessor(u32 index, std::string_view newModuleType = {}, BaseProcessor *newSubModule = nullptr) noexcept override;
		[[nodiscard]] BaseProcessor *createCopy(u64 parentModuleId) const noexcept override
		{ return createSubProcessor<EffectsLane>(*this, parentModuleId); }

	private:
		std::vector<EffectModule *> effectModules_{};
		ComplexDataSource chainDataSource_{};

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

	class EffectsState : public BaseProcessor
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

		EffectsState(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept;

		~EffectsState() noexcept override
		{
			for (auto &thread : chainThreads_)
				thread.~jthread();
			BaseProcessor::~BaseProcessor();
		}

		void writeInputData(const AudioBuffer<float> &inputBuffer) noexcept;
		void processChains() noexcept;
		void sumChains() noexcept;
		void writeOutputData(AudioBuffer<float> &outputBuffer) const noexcept;


		// Inherited via BaseProcessor
		void insertSubProcessor(u32 index, std::string_view newModuleType = {}, BaseProcessor *newSubModule = nullptr) noexcept override;
		BaseProcessor *deleteSubProcessor(u32 index) noexcept override;
		BaseProcessor *createCopy(u64 parentModuleId) const noexcept override
		{ COMPLEX_ASSERT(false && "You're trying to copy EffectsState, which is not meant to be copied"); return nullptr; }

		auto getUsedInputChannels() noexcept
		{
			for (auto &chain : chains_)
			{
				// if the input is not another chain's output and the chain is enabled
				if (auto chainInput = chain->processorParameters_[1]->getInternalValue<u32>();
					((chainInput & kChainInputMask) == 0) && chain->processorParameters_[0]->getInternalValue<u32>())
					usedInputs_[chainInput] = true;
			}

			std::array<bool, kNumTotalChannels> usedInputChannels{};
			for (size_t i = 0; i < usedInputChannels.size(); i++)
				usedInputChannels[i] = usedInputs_[i / kComplexSimdRatio];

			return usedInputChannels;
		}

		auto getUsedOutputChannels() noexcept
		{
			for (auto &chain : chains_)
			{
				// if the output is not defaulted and the chain is enabled
				if (auto chainOutput = chain->processorParameters_[2]->getInternalValue<u32>();
					(chainOutput != kDefaultOutput) && chain->processorParameters_[0]->getInternalValue<u32>())
					usedOutputs_[chainOutput] = true;
			}

			std::array<bool, kNumTotalChannels> usedOutputChannels{};
			for (size_t i = 0; i < usedOutputChannels.size(); i++)
				usedOutputChannels[i] = usedOutputs_[i / kComplexSimdRatio];

			return usedOutputChannels;
		}

		size_t getNumChains() const noexcept { return chains_.size(); }
		u32 getEffectiveFFTSize() const noexcept { return effectiveFFTSize_; }
		float getSampleRate() const noexcept { return sampleRate_; }

		void setFFTSize(u32 newEffectiveFFTSize) noexcept { effectiveFFTSize_ = newEffectiveFFTSize; }
		void setSampleRate(float newSampleRate) noexcept { sampleRate_ = newSampleRate; }

	private:
		void processIndividualChains(std::stop_token stoken, u32 chainIndex) noexcept;

		std::vector<EffectsLane *> chains_{};

		// current FFT process size
		u32 effectiveFFTSize_ = (1 << kDefaultFFTOrder) / 2;
		float sampleRate_ = kDefaultSampleRate;

		// if an input/output isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedInputs_{};
		std::array<bool, kNumInputsOutputs> usedOutputs_{};

		std::vector<std::jthread> chainThreads_{};

		static constexpr u32 kChainInputMask = kSignMask;
		static constexpr u32 kDefaultOutput = (u32)(-1);
	};
}

REFL_AUTO(type(Generation::EffectsLane, bases<Generation::BaseProcessor>))
REFL_AUTO(type(Generation::EffectsState, bases<Generation::BaseProcessor>))
