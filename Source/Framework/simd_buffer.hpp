/*
  ==============================================================================

    simd_buffer.hpp
    Created: 25 Oct 2021 11:14:40pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "sync_primitives.hpp"
#include "memory_block.hpp"
#include "utils.hpp"
#include "simd_utils.hpp"

namespace Framework
{
  template<typename T, SimdValue SIMD>
  class SimdBufferView;

  // T - base type
  // SIMD - simd type
  template<typename T, SimdValue SIMD>
  class SimdBuffer 
  {
  public:
    static_assert(alignof(SIMD) % alignof(T) == 0);

    SimdBuffer() = default;
    SimdBuffer(usize numChannels, usize size) noexcept { reserve(numChannels, size); }
    SimdBuffer(const SimdBuffer &other, bool doDataCopy = false) noexcept
    {
      COMPLEX_ASSERT(other.getChannels() > 0 && other.getSize() > 0);
      reserve(other.getChannels(), other.getSize());

      if (doDataCopy)
        applyToThisNoMask<utils::MathOperations::Assign>(*this, other, other.getChannels(), other.getSize());
    }

    SimdBuffer &operator=(const SimdBuffer &other) = delete;
    SimdBuffer(SimdBuffer &&other) = delete;
    SimdBuffer &operator=(SimdBuffer &&other) = delete;

    void copy(const SimdBuffer &other)
    {
      if (&other == this)
        return;

      if (other.getSize() * other.getChannels() == 0)
        return;

      data_.copy(other.data_);

      data_.getExtraData()->channels_ = other.getChannels();
      data_.getExtraData()->size_ = other.getSize();
      data_.getExtraData()->dataLock_.lock.store(0, std::memory_order_relaxed);
      data_.getExtraData()->dataLock_.lastLockId.store(0, std::memory_order_relaxed);
    }
    void copy(const SimdBuffer &other, u64 destination, u64 source, u64 size)
    { data_.copy(other.data_, destination, source, size); }
    void copy(const SimdBufferView<T, SIMD> &other);
    void copy(const SimdBufferView<T, SIMD> &other, u64 destination, u64 source, u64 size);

    void swap(SimdBuffer &other) noexcept
    {
      if (&other == this)
        return;

      MemoryBlock<SIMD, ExtraData> newData;
      newData.swap(other.data_);

      other.data_.swap(data_);

      data_.swap(newData);
    }

    void reserve(usize newChannels, usize newSize, bool fitToSize = false) noexcept
    {
      COMPLEX_ASSERT(newChannels > 0 && newSize > 0);
      if ((newChannels <= getChannels()) && (newSize <= getSize()) && !fitToSize)
        return;

      auto channels = getChannels();
      auto size = getSize();
      usize oldSimdChannels = getTotalSimdChannels(channels);
      usize newSimdChannels = getTotalSimdChannels(newChannels);
      MemoryBlock<SIMD, ExtraData> newData{ newSimdChannels * newSize, true };

      // checking if we even have any elements to move
      if (channels * size != 0)
      {
        usize simdChannelsToCopy = (newSimdChannels < oldSimdChannels) ? newSimdChannels : oldSimdChannels;
        usize capacityToCopy = (newSize < size) ? newSize : size;
        for (usize i = 0; i <= simdChannelsToCopy; i++)
          std::memcpy(&newData[i * newSize], &data_[i * size], capacityToCopy * sizeof(SIMD));
      }

      newData.getExtraData()->channels_ = newChannels;
      newData.getExtraData()->size_ = newSize;
      data_.swap(newData);
    }

    void clear() noexcept { data_.clear(); }


    // copies/does math operation on samples from "otherBuffer" to "thisBuffer"
    // specified by starting channels/indices
    // result is shifted by shiftMask and filtered with mergeMask
    // note: starting channels need to be congruent to kNumChannels
    static void applyToThis(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> otherBuffer, usize channels,
      usize samples, utils::MathOperations operation = utils::MathOperations::Assign, simd_mask mergeMask = kNoChangeMask,
      usize thisStartChannel = 0, usize otherStartChannel = 0, usize thisStartIndex = 0, usize otherStartIndex = 0) noexcept;

    // same thing without the mask
    template<utils::MathOperations Operation>
    static void applyToThisNoMask(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> otherBuffer,
      usize channels, usize samples, usize thisStartChannel = 0, usize otherStartChannel = 0,
      usize thisStartIndex = 0, usize otherStartIndex = 0, SIMD scaleFactor = SIMD(1)) noexcept;


    void addBuffer(const SimdBuffer &other, usize channels, usize samples,
      simd_mask mergeMask = kNoChangeMask, usize thisStartChannel = 0, usize otherStartChannel = 0,
      usize thisStartIndex = 0, usize otherStartIndex = 0) noexcept
    {
      COMPLEX_ASSERT(channels <= other.getChannels());
      COMPLEX_ASSERT(channels <= getChannels());

      if (simd_mask::notEqual(mergeMask, kNoChangeMask).anyMask() == 0)
        applyToThisNoMask<utils::MathOperations::Add>(*this, other, channels, samples,
          thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
      else
        applyToThis(*this, other, channels, samples, utils::MathOperations::Add,
          mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
    }

    void multiplyBuffer(const SimdBuffer &other, usize channels, usize samples,
      simd_mask mergeMask = kNoChangeMask, usize thisStartChannel = 0, usize otherStartChannel = 0,
      usize thisStartIndex = 0, usize otherStartIndex = 0) noexcept
    {
      COMPLEX_ASSERT(channels <= other.getChannels());
      COMPLEX_ASSERT(channels <= getChannels());

      if (simd_mask::notEqual(mergeMask, kNoChangeMask).anyMask() == 0)
        applyToThisNoMask<utils::MathOperations::Multiply>(*this, other, channels, samples,
          thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
      else
        applyToThis(*this, other, channels, samples, utils::MathOperations::Multiply,
          mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
    }
    
    void setBufferPosition(u32 position) noexcept { data_.getExtraData()->bufferPosition_ = position; }

    bool isEmpty() const noexcept { return getChannels() == 0 || getSize() == 0; }

    auto &getLock() const noexcept { return data_.getExtraData()->dataLock_; }
    usize getSize() const noexcept {	return (data_) ? data_.getExtraData()->size_ : 0; }
    usize getChannels() const noexcept { return (data_) ? data_.getExtraData()->channels_ : 0; }
    usize getSimdChannels() const noexcept { return getTotalSimdChannels(getChannels()); }
    u32 getBufferPosition() noexcept { return data_.getExtraData()->bufferPosition_; }

    auto get(usize simdChannel = 0) noexcept
    {
      COMPLEX_ASSERT(simdChannel * getRelativeSize() < getChannels());
      return data_.get().offset(simdChannel * getSize(), getSize());
    }
    auto get(usize simdChannel = 0) const noexcept
    {
      COMPLEX_ASSERT(simdChannel * getRelativeSize() < getChannels());
      return data_.get().offset(simdChannel * getSize(), getSize());
    }

    static constexpr usize getRelativeSize() noexcept { return sizeof(SIMD) / sizeof(T); }

  private:
    struct ExtraData
    {
      usize channels_ = 0;
      usize size_ = 0;
      // position in the overall audio callbacks
      u32 bufferPosition_ = 0;

      mutable utils::LockBlame<i32> dataLock_{ 0 };
    };

    MemoryBlock<SIMD, ExtraData> data_;

    static constexpr usize getTotalSimdChannels(usize numChannels)
    {	return (numChannels + getRelativeSize() - 1) / getRelativeSize(); }

    // internal function for calculating SIMD and internal positions of an element
    // first - index to the **SIMD** element 
    // second - channel index of T value inside the SIMD element
    static constexpr utils::pair<usize, usize> getAbsoluteIndices(usize channel, usize channelSize, usize index) noexcept
    { return { (channel / getRelativeSize()) * channelSize + index, channel % getRelativeSize() }; }

    static constexpr usize getSimdIndex(usize channel, usize channelSize, usize index) noexcept
    { return (channel / getRelativeSize()) * channelSize + index; }

    template<typename T, SimdValue SIMD>
    friend class SimdBufferView;
  };

  template<typename T, SimdValue SIMD>
  class SimdBufferView
  {
  public:
    SimdBufferView() = default;
    SimdBufferView(const SimdBufferView &) = default;
    SimdBufferView(const SimdBuffer<T, SIMD> &buffer, usize beginChannel = 0, usize channels = 0) noexcept
    {
      COMPLEX_ASSERT(beginChannel + channels <= buffer.getChannels());

      dataView_ = buffer.data_;
      beginChannel_ = beginChannel;
      channels_ = (channels) ? channels : buffer.getChannels() - beginChannel;
    }

    bool isEmpty() const noexcept { return getChannels() == 0 || getSize() == 0; }

    auto &getLock() const noexcept { return dataView_.getExtraData()->dataLock_; }
    usize getSize() const noexcept { return dataView_.getExtraData()->size_; }
    usize getChannels() const noexcept { return dataView_.getExtraData()->channels_; }
    usize getSimdChannels() const noexcept { return SimdBuffer<T, SIMD>::getTotalSimdChannels(channels_); }
    u32 getBufferPosition() noexcept { return dataView_.getExtraData()->bufferPosition_; }

    auto get(usize simdChannel = 0) const noexcept
    {
      COMPLEX_ASSERT(simdChannel * getRelativeSize() + beginChannel_ < getChannels());
      return dataView_.get().offset((simdChannel + beginChannel_ / getRelativeSize()) * getSize(), getSize());
    }

    static constexpr usize getRelativeSize() noexcept { return SimdBuffer<T, SIMD>::getRelativeSize(); }

    bool operator==(const SimdBuffer<T, SIMD> &other) const noexcept { return dataView_ == other.data_; }

  private:
    MemoryBlockView<SIMD, typename SimdBuffer<T, SIMD>::ExtraData> dataView_{};
    usize beginChannel_ = 0;
    usize channels_ = 0;

    template<typename T, SimdValue SIMD>
    friend class SimdBuffer;
  };

  template <typename T, SimdValue SIMD>
  void SimdBuffer<T, SIMD>::copy(const SimdBufferView<T, SIMD> &other) { data_.copy(other.dataView_); }

  template <typename T, SimdValue SIMD>
  void SimdBuffer<T, SIMD>::copy(const SimdBufferView<T, SIMD> &other, u64 destination, u64 source, u64 size)
  { data_.copy(other.dataView_, destination, source, size); }

  // copies/does math operation on samples from "otherBuffer" to "thisBuffer"
  // specified by starting channels/indices
  // result is shifted by shiftMask and filtered with mergeMask
  // note: starting channels need to be congruent to kNumChannels
  template<typename T, SimdValue SIMD>
  void SimdBuffer<T, SIMD>::applyToThis(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> otherBuffer, usize channels,
    usize samples, utils::MathOperations operation, simd_mask mergeMask, usize thisStartChannel, usize otherStartChannel,
    usize thisStartIndex, usize otherStartIndex) noexcept
  {
    COMPLEX_ASSERT(thisBuffer.getChannels() >= thisStartChannel + channels);
    COMPLEX_ASSERT(otherBuffer.getChannels() >= otherStartChannel + channels);
    COMPLEX_ASSERT(thisBuffer.getSize() >= thisStartIndex + samples);
    COMPLEX_ASSERT(otherBuffer.getSize() >= otherStartIndex + samples);
    COMPLEX_ASSERT(thisBuffer.data_ != otherBuffer.dataView_);

    // defining the math operations
    SIMD(*function)(SIMD, SIMD, simd_mask);
    switch (operation)
    {
    case utils::MathOperations::Add:

      function = [](SIMD one, SIMD two, simd_mask mask) { return utils::merge(one + two, one, mask); };
      break;

    case utils::MathOperations::Multiply:

      function = [](SIMD one, SIMD two, simd_mask mask) { return utils::merge(one * two, one, mask); };
      break;

    default:
    case utils::MathOperations::Assign:

      function = [](SIMD one, SIMD two, simd_mask mask) { return utils::merge(two, one, mask); };
      break;
    }

    // getting data and setting up variables
    auto thisDataBlock = thisBuffer.get();
    auto otherDataBlock = otherBuffer.get();

    auto thisBufferSize = thisBuffer.getSize();
    auto otherBufferSize = otherBuffer.getSize();
    usize simdNumChannels = getTotalSimdChannels(channels);

    for (usize i = 0; i < simdNumChannels; i++)
    {
      // getting indices to the beginning of the SIMD channel buffer blocks
      auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, thisStartIndex);
      auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, otherStartIndex);

      for (usize k = 0; k < samples; k++)
      {
        thisDataBlock[thisChannelIndices.first + k] = function(thisDataBlock[thisChannelIndices.first + k],
          otherDataBlock[otherChannelIndices.first + k], mergeMask);
      }
    }
  }

  template<typename T, SimdValue SIMD>
  template<utils::MathOperations Operation>
  void SimdBuffer<T, SIMD>::applyToThisNoMask(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> otherBuffer,
    usize channels, usize samples, usize thisStartChannel, usize otherStartChannel,
    usize thisStartIndex, usize otherStartIndex, SIMD scaleFactor) noexcept
  {
    COMPLEX_ASSERT(thisBuffer.getChannels() >= thisStartChannel + channels);
    COMPLEX_ASSERT(otherBuffer.getChannels() >= otherStartChannel + channels);
    COMPLEX_ASSERT(thisBuffer.getSize() >= thisStartIndex + samples);
    COMPLEX_ASSERT(otherBuffer.getSize() >= otherStartIndex + samples);
    COMPLEX_ASSERT(thisBuffer.data_ != otherBuffer.dataView_);

    // getting data and setting up variables
    auto thisData = thisBuffer.get();
    auto otherData = otherBuffer.get();

    auto thisSize = thisBuffer.getSize();
    auto otherSize = otherBuffer.getSize();
    usize simdNumChannels = getTotalSimdChannels(channels);

    if constexpr (Operation == utils::MathOperations::Add)
    {
      for (usize i = 0; i < simdNumChannels; i++)
      {
        // getting indices to the beginning of the SIMD channel buffer blocks
        auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
        auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

        for (usize k = 0; k < samples; k++)
          thisData[thisChannelIndices.first + k] += otherData[otherChannelIndices.first + k] * scaleFactor;
      }
    }
    else if constexpr (Operation == utils::MathOperations::Multiply)
    {
      for (usize i = 0; i < simdNumChannels; i++)
      {
        // getting indices to the beginning of the SIMD channel buffer blocks
        auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
        auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

        for (usize k = 0; k < samples; k++)
          thisData[thisChannelIndices.first + k] *= otherData[otherChannelIndices.first + k] * scaleFactor;
      }
    }
    else
    {
      for (usize i = 0; i < simdNumChannels; i++)
      {
        // getting indices to the beginning of the SIMD channel buffer blocks
        auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
        auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

        COMPLEX_ASSERT(thisBuffer.data_ != otherBuffer.dataView_ && 
          "We're copying from a buffer to itself consider using something else if this is intentional");

        std::memcpy(&thisData[thisChannelIndices.first], &otherData[otherChannelIndices.first], samples * sizeof(SIMD));
      }
    }
  }

  template<typename T>
  struct complex { T values[2]{}; };

  struct ComplexDataSource
  {
    // this is the phase in the currently processed block
    // see SoundEngine::blockPosition_ for more info
    float blockPhase = 0.0f;
    // SoundEngine::blockPosition_ copy to store in buffers
    u32 blockPosition = 0;
    SimdBufferView<complex<float>, simd_float> sourceBuffer;
    // scratch buffer for to be used during processing, initial data is undefined
    SimdBuffer<complex<float>, simd_float> scratchBuffer{};
  };
}
