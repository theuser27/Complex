/*
	==============================================================================

		EffectsState.h
		Created: 2 Oct 2021 8:53:05pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <thread>
#include "Framework/sync_primitives.h"
#include "Framework/simd_buffer.h"
#include "BaseProcessor.h"

namespace Generation
{
	class EffectModule;
	class EffectsState;

	class EffectsLane final : public BaseProcessor
	{
	public:
		EffectsLane() = delete;
		EffectsLane(const EffectsLane &) = delete;
		EffectsLane(EffectsLane &&) = delete;

		EffectsLane(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept;

		EffectsLane(const EffectsLane &other, u64 parentModuleId) noexcept :
			BaseProcessor(other, parentModuleId) { }
		EffectsLane &operator=(const EffectsLane &other) noexcept
		{
			BaseProcessor::operator=(other);
			return *this;
		}

		void initialise() noexcept override
		{
			BaseProcessor::initialise();
			status_.store(LaneStatus::Finished, std::memory_order_release);
			currentEffectIndex_.store(0, std::memory_order_release);
		}

		// Inherited via BaseProcessor
		bool insertSubProcessor(size_t index, BaseProcessor *newSubProcessor = nullptr) noexcept override;
		BaseProcessor *deleteSubProcessor(size_t index) noexcept override;

		auto *getEffectModule(size_t index) const noexcept { return effectModules_[index]; }

		[[nodiscard]] BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override;
		[[nodiscard]] BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept override
		{ return makeSubProcessor<EffectsLane>(*this, parentModuleId.value_or(parentProcessorId_)); }

	private:
		Framework::ComplexDataSource laneDataSource_{};
		std::vector<EffectModule *> effectModules_{};

		//// Parameters
		// 
		// 1. lane enabled
		// 2. input index
		// 3. output index
		// 4. gain match

		utils::shared_value<simd_float> volumeScale_{};
		std::atomic<u32> currentEffectIndex_ = 0;

		// Finished - finished all processing
		// Ready - ready for a thread to begin work
		// Running - processing is running
		// Stopped - temporarily stopped to wait for data from another lane
		enum class LaneStatus : u32 { Finished, Ready, Running, Stopped };

		std::atomic<LaneStatus> status_ = LaneStatus::Finished;

		friend class EffectsState;
	};

	class EffectsState final : public BaseProcessor
	{
	public:
		// data link between modules in different lanes
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
		~EffectsState() noexcept override;

		void deserialiseFromJson(std::any jsonData);

		// Inherited via BaseProcessor
		bool insertSubProcessor(size_t index, BaseProcessor *newSubProcessor) noexcept override;
		BaseProcessor *deleteSubProcessor(size_t index) noexcept override;
		[[nodiscard]] BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override;

		void writeInputData(const float *const *inputBuffer, size_t channels) noexcept;
		void processLanes() noexcept;
		void sumLanesAndWriteOutput(float *const *inputBuffer, size_t channels) noexcept;

		std::array<bool, kNumTotalChannels> getUsedInputChannels() noexcept;
		std::array<bool, kNumTotalChannels> getUsedOutputChannels() noexcept;

		auto getOutputBuffer(u32 channelCount, u32 beginChannel) const noexcept
		{ return Framework::SimdBufferView{ outputBuffer_, beginChannel, channelCount }; }

		size_t getLaneCount() const noexcept { return lanes_.size(); }
		u32 getEffectiveFFTSize() const noexcept { return binCount_; }
		float getSampleRate() const noexcept { return sampleRate_; }
		auto *getEffectsLane(size_t index) const noexcept { return lanes_[index]; }

		void setBinCount(u32 newEffectiveFFTSize) noexcept { binCount_ = newEffectiveFFTSize; }
		void setSampleRate(float newSampleRate) noexcept { sampleRate_ = newSampleRate; }

	private:
		struct Thread
		{
			Thread(EffectsState &state);
			Thread(Thread &&other) noexcept : thread(std::move(other.thread)), 
				shouldStop(other.shouldStop.load(std::memory_order_relaxed)) { }
			std::thread thread{};
			std::atomic<bool> shouldStop = false;
		};
		friend struct Thread;
		
		BaseProcessor *createCopy([[maybe_unused]] std::optional<u64> parentModuleId) const noexcept override
		{ COMPLEX_ASSERT_FALSE("You're trying to copy EffectsState, which is not meant to be copied"); return nullptr; }

		void checkUsage();
		
		void distributeWork() const noexcept;
		void processIndividualLanes(size_t laneIndex) const noexcept;

		std::vector<EffectsLane *> lanes_{};

		// current number of FFT bins (real-imaginary pairs)
		u32 binCount_ = (1 << kDefaultFFTOrder) / 2;
		float sampleRate_ = kDefaultSampleRate;

		// if an input/output isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedInputs_{};
		std::array<bool, kNumInputsOutputs> usedOutputs_{};

		Framework::SimdBuffer<Framework::complex<float>, simd_float> outputBuffer_{};

		std::vector<Thread> workerThreads_{};

		static constexpr u32 kLaneInputMask = kSignMask;
		static constexpr u32 kDefaultOutput = (u32)(-1);
	};
}
