/*
	==============================================================================

		circular_buffer.h
		Created: 7 Feb 2022 1:56:59am
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include <memory>

#include "common.h"
#include "utils.h"

namespace Framework
{
	class CircularBuffer
	{
	public:

		CircularBuffer() = default;

		CircularBuffer(u32 numChannels, u32 size) : channels_(numChannels), size_(size)
		{
			COMPLEX_ASSERT(numChannels > 0 && size > 0);
			data_.setSize(numChannels, size);
		}

		// currently does memory allocation, be careful when calling
		void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
		{
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= channels_) && (newSize <= size_) && !fitToSize)
				return;

			AudioBuffer<float> newData(newNumChannels, newSize);
			newData.clear();

			if (channels_ * size_)
			{
				// check for the smaller sizes, that is how much will be copied over
				u32 channelsToCopy = (newNumChannels < channels_) ? newNumChannels : channels_;
				u32 numSamplesToCopy = (newSize < size_) ? newSize : size_;
				u32 startCopy = (size_ + end_ - numSamplesToCopy) % size_;

				utils::copyBuffer(newData, data_, channelsToCopy, numSamplesToCopy, 0, startCopy);

				end_ = 0;
			}

			std::swap(newData, data_);

			size_ = newSize;
			channels_ = newNumChannels;
		}

		force_inline void clear(u32 begin, u32 numSamples) noexcept
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

		force_inline void clear() noexcept
		{ data_.clear(); }

		force_inline void advanceEnd(u32 numSamples) noexcept
		{ end_ = (end_ + numSamples) % size_; }

		force_inline void setEnd(u32 index) noexcept
		{ end_ = index % size_; }

		// - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
		//	readee's starting index = readerIndex + end_ and 
		//	reader's starting index = readeeIndex
		// - Can decide whether to advance the block or not
		/*force_inline*/ void readBuffer(AudioBuffer<float> &reader, u32 numChannels,
			u32 numSamples, u32 readeeIndex = 0, u32 readerIndex = 0) noexcept
		{
			utils::copyBuffer(reader, data_, numChannels, numSamples, readerIndex, readeeIndex);
		}

		// - A specified AudioBuffer writes own data starting at writerOffset to the end of the current buffer,
		//	which can be offset forwards or backwards with writeeOffset
		// - Adjusts end_ according to the new block written
		/*force_inline*/ void writeBuffer(const AudioBuffer<float> &writer, u32 numChannels, u32 numSamples,
			u32 writerIndex = 0, utils::Operations operation = utils::Operations::Assign) noexcept
		{
			utils::copyBuffer(data_, writer, numChannels, numSamples, end_, writerIndex, operation);
			advanceEnd(numSamples);
		}

		force_inline void add(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.setSample(channel, index, data_.getSample(channel, index) + value); 
		}

		/*force_inline*/ void addBuffer(const AudioBuffer<float> &other, u32 numChannels,
			u32 numSamples, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			utils::copyBuffer(data_, other, numChannels, numSamples, 
				thisStartIndex, otherStartIndex, utils::Operations::Add);
		}

		force_inline void multiply(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.setSample(channel, index, data_.getSample(channel, index) * value);
		}

		/*force_inline*/ void multiplyBuffer(const AudioBuffer<float> &other, u32 numChannels,
			u32 numSamples, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			utils::copyBuffer(data_, other, numChannels, numSamples, 
				thisStartIndex, otherStartIndex, utils::Operations::Multiply);
		}

		force_inline float getSample(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			return data_.getSample(channel, index);
		}

		force_inline auto& getData() noexcept
		{ return data_; }

		force_inline u32 getNumChannels() const noexcept
		{ return channels_; }

		force_inline u32 getSize() const noexcept
		{ return size_; }

		force_inline u32 getEnd() const noexcept
		{ return end_; }

	private:
		u32 channels_ = 0, size_ = 0;
		u32 end_ = 0;

		AudioBuffer<float> data_;
	};
}