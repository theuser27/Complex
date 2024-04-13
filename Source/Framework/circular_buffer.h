/*
	==============================================================================

		circular_buffer.h
		Created: 7 Feb 2022 1:56:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "AppConfig.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "utils.h"

namespace Framework
{
	class CircularBuffer
	{
	public:
		CircularBuffer() = default;

		CircularBuffer(u32 numChannels, u32 size) noexcept : channels_(numChannels), size_(size)
		{
			COMPLEX_ASSERT(numChannels > 0 && size > 0);
			data_.setSize(numChannels, size);
		}

		// currently does memory allocation, be careful when calling
		void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false) noexcept
		{
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= channels_) && (newSize <= size_) && !fitToSize)
				return;

			juce::AudioBuffer<float> newData(newNumChannels, newSize);
			newData.clear();

			if (channels_ * size_)
			{
				// check for the smaller sizes, that is how much will be copied over
				u32 channelsToCopy = std::min(newNumChannels, channels_);
				u32 numSamplesToCopy = std::min(newSize, size_);
				u32 startCopy = (size_ + end_ - numSamplesToCopy) % size_;

				applyToBuffer(newData, data_, channelsToCopy, numSamplesToCopy, 0, startCopy);

				end_ = 0;
			}

			std::swap(newData, data_);

			size_ = newSize;
			channels_ = newNumChannels;
		}

		void clear(u32 begin, u32 numSamples) noexcept
		{
			if (begin + numSamples <= size_)
			{
				data_.clear(begin, numSamples);
				return;
			}

			u32 samplesLeft = begin + numSamples - size_;
			data_.clear(begin, size_ - begin);
			data_.clear(0, samplesLeft);
		}

		void clear() noexcept { data_.clear(); }

		u32 advanceEnd(u32 numSamples) noexcept { return end_ = (end_ + numSamples) % size_; }
		u32 setEnd(u32 index) noexcept { return end_ = index % size_; }

		// applies on operation on the samples of otherBuffer and thisBuffer 
		// and writes the results to the respective channels of thisBuffer
		// while anticipating wrapping around in both buffers 
		static void applyToBuffer(juce::AudioBuffer<float>& thisBuffer, const juce::AudioBuffer<float>& otherBuffer,
			size_t numChannels, size_t numSamples, size_t thisStartIndex, size_t otherStartIndex,
			const bool* channelsToApplyTo = nullptr, utils::MathOperations operation = utils::MathOperations::Assign) noexcept
		{
			COMPLEX_ASSERT(static_cast<size_t>(thisBuffer.getNumChannels()) >= numChannels);
			COMPLEX_ASSERT(static_cast<size_t>(otherBuffer.getNumChannels()) >= numChannels);
			COMPLEX_ASSERT(static_cast<size_t>(thisBuffer.getNumSamples()) >= numSamples);
			COMPLEX_ASSERT(static_cast<size_t>(otherBuffer.getNumSamples()) >= numSamples);

			float(*opFunction)(float, float, float);
			switch (operation)
			{
			case utils::MathOperations::Add:
				opFunction = [](float one, float two, float) { return one + two; };
				break;
			case utils::MathOperations::Multiply:
				opFunction = [](float one, float two, float) { return one * two; };
				break;
			case utils::MathOperations::FadeInAdd:
				opFunction = [](float one, float two, float t) { return (1 - t) * one + t * (one + two); };
				break;
			case utils::MathOperations::FadeOutAdd:
				opFunction = [](float one, float two, float t) { return (1 - t) * (one + two) + t * two; };
				break;
			case utils::MathOperations::Interpolate:
				opFunction = [](float one, float two, float t) { return (1 - t) * one + t * two; };
				break;
			default:
			case utils::MathOperations::Assign:
				opFunction = [](float, float two, float) { return two; };
				break;
			}

			float increment = 1.0f / (float)numSamples;
			auto thisPointers = thisBuffer.getArrayOfWritePointers();
			auto otherPointers = otherBuffer.getArrayOfReadPointers();

			auto iterate = [&]<bool UseBitAnd>()
			{
				size_t thisBufferSize, otherBufferSize;

				if constexpr (UseBitAnd)
				{
					thisBufferSize = thisBuffer.getNumSamples() - 1;
					otherBufferSize = otherBuffer.getNumSamples() - 1;
				}
				else
				{
					thisBufferSize = thisBuffer.getNumSamples();
					otherBufferSize = otherBuffer.getNumSamples();
				}

				for (size_t i = 0; i < numChannels; i++)
				{
					if (channelsToApplyTo && !channelsToApplyTo[i])
						continue;

					float t = 0.0f;
					for (size_t k = 0; k < numSamples; k++)
					{
						size_t thisIndex, otherIndex;
						if constexpr (UseBitAnd)
						{
							thisIndex = (thisStartIndex + k) & thisBufferSize;
							otherIndex = (otherStartIndex + k) & otherBufferSize;
						}
						else
						{
							thisIndex = (thisStartIndex + k) % thisBufferSize;
							otherIndex = (otherStartIndex + k) % otherBufferSize;
						}

						thisPointers[i][thisIndex] = opFunction(thisPointers[i][thisIndex], 
							otherPointers[i][otherIndex], t);

						t += increment;
					}
				}
			};

			if (utils::isPowerOfTwo(static_cast<size_t>(thisBuffer.getNumSamples())) &&
				utils::isPowerOfTwo(static_cast<size_t>(otherBuffer.getNumSamples())))
				iterate.template operator()<true>();
			else
				iterate.template operator()<false>();
		}

		// - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
		//	readee's starting index = readerIndex + end_ and 
		//	reader's starting index = readeeIndex
		// - Can decide whether to advance the block or not
		void readBuffer(juce::AudioBuffer<float> &reader, u32 numChannels,u32 numSamples, 
			u32 readeeIndex = 0, u32 readerIndex = 0, const bool* channelsToRead = nullptr) const noexcept
		{
			applyToBuffer(reader, data_, numChannels, numSamples, readerIndex, readeeIndex, channelsToRead);
		}

		// - A specified AudioBuffer writes own data starting at writerOffset to the end of the current buffer,
		//	which can be offset forwards or backwards with writeeOffset
		// - Adjusts end_ according to the new block written
		// - Returns the new endpoint
		u32 writeToBufferEnd(const juce::AudioBuffer<float> &writer, u32 numChannels, u32 numSamples,
			u32 writerIndex = 0, const bool *channelsToWrite = nullptr) noexcept
		{
			applyToBuffer(data_, writer, numChannels, numSamples, end_, writerIndex, channelsToWrite);
			return advanceEnd(numSamples);
		}

		void writeToBuffer(const juce::AudioBuffer<float>& writer, u32 numChannels, u32 numSamples,
			u32 writeeIndex, u32 writerIndex = 0, const bool* channelsToWrite = nullptr) noexcept
		{
			applyToBuffer(data_, writer, numChannels, numSamples, writeeIndex, writerIndex, channelsToWrite);
		}

		void add(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.setSample(channel, index, data_.getSample(channel, index) + value); 
		}

		void addBuffer(const juce::AudioBuffer<float> &other, u32 numChannels, u32 numSamples,
			const bool *channelsToAdd = nullptr, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			applyToBuffer(data_, other, numChannels, numSamples, thisStartIndex, 
				otherStartIndex, channelsToAdd, utils::MathOperations::Add);
		}

		void multiply(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.setSample(channel, index, data_.getSample(channel, index) * value);
		}

		void multiplyBuffer(const juce::AudioBuffer<float> &other, u32 numChannels, u32 numSamples,
			const bool *channelsToAdd = nullptr, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			applyToBuffer(data_, other, numChannels, numSamples, thisStartIndex, 
				otherStartIndex, channelsToAdd, utils::MathOperations::Multiply);
		}

		float getSample(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			return data_.getSample(channel, index);
		}

		auto &getData() noexcept { return data_; }
		u32 getNumChannels() const noexcept { return channels_; }
		u32 getSize() const noexcept { return size_; }
		u32 getEnd() const noexcept { return end_; }

	private:
		u32 channels_ = 0;
		u32 size_ = 0;
		u32 end_ = 0;

		juce::AudioBuffer<float> data_;
	};
}