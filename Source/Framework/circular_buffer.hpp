
// Created: 2022-02-07 01:56:59

#pragma once

#include "utils.hpp"

namespace Framework
{
  struct Buffer
  {
    u32 channels = 0;
    u32 size = 0;
    const float *data = nullptr;

    void clear() noexcept { ::zeroset(const_cast<float *>(data), channels * size); }
    void clear(u32 begin, u32 count) noexcept
    {
      COMPLEX_ASSERT(begin + count <= size);
      
      for (u32 i = 0; i < channels; ++i)
        ::zeroset(get(i) + begin, count);
    }

    utils::ca<float>
    get(u32 channel = 0)
    {
      COMPLEX_ASSERT(channel < channels);
      return { const_cast<float *>(data) + channel * size, size };
    }
    utils::ca<const float>
    get(u32 channel = 0) const
    {
      COMPLEX_ASSERT(channel < channels);
      return { data + channel * size, size };
    }
  };

  // the oldest sample in destination will be at index 0
  strict_inline void copyCircular(Buffer destination, const Buffer &other, u32 otherEnd)
  {
    if (other.channels == 0 || other.size == 0 || 
      destination.channels == 0 || destination.size == 0)
      return;

    COMPLEX_HARD_ASSERT(destination.data);
    COMPLEX_HARD_ASSERT(other.data);

    // check for the smaller sizes, that is how much will be copied over
    u32 channelsToCopy = utils::min(other.channels, destination.channels);
    u32 sizeToCopy = utils::min(other.size, destination.size);
    u32 startCopy = (other.size + otherEnd - sizeToCopy) % other.size;

    if (startCopy + sizeToCopy <= other.size)
      for (u32 i = 0; i < channelsToCopy; ++i)
        ::valcpy(destination.get(i) + destination.size - sizeToCopy, other.get(i) + startCopy, sizeToCopy);
    else
    {
      u32 sizeLeft = startCopy + sizeToCopy - other.size;
      for (u32 i = 0; i < channelsToCopy; ++i)
      {
        ::valcpy(destination.get(i) + destination.size - sizeToCopy, other.get(i) + startCopy, other.size - startCopy);
        ::valcpy(destination.get(i) + destination.size - sizeLeft, other.get(i), sizeLeft);
      }
    }
  }

  // applies on operation on the samples of otherBuffer and thisBuffer 
  // and writes the results to the respective channels of thisBuffer
  // while anticipating wrapping around in both buffers 
  inline void applyToBuffer(const auto &operation, Buffer &thisBuffer, const Buffer &otherBuffer, 
    u32 channels, u32 samples, u32 thisStart, u32 otherStart, utils::span<bool> channelsToApplyTo = {}) noexcept
  {
    COMPLEX_ASSERT(thisBuffer.channels >= channels);
    COMPLEX_ASSERT(otherBuffer.channels >= channels);
    COMPLEX_ASSERT(thisBuffer.size >= samples);
    COMPLEX_ASSERT(otherBuffer.size >= samples);

    [[maybe_unused]] float increment = 1.0f / (float)samples;
      
    auto wrapIndex = [&]() -> u32 (*)(u32, u32)
    {
      if (utils::isPowerOfTwo(thisBuffer.size) && utils::isPowerOfTwo(otherBuffer.size))
        return [](u32 i, u32 m) { return i & (m - 1); };
      else
        return [](u32 i, u32 m) { return i % m; };
    }();

    for (u32 i = 0; i < channels; ++i)
    {
      if (!channelsToApplyTo.empty() && !channelsToApplyTo[i])
        continue;

      auto thisPointer = thisBuffer.get(i);
      auto otherPointer = otherBuffer.get(i);

      [[maybe_unused]] float t = 0.0f;
      for (u32 k = 0; k < samples; k++)
      {
        u32 thisIndex = wrapIndex(thisStart + k, thisBuffer.size);
        u32 otherIndex = wrapIndex(otherStart + k, otherBuffer.size);

        operation(thisPointer[thisIndex], otherPointer[otherIndex], t);
        t += increment;
      }
    }
  }

  struct CircularBuffer : Buffer
  {
    u32 end = 0;

    static constexpr auto assignBuffersFn = [](float &destination, const float &source, float) { destination = source; };

    using Buffer::clear;
    void clear(u32 begin, u32 samples) noexcept
    {
      if (begin + samples <= size)
      {
        for (u32 i = 0; i < channels; ++i)
          ::zeroset(get(i) + begin, samples);
        return;
      }

      u32 samplesLeft = begin + samples - size;
      for (u32 i = 0; i < channels; ++i)
        ::zeroset(get(i) + begin, size - begin);
      for (u32 i = 0; i < channels; ++i)
        ::zeroset(get(i), samplesLeft);
    }

    u32 advanceEnd(u32 samples) noexcept { return end = (end + samples) % size; }
    u32 setEnd(u32 index) noexcept { return end = index % size; }

    // - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
    //	readee's starting index = readerIndex + end_ and 
    //	reader's starting index = readeeIndex
    // - Can decide whether to advance the block or not
    void readAt(float *const *reader, u32 readChannels, u32 readSize,
      u32 readeeIndex = 0, utils::span<bool> channelsToRead = {}) const noexcept
    {
      for (u32 i = 0; i < readChannels; ++i)
      {
        if (!channelsToRead.empty() && !channelsToRead[i])
          continue;

        auto thisPointer = get(i);

        if (utils::isPowerOfTwo(size))
          for (u32 j = 0; j < readSize; ++j)
            reader[i][j] = thisPointer[(readeeIndex + j) & (size - 1)];
        else
          for (u32 j = 0; j < readSize; ++j)
            reader[i][j] = thisPointer[(readeeIndex + j) % size];
      }
    }

    // - A specified AudioBuffer reads from the current buffer's data and stores it in a reader, where
    //     readee's starting index = readerIndex + end_ and 
    //     reader's starting index = readeeIndex
    // - Can decide whether to advance the block or not
    void readAt(Buffer &reader, u32 readChannels, u32 readSize, 
      u32 readeeIndex = 0, u32 readerIndex = 0, utils::span<bool> channelsToRead = {}) const noexcept
    {
      applyToBuffer(assignBuffersFn, reader, *this,
        readChannels, readSize, readerIndex, readeeIndex, channelsToRead);
    }

    u32 writeAtEnd(const float *const *const writer, u32 writeChannels, u32 writeSize) noexcept
    {
      COMPLEX_HARD_ASSERT(writeChannels <= channels);
      COMPLEX_HARD_ASSERT(writeSize <= size);
      for (u32 i = 0; i < writeChannels; ++i)
      {
        auto thisPointer = get(i);

        if (utils::isPowerOfTwo(size))
          for (u32 j = 0; j < writeSize; ++j)
            thisPointer[(end + j) & (size - 1)] = writer[i][j];
        else
          for (u32 j = 0; j < writeSize; ++j)
            thisPointer[(end + j) % size] = writer[i][j];
      }

      return advanceEnd(writeSize);
    }

    // - A specified AudioBuffer writes own data starting at writerOffset to the end of the current buffer,
    //	which can be offset forwards or backwards with writeeOffset
    // - Adjusts end_ according to the new block written
    // - Returns the new endpoint
    u32 writeAtEnd(const Buffer &writer, u32 writeChannels, u32 writeSize,
      u32 writerIndex = 0, utils::span<bool> channelsToWrite = {}) noexcept
    {
      applyToBuffer(assignBuffersFn, *this, writer,
        writeChannels, writeSize, end, writerIndex, channelsToWrite);
      return advanceEnd(writeSize);
    }

    void writeAt(const Buffer &writer, u32 writeChannels, u32 writeSize,
      u32 writeeIndex, u32 writerIndex = 0, utils::span<bool> channelsToWrite = {}) noexcept
    {
      applyToBuffer(assignBuffersFn, *this, writer,
        writeChannels, writeSize, writeeIndex, writerIndex, channelsToWrite);
    }
  };
}