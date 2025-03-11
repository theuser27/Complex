/*
  ==============================================================================

    memory_block.hpp
    Created: 30 Dec 2021 12:29:35pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <stdlib.h>
#include <string.h>
#if COMPLEX_MSVC
  #include <vcruntime_new.h>
#else
  #include <new>
#endif
#include <atomic>

#include "platform_definitions.hpp"

namespace Framework
{
  // thin abstraction intended to be zero-cost in release builds 
  // but provide checked access for debug ones
  template<typename T>
  struct CheckedPointer
  {
    T *pointer = nullptr;
  #if COMPLEX_DEBUG
  private:
    [[maybe_unused]] usize size = 0;
  public:
  #endif

    constexpr CheckedPointer() = default;
    constexpr CheckedPointer(T *data, [[maybe_unused]] usize dataSize = 0) : pointer(data)
    #if COMPLEX_DEBUG
      , size(dataSize)
    #endif
    { }

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

  template <typename T, typename ExtraData = void>
  class MemoryBlockView;
  
  // used for allocating raw memory and using it for whatever purpose
  template <typename T, typename ExtraData = void>
  class MemoryBlock
  {
    struct Header
    {
      using U = utils::conditional_t<utils::is_same_v<ExtraData, void>, decltype([](){}), ExtraData>;

      COMPLEX_NO_UNIQUE_ADDRESS U extraData{};
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

    MemoryBlock deepCopy()
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

      memcpy(header_->data, other.header_->data, other.header_->size * sizeof(T));
    }
    void copy(const MemoryBlock &other, usize destination, usize source, usize size)
    {
      COMPLEX_ASSERT(header_ && other.header_);
      COMPLEX_ASSERT(header_->size >= destination + size);
      COMPLEX_ASSERT(other.header_->size >= source + size);
      memcpy(header_->data + destination, other.header_->data + source, size * sizeof(T));
    }
    void copy(const MemoryBlockView<T, ExtraData> &other, usize destination, usize source, usize size);

    void allocate(usize size, bool initialiseToZero = false, usize alignment = alignof(T))
    {
      COMPLEX_ASSERT(alignment >= alignof(T));
      reset(header_);

      usize headerSize = sizeof(Header);
      usize extra = ((headerSize + alignment - 1) / alignment) * alignment;
      usize totalSize = extra + size * sizeof(T);
    #ifdef COMPLEX_MSVC
      unsigned char *memory = (unsigned char *)_aligned_malloc(totalSize, alignment);
    #elif COMPLEX_MAC
      static_assert(alignof(max_align_t) >= sizeof(void *));
      unsigned char *memory = nullptr;
      if (alignment >= sizeof(void *)) // like in the posix docs
      {
        void *memory_;
        // aligned_alloc is broken on mac for me, but posix_memalign seems to work
        auto success = posix_memalign(&memory_, alignment, totalSize);
        if (success == 0)
          memory = (unsigned char *)memory_;
      }
      else // fall back to regular malloc
        memory = (unsigned char *)malloc(totalSize);
    #else
      unsigned char *memory = (unsigned char *)aligned_alloc(alignment, totalSize);
    #endif
      COMPLEX_ASSERT(memory, "Memory (%zu) could not be allocated with this alignment (%zu)", totalSize, alignment);
      header_ = new(memory) Header{ .alignment = alignment, .size = size };
      header_->data = new(memory + extra) T[size];

      if (initialiseToZero)
        memset(header_->data, 0, size * sizeof(T));
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
      memcpy(header_->data, previousHeader->data, ((previousHeader->size < size) ? previousHeader->size : size) * sizeof(T));
      if (initialiseToZero && previousHeader->size < size)
        memset(header_->data + previousHeader->size, 0, size * sizeof(T));

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

      memset(header_->data, 0, header_->size * sizeof(T));
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
      if (!header || header->refCount.fetch_sub(1, std::memory_order_acq_rel) > 1)
        return;

    #ifdef COMPLEX_MSVC
      _aligned_free(header);
    #else
      ::free(header);
    #endif
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

    MemoryBlockView(const MemoryBlock<T, ExtraData> &block)
    {
      block_.header_ = block.header_;
      if (block_.header_)
        block_.header_->refCount.fetch_add(1, std::memory_order_relaxed);
    }

    MemoryBlockView &operator=(const MemoryBlock<T, ExtraData> &block) noexcept
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