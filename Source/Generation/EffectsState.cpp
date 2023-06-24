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

		subProcessors_.insert(subProcessors_.begin() + index, newSubProcessor);

		for (auto *listener : listeners_)
			listener->insertedSubProcessor(index, newSubProcessor);

		return true;
	}

	BaseProcessor *EffectsLane::deleteSubProcessor(size_t index) noexcept
	{
		COMPLEX_ASSERT(index < subProcessors_.size());

		BaseProcessor *deletedModule = subProcessors_[index];
		subProcessors_.erase(subProcessors_.begin() + index);

		for (auto *listener : listeners_)
			listener->deletedSubProcessor(index, deletedModule);

		return deletedModule;
	}

	EffectsState::EffectsState(Plugin::ProcessorTree *moduleTree, u64 parentModuleId) noexcept :
		BaseProcessor(moduleTree, parentModuleId, getClassType())
	{
		static_assert(kMaxNumChains >= 1, "Why would you have 0 lanes???");

		chains_.reserve(kMaxNumChains);
		subProcessors_.reserve(kMaxNumChains);
		// size is half the max because a single SIMD package stores both real and imaginary parts
		dataBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		chainThreads_.reserve(kMaxNumChains);

		insertSubProcessor(0, makeSubProcessor<EffectsLane>());
	}

	bool EffectsState::insertSubProcessor([[maybe_unused]] size_t index, BaseProcessor *newSubProcessor) noexcept
	{
		COMPLEX_ASSERT(newSubProcessor && "A valid BaseProcessor needs to be passed in");
		COMPLEX_ASSERT(newSubProcessor->getProcessorType() == EffectsLane::getClassType() &&
			"You're trying to insert a non-EffectChain into EffectsState");

		// have we reached the max chain capacity?
		if (chains_.size() >= kMaxNumChains)
			return false;

		chains_.emplace_back(static_cast<EffectsLane *>(newSubProcessor));
		subProcessors_.emplace_back(newSubProcessor);
		chainThreads_.emplace_back(std::bind_front(&EffectsState::processIndividualChains, std::ref(*this)), chains_.size() - 1);

		for (auto *listener : listeners_)
			listener->insertedSubProcessor(index, newSubProcessor);

		return true;
	}

	BaseProcessor *EffectsState::deleteSubProcessor(size_t index) noexcept
	{
		COMPLEX_ASSERT(index < subProcessors_.size());

		chainThreads_[index].~jthread();
		BaseProcessor *deletedModule = chains_[index];
		chains_.erase(chains_.begin() + index);
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
		for (auto &chain : chains_)
			chain->isFinished_.store(false, std::memory_order_seq_cst);

		// waiting for chains to finish
		for (auto &chain : chains_)
			while (chain->isFinished_.load(std::memory_order_acquire) == false) { utils::wait(); }
	}

	void EffectsState::processIndividualChains(std::stop_token stoken, u32 chainIndex) const noexcept
	{
		while (true)
		{
			while (true)
			{
				// if chain has been deleted or program is shutting down we need to close thread
				if (stoken.stop_requested())
					return;

				// so long as the flag is set we don't execute anything
				if (chains_[chainIndex]->isFinished_.load(std::memory_order_acquire) == false)
					break;

				utils::wait();
			}

			auto thisChain = chains_[chainIndex];

			// Chain On/Off
			// if this chain is turned off, we set it as finished and skip everything
			if (!thisChain->processorParameters_[0]->getInternalValue<u32>(getSampleRate()))
			{
				thisChain->isFinished_.store(true, std::memory_order_release);
				continue;
			}

			thisChain->currentEffectIndex_.store(0, std::memory_order_release);
			auto &chainDataSource = thisChain->chainDataSource_;

			// Chain Input 
			// if this chain's input is another's output and that chain can be used,
			// we wait until it is finished and then copy its data
			if (auto inputIndex = thisChain->processorParameters_[1]
				->getInternalValue<u32>(getSampleRate()); inputIndex & kChainInputMask)
			{
				inputIndex ^= kChainInputMask;
				while (chains_[inputIndex]->isFinished_.load(std::memory_order_acquire) == false)
				{ utils::wait(); }

				chainDataSource.sourceBuffer = Framework::SimdBufferView(chains_[inputIndex]->dataBuffer_, kNumChannels, 0);
			}
			// input is not from a chain, we can begin processing
			else 
				chainDataSource.sourceBuffer = Framework::SimdBufferView(dataBuffer_, kNumChannels, inputIndex * kNumChannels);

			// main processing loop
			for (auto &effectModule : thisChain->subProcessors_)
			{
				// this is safe because by design only EffectModules are contained in a lane
				static_cast<EffectModule *>(effectModule)->processEffect(chainDataSource, effectiveFFTSize_, getSampleRate());

				// TODO: fit links between modules here

				// incrementing where we are currently
				thisChain->currentEffectIndex_.fetch_add(1, std::memory_order_acq_rel);
			}

			// to let other threads know that the data is in its final state
			// and to prevent the thread of instantly running again
			thisChain->isFinished_.store(true, std::memory_order_release);
		}
	}

	void EffectsState::sumChains() noexcept
	{
		dataBuffer_.clear();

		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (auto &chain : chains_)
		{
			if (chain->chainDataSource_.isPolar && chain->processorParameters_[0]->getInternalValue<u32>(getSampleRate()))
			{
				auto &buffer = chain->chainDataSource_.sourceBuffer;
				auto &conversionhBuffer = chain->chainDataSource_.conversionBuffer;
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
		for (auto &chain : chains_)
		{
			auto chainOutput = chain->processorParameters_[2]->getInternalValue<u32>(getSampleRate());
			if (chainOutput == kDefaultOutput)
				continue;

			multipliers[chainOutput]++;

			if (chain->chainDataSource_.isPolar)
			{
				auto source = Framework::SimdBufferView(chain->chainDataSource_.conversionBuffer);
				Framework::SimdBuffer<std::complex<float>, simd_float>::applyToThisNoMask(dataBuffer_,
					source, kComplexSimdRatio, effectiveFFTSize_, utils::MathOperations::Add, chainOutput * kComplexSimdRatio, 0);
			}
			else
				Framework::SimdBuffer<std::complex<float>, simd_float>::applyToThisNoMask(dataBuffer_,
					chain->chainDataSource_.sourceBuffer, kComplexSimdRatio, effectiveFFTSize_, utils::MathOperations::Add,
					chainOutput * kComplexSimdRatio, 0);
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
						&matrix.rows_[k], 2 * kComplexSimdRatio * sizeof(float));
			}
		}
	}
}