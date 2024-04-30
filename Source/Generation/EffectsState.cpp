/*
	==============================================================================

		EffectsState.cpp
		Created: 2 Oct 2021 8:53:05pm
		Author:  theuser27

	==============================================================================
*/

#include "EffectsState.h"

#include "Framework/spectral_support_functions.h"
#include "Framework/simd_utils.h"
#include "Framework/parameter_value.h"
#include "EffectModules.h"

namespace Generation
{
	EffectsLane::EffectsLane(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept :
		BaseProcessor(moduleTree, parentModuleId, Framework::BaseProcessors::EffectsLane::id().value())
	{
		subProcessors_.reserve(kInitialEffectCount);
		dataBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		insertSubProcessor(0, makeSubProcessor<EffectModule>(Framework::BaseProcessors::BaseEffect::Dynamics::id().value()));

		createProcessorParameters(Framework::BaseProcessors::EffectsLane::enum_names<nested_enum::OuterNodes>());
	}

	bool EffectsLane::insertSubProcessor(size_t index, BaseProcessor *newSubProcessor) noexcept
	{
		COMPLEX_ASSERT(newSubProcessor && "A valid BaseProcessor needs to be passed in");
		COMPLEX_ASSERT(newSubProcessor->getProcessorType() == Framework::BaseProcessors::EffectModule::id().value()
			&& "You're trying to move a non-EffectModule into EffectChain");

		effectModules_.insert(effectModules_.begin() + (std::ptrdiff_t)index, static_cast<EffectModule *>(newSubProcessor));
		subProcessors_.insert(subProcessors_.begin() + (std::ptrdiff_t)index, newSubProcessor);

		for (auto *listener : listeners_)
			listener->insertedSubProcessor(index, newSubProcessor);

		return true;
	}

	BaseProcessor *EffectsLane::deleteSubProcessor(size_t index) noexcept
	{
		COMPLEX_ASSERT(index < subProcessors_.size());

		BaseProcessor *deletedModule = subProcessors_[index];
		effectModules_.erase(effectModules_.begin() + index);
		subProcessors_.erase(subProcessors_.begin() + index);

		for (auto *listener : listeners_)
			listener->deletedSubProcessor(index, deletedModule);

		return deletedModule;
	}

	BaseProcessor *EffectsLane::createSubProcessor(std::string_view type) const noexcept
	{
		COMPLEX_ASSERT(Framework::BaseProcessors::BaseEffect::enum_value_by_id(type).has_value() &&
			"You're trying to create a subProcessor for EffectsLane, that isn't EffectModule");
		return makeSubProcessor<EffectModule>(type);
	}

	EffectsState::EffectsState(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept :
		BaseProcessor(moduleTree, parentModuleId, Framework::BaseProcessors::EffectsState::id().value())
	{
		static_assert(kMaxNumLanes > 0, "No lanes?");

		lanes_.reserve(kMaxNumLanes);
		subProcessors_.reserve(kMaxNumLanes);
		// size is half the max because a single SIMD package stores both real and imaginary parts
		dataBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		outputBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		workerThreads_.reserve(kMaxNumLanes);

		insertSubProcessor(0, makeSubProcessor<EffectsLane>());
	}

	EffectsState::~EffectsState() noexcept
	{
		for (auto &thread : workerThreads_)
			thread.thread.join();

		BaseProcessor::~BaseProcessor();
	}

	bool EffectsState::insertSubProcessor([[maybe_unused]] size_t index, BaseProcessor *newSubProcessor) noexcept
	{
		COMPLEX_ASSERT(newSubProcessor && "A valid BaseProcessor needs to be passed in");
		COMPLEX_ASSERT(newSubProcessor->getProcessorType() == Framework::BaseProcessors::EffectsLane::id().value() &&
			"You're trying to insert a non-EffectChain into EffectsState");

		// have we reached the max chain capacity?
		if (lanes_.size() >= kMaxNumLanes)
			return false;

		lanes_.emplace_back(utils::as<EffectsLane>(newSubProcessor));
		subProcessors_.emplace_back(newSubProcessor);

		for (auto *listener : listeners_)
			listener->insertedSubProcessor(index, newSubProcessor);

		return true;
	}

	BaseProcessor *EffectsState::deleteSubProcessor(size_t index) noexcept
	{
		COMPLEX_ASSERT(index < subProcessors_.size());

		BaseProcessor *deletedModule = lanes_[index];
		lanes_.erase(lanes_.begin() + index);
		subProcessors_.erase(subProcessors_.begin() + index);

		for (auto *listener : listeners_)
			listener->deletedSubProcessor(index, deletedModule);

		return deletedModule;
	}

	BaseProcessor *EffectsState::createSubProcessor([[maybe_unused]] std::string_view type) const noexcept
	{
		COMPLEX_ASSERT(type == Framework::BaseProcessors::EffectsLane::id().value() &&
			"You're trying to create a subProcessor for EffectsState, that isn't EffectsLane");
		return makeSubProcessor<EffectsLane>();
	}

	std::array<bool, kNumTotalChannels> EffectsState::getUsedInputChannels() noexcept
	{
		for (auto &lane : lanes_)
		{
			// if the input is not another chain's output and the chain is enabled
			if (auto laneInput = lane->processorParameters_[1]->getInternalValue<u32>(getSampleRate());
				((laneInput & kLaneInputMask) == 0) && lane->processorParameters_[0]->getInternalValue<u32>(getSampleRate()))
				usedInputs_[laneInput] = true;
		}

		std::array<bool, kNumTotalChannels> usedInputChannels{};
		for (size_t i = 0; i < usedInputChannels.size(); i++)
			usedInputChannels[i] = usedInputs_[i / kComplexSimdRatio];

		return usedInputChannels;
	}

	std::array<bool, kNumTotalChannels> EffectsState::getUsedOutputChannels() noexcept
	{
		for (auto &lane : lanes_)
		{
			// if the output is not defaulted and the chain is enabled
			if (auto laneOutput = lane->processorParameters_[2]->getInternalValue<u32>(getSampleRate());
				(laneOutput != kDefaultOutput) && lane->processorParameters_[0]->getInternalValue<u32>(getSampleRate()))
				usedOutputs_[laneOutput] = true;
		}

		std::array<bool, kNumTotalChannels> usedOutputChannels{};
		for (size_t i = 0; i < usedOutputChannels.size(); i++)
			usedOutputChannels[i] = usedOutputs_[i / kComplexSimdRatio];

		return usedOutputChannels;
	}

	void EffectsState::writeInputData(const float *const *inputBuffer, size_t channels) noexcept
	{
		COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);
		utils::ScopedLock g{ dataBuffer_.getLock(), true, utils::WaitMechanism::Spin };

		std::array<simd_float, kComplexSimdRatio> values;
		for (u32 i = 0; i < (u32)channels; i += kComplexSimdRatio)
		{
			// if the input is not used we skip it
			if (!usedInputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < binCount_; j++)
			{
				// skipping every second sample (complex signal) and every second complex pair (simd_float can take 2 pairs)
				for (u32 k = 0; k < values.size(); ++k)
					values[k] = utils::toSimdFloatFromUnaligned(inputBuffer[k] + j * 2 * kComplexSimdRatio);

				utils::complexTranspose(values);

				for (u32 k = 0; k < kComplexSimdRatio; ++k)
					dataBuffer_.writeSimdValueAt(values[k], i, j * kComplexSimdRatio + k);
			}
		}
	}

	void EffectsState::processLanes() noexcept
	{
		// sequential consistency just in case
		// triggers the chains to run again
		for (auto &lane : lanes_)
			lane->status_.store(EffectsLane::LaneStatus::Ready, std::memory_order_seq_cst);

		distributeWork();

		// waiting for chains to finish
		for (auto &lane : lanes_)
			while (lane->status_.load(std::memory_order_acquire) != EffectsLane::LaneStatus::Finished) { utils::longPause(5); }
	}

	EffectsState::Thread::Thread(EffectsState &state) : thread([this, &state]()
		{
			while (true)
			{
				if (shouldStop.load(std::memory_order_acquire))
					return;

				state.distributeWork();
			}
		}) { }

	void EffectsState::checkUsage()
	{
		// TODO: decide on a heuristic when to add worker threads
		//workerThreads_.emplace_back(*this);
	}

	void EffectsState::distributeWork() const noexcept
	{
		for (size_t i = 0; i < lanes_.size(); ++i)
		{
			if (lanes_[i]->status_.load(std::memory_order_acquire) == EffectsLane::LaneStatus::Ready)
			{
				auto expected = EffectsLane::LaneStatus::Ready;
				if (lanes_[i]->status_.compare_exchange_strong(expected, EffectsLane::LaneStatus::Running,
					std::memory_order_acq_rel, std::memory_order_relaxed))
					processIndividualLanes(i);
			}
		}
	}

	void EffectsState::processIndividualLanes(size_t laneIndex) const noexcept
	{
		using namespace Framework;

		auto thisLane = lanes_[laneIndex];

		thisLane->currentEffectIndex_.store(0, std::memory_order_release);
		thisLane->volumeScale_ = 1.0f;
		auto &laneDataSource = thisLane->laneDataSource_;
		bool isLaneOn = thisLane->getParameter(BaseProcessors::EffectsLane::LaneEnabled::name())
			->getInternalValue<u32>(getSampleRate());

		// Lane Input 
		// if this lane's input is another's output and that lane can be used,
		// we wait until it is finished and then copy its data
		if (auto inputIndex = thisLane->getParameter(BaseProcessors::EffectsLane::Input::name())
			->getInternalValue<u32>(getSampleRate()); inputIndex & kLaneInputMask)
		{
			inputIndex ^= kLaneInputMask;
			while (true)
			{
				auto otherLaneStatus = lanes_[inputIndex]->status_.load(std::memory_order_acquire);
				if (otherLaneStatus == EffectsLane::LaneStatus::Finished)
					break;

				utils::longPause(40);
			}

			// getting shared access to the lane's output
			// if this lane is turned off, we only grab the view from the other lane's buffer
			// since we won't be reading from it but it's necessary for the outputBuffer to know where the data is
			if (!isLaneOn)
			{
				// TODO: decide if it's even worth to wait, or we can grab a reference to the other lane's SimdBufferView
				laneDataSource.sourceBuffer = lanes_[inputIndex]->laneDataSource_.sourceBuffer;
				laneDataSource.dataType = lanes_[inputIndex]->laneDataSource_.dataType;

				thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
				return;
			}

			utils::lockAtomic(lanes_[inputIndex]->laneDataSource_.sourceBuffer.getLock(), false, utils::WaitMechanism::Spin);
			laneDataSource.sourceBuffer = lanes_[inputIndex]->laneDataSource_.sourceBuffer;
			laneDataSource.dataType = lanes_[inputIndex]->laneDataSource_.dataType;
		}
		// input is not from a lane, we can begin processing
		else
		{
			// if this lane is turned off, we set it as finished and grab view to the original dataBuffer's data
			if (!isLaneOn)
			{
				laneDataSource.sourceBuffer = SimdBufferView{ dataBuffer_, inputIndex * kNumChannels, kNumChannels };
				laneDataSource.dataType = ComplexDataSource::Cartesian;
				
				thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
				return;
			}
			
			// getting shared access to the state's transformed data
			COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);
			utils::lockAtomic(dataBuffer_.getLock(), false, utils::WaitMechanism::Spin);
			laneDataSource.sourceBuffer = SimdBufferView{ dataBuffer_, inputIndex * kNumChannels, kNumChannels };
			laneDataSource.dataType = ComplexDataSource::Cartesian;
		}

		simd_float inputVolume = 0.0f;
		simd_float loudnessScale = 1.0f / (float)binCount_;
		u32 isGainMatching = thisLane->getParameter(BaseProcessors::EffectsLane::GainMatching::name())->getInternalValue<u32>();

		auto getLoudness = [](const ComplexDataSource &laneDataSource, simd_float loudnessScale, u32 binCount)
		{
			simd_float loudness = 0.0f;
			// this is because we're squaring and then scaling inside the loops
			loudnessScale *= loudnessScale;
			auto data = laneDataSource.sourceBuffer.getData();
			if (laneDataSource.dataType == ComplexDataSource::Cartesian)
			{
				for (u32 i = 0; i < binCount; i += kComplexSimdRatio)
				{
					// magnitudes are: [left channel, right channel, left channel + 1, right channel + 1]
					simd_float values = utils::complexMagnitude({ data[i], data[i + 1] }, false) / loudnessScale;
					// [left channel,     left channel + 1, right channel,     right channel + 1] + 
					// [left channel + 1, left channel,     right channel + 1, right channel    ]
					loudness += utils::groupEven(values) + utils::groupEvenReverse(values);
				}
			}
			else if (laneDataSource.dataType == ComplexDataSource::Polar)
			{
				for (u32 i = 0; i < binCount; ++i)
				{
					simd_float values = data[i];
					loudness += values * values / loudnessScale;
				}

				loudness = utils::copyFromEven(loudness);
			}
			return loudness;
		};

		if (isGainMatching)
		{
			inputVolume = getLoudness(laneDataSource, loudnessScale, binCount_);
			inputVolume = utils::maskLoad(inputVolume, simd_float(1.0f), simd_float::equal(inputVolume, 0.0f));
		}
		else
			thisLane->volumeScale_ = 1.0f;

		// main processing loop
		for (auto effectModule : thisLane->subProcessors_)
		{
			// this is safe because by design only EffectModules are contained in a lane
			utils::as<EffectModule>(effectModule)->processEffect(laneDataSource, binCount_, getSampleRate());

			// incrementing where we are currently
			thisLane->currentEffectIndex_.fetch_add(1, std::memory_order_acq_rel);
		}

		if (isGainMatching)
		{
			simd_float outputVolume = getLoudness(laneDataSource, loudnessScale, binCount_);
			outputVolume = utils::maskLoad(outputVolume, simd_float(1.0f), simd_float::equal(outputVolume, 0.0f));
			simd_float scale = inputVolume / outputVolume;

			// some arbitrary limits taken from dtblkfx
			simd_float x = 1.0e30f;
			scale = utils::maskLoad(scale, simd_float(1.0f), simd_float::greaterThan(scale, 1.0e30f));
			scale = utils::maskLoad(scale, simd_float(0.0f), simd_float::lessThan(scale, 1.0e-30f));

			thisLane->volumeScale_ = simd_float::sqrt(scale);
		}

		// unlocking the last module's buffer
		if (laneDataSource.sourceBuffer != laneDataSource.conversionBuffer)
			utils::unlockAtomic(laneDataSource.sourceBuffer.getLock(), false, utils::WaitMechanism::Spin);

		COMPLEX_ASSERT(dataBuffer_.getLock().lock.load() >= 0);

		// to let other threads know that the data is in its final state
		thisLane->status_.store(EffectsLane::LaneStatus::Finished, std::memory_order_release);
	}

	void EffectsState::sumLanesAndWriteOutput(float *const *inputBuffer, size_t channels) noexcept
	{
		using namespace Framework;

		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (auto &lane : lanes_)
		{
			utils::ScopedLock g1{ lane->laneDataSource_.sourceBuffer.getLock(), false, utils::WaitMechanism::Spin };

			if (lane->laneDataSource_.dataType == ComplexDataSource::Polar)
			{
				utils::convertBuffer<utils::complexPolarToCart>(
					lane->laneDataSource_.sourceBuffer, lane->laneDataSource_.conversionBuffer, binCount_);
				lane->laneDataSource_.sourceBuffer = lane->laneDataSource_.conversionBuffer;
				lane->laneDataSource_.dataType = ComplexDataSource::Cartesian;
			}
		}

		utils::ScopedLock g{ outputBuffer_.getLock(), true, utils::WaitMechanism::Spin };
		outputBuffer_.clear();

		// multipliers for scaling the multiple chains going into the same output
		std::array<float, kNumInputsOutputs> multipliers{};
		for (u32 i = 0; i < lanes_.size(); i++)
		{
			auto laneOutput = lanes_[i]->getParameter(BaseProcessors::EffectsLane::Output::name())
				->getInternalValue<u32>(getSampleRate());

			if (laneOutput != kDefaultOutput)
				multipliers[laneOutput]++;
		}

		// for every lane we add its scaled output to the main sourceBuffer_ at the designated output channels
		for (auto &lane : lanes_)
		{
			auto laneOutput = lane->getParameter(BaseProcessors::EffectsLane::Output::name())->getInternalValue<u32>(getSampleRate());
			if (laneOutput == kDefaultOutput)
				continue;

			utils::ScopedLock g1{ lane->laneDataSource_.sourceBuffer.getLock(), false, utils::WaitMechanism::Spin };

			simd_float multiplier = simd_float::max(1.0f, multipliers[laneOutput]);
			SimdBuffer<complex<float>, simd_float>::applyToThisNoMask<utils::MathOperations::Add>(outputBuffer_,
				lane->laneDataSource_.sourceBuffer, kComplexSimdRatio, binCount_, laneOutput * kComplexSimdRatio, 0, 0, 0, 
				lane->volumeScale_.get() / multiplier);
		}

		std::array<simd_float, kComplexSimdRatio> values;
		auto *data = outputBuffer_.getData().getData().get();
		size_t size = outputBuffer_.getSize();
		for (size_t i = 0; i < channels; i += kComplexSimdRatio)
		{
			if (!usedOutputs_[i / kComplexSimdRatio])
				continue;

			for (size_t j = 0; j < binCount_; j++)
			{
				for (size_t k = 0; k < kComplexSimdRatio; ++k)
					values[k] = data[i * size + j * 2 + k];

				utils::complexTranspose(values);

				for (size_t k = 0; k < kComplexSimdRatio; k++)
					std::memcpy(inputBuffer[i + k] + j * 2 * kComplexSimdRatio, 
						&values[k], 2U * kComplexSimdRatio * sizeof(float));
			}
		}
	}
}