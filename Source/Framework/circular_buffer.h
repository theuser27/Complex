/*
	==============================================================================

		circular_buffer.h
		Created: 7 Feb 2022 1:56:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "utils.h"
#include "memory_block.h"

namespace Framework
{
	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(size_t channels, size_t size, bool initialiseToZero = false) noexcept :
			data_(channels * size, initialiseToZero), channels_(channels), size_(size) { }

		void reserve(size_t newNumChannels, size_t newSize, bool fitToSize = false) noexcept
		{
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= getChannels()) && (newSize <= getSize()) && !fitToSize)
				return;

			MemoryBlock<float, 64> newData(newNumChannels * newSize, true);

			if (getChannels() * getSize())
			{
				// check for the smaller sizes, that is how much will be copied over
				size_t channelsToCopy = std::min(newNumChannels, getChannels());
				size_t samplesToCopy = std::min(newSize, getSize());

				for (size_t i = 0; i < channelsToCopy; ++i)
					newData.copy(data_, i * newSize, i * size_, samplesToCopy);
			}

			data_.swap(newData);

			size_ = newSize;
			channels_ = newNumChannels;
		}

		void clear() noexcept { data_.clear(); }
		void clear(size_t begin, size_t samples) noexcept
		{
			COMPLEX_ASSERT(begin + samples <= getSize());
			
			for (size_t i = 0; i < channels_; ++i)
				memset(data_.getData().get() + size_ * i + begin, 0, samples * sizeof(float));
		}

		float read(size_t channel, size_t index) const noexcept
		{
			COMPLEX_ASSERT(channel * size_ + index < channels_ * size_);
			return data_[channel * size_ + index];
		}

		void write(float value, size_t channel, size_t index) noexcept
		{
			COMPLEX_ASSERT(channel * size_ + index < channels_ * size_);
			data_[channel * size_ + index] = value;
		}

		size_t getChannels() const noexcept { return channels_; }
		size_t getSize() const noexcept { return size_; }
		auto &getData() noexcept { return data_; }
		const auto &getData() const noexcept { return data_; }

	private:
		MemoryBlock<float, 64> data_{};
		size_t channels_ = 0;
		size_t size_ = 0;
	};

	class CircularBuffer
	{
	public:
		CircularBuffer() = default;

		CircularBuffer(u32 channels, u32 size) noexcept : data_(channels, size), 
			channels_(channels), size_(size) { }

		void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false) noexcept
		{
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= getChannels()) && (newSize <= getSize()) && !fitToSize)
				return;

			Buffer newData(newNumChannels, newSize, true);

			if (getChannels() * getSize())
			{
				// check for the smaller sizes, that is how much will be copied over
				u32 channelsToCopy = std::min(newNumChannels, getChannels());
				u32 numSamplesToCopy = std::min(newSize, getSize());
				u32 startCopy = (getSize() + end_ - numSamplesToCopy) % getSize();

				applyToBuffer(newData, data_, channelsToCopy, numSamplesToCopy, 0, startCopy);

				end_ = 0;
			}

			std::swap(newData, data_);

			size_ = newSize;
			channels_ = newNumChannels;
		}

		void clear(u32 begin, u32 samples) noexcept
		{
			if (begin + samples <= getSize())
			{
				data_.clear(begin, samples);
				return;
			}

			u32 samplesLeft = begin + samples - getSize();
			data_.clear(begin, getSize() - begin);
			data_.clear(0, samplesLeft);
		}

		void clear() noexcept { data_.clear(); }

		u32 advanceEnd(u32 samples) noexcept { return end_ = (end_ + samples) % getSize(); }
		u32 setEnd(u32 index) noexcept { return end_ = index % getSize(); }

		// applies on operation on the samples of otherBuffer and thisBuffer 
		// and writes the results to the respective channels of thisBuffer
		// while anticipating wrapping around in both buffers 
		template<utils::MathOperations Operation = utils::MathOperations::Assign>
		static void applyToBuffer(Buffer &thisBuffer, const Buffer &otherBuffer,
			size_t channels, size_t samples, size_t thisStart, size_t otherStart,
			const bool *channelsToApplyTo = nullptr) noexcept
		{
			COMPLEX_ASSERT(thisBuffer.getChannels() >= channels);
			COMPLEX_ASSERT(otherBuffer.getChannels() >= channels);
			COMPLEX_ASSERT(thisBuffer.getSize() >= samples);
			COMPLEX_ASSERT(otherBuffer.getSize() >= samples);

			float increment = 1.0f / (float)samples;
			auto thisPointer = thisBuffer.getData().getData().get();
			auto otherPointers = otherBuffer.getData().getData().get();

			auto iterate = [&]<bool UseBitAnd>()
			{
				size_t thisSize = thisBuffer.getSize();
				size_t otherSize = otherBuffer.getSize();

				for (size_t i = 0; i < channels; ++i)
				{
					if (channelsToApplyTo && !channelsToApplyTo[i])
						continue;

					[[maybe_unused]] float t = 0.0f;
					for (size_t k = 0; k < samples; k++)
					{
						size_t thisIndex, otherIndex;
						if constexpr (UseBitAnd)
						{
							thisIndex = (thisStart + k) & (thisSize - 1);
							otherIndex = (otherStart + k) & (otherSize - 1);
						}
						else
						{
							thisIndex = (thisStart + k) % thisSize;
							otherIndex = (otherStart + k) % otherSize;
						}

						if constexpr (Operation == utils::MathOperations::Add)
							thisPointer[i * thisSize + thisIndex] += otherPointers[i * otherSize + otherIndex];
						else if constexpr (Operation == utils::MathOperations::Multiply)
							thisPointer[i * thisSize + thisIndex] *= otherPointers[i * otherSize + otherIndex];
						else if constexpr (Operation == utils::MathOperations::FadeInAdd)
						{
							thisPointer[i * thisSize + thisIndex] = (1.0f - t) * thisPointer[i * thisSize + thisIndex] + 
								t * (thisPointer[i * thisSize + thisIndex] + otherPointers[i * otherSize + otherIndex]);
							t += increment;
						}
						else if constexpr (Operation == utils::MathOperations::FadeOutAdd)
						{
							thisPointer[i * thisSize + thisIndex] = (1.0f - t) * 
								(thisPointer[i * thisSize + thisIndex] + otherPointers[i * otherSize + otherIndex]) + 
								t * otherPointers[i * otherSize + otherIndex];
							t += increment;
						}
						else if constexpr (Operation == utils::MathOperations::Interpolate)
						{
							thisPointer[i * thisSize + thisIndex] = (1.0f - t) * thisPointer[i * thisSize + thisIndex] +
								t * otherPointers[i * otherSize + otherIndex];
							t += increment;
						}
						else
							thisPointer[i * thisSize + thisIndex] = otherPointers[i * otherSize + otherIndex];
					}
				}
			};

			if (utils::isPowerOfTwo(thisBuffer.getSize()) && utils::isPowerOfTwo(otherBuffer.getSize()))
				iterate.template operator()<true>();
			else
				iterate.template operator()<false>();
		}

		u32 writeToBufferEnd(const float *const *const writer, u32 channels, u32 samples) noexcept
		{
			float *thisPointer = data_.getData().getData().get();
			for (size_t i = 0; i < channels; ++i)
			{
				const size_t thisSize = getSize();
				const size_t thisStart = i * thisSize;

				if (utils::isPowerOfTwo(thisSize))
					for (size_t j = 0; j < samples; ++j)
						thisPointer[thisStart + ((end_ + j) & (thisSize - 1))] = writer[i][j];
				else
					for (size_t j = 0; j < samples; ++j)
						thisPointer[thisStart + ((end_ + j) % thisSize)] = writer[i][j];
			}

			return advanceEnd(samples);
		}

		// - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
		//	readee's starting index = readerIndex + end_ and 
		//	reader's starting index = readeeIndex
		// - Can decide whether to advance the block or not
		void readBuffer(float *const *const reader, u32 channels, u32 samples,
			u32 readeeIndex = 0, const bool *channelsToRead = nullptr) const noexcept
		{
			const float *thisPointer = data_.getData().getData().get();
			for (size_t i = 0; i < channels; ++i)
			{
				if (channelsToRead && !channelsToRead[i])
					continue;

				const size_t thisSize = getSize();
				const size_t thisStart = i * thisSize;

				if (utils::isPowerOfTwo(thisSize))
					for (size_t j = 0; j < samples; ++j)
						reader[i][j] = thisPointer[thisStart + ((readeeIndex + j) & (thisSize - 1))];
				else
					for (size_t j = 0; j < samples; ++j)
						reader[i][j] = thisPointer[thisStart + ((readeeIndex + j) % thisSize)];
			}
		}

		// - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
		//	readee's starting index = readerIndex + end_ and 
		//	reader's starting index = readeeIndex
		// - Can decide whether to advance the block or not
		void readBuffer(Buffer &reader, u32 channels, u32 samples, 
			u32 readeeIndex = 0, u32 readerIndex = 0, const bool* channelsToRead = nullptr) const noexcept
		{ applyToBuffer(reader, data_, channels, samples, readerIndex, readeeIndex, channelsToRead); }

		// - A specified AudioBuffer writes own data starting at writerOffset to the end of the current buffer,
		//	which can be offset forwards or backwards with writeeOffset
		// - Adjusts end_ according to the new block written
		// - Returns the new endpoint
		u32 writeToBufferEnd(const Buffer &writer, u32 channels, u32 samples,
			u32 writerIndex = 0, const bool *channelsToWrite = nullptr) noexcept
		{
			applyToBuffer(data_, writer, channels, samples, end_, writerIndex, channelsToWrite);
			return advanceEnd(samples);
		}

		void writeToBuffer(const Buffer &writer, u32 channels, u32 samples,
			u32 writeeIndex, u32 writerIndex = 0, const bool* channelsToWrite = nullptr) noexcept
		{ applyToBuffer(data_, writer, channels, samples, writeeIndex, writerIndex, channelsToWrite); }

		void add(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.write(data_.read(channel, index) + value, channel, index);
		}

		void addBuffer(const Buffer &other, u32 channels, u32 samples,
			const bool *channelsToAdd = nullptr, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			applyToBuffer<utils::MathOperations::Add>(data_, other, channels, samples, 
				thisStartIndex, otherStartIndex, channelsToAdd);
		}

		void multiply(float value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());
			data_.write(data_.read(channel, index) * value, channel, index);
		}

		void multiplyBuffer(const Buffer &other, u32 channels, u32 samples,
			const bool *channelsToAdd = nullptr, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
		{
			applyToBuffer<utils::MathOperations::Multiply>(data_, other, channels, samples, 
				thisStartIndex, otherStartIndex, channelsToAdd);
		}

		float read(u32 channel, u32 index) const noexcept { return data_.read(channel, index); }
		void write(float value, u32 channel, u32 index) noexcept { return data_.write(value, channel, index); }

		auto &getData() noexcept { return data_; }
		u32 getChannels() const noexcept { return channels_; }
		u32 getSize() const noexcept { return size_; }
		u32 getEnd() const noexcept { return end_; }

	private:
		Buffer data_;
		u32 channels_ = 0;
		u32 size_ = 0;
		u32 end_ = 0;
	};
}