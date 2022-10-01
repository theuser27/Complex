/*
	==============================================================================

		SoundEngine.cpp
		Created: 12 Aug 2021 2:12:59am
		Author:  theuser27

	==============================================================================
*/

#include "SoundEngine.h"

namespace Generation
{
	// as the topmost module its parentModuleId is going to be itself
	SoundEngine::SoundEngine(AllModules *globalModulesState) noexcept : 
		PluginModule(globalModulesState, globalModulesState->getId(true), Framework::kPluginModules[0])
	{
		transforms.reserve(kMaxFFTOrder - kMinFFTOrder + 1);
		for (u32 i = 0; i < (kMaxFFTOrder - kMinFFTOrder + 1); i++)
		{
			u32 bits = kMinFFTOrder + i;
			transforms.emplace_back(std::make_unique<Framework::FFT>(bits));
		}

		inputBuffer.reserve(kNumTotalChannels, kMaxPreBufferLength);
		// needs to be double the max FFT, otherwise we get out of bounds errors
		FFTBuffer.setSize(kNumTotalChannels, kMaxFFTBufferLength * 2, false, true);
		outBuffer.reserve(kNumTotalChannels, kMaxFFTBufferLength * 2);
		windows = Framework::Window::getInstance();

		auto newSubModule = createSubModule<EffectsState>();
		subModules_.emplace_back(std::move(newSubModule));

		moduleParameters_.data.reserve(Framework::globalPluginParameterList.size());
		createModuleParameters(Framework::globalPluginParameterList.data(), Framework::globalPluginParameterList.size());
	}

	void SoundEngine::Initialise(float sampleRate, u32 samplesPerBlock) noexcept
	{
		sampleRate_ = (sampleRate >= kDefaultSampleRate) ? sampleRate : kDefaultSampleRate;
		samplesPerBlock_ = samplesPerBlock;
	}

	perf_inline void SoundEngine::CopyBuffers(AudioBuffer<float> &buffer, u32 numInputs, u32 numSamples) noexcept
	{
		// assume that we don't get blocks bigger than our buffer size
		inputBuffer.writeBuffer(buffer, numInputs, numSamples);

		// we update them here because we could get broken up blocks if done inside the loop
		auto effectsStatePointer = static_cast<EffectsState*>(subModules_[0].get());
		usedInputChannels_ = effectsStatePointer->getUsedInputChannels();
		usedOutputChannels_ = effectsStatePointer->getUsedOutputChannels();
	}

	perf_inline void SoundEngine::IsReadyToPerform(u32 numSamples) noexcept
	{
		// if there are scaled and/or processed samples 
		// that haven't already been output we don't need to perform
		u32 samplesReady = outBuffer.getBeginOutputToToScaleOutput() +
			outBuffer.getToScaleOutputToAddOverlap();
		if (samplesReady >= numSamples)
		{
			isPerforming_ = false;
			hasEnoughSamples_ = true;
			return;
		}

		// are there enough samples ready to be processed?
		u32 availableSamples = inputBuffer.newSamplesToRead(nextOverlapOffset_);
		if (availableSamples < FFTNumSamples_)
		{
			isPerforming_ = false;
			hasEnoughSamples_ = false;
			return;
		}

		static u32 prevFFTNumSamples = 1 << kDefaultFFTOrder;
		prevFFTNumSamples = FFTNumSamples_;
		// how many samples we're processing currently
		FFTNumSamples_ = getFFTNumSamples();
		
		i32 FFTChangeOffset = prevFFTNumSamples - FFTNumSamples_;

		// clearing upper samples that could remain
		// after changing from a higher to lower FFTSize
		for (size_t i = 0; i < FFTBuffer.getNumChannels(); i++)
		{
			i32 samplesToClear = std::max(FFTChangeOffset, 0);
			utils::zeroBuffer(FFTBuffer.getWritePointer((int)i, FFTNumSamples_), samplesToClear);
		}

		inputBuffer.readBuffer(FFTBuffer, FFTBuffer.getNumChannels(), usedInputChannels_.data(), 
			FFTNumSamples_, inputBuffer.BlockBegin, nextOverlapOffset_ + FFTChangeOffset);



		isPerforming_ = true;
	}

	void SoundEngine::UpdateParameters(UpdateFlag flag) noexcept
	{
		globalModulesState_->setUpdateFlag(flag);
		updateParameters(flag, true);

		switch (flag)
		{
		case common::UpdateFlag::Realtime:
			overlap_ = moduleParameters_[2]->getInternalValue<float>();
			windowType_ = static_cast<Framework::WindowTypes>(moduleParameters_[3]->getInternalValue<u32>());
			alpha_ = moduleParameters_[4]->getInternalValue<float>();

			// getting the next overlapOffset
			nextOverlapOffset_ = getOverlapOffset();

			break;
		case common::UpdateFlag::BeforeProcess:
			mix_ = moduleParameters_[0]->getInternalValue<float>();
			FFTOrder_ = moduleParameters_[1]->getInternalValue<u32>();
			outGain_ = moduleParameters_[5]->getInternalValue<float>();

			break;
		default:
			break;
		}
	}

	perf_inline void SoundEngine::DoFFT() noexcept
	{
		// windowing
		windows->applyWindow(FFTBuffer, FFTBuffer.getNumChannels(), usedInputChannels_.data(), FFTNumSamples_, windowType_, alpha_);

		// in-place FFT
		// FFT-ed only if the input is used
		for (u32 i = 0; i < kNumTotalChannels; i++)
			if (usedInputChannels_[i])
				transforms[getFFTPlan()]->transformRealForward(FFTBuffer.getWritePointer(i));
	}

	perf_inline void SoundEngine::ProcessFFT() noexcept
	{
		auto effectsStatePointer = static_cast<EffectsState *>(subModules_[0].get());

		effectsStatePointer->setFFTSize(FFTNumSamples_);
		effectsStatePointer->setSampleRate(sampleRate_);

		effectsStatePointer->writeInputData(FFTBuffer);
		effectsStatePointer->distributeData();
		effectsStatePointer->processChains();
		effectsStatePointer->sumChains();
		effectsStatePointer->writeOutputData(FFTBuffer);
	}

	perf_inline void SoundEngine::DoIFFT() noexcept
	{
		// in-place IFFT
		for (u32 i = 0; i < kNumTotalChannels; i++)
			if (usedOutputChannels_[i])
				transforms[getFFTPlan()]->transformRealInverse(FFTBuffer.getWritePointer(i));

		// if the FFT size is big enough to guarantee that even with max overlap 
		// a block >= samplesPerBlock can be finished, we don't offset
		// otherwise, we offset 2 block sizes back
		u32 latencyOffset = (getProcessingDelay() != FFTNumSamples_) ? 2 * samplesPerBlock_ : 0;
		outBuffer.setLatencyOffset(latencyOffset);

		// overlap-adding
		outBuffer.addOverlapBuffer(FFTBuffer, outBuffer.getNumChannels(), 
			usedOutputChannels_.data(),  FFTNumSamples_, nextOverlapOffset_);
	}

	// when the overlap is more than what the window requires
	// there will be an increase in gain, so we need to offset that
	perf_inline void SoundEngine::ScaleDown() noexcept
	{
		// TODO: use an extra overlap_ variable to store the overlap param 
		// from previous scaleDown run in order to apply extra attenuation 
		// when moving the overlap control (essentially becomes linear interpolation)
		static float lastOverlap = kDefaultWindowOverlap;

		float mult = 1.0f;
		u32 start = outBuffer.getToScaleOutput();
		u32 toScaleNumSamples = outBuffer.getToScaleOutputToAddOverlap();
		for (u32 i = 0; i < kNumTotalChannels; i++)
		{
			if (!usedOutputChannels_[i])
				continue;

			switch (windowType_)
			{
			case Framework::WindowTypes::Rectangle:
				break;
			case Framework::WindowTypes::Hann:
			case Framework::WindowTypes::Triangle:
				if (overlap_ <= 0.5f)
					break;

				mult = (1.0f - overlap_) * 2.0f;
				for (u32 j = 0; j < toScaleNumSamples; j++)
				{
					u32 sampleIndex = (start + j) % outBuffer.getSize();
					outBuffer.multiply(mult, i, sampleIndex);
				}
				break;
			case Framework::WindowTypes::Hamming:
				if (overlap_ <= 0.5f)
					break;

				// optimal multiplier empirically found
				// https://www.desmos.com/calculator/z21xz7r2c9
				mult = (1.0f - overlap_) * 1.84f;
				for (u32 j = 0; j < toScaleNumSamples; j++)
				{
					u32 sampleIndex = (start + j) % outBuffer.getSize();
					outBuffer.multiply(mult, i, sampleIndex);
				}
				break;
			case Framework::WindowTypes::Sine:
				if (overlap_ <= 0.33333333f)
					break;

				// optimal multiplier empirically found
				// https://www.desmos.com/calculator/mmjwlj0gqe
				mult = (1.0f - overlap_) * 1.57f;
				for (u32 j = 0; j < toScaleNumSamples; j++)
				{
					u32 sampleIndex = (start + j) % outBuffer.getSize();
					outBuffer.multiply(mult, i, sampleIndex);
				}
				break;
			case Framework::WindowTypes::Exponential:
				break;
			case Framework::WindowTypes::HannExponential:
				break;
			case Framework::WindowTypes::Lanczos:
				break;
			case Framework::WindowTypes::Blackman:
				break;
			case Framework::WindowTypes::BlackmanHarris:
				break;
			default:
				break;
			}
		}

		outBuffer.advanceToScaleOutput(toScaleNumSamples);
		lastOverlap = overlap_;
	}

	perf_inline void SoundEngine::MixOut(u32 numSamples) noexcept
	{
		if (!hasEnoughSamples_)
			return;

		// scale down only if we are moving
		if (nextOverlapOffset_ > 0)
			ScaleDown();

		// only wet
		if (getMix() == 1.0f)
		{
			inputBuffer.advanceLastOutputBlock(numSamples);
			return;
		}
		// TODO: FIX THIS
		static u32 prevFFTNumSamples = 1 << kDefaultFFTOrder;
		i32 FFTChangeOffset = prevFFTNumSamples - FFTNumSamples_;

		// only dry
		if (getMix() == 0.0f)
		{
			inputBuffer.outBufferRead(outBuffer.getBuffer(), kNumTotalChannels, usedOutputChannels_.data(),
				numSamples, outBuffer.getBeginOutput(), FFTChangeOffset - (i32)(outBuffer.getLatencyOffset()));

			// advancing buffer indices
			inputBuffer.advanceLastOutputBlock(numSamples);

			return;
		}
			
		// mix both
		float wetMix = getMix();
		float dryMix = 1.0f - wetMix;
		auto &dryBuffer = inputBuffer.getBuffer();
		for (u32 i = 0; i < kNumTotalChannels; i++)
		{
			if (!usedOutputChannels_[i])
				continue;

			u32 beginOutput = outBuffer.getBeginOutput();
			u32 outBufferSize = outBuffer.getSize();

			// mixing wet
			for (u32 j = 0; j < numSamples; j++)
			{
				u32 outSampleIndex = (beginOutput + j) % outBufferSize;
				outBuffer.multiply(wetMix, i, outSampleIndex);
			}
			
			i32 latencyOffset = FFTChangeOffset - (i32)outBuffer.getLatencyOffset();
			u32 inputBufferLastBlock = inputBuffer.getLastOutputBlock();
			u32 inputBufferSize = inputBuffer.getSize();

			// mixing dry
			for (u32 j = 0; j < numSamples; j++)
			{
				u32 outSampleIndex = (beginOutput + j) % outBufferSize;
				u32 inSampleIndex = ((u32)(inputBufferSize + inputBufferLastBlock + latencyOffset) + j) % inputBufferSize;

				outBuffer.add(dryBuffer.getSample(i, inSampleIndex) * dryMix, i, outSampleIndex);
			}
		}
		inputBuffer.advanceLastOutputBlock(numSamples);
	}

	perf_inline void SoundEngine::FillOutput(AudioBuffer<float> &buffer, u32 numOutputs, u32 numSamples) noexcept
	{
		// if we don't have enough samples we simply output silence
		if (!hasEnoughSamples_)
		{
			auto channelPointers = buffer.getArrayOfWritePointers();
			for (u32 i = 0; i < outBuffer.getNumChannels(); i++)
				std::memset(&channelPointers[i][0], 0, sizeof(float) * numSamples);
			return;
		}

		outBuffer.readOutput(buffer, numOutputs, usedOutputChannels_.data(), numSamples);
		outBuffer.advanceBeginOutput(numSamples);

		prevFFTNumSamples_ = FFTNumSamples_;
	}

	void SoundEngine::MainProcess(AudioBuffer<float> &buffer, u32 numSamples, u32 numInputs, u32 numOutputs) noexcept
	{
		// copying input in the main circular buffer
		CopyBuffers(buffer, numInputs, numSamples);

		while (true)
		{
			IsReadyToPerform(numSamples);
			if (!isPerforming_)
				break;
			
			UpdateParameters(UpdateFlag::Realtime);
			DoFFT();
			ProcessFFT();
			DoIFFT();
		}

		// copying and scaling the dry signal to the output
		MixOut(numSamples);
		// copying output to buffer
		FillOutput(buffer, numOutputs, numSamples);
	}
}