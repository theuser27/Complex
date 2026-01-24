/*
  ==============================================================================

    memory_block.hpp
    Created: 30 Dec 2021 12:29:35
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "c_forwards.hpp"
#include "satomi.hpp"
#include "platform_definitions.hpp"

namespace utils
{
  byte *
  allocate(usize size, usize alignment);
  void deallocate(void *memory);
}

namespace Framework
{
  template <typename T, typename ExtraData = void>
  class MemoryBlockView;
  
  // used for allocating raw memory and using it for whatever purpose
  template <typename T, typename ExtraData = void>
  class MemoryBlock
  {
    struct Header
    {
      using U = utils::conditional_t<utils::is_same_v<ExtraData, void>, decltype(utils::ignore), ExtraData>;

      COMPLEX_NO_UNIQUE_ADDRESS U extraData{};
      satomi::atomic<usize> refCount = 1;
      usize alignment = 0;
      usize size = 0;
      T *data = nullptr;
    };

  public:
    MemoryBlock() = default;
    MemoryBlock(usize size, bool initialiseToZero = false, usize alignment = alignof(T)) noexcept
    {
      allocate(size, initialiseToZero, alignment);
    }
    ~MemoryBlock() noexcept
    {
      // guarding against multiple decrements
      auto *header = header_;
      header_ = nullptr;

      reset(header);
    }

    MemoryBlock(const MemoryBlock &other) = delete;
    MemoryBlock &operator=(const MemoryBlock &other) = delete;

    MemoryBlock(MemoryBlock &&other) noexcept
    {
      header_ = other.header_;
      other.header_ = nullptr;
    }

    MemoryBlock& operator=(MemoryBlock &&other) noexcept
    {
      if (this == &other)
        return *this;

      this->~MemoryBlock();

      header_ = other.header_;
      other.header_ = nullptr;

      return *this;
    }

    MemoryBlock
    deepCopy()
    {
      auto newBlock = MemoryBlock{};
      newBlock.copy(*this);
      return newBlock;
    }
    void copy(const MemoryBlock &other)
    {
      if (header_ == other.header_ || !other.header_)
        return;
      
      if (!header_ || header_->size < other.header_->size)
        allocate(other.header_->size, false, other.header_->alignment);

      ::memcpy(header_->data, other.header_->data, other.header_->size * sizeof(T));
    }
    void copy(const MemoryBlock &other, usize destination, usize source, usize size)
    {
      COMPLEX_ASSERT(header_ && other.header_);
      COMPLEX_ASSERT(header_->size >= destination + size);
      COMPLEX_ASSERT(other.header_->size >= source + size);
      ::memcpy(header_->data + destination, other.header_->data + source, size * sizeof(T));
    }
    void copy(const MemoryBlockView<T, ExtraData> &other, usize destination, usize source, usize size);

    void allocate(usize size, bool initialiseToZero = false, usize alignment = alignof(T))
    {
      COMPLEX_ASSERT(alignment >= alignof(T));
      reset(header_);

      usize headerSize = sizeof(Header);
      usize extra = ((headerSize + alignment - 1) / alignment) * alignment;
      usize totalSize = extra + size * sizeof(T);

      byte *memory = utils::allocate(totalSize, alignment);
      header_ = new(memory) Header{ .alignment = alignment, .size = size };
      header_->data = new(memory + extra) T[size];

      if (initialiseToZero)
        ::zeroset(header_->data, size);
    }

    void realloc(usize size, bool initialiseToZero = false, usize alignment = alignof(T))
    {
      COMPLEX_ASSERT(alignment >= alignof(T));
      if (!header_)
      {
        allocate(size, false, alignment);
        return;
      }
      
      COMPLEX_ASSERT(alignment >= header_->alignment);
      auto previousHeader = header_;
      header_ = nullptr;
      this->~MemoryBlock();

      allocate(size, false, alignment);      
      ::memcpy(header_->data, previousHeader->data, ((previousHeader->size < size) ? previousHeader->size : size) * sizeof(T));
      if (initialiseToZero && previousHeader->size < size)
        ::zeroset(header_->data + previousHeader->size, size);

      reset(previousHeader);
    }

    void free() noexcept { this->~MemoryBlock(); }

    void swap(MemoryBlock &other) noexcept
    {
      if (&other == this)
        return;

      auto *temp = other.header_;
      other.header_ = header_;
      header_ = temp;
    }

    void clear() noexcept
    {
      if (!header_)
        return;

      ::zeroset(header_->data, header_->size);
    }

    T read(usize index) const noexcept
    { 
      COMPLEX_ASSERT(header_);
      COMPLEX_ASSERT(index < header_->size);
      return header_->data[index];
    }

    void write(T value, usize index) noexcept
    { 
      COMPLEX_ASSERT(header_);
      COMPLEX_ASSERT(index < header_->size);
      header_->data[index] = value;
    }

    T &operator[](usize index) noexcept
    { 
      COMPLEX_ASSERT(header_);
      COMPLEX_ASSERT(index < header_->size);
      return header_->data[index];
    }

    const T &operator[](usize index) const noexcept
    {
      COMPLEX_ASSERT(header_);
      COMPLEX_ASSERT(index < header_->size);
      return header_->data[index];
    }

    bool operator==(const T* otherPointer) const noexcept { return (header_) ? otherPointer == header_->data : false; }
    bool operator==(const MemoryBlock &other) const noexcept { return header_ == other.header_; }
    bool operator==(const MemoryBlockView<T, ExtraData> &other) const noexcept;

    auto getSize() const noexcept -> usize { return (header_) ? header_->size : 0; }
    auto getSizeInBytes() const noexcept -> usize { return (header_) ? header_->size * sizeof(T) : 0; }
    auto get() noexcept -> CheckedPointer<T> { return (header_) ? CheckedPointer{ header_->data, header_->size } : CheckedPointer<T>{}; }
    auto get() const noexcept -> CheckedPointer<const T> 
    { return (header_) ? CheckedPointer<const T>{ header_->data, header_->size } : CheckedPointer<const T>{}; }

    auto getExtraData() noexcept -> ExtraData * requires (!utils::is_same_v<ExtraData, void>)
    { return (header_) ? &header_->extraData : nullptr; }
    auto getExtraData() const noexcept -> const ExtraData * requires (!utils::is_same_v<ExtraData, void>)
    { return (header_) ? &header_->extraData : nullptr; }

    explicit operator bool() const noexcept { return header_; }

  private:
    void reset(Header *header)
    {
      if (!header || header->refCount.fetch_sub<satomi::memory_order_acq_rel>(1) > 1)
        return;

      utils::deallocate(header);
    }

    Header *header_ = nullptr;

    template <typename U, typename OtherExtraData>
    friend class MemoryBlockView;
  };

  template <typename T, typename ExtraData>
  class MemoryBlockView
  {
  public:
    MemoryBlockView() = default;
    MemoryBlockView(const MemoryBlockView &other)
    {
      block_.header_ = other.block_.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add<satomi::memory_order_relaxed>(1);
    }

    MemoryBlockView &operator=(const MemoryBlockView &other) noexcept
    {
      if (this == &other)
        return *this;
      
      block_.~MemoryBlock();

      block_.header_ = other.block_.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add<satomi::memory_order_relaxed>(1);
      
      return *this;
    }

    MemoryBlockView(const MemoryBlock<T, ExtraData> &block)
    {
      block_.header_ = block.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add<satomi::memory_order_relaxed>(1);
    }

    MemoryBlockView &operator=(const MemoryBlock<T, ExtraData> &block) noexcept
    {
      if (block_ == block)
        return *this;

      block_.~MemoryBlock();

      block_.header_ = block.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add<satomi::memory_order_relaxed>(1);

      return *this;
    }

    T read(usize index) const noexcept { return block_.read(index); }
    const T &operator[](usize index) const noexcept { return block_[index]; }

    auto get() const noexcept { return block_.get(); }
    auto getSizeInBytes() const noexcept -> usize { return block_.getSizeInBytes(); }
    auto getExtraData() const noexcept -> const ExtraData * requires (!utils::is_same_v<ExtraData, void>)
    { return block_.getExtraData(); }

    bool operator==(const MemoryBlock<T, ExtraData> &other) noexcept { return block_ == other; }
    explicit operator bool() const noexcept { return block_; }

  private:
    MemoryBlock<T, ExtraData> block_{};

    template<typename U, typename OtherExtraData>
    friend class MemoryBlock;
  };

  template<typename T, typename ExtraData>
  inline bool MemoryBlock<T, ExtraData>::operator==(const MemoryBlockView<T, ExtraData> &other) const noexcept { return *this == other.block_; }

  template <typename T, typename ExtraData>
  void MemoryBlock<T, ExtraData>::copy(const MemoryBlockView<T, ExtraData> &other, usize destination, usize source, usize size)
  { copy(other.block_, destination, source, size); }
}