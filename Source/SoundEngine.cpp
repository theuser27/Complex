/*
	==============================================================================

	SoundEngine.cpp
	Created: 12 Aug 2021 2:12:59am
	Author:  Lenovo

	==============================================================================
*/

#include "SoundEngine.h"

namespace Generation
{
	SoundEngine::SoundEngine()
	{
		transforms.reserve(kMaxFFTOrder - kMinFFTOrder + 1);
		for (u32 i = 0; i < (kMaxFFTOrder - kMinFFTOrder + 1); i++)
		{
			u32 bits = kMinFFTOrder + i;
			transforms.emplace_back(std::make_shared<Framework::FFT>(bits));
		}

		inputBuffer.reserve(kNumTotalChannels, kMaxPreBufferLength);
		FFTBuffer.setSize(kNumTotalChannels, kMaxFFTBufferLength, false, true);
		outBuffer.reserve(kNumTotalChannels, kMaxFFTBufferLength);
		//isFirstRun_ = true;

		// assigning first input to first chain
		chainInputs[0] = 0;
		// assigning the first chain to the first output
		chainOutputs[0] = 0;
	}
	SoundEngine::~SoundEngine() = default;

	void SoundEngine::Initialise(double sampleRate, u32 samplesPerBlock)
	{
		sampleRate_ = (sampleRate >= kDefaultSampleRate) ? sampleRate : kDefaultSampleRate;
		samplesPerBlock_ = samplesPerBlock;
	}

	force_inline void SoundEngine::UpdateParameters()
	{
	}

	/*force_inline*/ void SoundEngine::CopyBuffers(AudioBuffer<float> &buffer, u32 numInputs, u32 numSamples)
	{
		//// skipping silence can lead to weird behaviour when receiving new non-silent blocks
		// checking whether we even have anything or the buffer is pure silence
		/*isInputSilent_ = true;
		for (int i = 0; i < numInputs; i++)
		{
			if (!utils::isSilent(buffer.getReadPointer(i), numSamples))
			{
				isInputSilent_ = false;
				break;
			}
		}
		// if the input is silent, don't write anything
		if (isInputSilent_)
			return;*/

		// assume that we don't get blocks bigger than our buffer size
		inputBuffer.writeBuffer(buffer, numInputs, numSamples);
	}

	/*force_inline*/ void SoundEngine::IsReadyToPerform(u32 numSamples)
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

		//FFTBuffer.clear();
		for (size_t i = 0; i < FFTBuffer.getNumChannels(); i++)
		{
			i32 samplesToClear = std::max(FFTChangeOffset, 0);
			utils::zeroBuffer(FFTBuffer.getWritePointer(i, FFTNumSamples_), samplesToClear);
		}

		inputBuffer.readBuffer(FFTBuffer, FFTBuffer.getNumChannels(), FFTNumSamples_,
			inputBuffer.BlockBegin, nextOverlapOffset_ + FFTChangeOffset);

		// getting the next overlapOffset
		nextOverlapOffset_ = getOverlapOffset();

		isPerforming_ = true;
	}

	/*force_inline*/ void SoundEngine::DoFFT()
	{
		// windowing
		windows.applyWindow(FFTBuffer, FFTBuffer.getNumChannels(), FFTNumSamples_, windowType_, alpha_);

		// in-place FFT
		for (size_t i = 0; i < kNumTotalChannels; i++)
			transforms[getFFTPlan()]->transformRealForward(FFTBuffer.getWritePointer(i));
	}

	/*force_inline*/ void SoundEngine::ProcessFFT()
	{
		effectsState.setFFTSize(FFTNumSamples_);
		effectsState.setSampleRate(sampleRate_);

		effectsState.writeInputData(FFTBuffer);
		effectsState.distributeData(chainInputs);
		effectsState.processChains();
		effectsState.sumChains(chainOutputs);
		effectsState.writeOutputData(FFTBuffer);
	}

	/*force_inline*/ void SoundEngine::DoIFFT()
	{
		// in-place IFFT
		for (u32 i = 0; i < kNumTotalChannels; i++)
			transforms[getFFTPlan()]->transformRealInverse(FFTBuffer.getWritePointer(i));


		// if the FFT size is big enough to guarantee that even with max overlap 
		// a block >= samplesPerBlock can be finished, we don't offset
		// otherwise, we offset 2 block sizes back
		u32 latencyOffset = (getProcessingDelay() != FFTNumSamples_) ? 2 * samplesPerBlock_ : 0;
		outBuffer.setLatencyOffset(latencyOffset);

		// overlap-adding
		outBuffer.addOverlapBuffer(FFTBuffer, outBuffer.getNumChannels(), FFTNumSamples_, nextOverlapOffset_);
	}

	// when the overlap is more than what the window requires
	// there will be an increase in gain, so we need to offset that
	void SoundEngine::ScaleDown()
	{
		// TODO: use an extra overlap_ variable to store the overlap param 
		// from previous scaleDown run in order to apply extra attenuation 
		// when moving the overlap control (essentially becomes linear interpolation)
		static float lastOverlap = kDefaultWindowOverlap;

		float mult = 1.0f;
		u32 start = outBuffer.getToScaleOutput();
		u32 toScaleNumSamples = outBuffer.getToScaleOutputToAddOverlap();
		for (u32 i = 0; i < kNumChannels; i++)
		{
			switch (windowType_)
			{
			case Framework::WindowTypes::Rectangle:
				break;
			case Framework::WindowTypes::Hann:
			case Framework::WindowTypes::Hamming:
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
			case Framework::WindowTypes::Sine:
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

	/*force_inline */ void SoundEngine::MixOut(u32 numSamples)
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

		static u32 prevFFTNumSamples = 1 << kDefaultFFTOrder;
		i32 FFTChangeOffset = prevFFTNumSamples - FFTNumSamples_;

		// only dry
		if (getMix() == 0.0f)
		{
			inputBuffer.outBufferRead(outBuffer.buffer_, kNumChannels,
				numSamples, outBuffer.getBeginOutput(), FFTChangeOffset - (i32)(outBuffer.getLatencyOffset()));

			// advancing buffer indices
			inputBuffer.advanceLastOutputBlock(numSamples);

			return;
		}
			
		// mix both
		float wetMix = getMix();
		float dryMix = 1.0f - wetMix;
		for (u32 i = 0; i < kNumChannels; i++)
		{
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

				outBuffer.add(inputBuffer.getSample(i, inSampleIndex) * dryMix, i, outSampleIndex);
			}
		}
		inputBuffer.advanceLastOutputBlock(numSamples);
	}

	void SoundEngine::FillOutput(AudioBuffer<float> &buffer, u32 numOutputs, u32 numSamples)
	{
		// if we don't have enough samples we simply output silence
		if (!hasEnoughSamples_)
		{
			auto channelPointers = buffer.getArrayOfWritePointers();
			for (u32 i = 0; i < outBuffer.getNumChannels(); i++)
				std::memset(&channelPointers[i][0], 0, sizeof(float) * numSamples);
			return;
		}

		outBuffer.readOutput(buffer, numSamples);
		outBuffer.advanceBeginOutput(numSamples);

		prevFFTNumSamples_ = FFTNumSamples_;
	}

	void SoundEngine::MainProcess(AudioBuffer<float> &buffer, int numSamples, int numInputs, int numOutputs)
	{
		// copying input in the main circular buffer
		CopyBuffers(buffer, numInputs, numSamples);

		while (true)
		{
			IsReadyToPerform(numSamples);
			if (!isPerforming_)
				break;
			
			DoFFT();
			// TODO: poll/update variables; check whether it makes sense to do it here
			UpdateParameters();
			ProcessFFT();
			DoIFFT();
		}

		// copying and scaling the dry signal to the output
		MixOut(numSamples);
		// copying output to buffer
		FillOutput(buffer, numOutputs, numSamples);
	}
}