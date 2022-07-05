/*
	==============================================================================

		EffectsState.cpp
		Created: 2 Oct 2021 8:53:05pm
		Author:  Lenovo

	==============================================================================
*/

#include "EffectsState.h"
#include "./Framework/simd_utils.h"
#include <thread>

namespace Generation
{
	void EffectsState::writeInputData(const AudioBuffer<float> &inputBuffer)
	{
		auto channelPointers = inputBuffer.getArrayOfReadPointers();

		for (u32 i = 0; i < inputBuffer.getNumChannels(); i += kComplexSimdRatio)
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

	void EffectsState::distributeData()
	{
		for (u32 i = 0; i < chains_.size(); i++)
		{
			// if the input is dependent on another chain's output we don't do anything yet
			auto chainIndex = chains_[i].inputIndex;
			if (chainIndex & kChainInputMask)
				continue;

			auto currentChain = chains_[i].getChainData().lock();
			Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(currentChain->sourceBuffer, sourceBuffer_,
				kNumChannels, FFTSize_, utils::Operations::Assign, 0, chainIndex * kComplexSimdRatio);
		}
	}

	void EffectsState::processChains()
	{
		// triggers the chains to run again
		for (size_t i = 0; i < chains_.size(); i++)
			chains_[i].isFinished.store(false, std::memory_order_release);

		// waiting for chanins to finish
		for (size_t i = 0; i < chains_.size(); i++)
			while (chains_[i].isFinished.load(std::memory_order_acquire) == false);
	}

	void EffectsState::processIndividualChains(std::stop_token stoken, const EffectsState &state, u32 chainIndex)
	{
		while (true)
		{
			// so long as the flag is not set we don't execute anything
			while (state.chains_[chainIndex].isFinished.load(std::memory_order_acquire) == true)
			{
				// if chain has been deleted or program is shutting down we need to close thread
				if (stoken.stop_requested())
					return;
			}

			// we're getting pointers every time the chain runs because
			// there might be changes/resizes of container objects
			auto thisChain = &state.chains_[chainIndex];
			auto thisChainSourceBuffer = &thisChain->chainData->sourceBuffer;
			auto thisChainWorkBuffer = &thisChain->chainData->workBuffer;
			auto thisChainEffectOrder = thisChain->effectOrder.data();
			auto thisChainEffectOrderSize = thisChain->effectOrder.size();
			auto thisChainDataIsInWork = &thisChain->chainData->dataIsInWork;

			auto FFTSize = state.FFTSize_;
			float sampleRate = state.sampleRate_;

			// if this chain's input is another's output
			// we wait until that one is finished and then copy its data
			if (thisChain->inputIndex & EffectsState::kChainInputMask)
			{
				u32 chainInputIndex = thisChain->inputIndex ^ EffectsState::kChainInputMask;
				while (state.chains_[chainInputIndex].isFinished.load(std::memory_order_acquire) == false);

				auto inputChainData = state.chains_[chainInputIndex].getChainData().lock();
				Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(*thisChainSourceBuffer,
					inputChainData->sourceBuffer, kNumChannels, state.FFTSize_, utils::Operations::Assign);
			}

			// main processing loop
			for (size_t i = 0; i < thisChainEffectOrderSize; i++)
			{
				thisChainEffectOrder[i].processEffect(*thisChainSourceBuffer,
					*thisChainWorkBuffer, FFTSize, sampleRate);
				// TODO: fit links between modules here
				std::swap(thisChainSourceBuffer, thisChainWorkBuffer);
				*thisChainDataIsInWork = !(*thisChainDataIsInWork);
			}

			// if the latest data is in the wrong buffer, we swap
			if (*thisChainDataIsInWork)
			{
				thisChainSourceBuffer->swap(*thisChainWorkBuffer);
				*thisChainDataIsInWork = false;
			}

			// to let other threads know that the data is in its final state
			// and to prevent the thread of instantly running again
			thisChain->isFinished.store(true, std::memory_order_release);
		}
	}

	void EffectsState::sumChains()
	{
		sourceBuffer_.clear();

		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (u32 i = 0; i < chains_.size(); i++)
		{
			auto currentChainData = chains_.at(i).getChainData().lock();
			if (!currentChainData->isCartesian && chains_[i].isEnabled)
			{
				auto &buffer = currentChainData->sourceBuffer;
				for (u32 i = 0; i < buffer.getNumSimdChannels(); i++)
				{
					for (u32 j = 0; j < buffer.getSize(); j += 2)
					{
						simd_float one = buffer.getSIMDValueAt(i * kComplexSimdRatio, j);
						simd_float two = buffer.getSIMDValueAt(i * kComplexSimdRatio, j + 1);
						utils::complexPolarToCart(one, two);
						buffer.writeSIMDValueAt(one, i * kComplexSimdRatio, j);
						buffer.writeSIMDValueAt(two, i * kComplexSimdRatio, j + 1);
					}
				}

				currentChainData->isCartesian = true;
			}
		}

		// multipliers for scaling the multiple chains going into the same output
		std::array<float, kNumInputsOutputs> multipliers;
		for (u32 i = 0; i < multipliers.size(); i++)
		{
			float scale = 0.0f;
			for (size_t i = 0; i < chains_.size(); i++)
				if (chains_[i].outputIndex == i)
					scale++;
				
			// if an output isn't chosen, we don't do anything
			scale = std::max(1.0f, scale);
			multipliers[i] = (1.0f / scale);
		}

		// for every chain we add its scaled output to the main sourceBuffer_ at the designated output channels
		for (u32 i = 0; i < chains_.size(); i++)
		{
			if (chains_[i].outputIndex == kDefaultOutput)
				continue;

			auto currentChainData = chains_[i].getChainData().lock();
			auto currentChainBuffer = &currentChainData->sourceBuffer;

			Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThisNoMask(sourceBuffer_,
				*currentChainBuffer, kComplexSimdRatio, FFTSize_, utils::Operations::Add,
				chains_[i].outputIndex * kComplexSimdRatio, 0);
		}

		// scaling all outputs
		for (u32 i = 0; i < kNumInputsOutputs; i++)
		{
			if (multipliers[i] == 1.0f)
				continue;

			simd_float multiplier = multipliers[i];
			for (u32 j = 0; j < FFTSize_; j++)
				sourceBuffer_.multiply(multiplier, i * kComplexSimdRatio, j);
		}
	}

	void EffectsState::writeOutputData(AudioBuffer<float> &outputBuffer)
	{
		std::array<simd_float, kComplexSimdRatio> simdValues;

		auto channelPointers = outputBuffer.getArrayOfWritePointers();

		for (u32 i = 0; i < outputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
			if (!usedOutputs_[i / kComplexSimdRatio])
				continue;

			for (u32 j = 0; j < FFTSize_ / 2; j++)
			{
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					simdValues[k] = sourceBuffer_.getSIMDValueAt(i, j * 2 + k);
				Framework::matrix matrix(simdValues);

				matrix.complexTranspose();
				for (u32 k = 0; k < kComplexSimdRatio; k++)
					outputBuffer.copyFrom(i + k, j * 2 * kComplexSimdRatio,
						matrix.rows_[k].getArrayOfValues().data(), 2 * kComplexSimdRatio);
			}
		}
	}

	void EffectsState::addEffect(u32 chainIndex, u32 effectIndex, ModuleTypes type)
	{
	}

	void EffectsState::deleteEffect(u32 chainIndex, u32 effectIndex)
	{
	}

	void EffectsState::moveEffect(u32 currentChainIndex, u32 currentEffectIndex, u32 newChainIndex, u32 newEffectIndex)
	{
	}

	void EffectsState::copyEffect(u32 chainIndex, u32 effectIndex, u32 copyChainIndex, u32 copyEffectIndex)
	{
	}
}