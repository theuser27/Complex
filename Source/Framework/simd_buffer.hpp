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

      data_.copy(other.data_);

      channels_ = other.channels_;
      size_ = other.size_;
    }
    void copy(const SimdBuffer &other, u64 destination, u64 source, u64 size)
    { data_.copy(other.data_, destination, source, size); }
    void copy(const SimdBufferView<T, SIMD> &other);
    void copy(const SimdBufferView<T, SIMD> &other, u64 destination, u64 source, u64 size);

    void swap(SimdBuffer &other) noexcept
    {
      if (&other == this)
        return;

      usize newChannels = other.channels_;
      usize newSize = other.size_;
      MemoryBlock<SIMD> newData;
      newData.swap(other.data_);

      other.data_.swap(data_);
      other.channels_ = channels_;
      other.size_ = size_;

      data_.swap(newData);
      channels_ = newChannels;
      size_ = newSize;
    }

    void reserve(usize newNumChannels, usize newSize, bool fitToSize = false) noexcept
    {
      // TODO: make it so the memory allocation happens on a different thread
      COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
      if ((newNumChannels <= channels_) && (newSize <= size_) && !fitToSize)
        return;

      usize oldSimdChannels = getTotalSimdChannels(channels_);
      usize newSimdChannels = getTotalSimdChannels(newNumChannels);
      MemoryBlock<SIMD> newData(newSimdChannels * newSize, true);

      // checking if we even have any elements to move
      if (channels_ * size_ != 0)
      {
        usize simdChannelsToCopy = (newSimdChannels < oldSimdChannels) ? newSimdChannels : oldSimdChannels;
        usize capacityToCopy = (newSize < size_) ? newSize : size_;
        for (usize i = 0; i <= simdChannelsToCopy; i++)
          std::memcpy(&newData[i * newSize], &data_[i * size_], capacityToCopy * sizeof(SIMD));
      }

      data_.swap(newData);

      channels_ = newNumChannels;
      size_ = newSize;
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


    void add(SIMD value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());
      
      auto indices = getAbsoluteIndices(channel, size_, index);
      data_[indices.first] += value;
    }

    // prefer the SIMD version
    void add(T value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto scalar = readValueAt(channel, index);
      writeValueAt(scalar + value, channel, index);
    }

    void addBuffer(const SimdBuffer &other, usize numChannels, usize numSamples,
      simd_mask mergeMask = kNoChangeMask, usize thisStartChannel = 0, usize otherStartChannel = 0,
      usize thisStartIndex = 0, usize otherStartIndex = 0) noexcept
    {
      COMPLEX_ASSERT(numChannels <= other.getChannels());
      COMPLEX_ASSERT(numChannels <= getChannels());

      if (simd_mask::notEqual(mergeMask, kNoChangeMask).sum() == 0)
        applyToThisNoMask<utils::MathOperations::Add>(*this, other, numChannels, numSamples,
          thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
      else
        applyToThis(*this, other, numChannels, numSamples, utils::MathOperations::Add, 
          mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
    }

    void multiply(SIMD value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto indices = getAbsoluteIndices(channel, size_, index);
      data_[indices.first] *= value;
    }

    // prefer the SIMD version
    void multiply(T value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto scalar = readValueAt(channel, index);
      writeValueAt(scalar * value, channel, index);
    }

    void multiplyBuffer(const SimdBuffer &other, usize numChannels, usize numSamples,
      simd_mask mergeMask = kNoChangeMask, usize thisStartChannel = 0, usize otherStartChannel = 0,
      usize thisStartIndex = 0, usize otherStartIndex = 0) noexcept
    {
      COMPLEX_ASSERT(numChannels <= other.getChannels());
      COMPLEX_ASSERT(numChannels <= getChannels());

      if (simd_mask::notEqual(mergeMask, kNoChangeMask).sum() == 0)
        applyToThisNoMask<utils::MathOperations::Multiply>(*this, other, numChannels, numSamples,
          thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
      else
        applyToThis(*this, other, numChannels, numSamples, utils::MathOperations::Multiply, 
          mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
    }
    

    // returns packed values from the buffer
    SIMD readSimdValueAt(usize channel, usize index) const noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      return data_.read(getSimdIndex(channel, size_, index));
    }

    // returns a single value from the buffer
    T readValueAt(usize channel, usize index) const noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto indices = getAbsoluteIndices(channel, size_, index);
      auto scalars = data_.read(indices.first).template getArrayOfValues<T>();

      return scalars[indices.second];
    }

    // writes packed values to the buffer
    void writeSimdValueAt(SIMD value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      data_.write(value, getSimdIndex(channel, size_, index));
    }

    void writeMaskedSimdValueAt(SIMD value, simd_mask mask, usize channel, usize index)
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto indices = getAbsoluteIndices(channel, size_, index);
      data_.write(utils::merge(data_.read(indices.first), value, mask), indices.first);
    }

    // writes a single value to the buffer
    void writeValueAt(T value, usize channel, usize index) noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto indices = getAbsoluteIndices(channel, size_, index);
      auto scalars = data_.read(indices.first).template getArrayOfValues<T>();
      scalars[indices.second] = value;
      data_.write(SIMD(scalars), indices.first);
    }

    bool isEmpty() const noexcept { return channels_ == 0 || size_ == 0; }
    usize getSize() const noexcept {	return size_; }
    usize getChannels() const noexcept { return channels_; }
    usize getSimdChannels() const noexcept { return getTotalSimdChannels(channels_); }

    auto get() noexcept { return data_.get(); }
    const auto get() const noexcept { return data_.get(); }
    MemoryBlock<SIMD> &getData() noexcept { return data_; }
    const MemoryBlock<SIMD> &getData() const noexcept { return data_; }
    auto &getLock() const noexcept { return dataLock_; }

    static constexpr usize getRelativeSize() noexcept { return sizeof(SIMD) / sizeof(T); }

  private:
    MemoryBlock<SIMD> data_;
    usize channels_ = 0;
    usize size_ = 0;

    mutable utils::LockBlame<i32> dataLock_{ 0 };

    static usize getTotalSimdChannels(usize numChannels)
    {	return (usize)std::ceil((double)numChannels / (double)getRelativeSize()); }

    // internal function for calculating SIMD and internal positions of an element
    // first - index to the **SIMD** element 
    // second - channel index of T value inside the SIMD element
    static constexpr std::pair<usize, usize> getAbsoluteIndices(usize channel, usize channelSize, usize index) noexcept
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
      size_ = buffer.getSize();
      dataLock_ = &buffer.dataLock_;
    }

    SIMD readSimdValueAt(usize channel, usize index) const noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      return dataView_.read(SimdBuffer<T, SIMD>::getSimdIndex(beginChannel_ + channel, size_, index));
    }

    T readValueAt(usize channel, usize index) const noexcept
    {
      COMPLEX_ASSERT(channel < getChannels());
      COMPLEX_ASSERT(index < getSize());

      auto indices = SimdBuffer<T, SIMD>::getAbsoluteIndices(beginChannel_ + channel, size_, index);
      auto scalars = dataView_.read(indices.first).template getArrayOfValues<T>();

      return scalars[indices.second];
    }

    auto &getLock() const noexcept { return *dataLock_; }

    bool isEmpty() const noexcept { return channels_ == 0 || size_ == 0; }

    usize getSize() const noexcept { return size_; }
    usize getChannels() const noexcept { return channels_; }
    usize getSimdChannels() const noexcept { return SimdBuffer<T, SIMD>::getTotalSimdChannels(channels_); }

    static constexpr usize getRelativeSize() noexcept { return SimdBuffer<T, SIMD>::getRelativeSize(); }

    const auto get() const noexcept { return dataView_.get(); }
    const MemoryBlockView<SIMD> &getData() const noexcept { return dataView_; }

  private:
    MemoryBlockView<SIMD> dataView_{};
    usize beginChannel_ = 0;
    usize channels_ = 0;
    usize size_ = 0;

    utils::LockBlame<i32> *dataLock_ = nullptr;

    template<typename T, SimdValue SIMD>
    friend class SimdBuffer;
  };

  template<typename T, SimdValue SIMD>
  bool operator==(const SimdBufferView<T, SIMD> &lhs, const SimdBuffer<T, SIMD> &rhs) noexcept { return lhs.getData() == rhs.getData(); }

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
    COMPLEX_ASSERT(thisBuffer.getData() != otherBuffer.getData());

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
    auto thisDataBlock = thisBuffer.getData();
    auto otherDataBlock = otherBuffer.getData();

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
    COMPLEX_ASSERT(thisBuffer.getData() != otherBuffer.getData());

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

        COMPLEX_ASSERT(thisBuffer.getData() != otherBuffer.getData() && 
          "We're copying from a buffer to itself consider using something else if this is intentional");

        std::memcpy(&thisData[thisChannelIndices.first], &otherData[otherChannelIndices.first], samples * sizeof(SIMD));
      }
    }
  }

  template<typename T>
  struct complex { T values[2]{}; };

  struct ComplexDataSource
  {
    enum DataSourceType : u8 { Cartesian, Polar, Both };

    // is the data in sourceBuffer polar or cartesian
    DataSourceType dataType = Cartesian;
    // this is the normalised phase currently processed block
    // see SoundEngine::phaseInsideBlock_ for more info
    float normalisedPhase = 0.0f;
    SimdBufferView<complex<float>, simd_float> sourceBuffer;
    // scratch buffer for conversion between cartesian and polar data
    SimdBuffer<complex<float>, simd_float> conversionBuffer{};
  };
}
