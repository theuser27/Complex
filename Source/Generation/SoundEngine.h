/*
	==============================================================================

		SoundEngine.h
		Created: 12 Aug 2021 2:12:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/utils.h"
#include "Framework/circular_buffer.h"
#include "Framework/windows.h"
#include "BaseProcessor.h"

namespace Framework
{
	class FFT;
}

namespace Plugin
{
	class ProcessorTree;
}

namespace Generation
{
	class EffectsState;

	class SoundEngine final : public BaseProcessor
	{
	public:
		SoundEngine(Plugin::ProcessorTree *processorTree, u64 parentProcessorId) noexcept;
		~SoundEngine() noexcept override;

		SoundEngine(const SoundEngine &) = delete;
		SoundEngine(SoundEngine &&) = delete;
		SoundEngine &operator= (SoundEngine &&) = delete;
		SoundEngine &operator= (const SoundEngine &) = delete;

	private:
		//=========================================================================================
		// Data
		//
		// pre FFT-ed data buffer; size is as big as it can be (while still being reasonable)
		class 
		{
		public:
			enum BeginPoint
			{ LastOutputBlock, BlockBegin, BlockEnd, End };

			void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
			{
				COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
				if ((newNumChannels <= getChannels()) && (newSize <= getSize()) && !fitToSize)
					return;

				// recalculating indices based on the new size
				if (getChannels() * getSize())
				{
					blockEnd_ = utils::clamp(((i32)newSize - (i32)getBlockEndToEnd()) % (i32)newSize, 0, (i32)newSize - 1);
					blockBegin_ = utils::clamp(((i32)blockEnd_ - (i32)getBlockBeginToBlockEnd()) % (i32)newSize, 0, (i32)newSize - 1);
					lastOutputBlock_ = utils::clamp(((i32)blockBegin_ - (i32)getLastOutputBlockToBlockBegin()) % (i32)newSize, 0, (i32)newSize - 1);
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

			strict_inline void advanceLastOutputBlock(i32 samples)
			{ lastOutputBlock_ = (lastOutputBlock_ + samples) % getSize(); }

			// used for manually advancing the block to a desired position
			strict_inline void advanceBlock(u32 newBegin, i32 samples)
			{
				blockBegin_ = newBegin;
				blockEnd_ = (newBegin + samples) % getSize();
			}

			// returns how many samples in the buffer can be read 
			// starting at blockBegin_/blockEnd_until end_
			strict_inline u32 newSamplesToRead(i32 overlapOffset = 0, BeginPoint beginPoint = BlockBegin) const noexcept
			{
				u32 begin;
				switch (beginPoint)
				{
				case BlockBegin:
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

			strict_inline void readBuffer(Framework::Buffer &reader, u32 channels,
				const bool* channelsToCopy, u32 samples, BeginPoint beginPoint = BeginPoint::BlockBegin, 
				i32 inputBufferOffset = 0, u32 readerBeginIndex = 0, bool advanceBlock = true) noexcept
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
				
				buffer_.readBuffer(reader, channels, samples, 
					currentBufferBegin, readerBeginIndex, channelsToCopy);

				if (advanceBlock)
					this->advanceBlock(currentBufferBegin, (i32)samples);
			}

			strict_inline void writeToBufferEnd(const float *const *const writer,
				u32 channels, u32 samples) noexcept
			{
				buffer_.writeToBufferEnd(writer, channels, samples);
			}

			strict_inline void outBufferRead(Framework::CircularBuffer &outBuffer,
				u32 channels, const bool* channelsToCopy, u32 samples, u32 outBufferIndex = 0, 
				i32 inputBufferOffset = 0, BeginPoint beginPoint = BeginPoint::LastOutputBlock) const noexcept
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
				buffer_.readBuffer(outBuffer.getData(), channels, samples, inputBufferIndex, outBufferIndex, channelsToCopy);
			}

			strict_inline Framework::CircularBuffer& getBuffer() noexcept {	return buffer_; }
			strict_inline u32 getChannels() const noexcept { return buffer_.getChannels(); }
			strict_inline u32 getSize() const noexcept { return buffer_.getSize(); }

			strict_inline u32 getLastOutputBlock() const noexcept { return lastOutputBlock_; }
			strict_inline u32 getBlockBegin() const noexcept { return blockBegin_; }
			strict_inline u32 getBlockEnd() const noexcept { return blockEnd_; }
			strict_inline u32 getEnd() const noexcept { return buffer_.getEnd(); }

			strict_inline u32 getLastOutputBlockToBlockBegin() const noexcept
			{ return (getSize() + blockBegin_ - lastOutputBlock_) % getSize(); }

			strict_inline u32 getBlockBeginToBlockEnd() const noexcept
			{ return (getSize() + blockEnd_ - blockBegin_) % getSize(); }

			strict_inline u32 getBlockEndToEnd() const noexcept
			{ return (getSize() + getEnd() - blockEnd_) % getSize(); }

		private:
			Framework::CircularBuffer buffer_;
			u32 lastOutputBlock_ = 0;
			u32 blockBegin_ = 0;
			u32 blockEnd_ = 0;

		} inputBuffer;
		//
		// FFT-ed data buffer, size is double the max FFT block
		// even indices - freqs; odd indices - phases
		Framework::Buffer FFTBuffer;
		float *FFTedBuffer[kNumTotalChannels]{};
		//
		// if an input isn't used there's no need to process it at all
		std::array<bool, kNumTotalChannels> usedInputChannels_{};
		std::array<bool, kNumTotalChannels> usedOutputChannels_{};
		//
		// output buffer containing dry and wet data
		class 
		{
		public:
			void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
			{
				COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
				if ((newNumChannels <= getNumChannels()) && (newSize <= getSize()) && !fitToSize)
					return;

				// recalculating indices based on the new size
				if (getNumChannels() * getSize())
				{
					addOverlap_ = utils::clamp(((i32)newSize - (i32)getAddOverlapToEnd()) % (i32)newSize, i32(0), (i32)newSize - 1);
					toScaleOutput_ = utils::clamp(((i32)addOverlap_ - (i32)getToScaleOutputToAddOverlap()) % (i32)newSize, i32(0), (i32)newSize - 1);
					beginOutput_ = utils::clamp(((i32)toScaleOutput_ - (i32)getBeginOutputToToScaleOutput()) % (i32)newSize, i32(0), (i32)newSize - 1);
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

			perf_inline void readOutput(float *const *outputBuffer, u32 outputs,
				const bool* channelsToCopy, u32 samples, float outGain) const noexcept
			{
				COMPLEX_ASSERT(outputs <= kNumTotalChannels);
				buffer_.readBuffer(outputBuffer, outputs, samples, getBeginOutput(), channelsToCopy);

				// zero out non-copied channels
				for (u32 i = 0; i < outputs; i++)
				{
					if (!channelsToCopy[i])
					{
						std::memset(outputBuffer[i], 0, samples * sizeof(float));
						continue;
					}
					
					if (outGain != 1.0f)
						for (u32 j = 0; j < samples; ++j)
							outputBuffer[i][j] *= outGain;
				}
			}

			perf_inline void addOverlapBuffer(const Framework::Buffer &other, u32 channels,
				const bool* channelsToOvelap, u32 samples, i32 beginOutputOffset, Framework::WindowTypes windowType) noexcept
			{
				u32 bufferSize = getSize();
				u32 oldEnd = getEnd();
				u32 newEnd = buffer_.setEnd((addOverlap_ + samples) % bufferSize);

				// getting how many samples are going to be overlapped
				// we clamp the value to max samples in case the FFT size was changed
				u32 overlappedSamples = std::min((bufferSize + oldEnd - addOverlap_) % bufferSize, samples);

				// writing stuff that isn't overlapped
				if (u32 assignSamples = samples - overlappedSamples; assignSamples)
				{
					buffer_.clear((bufferSize + newEnd - assignSamples) % bufferSize, assignSamples);
					buffer_.writeToBuffer(other, channels, assignSamples,
						(bufferSize + newEnd - assignSamples) % bufferSize, overlappedSamples, channelsToOvelap);
				}

				// overlapping
				if (overlappedSamples)
				{
					if (windowType == Framework::WindowTypes::Lerp)
						Framework::CircularBuffer::applyToBuffer<utils::MathOperations::Interpolate>(buffer_.getData(), other, channels, overlappedSamples,
							addOverlap_, 0, channelsToOvelap);
					else
					{
						// fading edges and overlapping
						u32 fadeSamples = overlappedSamples / 4;

						// fade in overlap
						Framework::CircularBuffer::applyToBuffer<utils::MathOperations::FadeInAdd>(buffer_.getData(), other, channels, fadeSamples,
							addOverlap_, 0, channelsToOvelap);

						// overlap
						buffer_.addBuffer(other, channels, overlappedSamples - 2 * fadeSamples,
							channelsToOvelap, (addOverlap_ + fadeSamples) % bufferSize, fadeSamples);


						// fade out overlap
						Framework::CircularBuffer::applyToBuffer<utils::MathOperations::FadeOutAdd>(buffer_.getData(), other, channels, fadeSamples,
							(addOverlap_ + overlappedSamples - fadeSamples) % bufferSize,
							overlappedSamples - fadeSamples, channelsToOvelap);
					}
				}					

				// offsetting the overlap index for the next block
				addOverlap_ = (addOverlap_ + beginOutputOffset) % bufferSize;
			}

			strict_inline void add(float value, u32 channel, u32 index) noexcept
			{ buffer_.add(value, channel, index); }

			strict_inline void multiply(float value, u32 channel, u32 index) noexcept
			{ buffer_.multiply(value, channel, index); }


			strict_inline void setLatencyOffset(i32 newLatencyOffset) noexcept
			{
				if (latencyOffset_ == newLatencyOffset)
					return;

				beginOutput_ = (getSize() - newLatencyOffset) % getSize();
				toScaleOutput_ = 0;
				addOverlap_ = 0;

				latencyOffset_ = newLatencyOffset;

				buffer_.clear();
			}

			strict_inline void advanceBeginOutput(u32 samples) noexcept
			{ beginOutput_ = (beginOutput_ + samples) % getSize(); }

			strict_inline void advanceToScaleOutput(u32 samples) noexcept
			{ toScaleOutput_ = (toScaleOutput_ + samples) % getSize(); }

			strict_inline void advanceAddOverlap(u32 samples) noexcept
			{ addOverlap_ = (addOverlap_ + samples) % getSize(); }


			strict_inline Framework::CircularBuffer& getBuffer() noexcept {	return buffer_; }
			strict_inline u32 getNumChannels() const noexcept { return buffer_.getChannels(); }
			strict_inline u32 getSize() const noexcept { return buffer_.getSize(); }

			strict_inline i32 getLatencyOffset() const noexcept { return latencyOffset_; }
			strict_inline u32 getBeginOutput() const noexcept { return beginOutput_; }
			strict_inline u32 getToScaleOutput() const noexcept { return toScaleOutput_; }
			strict_inline u32 getAddOverlap() const noexcept { return addOverlap_; }
			strict_inline u32 getEnd() const noexcept { return buffer_.getEnd(); }

			strict_inline u32 getBeginOutputToToScaleOutput() const noexcept
			{ return (getSize() + toScaleOutput_ - beginOutput_) % getSize(); }
			
			strict_inline u32 getToScaleOutputToAddOverlap() const noexcept
			{ return (getSize() + addOverlap_ - toScaleOutput_) % getSize(); }

			strict_inline u32 getAddOverlapToEnd() const noexcept
			{ return (getSize() + getEnd() - addOverlap_) % getSize(); }

		private:
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
		//
		// windows pointer for accessing windowing types
		Framework::Window windows{};
		//
		// pointer to an array of fourier transforms
		std::unique_ptr<Framework::FFT> transforms;
		//
		//
		EffectsState *effectsState_ = nullptr;

		//=========================================================================================
		// Methods
		//
		void CopyBuffers(const float *const *buffer, u32 numInputs, u32 samples) noexcept;
		void IsReadyToPerform(u32 samples) noexcept;
		void DoFFT() noexcept;		
		void ProcessFFT(float sampleRate) noexcept;
		void DoIFFT() noexcept;
		void ScaleDown() noexcept;
		void MixOut(u32 samples) noexcept;
		void FillOutput(float *const *buffer, u32 numOutputs, u32 samples) noexcept;

		// Inherited via BaseProcessor
		BaseProcessor *createCopy([[maybe_unused]] std::optional<u64> parentModuleId) const noexcept override
		{ COMPLEX_ASSERT_FALSE("You're trying to copy SoundEngine, which is not meant to be copied"); return nullptr; }
		BaseProcessor *createSubProcessor([[maybe_unused]] std::string_view type) const noexcept override
		{
			COMPLEX_ASSERT_FALSE("You're trying to create a subProcessor for SoundEngine, which is not meant to be happen");
			return nullptr;
		}

	public:
		// initialising pointers and FFT plans
		void ResetBuffers() noexcept;
		void UpdateParameters(UpdateFlag flag, float sampleRate) noexcept;
		void MainProcess(float *const *buffer, u32 samples,
			float sampleRate, u32 numInputs, u32 numOutputs) noexcept;

		u32 getProcessingDelay() const noexcept;
		auto &getEffectsState() const noexcept { return *effectsState_; }

		void setMix(float mix) noexcept { mix_ = mix; }
		void setFFTOrder(u32 order) noexcept { FFTOrder_ = order; }
		void setOverlap(float overlap) noexcept { overlap_ = overlap; }
		void setWindowType(Framework::WindowTypes type) noexcept { windowType_ = type; }

		void deserialiseFromJson(std::any jsonData);
	private:
		//=========================================================================================
		// Parameters
		//
		// 1. Master Mix
		// 2. Block Size
		// 3. Overlap
		// 4. Window Type
		// 5. Window Alpha
		// 6. Out Gain
		// 
		//=========================================================================================
		// Variables
		// 
		// mix amount with dry signal
		float mix_ = 1.0f;
		//
		// FFT order
		u32 FFTOrder_ = kDefaultFFTOrder;
		//
		// amount of overlap with previous block
		float overlap_ = kDefaultWindowOverlap;
		//
		// window type
		Framework::WindowTypes windowType_ = Framework::WindowTypes::Hann;
		//
		// window alpha
		float alpha_ = 0.0f;
		//
		// output gain
		float outGain_ = 1.0f;
		//
		// have we performed for this last run?
		bool isPerforming_ = false;
		//
		// do we have enough processed samples to output?
		bool hasEnoughSamples_ = false;
		//
		// current FFT plan in samples
		u32 FFTNumSamples_ = 1 << kDefaultFFTOrder;
		u32 prevFFTNumSamples_ = 1 << kDefaultFFTOrder;
		//
		// how many samples we are moving forward in the outBuffer after the current block
		u32 nextOverlapOffset_ = 0;
		u32 currentOverlapOffset_ = 0;

		//=========================================================================================
		// internal methods
		// 
		// getting the number of FFT samples
		u32 getFFTNumSamples() const noexcept { return 1 << FFTOrder_; }
		//
		// getting array position of FFT
		u32 getFFTPlan() const noexcept { return FFTOrder_ - kMinFFTOrder; }
		//
		// getting the dry/wet balance for the whole plugin
		float getMix() const noexcept { return mix_; }
		//
		// calculating the overlap offset
		u32 getOverlapOffset() const noexcept
		{ return (u32)std::floorf((float)FFTNumSamples_ * (1.0f - overlap_)); }
	};
}
