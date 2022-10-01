/*
	==============================================================================

		EffectsState.cpp
		Created: 2 Oct 2021 8:53:05pm
		Author:  theuser27

	==============================================================================
*/

#include "EffectsState.h"
#include "./Framework/simd_utils.h"

namespace Generation
{
	bool EffectsChain::insertSubModule(u32 index, std::string_view moduleType) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT(Framework::kEffectModuleNames.end() != std::find(Framework::kEffectModuleNames.begin(),
			Framework::kEffectModuleNames.end(), moduleType) && "You're trying to insert a non-EffectModule into EffectChain");

		subModules_.insert(subModules_.begin() + index, std::move(createSubModule<EffectModule>(moduleType)));

		return true;
	}

	bool EffectsChain::deleteSubModule(u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		subModules_.erase(subModules_.begin() + index);
		return true;
	}

	bool EffectsChain::copySubModule(const std::shared_ptr<PluginModule> &newSubModule, u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT(newSubModule->getModuleType() == Framework::kPluginModules[3] &&
			"You're trying to copy a non-EffectModule into EffectChain");

		subModules_.insert(subModules_.begin() + index, std::move(createSubModule<EffectModule>(*newSubModule)));

		return true;
	}

	bool EffectsChain::moveSubModule(std::shared_ptr<PluginModule> newSubModule, u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT(newSubModule->getModuleType() == Framework::kPluginModules[3] &&
			"You're trying to copy a non-EffectModule into EffectChain");

		auto *subModule = static_cast<EffectModule *>(newSubModule.get());

		subModules_.insert(subModules_.begin() + index, std::move(createSubModule<EffectModule>(*newSubModule)));
		//globalModulesState_->addModule(subModules_[index]);

		return true;
	}

	bool EffectsState::insertSubModule([[maybe_unused]] u32 index, [[maybe_unused]] std::string_view moduleType) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT(moduleType == Framework::kPluginModules[2] &&
			"You're trying to insert a non-EffectChain into EffectsState");

		// searching for chains that are unused first
		for (size_t i = 0; i < subModules_.size(); i++)
		{
			auto &instance = subModules_[i];
			if (instance->getNumCurrentUsers() >= 0)
				continue;

			instance->clearSubModules();
			instance->initialise();
			instance->reuse();

			std::jthread newThread = std::jthread(processIndividualChains, std::ref(*this), i);
			chainThreads_[i].swap(newThread);

			return true;
		}

		// have we reached the max chain capacity?
		if (subModules_.size() >= kMaxNumChains)
			return false;

		// if there are no unused objects and we haven't reached max capacity, we can add
		subModules_.emplace_back(std::move(createSubModule<EffectsChain>()));
		chainThreads_.emplace_back(processIndividualChains, std::ref(*this), subModules_.size() - 1);

		return true;
	}

	bool EffectsState::deleteSubModule(u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		chainThreads_[index].~jthread();
		subModules_[index]->softDelete();

		return true;
	}

	bool EffectsState::copySubModule(const std::shared_ptr<PluginModule> &newSubModule, [[maybe_unused]] u32 index) noexcept
	{
		if (!checkUpdateFlag())
			return false;

		COMPLEX_ASSERT(newSubModule->getModuleType() == Framework::kPluginModules[2] &&
			"You're trying to copy a non-EffectsChain into EffectsState");

		const auto *subModule = static_cast<EffectsChain *>(newSubModule.get());

		// searching for chains that are unused first
		for (size_t i = 0; i < subModules_.size(); i++)
		{
			auto *instance = static_cast<EffectsChain *>(subModules_[i].get());
			if (instance->getNumCurrentUsers() >= 0)
				continue;

			*instance = *subModule;
			instance->reuse();

			std::jthread newThread = std::jthread(processIndividualChains, std::ref(*this), i);
			chainThreads_[i].swap(newThread);

			return true;
		}

		// we've reached the max chain capacity
		if (subModules_.size() >= kMaxNumChains)
			return false;

		// if there are no unused objects and we haven't reached max capacity, we can add
		subModules_.emplace_back(std::move(createSubModule<EffectsChain>(*newSubModule)));
		chainThreads_.emplace_back(processIndividualChains, std::ref(*this), subModules_.size() - 1);

		return true;
	}

	void EffectsState::writeInputData(const AudioBuffer<float> &inputBuffer) noexcept
	{
		auto channelPointers = inputBuffer.getArrayOfReadPointers();

		for (u32 i = 0; i < (u32)inputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
			// if the input is not used we skip it
			if (!usedInputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < FFTSize_ / 2 ; j++)
			{
				// skipping every second sample (complex signal) and every second pair (matrix takes 2 pairs)
				auto matrix = utils::getValueMatrix<kComplexSimdRatio>(channelPointers + i, j * 2 * kComplexSimdRatio);

				matrix.complexTranspose();
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					sourceBuffer_.writeSIMDValueAt(matrix.rows_[k], i, j * kComplexSimdRatio + k);
			}
		}
	}

	void EffectsState::distributeData() noexcept
	{
		for (u32 i = 0; i < subModules_.size(); i++)
		{
			auto chainPointer = static_cast<EffectsChain *>(subModules_[i].get());
			// if the input is dependent on another chain's output we don't do anything yet
			auto chainIndex = chainPointer->moduleParameters_[1]->getInternalValue<u32>();
			if (chainIndex & kChainInputMask)
				continue;

			auto currentChain = chainPointer->chainData.get();
			Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(currentChain->sourceBuffer, sourceBuffer_,
				kNumChannels, FFTSize_, utils::MathOperations::Assign, 0, chainIndex * kComplexSimdRatio);
		}
	}

	void EffectsState::processChains() noexcept
	{
		// triggers the chains to run again
		for (size_t i = 0; i < subModules_.size(); i++)
			static_cast<EffectsChain *>(subModules_[i].get())
			->isFinished_.store(false, std::memory_order_release);

		// waiting for chains to finish
		for (size_t i = 0; i < subModules_.size(); i++)
			while (static_cast<EffectsChain *>(subModules_[i].get())
				->isFinished_.load(std::memory_order_acquire) == false);
	}

	void EffectsState::processIndividualChains(std::stop_token stoken, EffectsState &state, u32 chainIndex) noexcept
	{
		while (true)
		{
			while (true)
			{
				// so long as the flag is set we don't execute anything
				if (static_cast<EffectsChain *>(state.subModules_[chainIndex].get())->isFinished_.load(std::memory_order_acquire) == false)
					break;

				// if chain has been deleted or program is shutting down we need to close thread
				if (stoken.stop_requested())
					return;

				utils::wait();
			}

			auto thisChain = static_cast<EffectsChain *>(state.subModules_[chainIndex].get());

			// Chain On/Off
			// if this chain is turned off, we set it as finished and skip everything
			if (!thisChain->moduleParameters_[0]->getInternalValue<u32>())
			{
				thisChain->isFinished_.store(true, std::memory_order_release);
				continue;
			}

			// we're getting pointers every time the chain runs because
			// there might be changes/resizes of container objects
			auto thisChainSourceBuffer = &thisChain->chainData->sourceBuffer;
			auto thisChainWorkBuffer = &thisChain->chainData->workBuffer;
			auto thisChainEffectOrder = thisChain->subModules_.data();
			auto thisChainEffectOrderSize = thisChain->subModules_.size();
			auto thisChainDataIsInWork = &thisChain->chainData->dataIsInWork;

			// when doing FFT we only receive the positive frequencies and negative are discarded
			// so we're actually working with half the number of bins
			auto effectiveFFTSize = state.FFTSize_ / 2;
			float sampleRate = state.sampleRate_;

			// Chain Input 
			// if this chain's input is another's output and that chain can be used,
			// we wait until it is finished and then copy its data
			if (auto thisChainInputIndex = thisChain->moduleParameters_[1]->getInternalValue<u32>(); 
				(thisChainInputIndex & EffectsState::kChainInputMask) && thisChain->getNumCurrentUsers() >= 0)
			{
				u32 chainInputIndex = thisChainInputIndex ^ EffectsState::kChainInputMask;
				while (static_cast<EffectsChain *>(state.subModules_[chainInputIndex].get())
					->isFinished_.load(std::memory_order_acquire) == false)
				{ utils::wait(); }

				auto &inputChainData = static_cast<EffectsChain *>(state.subModules_[chainInputIndex].get())->chainData;
				Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(*thisChainSourceBuffer,
					inputChainData->sourceBuffer, kNumChannels, state.FFTSize_, utils::MathOperations::Assign);
			}

			thisChain->currentEffectIndex_.store(0, std::memory_order_release);

			// main processing loop
			for (size_t i = 0; i < thisChainEffectOrderSize; i++)
			{
				static_cast<EffectModule *>(thisChainEffectOrder[i].get())
					->processEffect(*thisChainSourceBuffer, *thisChainWorkBuffer, effectiveFFTSize, sampleRate);
				// TODO: fit links between modules here
				std::swap(thisChainSourceBuffer, thisChainWorkBuffer);
				*thisChainDataIsInWork = !(*thisChainDataIsInWork);

				// incrementing where we are currently
				thisChain->currentEffectIndex_.fetch_add(1, std::memory_order_acq_rel);
			}

			// if the latest data is in the wrong buffer, we swap
			if (*thisChainDataIsInWork)
			{
				thisChainSourceBuffer->swap(*thisChainWorkBuffer);
				*thisChainDataIsInWork = false;
			}

			// to let other threads know that the data is in its final state
			// and to prevent the thread of instantly running again
			thisChain->isFinished_.store(true, std::memory_order_release);
		}
	}

	void EffectsState::sumChains() noexcept
	{
		sourceBuffer_.clear();

		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (u32 i = 0; i < subModules_.size(); i++)
		{
			auto currentChain = static_cast<EffectsChain *>(subModules_[i].get());
			if (!currentChain->chainData->isCartesian && currentChain->moduleParameters_[0]->getInternalValue<u32>())
			{
				auto &buffer = currentChain->chainData->sourceBuffer;
				for (u32 j = 0; j < buffer.getNumSimdChannels(); j++)
				{
					for (u32 k = 0; k < buffer.getSize(); k += 2)
					{
						simd_float one = buffer.getSIMDValueAt(j * kComplexSimdRatio, k);
						simd_float two = buffer.getSIMDValueAt(j * kComplexSimdRatio, k + 1);
						utils::complexPolarToCart(one, two);
						buffer.writeSIMDValueAt(one, j * kComplexSimdRatio, k);
						buffer.writeSIMDValueAt(two, j * kComplexSimdRatio, k + 1);
					}
				}

				currentChain->chainData->isCartesian = true;
			}
		}

		// multipliers for scaling the multiple chains going into the same output
		std::array<float, kNumInputsOutputs> multipliers{};

		// for every chain we add its scaled output to the main sourceBuffer_ at the designated output channels
		for (u32 i = 0; i < subModules_.size(); i++)
		{
			auto currentChain = static_cast<EffectsChain *>(subModules_[i].get());
			auto currentChainOutput = currentChain->moduleParameters_[2]->getInternalValue<u32>();
			if (currentChainOutput == kDefaultOutput)
				continue;

			multipliers[currentChainOutput]++;

			auto currentChainData = currentChain->chainData.get();
			auto currentChainBuffer = &currentChainData->sourceBuffer;

			Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(sourceBuffer_,
				*currentChainBuffer, kComplexSimdRatio, FFTSize_, utils::MathOperations::Add,
				currentChainOutput * kComplexSimdRatio, 0);
		}

		// scaling all outputs
		for (u32 i = 0; i < kNumInputsOutputs; i++)
		{
			// if there's only one or no chains going to this output, we don't scale
			if (multipliers[i] == 0.0f || multipliers[i] == 1.0f)
				continue;

			simd_float multiplier = 1.0f / multipliers[i];
			for (u32 j = 0; j < FFTSize_; j++)
				sourceBuffer_.multiply(multiplier, i * kComplexSimdRatio, j);
		}
	}

	void EffectsState::writeOutputData(AudioBuffer<float> &outputBuffer) noexcept
	{
		Framework::Matrix matrix;

		for (u32 i = 0; i < (u32)outputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
			if (!usedOutputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < FFTSize_ / 2; j++)
			{
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					matrix.rows_[k] = sourceBuffer_.getSIMDValueAt(i, j * 2 + k);

				matrix.complexTranspose();
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					std::memcpy(outputBuffer.getWritePointer(i + k, j * 2 * kComplexSimdRatio), 
						&matrix.rows_[k], 2 * kComplexSimdRatio * sizeof(float));
			}
		}
	}
}