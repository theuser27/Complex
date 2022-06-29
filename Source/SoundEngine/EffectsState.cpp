/*
	==============================================================================

		EffectsState.cpp
		Created: 2 Oct 2021 8:53:05pm
		Author:  Lenovo

	==============================================================================
*/

#include "EffectsState.h"
#include "./Framework/simd_utils.h"

namespace Generation
{
	void EffectsState::writeInputData(AudioBuffer<float> &inputBuffer)
	{
		auto channelPointers = inputBuffer.getArrayOfReadPointers();

		for (u32 i = 0; i < inputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
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

	void EffectsState::distributeData(std::array<u32, kMaxNumChains> &chainInputs)
	{
		// TODO: redo when you get to multiple outputs
		for (u32 i = 0; i < chains_.size(); i++)
		{
			auto currentChain = chains_[i].getChainData().lock();
			Framework::SimdBuffer<std::complex<float>, simd_float>::copyToThis(currentChain->intermediateBuffer, sourceBuffer_, kNumChannels,
				FFTSize_, utils::Operations::Assign, kNoChangeMask, kNoChangeMask, 0, chainInputs[i] * kNumChannels);
		}
	}

	void EffectsState::processChains()
	{
		
	}

	void EffectsState::sumChains(std::array<u32, kMaxNumChains> &chainOutputs)
	{
		// TODO: redo when you get to multiple outputs
		// checks whether all of the chains are real-imaginary pairs
		// (instead of magnitude-phase pairs) and if not, gets converted
		for (u32 i = 0; i < chains_.size(); i++)
		{
			auto currentChainData = chains_.at(i).getChainData().lock();
			if (!currentChainData->isCartesian)
			{
				auto &buffer = currentChainData->intermediateBuffer;
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
			float multiplier = 0.0f;
			for (auto output : chainOutputs)
				if (output == i)
					multiplier++;
			// if an output isn't chosen, we don't do anything
			multiplier = (multiplier == 0.0f) ? 1.0f : multiplier;
			multipliers[i] = (1.0f / multiplier);
		}

		// for every chain we add its scaled output to the main sourceBuffer_ at the designated output channels
		for (u32 i = 0; i < chains_.size(); i++)
		{
			auto currentChainData = &chains_[i].getChainData().lock()->intermediateBuffer;
			for (u32 j = 0; j < currentChainData->getNumSimdChannels(); j++)
			{
				for (u32 k = 0; k < FFTSize_; k++)
					sourceBuffer_.add(currentChainData->getSIMDValueAt
					(j * kComplexSimdRatio, k) * multipliers[chainOutputs[i]], chainOutputs[i] * kNumChannels, k);
			}
		}
	}

	void EffectsState::writeOutputData(AudioBuffer<float> &outputBuffer)
	{
		// TODO: redo when you get to multiple outputs
		std::array<simd_float, kComplexSimdRatio> simdValues;

		auto channelPointers = outputBuffer.getArrayOfWritePointers();

		for (u32 i = 0; i < outputBuffer.getNumChannels(); i += kComplexSimdRatio)
		{
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