
// Created: 2021-10-25 23:14:40

#pragma once

#include "memory.hpp"
#include "sync_primitives.hpp"
#include "simd_utils.hpp"

namespace Framework
{
  struct SimdBuffer
  {
    static constexpr u32 kRelativeSize = sizeof(simd_float) / sizeof(float[2]);

    u32 channels = 0;
    u32 size = 0;
    mutable utils::LockBlame<i32> dataLock{ 0 };

    simd_float data[COMPLEX_FAM];

    void clear() { ::zeroset(data, getSimdChannels() * size); }

    utils::ca<simd_float>
    get(u32 simdChannel = 0)
    {
      COMPLEX_ASSERT(simdChannel * kRelativeSize < channels);
      return { data + simdChannel * size, size };
    }
    utils::ca<const simd_float>
    get(u32 simdChannel = 0) const
    {
      COMPLEX_ASSERT(simdChannel * kRelativeSize < channels);
      return { data + simdChannel * size, size };
    }
    
    u32 getSimdChannels() const { return getSimdChannels(channels); }
    static constexpr u32 getSimdChannels(u32 channels) { return (channels + kRelativeSize - 1) / kRelativeSize; }

    // first - index to the simd element 
    // second - channel index of T value inside the SIMD element
    static constexpr utils::pair<u32, u32> 
    getAbsoluteIndices(u32 channel, u32 channelSize, u32 index)
    { return { (channel / kRelativeSize) * channelSize + index, channel % kRelativeSize }; }

    static SimdBuffer *
    create(utils::Allocator allocator, u32 channels, u32 size, bool clean = false)
    {
      u32 simdChannels = Framework::SimdBuffer::getSimdChannels(channels);
      u32 dataSize = size * simdChannels;

      usize extra = utils::roundUpToMultiple(sizeof(Framework::SimdBuffer), alignof(simd_float));
      usize totalSize = extra + dataSize * sizeof(simd_float);

      byte *memory = allocator.insert(totalSize, alignof(simd_float), clean);
      (void)new(memory + extra) simd_float[dataSize];

      return new(memory) Framework::SimdBuffer{ .channels = channels, .size = size };
    }
  };

  // copies/does math operation on samples from "otherBuffer" to "thisBuffer"
  // specified by starting channels/indices
  // result is shifted by shiftMask and filtered with mergeMask
  // note: starting channels need to be congruent to kNumChannels
  strict_inline void applyToThis(SimdBuffer *thisBuffer, const SimdBuffer *otherBuffer, 
    u32 channels, u32 samples, utils::MathOperations operation, 
    simd_mask mergeMask, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
    u32 thisStartIndex = 0, u32 otherStartIndex = 0)
  {
    COMPLEX_HARD_ASSERT(thisBuffer->channels >= thisStartChannel + channels);
    COMPLEX_HARD_ASSERT(otherBuffer->channels >= otherStartChannel + channels);
    COMPLEX_HARD_ASSERT(thisBuffer->size >= thisStartIndex + samples);
    COMPLEX_HARD_ASSERT(otherBuffer->size >= otherStartIndex + samples);
    COMPLEX_HARD_ASSERT(thisBuffer != otherBuffer);

    // defining the math operations
    simd_float (*function)(simd_float, simd_float, simd_mask);
    switch (operation)
    {
    case utils::MathOperations::Add:
      function = [](simd_float one, simd_float two, simd_mask mask) { return utils::merge(one + two, one, mask); };
      break;
    case utils::MathOperations::Multiply:
      function = [](simd_float one, simd_float two, simd_mask mask) { return utils::merge(one * two, one, mask); };
      break;
    default:
    case utils::MathOperations::Assign:
      function = [](simd_float one, simd_float two, simd_mask mask) { return utils::merge(two, one, mask); };
      break;
    }

    u32 simdChannelCount = SimdBuffer::getSimdChannels(channels);
    thisStartChannel /= SimdBuffer::kRelativeSize;
    otherStartChannel /= SimdBuffer::kRelativeSize;

    for (u32 i = 0; i < simdChannelCount; i++)
    {
      auto thisChannel = thisBuffer->get(thisStartChannel + i).offset(thisStartIndex);
      auto otherChannel = otherBuffer->get(otherStartChannel + i).offset(otherStartIndex);

      for (u32 j = 0; j < samples; j++)
        thisChannel[j] = function(thisChannel[j], otherChannel[j], mergeMask);
    }
  }

  template<utils::MathOperations Operation>
  strict_inline void applyToThisNoMask(SimdBuffer *thisBuffer, const SimdBuffer *otherBuffer,
    u32 channels, u32 samples, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
    u32 thisStartIndex = 0, u32 otherStartIndex = 0, simd_float scaleFactor = 1.0f)
  {
    COMPLEX_ASSERT(thisBuffer->channels >= thisStartChannel + channels);
    COMPLEX_ASSERT(otherBuffer->channels >= otherStartChannel + channels);
    COMPLEX_ASSERT(thisBuffer->size >= thisStartIndex + samples);
    COMPLEX_ASSERT(otherBuffer->size >= otherStartIndex + samples);
    COMPLEX_ASSERT(thisBuffer != otherBuffer);

    u32 simdChannelCount = SimdBuffer::getSimdChannels(channels);
    thisStartChannel /= SimdBuffer::kRelativeSize;
    otherStartChannel /= SimdBuffer::kRelativeSize;

    if constexpr (Operation == utils::MathOperations::Add)
    {
      for (u32 i = 0; i < simdChannelCount; i++)
      {
        auto thisChannel = thisBuffer->get(thisStartChannel + i).offset(thisStartIndex);
        auto otherChannel = otherBuffer->get(otherStartChannel + i).offset(otherStartIndex);

        for (u32 j = 0; j < samples; j++)
          thisChannel[j] += otherChannel[j] * scaleFactor;
      }
    }
    else if constexpr (Operation == utils::MathOperations::Multiply)
    {
      for (u32 i = 0; i < simdChannelCount; i++)
      {
        auto thisChannel = thisBuffer->get(thisStartChannel + i).offset(thisStartIndex);
        auto otherChannel = otherBuffer->get(otherStartChannel + i).offset(otherStartIndex);

        for (u32 j = 0; j < samples; j++)
          thisChannel[j] *= otherChannel[j] * scaleFactor;
      }
    }
    else
    {
      for (u32 i = 0; i < simdChannelCount; i++)
      {
        auto thisChannel = thisBuffer->get(thisStartChannel + i).offset(thisStartIndex);
        auto otherChannel = otherBuffer->get(otherStartChannel + i).offset(otherStartIndex);

        ::valcpy(thisChannel.pointer, otherChannel.pointer, samples);
      }
    }
  }

  struct ComplexDataSource
  {
    // this is the phase in the currently processed block
    // see SoundEngine::blockPosition_ for more info
    float blockPhase = 0.0f;
    // SoundEngine::blockPosition_ copy to store in buffers
    u32 blockPosition = 0;
    const SimdBuffer *sourceBuffer = nullptr;
    u32 simdChannelOffset = 0;
    // scratch buffer for to be used during processing, initial data is undefined
    SimdBuffer *scratchBuffer = nullptr;
  };
}
