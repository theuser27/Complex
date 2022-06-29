/*
	==============================================================================

		SoundEngine.h
		Created: 12 Aug 2021 2:12:59am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "./Framework/common.h"
#include "./Framework/fourier_transform.h"
#include "./Framework/utils.h"
#include "./Framework/circular_buffer.h"
#include "./Framework/simd_buffer.h"
#include "./Framework/windows.h"
#include "./Framework/spectral_support_functions.h"
#include "EffectModules.h"
#include "EffectsState.h"


namespace Generation
{
	class SoundEngine
	{
	public:
		SoundEngine();
		~SoundEngine();

	private:
		//=========================================================================================
		// Data

		// pre FFT-ed data buffer; size is as big as it can be (while still being reasonable)
		struct
		{
			enum BeginPoint
			{ LastOutputBlock, BlockBegin, BlockEnd, End };

			void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
			{
				COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
				if ((newNumChannels <= getNumChannels()) && (newSize <= getSize()) && !fitToSize)
					return;

				// recalculating indices based on the new size
				if (getNumChannels() * getSize())
				{
					blockEnd_ = std::clamp(((i32)newSize - (i32)getBlockEndToEnd()) % (i32)newSize, i32(0), (i32)newSize - 1);
					blockBegin_ = std::clamp(((i32)blockEnd_ - (i32)getBlockBeginToBlockEnd()) % (i32)newSize, i32(0), (i32)newSize - 1);
					lastOutputBlock_ = std::clamp(((i32)blockBegin_ - (i32)getLastOutputBlockToBlockBegin()) % (i32)newSize, i32(0), (i32)newSize - 1);
				}
				else
				{
					// sanity check
					lastOutputBlock_ = 0;
					blockBegin_ = 0;
					blockEnd_ = 0;
				}

				buffer_.reserve(newNumChannels, newSize, fitToSize);
			}

			perf_inline void advanceLastOutputBlock(i32 numSamples)
			{ lastOutputBlock_ = (lastOutputBlock_ + numSamples) % getSize(); }

			// used for manually advancing the block to a desired position
			perf_inline void advanceBlock(u32 newBegin, i32 numSamples)
			{
				blockBegin_ = newBegin;
				blockEnd_ = (newBegin + numSamples) % getSize();
			}

			// returns how many samples in the buffer can be read 
			// starting at blockBegin_/blockEnd_until end_
			strict_inline u32 newSamplesToRead(i32 overlapOffset = 0, BeginPoint beginPoint = BeginPoint::BlockBegin) const noexcept
			{
				u32 begin = 0;
				switch (beginPoint)
				{
				case BeginPoint::BlockBegin:
					begin = blockBegin_;
					break;
				default:
					begin = blockEnd_;
					break;
				}

				// calculating the start position of the current block
				u32 currentBufferStart = (begin + overlapOffset) % getSize();

				// calculating how many samples can be read from the current start position
				return (getSize() + getEnd() - currentBufferStart) % getSize();
			}

			perf_inline void readBuffer(AudioBuffer<float> &reader, u32 numChannels,
				u32 numSamples, BeginPoint beginPoint = BeginPoint::BlockBegin, i32 inputBufferOffset = 0,
				u32 readerBeginIndex = 0, bool advanceBlock = true) noexcept
			{
				u32 begin = 0;
				switch (beginPoint)
				{
				case BeginPoint::LastOutputBlock:
					begin = lastOutputBlock_;
					break;
				case BeginPoint::BlockBegin:
					begin = blockBegin_;
					break;
				case BeginPoint::BlockEnd:
					begin = blockEnd_;
					break;
				case BeginPoint::End:
					begin = getEnd();
					break;
				}

				u32 currentBufferBegin = (getSize() + begin + inputBufferOffset) % getSize();
				
				buffer_.readBuffer(reader, numChannels, numSamples, currentBufferBegin, readerBeginIndex);
				if (advanceBlock)
					this->advanceBlock(currentBufferBegin, numSamples);
			}

			perf_inline void writeBuffer(const AudioBuffer<float> &writer, u32 numChannels, u32 numSamples,
				u32 writerIndex = 0, utils::Operations operation = utils::Operations::Assign) noexcept
			{
				buffer_.writeBuffer(writer, numChannels, numSamples, writerIndex, operation);
			}

			perf_inline void outBufferRead(Framework::CircularBuffer &outBuffer,
				u32 numChannels, u32 numSamples, u32 outBufferIndex = 0, i32 inputBufferOffset = 0,
				BeginPoint beginPoint = BeginPoint::LastOutputBlock) noexcept
			{
				u32 inputBufferBegin = 0;
				switch (beginPoint)
				{
				case BeginPoint::LastOutputBlock:
					inputBufferBegin = lastOutputBlock_;
					break;
				case BeginPoint::BlockBegin:
					inputBufferBegin = blockBegin_;
					break;
				case BeginPoint::BlockEnd:
					inputBufferBegin = blockEnd_;
					break;
				case BeginPoint::End:
					inputBufferBegin = getEnd();
					break;
				}

				u32 inputBufferIndex = (getSize() + inputBufferBegin + inputBufferOffset) % getSize();
				buffer_.readBuffer(outBuffer.getData(), numChannels, numSamples, inputBufferIndex, outBufferIndex);
			}

			strict_inline float getSample(u32 channel, u32 index) const noexcept
			{ return buffer_.getSample(channel, index); }

			strict_inline u32 getNumChannels() const noexcept
			{ return buffer_.getNumChannels(); }

			strict_inline u32 getSize() const noexcept
			{ return buffer_.getSize(); }

			strict_inline u32 getLastOutputBlock() const noexcept
			{ return lastOutputBlock_; }

			strict_inline u32 getBlockBegin() const noexcept
			{ return blockBegin_; }

			strict_inline u32 getBlockEnd() const noexcept
			{ return blockEnd_; }

			strict_inline u32 getEnd() const noexcept
			{ return buffer_.getEnd(); }

			perf_inline u32 getLastOutputBlockToBlockBegin() const noexcept
			{ return (getSize() + blockBegin_ - lastOutputBlock_) % getSize(); }

			perf_inline u32 getBlockBeginToBlockEnd() const noexcept
			{ return (getSize() + blockEnd_ - blockBegin_) % getSize(); }

			perf_inline u32 getBlockEndToEnd() const noexcept
			{ return (getSize() + getEnd() - blockEnd_) % getSize(); }

			Framework::CircularBuffer buffer_;
			u32 lastOutputBlock_ = 0;
			u32 blockBegin_ = 0;
			u32 blockEnd_ = 0;

		} inputBuffer;
		

		// FFT-ed data buffer, size is double the max FFT block
		// even indices - freqs; odd indices - phases
		AudioBuffer<float> FFTBuffer;

		// a master object that controls all effects chains
		EffectsState effectsState;

		// volume of the incoming dry signal(s)
		static simd_float inputVolume;

		// output buffer containing dry and wet data
		struct
		{
			void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
			{
				COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
				if ((newNumChannels <= getNumChannels()) && (newSize <= getSize()) && !fitToSize)
					return;

				// recalculating indices based on the new size
				if (getNumChannels() * getSize())
				{
					addOverlap_ = std::clamp(((i32)newSize - (i32)getAddOverlapToEnd()) % (i32)newSize, i32(0), (i32)newSize - 1);
					toScaleOutput_ = std::clamp(((i32)addOverlap_ - (i32)getToScaleOutputToAddOverlap()) % (i32)newSize, i32(0), (i32)newSize - 1);
					beginOutput_ = std::clamp(((i32)toScaleOutput_ - (i32)getBeginOutputToToScaleOutput()) % (i32)newSize, i32(0), (i32)newSize - 1);
				}
				else
				{
					// sanity check
					beginOutput_ = 0;
					toScaleOutput_ = 0;
					addOverlap_ = 0;
				}

				buffer_.reserve(newNumChannels, newSize, fitToSize);
			}

			perf_inline void readOutput(AudioBuffer<float> &output, u32 numSamples) noexcept
			{
				buffer_.readBuffer(output, output.getNumChannels(), numSamples, getBeginOutput(), 0);
			}

			perf_inline void addOverlapBuffer(const AudioBuffer<float> &other,
				u32 numChannels, u32 numSamples, i32 beginOutputOffset) noexcept
			{
				// clearing samples from previous blocks
				u32 oldEnd = getEnd();
				buffer_.setEnd((addOverlap_ + numSamples) % getSize());
				buffer_.clear(oldEnd, (getSize() + getEnd() - oldEnd) % getSize());

				buffer_.addBuffer(other, numChannels, numSamples, addOverlap_, 0);

				// offsetting the overlap index for the next block
				addOverlap_ = (addOverlap_ + beginOutputOffset) % getSize();
			}

			strict_inline void add(float value, u32 channel, u32 index) noexcept
			{ buffer_.add(value, channel, index); }

			strict_inline void multiply(float value, u32 channel, u32 index) noexcept
			{ buffer_.multiply(value, channel, index); }


			perf_inline void setLatencyOffset(i32 newLatencyOffset) noexcept
			{
				if (latencyOffset_ == newLatencyOffset)
					return;

				beginOutput_ = (getSize() - newLatencyOffset) % getSize();
				toScaleOutput_ = 0;
				addOverlap_ = 0;

				latencyOffset_ = newLatencyOffset;

				buffer_.clear();
			}

			strict_inline void advanceBeginOutput(u32 numSamples) noexcept
			{ beginOutput_ = (beginOutput_ + numSamples) % getSize(); }

			strict_inline void advanceToScaleOutput(u32 numSamples) noexcept
			{ toScaleOutput_ = (toScaleOutput_ + numSamples) % getSize(); }

			strict_inline void advanceAddOverlap(u32 numSamples) noexcept
			{ addOverlap_ = (addOverlap_ + numSamples) % getSize(); }


			strict_inline u32 getNumChannels() const noexcept
			{ return buffer_.getNumChannels(); }

			strict_inline u32 getSize() const noexcept
			{ return buffer_.getSize(); }

			strict_inline i32 getLatencyOffset() const noexcept
			{ return latencyOffset_; }
			
			strict_inline u32 getBeginOutput() const noexcept
			{ return beginOutput_; }
			
			strict_inline u32 getToScaleOutput() const noexcept
			{ return toScaleOutput_; }

			strict_inline u32 getAddOverlap() const noexcept
			{ return addOverlap_; }

			strict_inline u32 getEnd() const noexcept
			{ return buffer_.getEnd(); }

			perf_inline u32 getBeginOutputToToScaleOutput() const noexcept
			{ return (getSize() + toScaleOutput_ - beginOutput_) % getSize(); }
			
			perf_inline u32 getToScaleOutputToAddOverlap() const noexcept
			{ return (getSize() + addOverlap_ - toScaleOutput_) % getSize(); }

			perf_inline u32 getAddOverlapToEnd() const noexcept
			{ return (getSize() + getEnd() - addOverlap_) % getSize(); }

			Framework::CircularBuffer buffer_;
			// static offset equal to the additional latency caused by overlap
			i32 latencyOffset_ = 0;
			// index of the first new sample that can be output
			u32 beginOutput_ = 0;
			// index of the first add-overlapped sample that hasn't been scaled
			u32 toScaleOutput_ = 0;
			// index of the first sample of the last add-overlapped block
			u32 addOverlap_ = 0;
		} outBuffer;
		

		// windows pointer for accessing windowing types
		Framework::Window windows;

		// pointer to an array of fourier transforms
		std::vector<std::shared_ptr<Framework::FFT>> transforms;

	public:
		// initialising pointers and FFT plans
		void Initialise(double sampleRate, u32 samplesPerBlock);
		void UpdateParameters();
		void CopyBuffers(AudioBuffer<float> &buffer, u32 numInputs, u32 numSamples);
		void IsReadyToPerform(u32 numSamples);
		void DoFFT();
		void ProcessFFT();
		void ScaleDown();
		void DoIFFT();
		void MixOut(u32 numSamples);
		void FillOutput(AudioBuffer<float> &buffer, u32 numOutputs, u32 numSamples);
		void MainProcess(AudioBuffer<float> &buffer, int numSamples, int numInputs, int numOutputs);

		// Getter Methods
		//
		strict_inline u32 getProcessingDelay() const noexcept { return FFTNumSamples_ + samplesPerBlock_; }
		strict_inline u32 getSamplesPerBlock() const noexcept { return samplesPerBlock_; }
		strict_inline u32 getSampleRate() const noexcept { return sampleRate_; }

		// Setter Methods
		//
		strict_inline void setMix(float mix) noexcept { mix_ = mix; }
		strict_inline void setFFTOrder(u32 order) noexcept { FFTOrder_ = order; }
		strict_inline void setOverlap(float overlap) noexcept { overlap_ = overlap; }
		strict_inline void setWindowType(Framework::WindowTypes type) noexcept { windowType_ = type; }
		//strict_inline void setPowerMatching(bool isPowerMatching) noexcept { isPowerMatching_ = isPowerMatching; }

	private:
		//=========================================================================================
		// Variables
		//
		double sampleRate_;
		u32 samplesPerBlock_;
		bool isInitialised = false;
		// have we performed for this last run?
		bool isPerforming_ = false;
		// do we have enough processed samples to output?
		bool hasEnoughSamples_ = false;
		// current FFT plan in samples
		u32 FFTNumSamples_ = 1 << kDefaultFFTOrder;
		u32 prevFFTNumSamples_ = 1 << kDefaultFFTOrder;
		// how many samples we are moving forward in the outBuffer after the current block
		u32 nextOverlapOffset_ = 0;
		u32 currentOverlapOffset_ = 0;
		// it this is the first run, we don't offset
		//bool isFirstRun_ = true;
		// if all the samples have been delivered, procede with performing
		//u32 samplesDelivered = 0;

		//=========================================================================================
		// global parameters
		//
		// mixback amount with dry signal
		float mix_ = 1.0f;
		// power matching with input signal
		//bool isPowerMatching_;
		// amount of overlap with previous block
		float overlap_ = kDefaultWindowOverlap;
		// window type
		Framework::WindowTypes windowType_ = Framework::WindowTypes::Hann;
		// window alpha
		float alpha_ = 0.0f;
		// FFT order
		u32 FFTOrder_ = kDefaultFFTOrder;
		// input routing for the chains
		std::array<u32, kMaxNumChains> chainInputs;
		// output routing for the chains
		std::array<u32, kMaxNumChains> chainOutputs;

		//=========================================================================================
		// internal methods
		// 
		// getting the number of FFT samples
		strict_inline u32 getFFTNumSamples() const noexcept { return (u32)1 << FFTOrder_; }
		// getting array position of FFT
		strict_inline u32 getFFTPlan() const noexcept { return FFTOrder_ - common::kMinFFTOrder; }
		// getting the dry/wet balance for the whole plugin
		strict_inline float getMix() const noexcept { return mix_; }
		// calculating the overlap offset
		// if this is the first run, we don't move
		strict_inline u32 getOverlapOffset() const noexcept
		{ return (u32)std::floorf((float)FFTNumSamples_ * (1.0f - overlap_)); }
		//{ return (!isFirstRun_) ? (u32)std::floorf((float)FFTNumSamples_ * (1.0f - overlap_)) : 0; }
	};
}