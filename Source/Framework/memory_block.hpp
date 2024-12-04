/*
  ==============================================================================

    memory_block.hpp
    Created: 30 Dec 2021 12:29:35pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <cstdlib>
#include <new>
#include <atomic>

#include "platform_definitions.hpp"

namespace Framework
{
  // thin abstraction intended to be zero-cost in release builds 
  // but provide checked access for debug ones
  template<typename T>
  struct CheckedPointer
  {
    constexpr CheckedPointer() = default;
    constexpr CheckedPointer(T *data, [[maybe_unused]] usize dataSize = 0) : pointer(data)
    #if COMPLEX_DEBUG
      , size(dataSize)
    #endif
    { }
    T *pointer = nullptr;
  private:

  #if COMPLEX_DEBUG
    [[maybe_unused]] usize size = 0;
  #endif

  public:
    constexpr CheckedPointer offset(usize offset, [[maybe_unused]] usize explicitSize = 0) noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(offset < size);
      COMPLEX_ASSERT(explicitSize == 0 || explicitSize <= size - offset);
      return CheckedPointer
      {
        pointer + offset 
      #if COMPLEX_DEBUG
        , (explicitSize) ? explicitSize : size - offset
      #endif
      };
    }

    constexpr T &operator[](usize index) noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(index < size);
      return pointer[index];
    }

    constexpr const T &operator[](usize index) const noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(index < size);
      return pointer[index];
    }

    constexpr operator T *() noexcept { return pointer; }
  };

  template <typename T>
  class MemoryBlockView;
  
  // used for allocating raw memory and using it for whatever purpose
  template <typename T>
  class MemoryBlock
  {
    struct Header
    {
      std::atomic<usize> refCount = 1;
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

      if (!header || header->refCount.fetch_sub(1, std::memory_order_acq_rel) > 1)
        return;

    #ifdef COMPLEX_MSVC
      _aligned_free(header);
    #else
      std::free(header);
    #endif
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

    MemoryBlock deepCopy()
    {
      auto newBlock = MemoryBlock{ header_->size };
      newBlock.copy(*this);
      return newBlock;
    }
    void copy(const MemoryBlock &other)
    {
      if (header_ == other.header_ || !other.header_)
        return;
      
      if (!header_ || header_->size < other.header_->size)
        allocate(other.header_->size, false, other.header_->alignment);

      std::memcpy(header_->data, other.header_->data, other.header_->size * sizeof(T));
    }
    void copy(const MemoryBlock &other, usize destination, usize source, usize size)
    {
      COMPLEX_ASSERT(header_ && other.header_);
      COMPLEX_ASSERT(header_->size >= destination + size);
      COMPLEX_ASSERT(other.header_->size >= source + size);
      std::memcpy(header_->data + destination, other.header_->data + source, size * sizeof(T));
    }
    void copy(const MemoryBlockView<T> &other, usize destination, usize source, usize size);

    void allocate(usize size, bool initialiseToZero = false, usize alignment = alignof(T))
    {
      COMPLEX_ASSERT(alignment >= alignof(T));
      if (header_)
        this->~MemoryBlock();

      auto extra = ((sizeof(Header) + alignment - 1) / alignment) * alignment;

      unsigned char *memory;
    #ifdef COMPLEX_MSVC
      memory = (unsigned char *)_aligned_malloc(extra + size * sizeof(T), alignment);
    #else
      memory = (unsigned char *)std::aligned_alloc(alignment, extra + size * sizeof(T)));
    #endif
      header_ = new(memory) Header{ .alignment = alignment, .size = size };
      header_->data = new(memory + extra) T[size];

      if (initialiseToZero)
        std::memset(header_->data, 0, size * sizeof(T));
    }

    void realloc(usize size, usize alignment = alignof(T))
    {
      COMPLEX_ASSERT(alignment >= alignof(T));
      if (!header_)
      {
        allocate(size, false, alignment);
        return;
      }
      
      COMPLEX_ASSERT(alignment >= header_->alignment);
      auto extra = ((sizeof(Header) + alignment - 1) / alignment) * alignment;

      unsigned char *memory;
      Header *header;
    #ifdef COMPLEX_MSVC
      memory = (unsigned char *)_aligned_malloc(extra + size * sizeof(T), alignment);
    #else
      memory = (unsigned char *)std::aligned_alloc(alignment, extra + size * sizeof(T)));
    #endif
      header = new(memory) Header{ .alignment = alignment, .size = size };
      header->data = new(memory + extra) T[size];
      std::memcpy(header->data, header_->data, ((header_->size < size) ? header_->size : size) * sizeof(T));

      this->~MemoryBlock();
      header_ = header;
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

      std::memset(header_->data, 0, header_->size * sizeof(T));
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
    bool operator==(const MemoryBlockView<T> &other) const noexcept;

    usize getSize() const noexcept { return (header_) ? header_->size : 0; }
    usize getSizeInBytes() const noexcept { return (header_) ? header_->size * sizeof(T) : 0; }
    auto get() noexcept { return (header_) ? CheckedPointer{ header_->data, header_->size } : CheckedPointer<T>{}; }
    const auto get() const noexcept { return (header_) ? CheckedPointer{ header_->data, header_->size } : CheckedPointer<T>{}; }

  private:
    Header *header_ = nullptr;

    template <typename T>
    friend class MemoryBlockView;
  };

  template <typename T>
  class MemoryBlockView
  {
  public:
    MemoryBlockView() = default;
    MemoryBlockView(const MemoryBlockView &other)
    {
      block_.header_ = other.block_.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add(1, std::memory_order_relaxed);
    }

    MemoryBlockView &operator=(const MemoryBlockView &other) noexcept
    {
      if (this == &other)
        return *this;
      
      block_.~MemoryBlock();

      block_.header_ = other.block_.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add(1, std::memory_order_relaxed);
      
      return *this;
    }

    MemoryBlockView(const MemoryBlock<T> &block)
    {
      block_.header_ = block.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add(1, std::memory_order_relaxed);
    }

    MemoryBlockView &operator=(const MemoryBlock<T> &block) noexcept
    {
      if (block_ == block)
        return *this;

      block_.~MemoryBlock();

      block_.header_ = block.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add(1, std::memory_order_relaxed);

      return *this;
    }

    T read(usize index) const noexcept { return block_.read(index); }
    const T& operator[](usize index) const noexcept { return block_[index]; }

    const auto get() const noexcept { return block_.get(); }

    bool operator==(const MemoryBlock<T> &other) noexcept { return block_ == other; }

  private:
    MemoryBlock<T> block_{};

    template<typename T>
    friend class MemoryBlock;
  };

  template<typename T>
  inline bool MemoryBlock<T>::operator==(const MemoryBlockView<T> &other) const noexcept { return *this == other.block_; }

  template <typename T>
  void MemoryBlock<T>::copy(const MemoryBlockView<T> &other, usize destination, usize source, usize size)
  { copy(other.block_, destination, source, size); }
}