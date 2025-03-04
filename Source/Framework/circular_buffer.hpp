/*
  ==============================================================================

    circular_buffer.hpp
    Created: 7 Feb 2022 1:56:59am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "utils.hpp"
#include "memory_block.hpp"

namespace Framework
{
  class Buffer
  {
  public:
    Buffer() = default;
    Buffer(usize channels, usize size, bool initialiseToZero = false) noexcept :
      data_{ channels * size, initialiseToZero, 64 }, channels_(channels), size_(size) { }

    void reserve(usize channels, usize size, bool fitToSize = false) noexcept
    {
      COMPLEX_ASSERT(channels > 0 && size > 0);
      if ((channels <= getChannels()) && (size <= getSize()) && !fitToSize)
        return;

      auto newData = MemoryBlock<float>{ channels * size, true, 64 };

      if (getChannels() * getSize())
      {
        // check for the smaller sizes, that is how much will be copied over
        usize channelsToCopy = utils::min(channels, getChannels());
        usize samplesToCopy = utils::min(size, getSize());

        for (usize i = 0; i < channelsToCopy; ++i)
          newData.copy(data_, i * size, i * getSize(), samplesToCopy);
      }

      data_.swap(newData);

      size_ = size;
      channels_ = channels;
    }

    void clear() noexcept { data_.clear(); }
    void clear(usize begin, usize count) noexcept
    {
      COMPLEX_ASSERT(begin + count <= getSize());
      
      for (usize i = 0; i < getChannels(); ++i)
        memset(data_.get() + getSize() * i + begin, 0, count * sizeof(float));
    }

    float read(usize channel, usize index) const noexcept
    {
      COMPLEX_ASSERT(channel * getSize() + index < getChannels() * getSize());
      return data_[channel * getSize() + index];
    }

    void write(float value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel * getSize() + index < getChannels() * getSize());
      data_[channel * getSize() + index] = value;
    }

    usize getChannels() const noexcept { return channels_; }
    usize getSize() const noexcept { return size_; }
    auto get(usize channel = 0) noexcept
    {
      COMPLEX_ASSERT(channel < channels_);
      return data_.get().offset(channel * size_, size_);
    }
    auto get(usize channel = 0) const noexcept
    {
      COMPLEX_ASSERT(channel < channels_);
      return data_.get().offset(channel * size_, size_);
    }

  private:
    MemoryBlock<float> data_{};
    usize channels_ = 0;
    usize size_ = 0;
  };

  class CircularBuffer
  {
  public:
    CircularBuffer() = default;

    CircularBuffer(u32 channels, u32 size) noexcept :
      data_{ channels, size } { }

    void reserve(u32 channels, u32 size, bool fitToSize = false) noexcept
    {
      COMPLEX_ASSERT(channels > 0 && size > 0);
      if ((channels <= getChannels()) && (size <= getSize()) && !fitToSize)
        return;

      Buffer newData{ channels, size, true };

      if (getChannels() * getSize())
      {
        // check for the smaller sizes, that is how much will be copied over
        u32 channelsToCopy = utils::min(channels, getChannels());
        u32 sizeToCopy = utils::min(size, getSize());
        u32 startCopy = (getSize() + end_ - sizeToCopy) % getSize();

        applyToBuffer(newData, data_, channelsToCopy, sizeToCopy, 0, startCopy);

        end_ = 0;
      }

      data_ = COMPLEX_MOVE(newData);
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
      usize channels, usize samples, usize thisStart, usize otherStart,
      utils::span<char> channelsToApplyTo = {}) noexcept
    {
      COMPLEX_ASSERT(thisBuffer.getChannels() >= channels);
      COMPLEX_ASSERT(otherBuffer.getChannels() >= channels);
      COMPLEX_ASSERT(thisBuffer.getSize() >= samples);
      COMPLEX_ASSERT(otherBuffer.getSize() >= samples);

      usize thisSize = thisBuffer.getSize();
      usize otherSize = otherBuffer.getSize();
      [[maybe_unused]] float increment = 1.0f / (float)samples;
      
      auto wrapIndex = [&]() -> usize (*)(usize, usize)
      {
        if (utils::isPowerOfTwo(thisSize) && utils::isPowerOfTwo(otherSize))
          return [](usize i, usize m) { return i & (m - 1); };
        else
          return [](usize i, usize m) { return i % m; };
      }();

      for (usize i = 0; i < channels; ++i)
      {
        if (!channelsToApplyTo.empty() && !channelsToApplyTo[i])
          continue;

        auto thisPointer = thisBuffer.get(i);
        auto otherPointer = otherBuffer.get(i);

        [[maybe_unused]] float t = 0.0f;
        for (usize k = 0; k < samples; k++)
        {
          usize thisIndex = wrapIndex(thisStart + k, thisSize);
          usize otherIndex = wrapIndex(otherStart + k, otherSize);

          if constexpr (Operation == utils::MathOperations::Add)
            thisPointer[thisIndex] += otherPointer[otherIndex];
          else if constexpr (Operation == utils::MathOperations::Multiply)
            thisPointer[thisIndex] *= otherPointer[otherIndex];
          else if constexpr (Operation == utils::MathOperations::FadeInAdd)
          {
            thisPointer[thisIndex] = (1.0f - t) * thisPointer[thisIndex] + 
              t * (thisPointer[thisIndex] + otherPointer[otherIndex]);
            t += increment;
          }
          else if constexpr (Operation == utils::MathOperations::FadeOutAdd)
          {
            thisPointer[thisIndex] = (1.0f - t) * 
              (thisPointer[thisIndex] + otherPointer[otherIndex]) +
              t * otherPointer[otherIndex];
            t += increment;
          }
          else if constexpr (Operation == utils::MathOperations::Interpolate)
          {
            thisPointer[thisIndex] = (1.0f - t) * thisPointer[thisIndex] +
              t * otherPointer[otherIndex];
            t += increment;
          }
          else
            thisPointer[thisIndex] = otherPointer[otherIndex];
        }
      }
    }

    u32 writeToBufferEnd(const float *const *const writer, u32 channels, u32 samples) noexcept
    {
      usize thisSize = getSize();
      for (usize i = 0; i < channels; ++i)
      {
        auto thisPointer = data_.get(i);

        if (utils::isPowerOfTwo(thisSize))
          for (usize j = 0; j < samples; ++j)
            thisPointer[(end_ + j) & (thisSize - 1)] = writer[i][j];
        else
          for (usize j = 0; j < samples; ++j)
            thisPointer[(end_ + j) % thisSize] = writer[i][j];
      }

      return advanceEnd(samples);
    }

    // - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
    //	readee's starting index = readerIndex + end_ and 
    //	reader's starting index = readeeIndex
    // - Can decide whether to advance the block or not
    void readBuffer(float *const *reader, u32 channels, u32 samples,
      u32 readeeIndex = 0, utils::span<char> channelsToRead = {}) const noexcept
    {
      usize thisSize = getSize();
      for (usize i = 0; i < channels; ++i)
      {
        if (!channelsToRead.empty() && !channelsToRead[i])
          continue;

        auto thisPointer = data_.get(i);

        if (utils::isPowerOfTwo(thisSize))
          for (usize j = 0; j < samples; ++j)
            reader[i][j] = thisPointer[(readeeIndex + j) & (thisSize - 1)];
        else
          for (usize j = 0; j < samples; ++j)
            reader[i][j] = thisPointer[(readeeIndex + j) % thisSize];
      }
    }

    // - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
    //	readee's starting index = readerIndex + end_ and 
    //	reader's starting index = readeeIndex
    // - Can decide whether to advance the block or not
    void readBuffer(Buffer &reader, u32 channels, u32 samples, 
      u32 readeeIndex = 0, u32 readerIndex = 0, utils::span<char> channelsToRead = {}) const noexcept
    { applyToBuffer(reader, data_, channels, samples, readerIndex, readeeIndex, channelsToRead); }

    // - A specified AudioBuffer writes own data starting at writerOffset to the end of the current buffer,
    //	which can be offset forwards or backwards with writeeOffset
    // - Adjusts end_ according to the new block written
    // - Returns the new endpoint
    u32 writeToBufferEnd(const Buffer &writer, u32 channels, u32 samples,
      u32 writerIndex = 0, utils::span<char> channelsToWrite = {}) noexcept
    {
      applyToBuffer(data_, writer, channels, samples, end_, writerIndex, channelsToWrite);
      return advanceEnd(samples);
    }

    void writeToBuffer(const Buffer &writer, u32 channels, u32 samples,
      u32 writeeIndex, u32 writerIndex = 0, utils::span<char> channelsToWrite = {}) noexcept
    { applyToBuffer(data_, writer, channels, samples, writeeIndex, writerIndex, channelsToWrite); }

    void add(float value, u32 channel, u32 index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());
      data_.write(data_.read(channel, index) + value, channel, index);
    }

    void addBuffer(const Buffer &other, u32 channels, u32 samples,
      utils::span<char> channelsToAdd = {}, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
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
      utils::span<char> channelsToAdd = {}, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
    {
      applyToBuffer<utils::MathOperations::Multiply>(data_, other, channels, samples, 
        thisStartIndex, otherStartIndex, channelsToAdd);
    }

    float read(u32 channel, u32 index) const noexcept { return data_.read(channel, index); }
    void write(float value, u32 channel, u32 index) noexcept { return data_.write(value, channel, index); }

    auto &getData() noexcept { return data_; }
    u32 getChannels() const noexcept { return (u32)data_.getChannels(); }
    u32 getSize() const noexcept { return (u32)data_.getSize(); }
    u32 getEnd() const noexcept { return end_; }

  private:
    Buffer data_;
    u32 end_ = 0;
  };
}