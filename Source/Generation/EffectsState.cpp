/*
	==============================================================================

		EffectsState.cpp
		Created: 2 Oct 2021 8:53:05pm
		Author:  theuser27

	==============================================================================
*/

#include "EffectsState.h"
#include "Framework/simd_utils.h"

namespace Generation
{
	EffectsLane::EffectsLane(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept :
		BaseProcessor(moduleTree, parentModuleId, getClassType())
	{
		subProcessors_.reserve(kInitialNumEffects);
		dataBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		insertSubProcessor(0, makeSubProcessor<EffectModule>(dynamicsEffect::getClassType()));

		processorParameters_.data.reserve(Framework::effectChainParameterList.size());
		createProcessorParameters(Framework::effectChainParameterList.data(), Framework::effectChainParameterList.size());
	}

	bool EffectsLane::insertSubProcessor(size_t index, BaseProcessor *newSubProcessor) noexcept
	{
		COMPLEX_ASSERT(newSubProcessor && "A valid BaseProcessor needs to be passed in");
		COMPLEX_ASSERT(newSubProcessor->getProcessorType() == EffectModule::getClassType()
			&& "You're trying to move a non-EffectModule into EffectChain");

		effectModules_.insert(effectModules_.begin() + index, static_cast<EffectModule *>(newSubProcessor));
		subProcessors_.insert(subProcessors_.begin() + index, newSubProcessor);

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

	EffectsState::EffectsState(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept :
		BaseProcessor(moduleTree, parentModuleId, getClassType())
	{
		waitIterations_ = 12;

		static_assert(kMaxNumLanes >= 1, "Why would you have 0 lanes??");

		lanes_.reserve(kMaxNumLanes);
		subProcessors_.reserve(kMaxNumLanes);
		// size is half the max because a single SIMD package stores both real and imaginary parts
		dataBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		laneThreads_.reserve(kMaxNumLanes);

		insertSubProcessor(0, makeSubProcessor<EffectsLane>());
	}

	bool EffectsState::insertSubProcessor([[maybe_unused]] size_t index, BaseProcessor *newSubProcessor) noexcept
	{
		COMPLEX_ASSERT(newSubProcessor && "A valid BaseProcessor needs to be passed in");
		COMPLEX_ASSERT(newSubProcessor->getProcessorType() == EffectsLane::getClassType() &&
			"You're trying to insert a non-EffectChain into EffectsState");

		// have we reached the max chain capacity?
		if (lanes_.size() >= kMaxNumLanes)
			return false;

		// TODO: remedy the way lanes are being processed by threads
		lanes_.emplace_back(static_cast<EffectsLane *>(newSubProcessor));
		subProcessors_.emplace_back(newSubProcessor);
		laneThreads_.emplace_back(std::bind_front(&EffectsState::processIndividualChains, std::ref(*this)), lanes_.size() - 1);

		for (auto *listener : listeners_)
			listener->insertedSubProcessor(index, newSubProcessor);

		return true;
	}

	BaseProcessor *EffectsState::deleteSubProcessor(size_t index) noexcept
	{
		COMPLEX_ASSERT(index < subProcessors_.size());

		laneThreads_[index].~jthread();
		BaseProcessor *deletedModule = lanes_[index];
		lanes_.erase(lanes_.begin() + index);
		subProcessors_.erase(subProcessors_.begin() + index);

		for (auto *listener : listeners_)
			listener->deletedSubProcessor(index, deletedModule);

		return deletedModule;
	}

	void EffectsState::writeInputData(const AudioBuffer<float> &inputBuffer) noexcept
	{
		auto channelPointers = inputBuffer.getArrayOfReadPointers();
		for (u32 i = 0; i < (u32)inputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
			// if the input is not used we skip it
			if (!usedInputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < effectiveFFTSize_; j++)
			{
				// skipping every second sample (complex signal) and every second pair (matrix takes 2 pairs)
				auto matrix = utils::getValueMatrix<kComplexSimdRatio>(channelPointers + i, j * 2 * kComplexSimdRatio);

				matrix.complexTranspose();
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					dataBuffer_.writeSimdValueAt(matrix.rows_[k], i, j * kComplexSimdRatio + k);
			}
		}


	}

	void EffectsState::processChains() const noexcept
	{
		// the reason for sequential consistency for the triggers is because we don't want
		// the waiting and trigger stage to be rearranged/out-of-order executed
		// (release/acquire barriers can cross each other but not seq_cst/acquire)

		// triggers the chains to run again
		for (auto &chain : lanes_)
			chain->isFinished_.store(false, std::memory_order_seq_cst);

		// waiting for chains to finish
		for (auto &chain : lanes_)
			while (chain->isFinished_.load(std::memory_order_acquire) == false) { utils::longWait(5); }
	}

	void EffectsState::processIndividualChains(std::stop_token stoken, size_t chainIndex) const noexcept
	{
		using namespace Framework;

		while (true)
		{
			while (true)
			{
				// if chain has been deleted or program is shutting down we need to close thread
				if (stoken.stop_requested())
					return;

				// so long as the flag is set we don't execute anything
				if (lanes_[chainIndex]->isFinished_.load(std::memory_order_acquire) == false)
					break;

				utils::longWait(waitIterations_);
			}

			auto thisLane = lanes_[chainIndex];

			// Chain On/Off
			// if this chain is turned off, we set it as finished and skip everything
			if (!thisLane->getParameterUnchecked((u32)EffectsLaneParameters::LaneEnabled)->getInternalValue<u32>(getSampleRate()))
			{
				thisLane->isFinished_.store(true, std::memory_order_release);
				continue;
			}

			thisLane->currentEffectIndex_.store(0, std::memory_order_release);
			auto &laneDataSource = thisLane->laneDataSource_;

			// Chain Input 
			// if this chain's input is another's output and that chain can be used,
			// we wait until it is finished and then copy its data
			if (auto inputIndex = thisLane->getParameterUnchecked((u32)EffectsLaneParameters::Input)
				->getInternalValue<u32>(getSampleRate()); inputIndex & kLaneInputMask)
			{
				inputIndex ^= kLaneInputMask;
				while (lanes_[inputIndex]->isFinished_.load(std::memory_order_acquire) == false)
				{ utils::wait(); }

				laneDataSource.sourceBuffer = SimdBufferView(lanes_[inputIndex]->dataBuffer_, kNumChannels, 0);
			}
			// input is not from a chain, we can begin processing
			else 
				laneDataSource.sourceBuffer = SimdBufferView(dataBuffer_, kNumChannels, inputIndex * kNumChannels);

			// main processing loop
			for (auto &effectModule : thisLane->subProcessors_)
			{
				// this is safe because by design only EffectModules are contained in a lane
				static_cast<EffectModule *>(effectModule)->processEffect(laneDataSource, effectiveFFTSize_, getSampleRate());

				// TODO: fit links between modules here

				// incrementing where we are currently
				thisLane->currentEffectIndex_.fetch_add(1, std::memory_order_acq_rel);
			}

			// to let other threads know that the data is in its final state
			// and to prevent the thread of instantly running again
			thisLane->isFinished_.store(true, std::memory_order_release);
		}
	}

	void EffectsState::sumChains() noexcept
	{
		using namespace Framework;

		dataBuffer_.clear();

		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (auto &lane : lanes_)
		{
			if (!lane->getParameterUnchecked((u32)EffectsLaneParameters::LaneEnabled)->getInternalValue<u32>(getSampleRate()))
				continue;

			if (lane->laneDataSource_.isPolar)
			{
				auto &buffer = lane->laneDataSource_.sourceBuffer;
				auto &conversionhBuffer = lane->laneDataSource_.conversionBuffer;
				for (u32 j = 0; j < buffer.getSimdChannels(); j++)
				{
					for (u32 k = 0; k < buffer.getSize(); k += 2)
					{
						simd_float one = buffer.readSimdValueAt(j * kComplexSimdRatio, k);
						simd_float two = buffer.readSimdValueAt(j * kComplexSimdRatio, k + 1);
						utils::complexPolarToCart(one, two);
						conversionhBuffer.writeSimdValueAt(one, j * kComplexSimdRatio, k);
						conversionhBuffer.writeSimdValueAt(two, j * kComplexSimdRatio, k + 1);
					}
				}
			}
		}

		// multipliers for scaling the multiple chains going into the same output
		std::array<float, kNumInputsOutputs> multipliers{};

		// for every chain we add its scaled output to the main sourceBuffer_ at the designated output channels
		for (auto &lane : lanes_)
		{
			if (!lane->getParameterUnchecked((u32)EffectsLaneParameters::LaneEnabled)->getInternalValue<u32>(getSampleRate()))
				continue;

			auto laneOutput = lane->getParameterUnchecked((u32)EffectsLaneParameters::Output)->getInternalValue<u32>(getSampleRate());
			if (laneOutput == kDefaultOutput)
				continue;

			multipliers[laneOutput]++;

			if (lane->laneDataSource_.isPolar)
			{
				auto source = SimdBufferView(lane->laneDataSource_.conversionBuffer);
				SimdBuffer<std::complex<float>, simd_float>::applyToThisNoMask(dataBuffer_,
					source, kComplexSimdRatio, effectiveFFTSize_, utils::MathOperations::Add, laneOutput * kComplexSimdRatio, 0);
			}
			else
				SimdBuffer<std::complex<float>, simd_float>::applyToThisNoMask(dataBuffer_,
					lane->laneDataSource_.sourceBuffer, kComplexSimdRatio, effectiveFFTSize_, utils::MathOperations::Add,
					laneOutput * kComplexSimdRatio, 0);
		}

		// scaling all outputs
		for (u32 i = 0; i < kNumInputsOutputs; i++)
		{
			// if there's only one or no chains going to this output, we don't scale
			if (multipliers[i] == 0.0f || multipliers[i] == 1.0f)
				continue;

			simd_float multiplier = 1.0f / multipliers[i];
			for (u32 j = 0; j < effectiveFFTSize_; j++)
				dataBuffer_.multiply(multiplier, i * kComplexSimdRatio, j);
		}
	}

	void EffectsState::writeOutputData(AudioBuffer<float> &outputBuffer) const noexcept
	{
		Framework::Matrix matrix;

		for (u32 i = 0; i < (u32)outputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
			if (!usedOutputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < effectiveFFTSize_; j++)
			{
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					matrix.rows_[k] = dataBuffer_.readSimdValueAt(i, j * 2 + k);

				matrix.complexTranspose();
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					std::memcpy(outputBuffer.getWritePointer(i + k, j * 2 * kComplexSimdRatio), 
						&matrix.rows_[k], size_t(2) * kComplexSimdRatio * sizeof(float));
			}
		}
	}
}