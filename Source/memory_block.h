/*
  ==============================================================================

    memory_block.h
    Created: 30 Dec 2021 12:29:35pm
    Author:  Lenovo

  ==============================================================================
*/

#pragma once

#include <cstdlib>
#include "common.h"

namespace Framework
{
  template <typename T, u64 alignment = alignof(T)>
  class memory_block
  {
  public:
    memory_block() = default;

    explicit memory_block(u64 numElements)
    {
      allocate(numElements);
    }

    memory_block(u64 numElements, bool initialiseToZero)
    {
      allocate(numElements, initialiseToZero);
    }

    ~memory_block() = default;

    memory_block(const memory_block<T, alignment> &other) = delete;
    memory_block &operator= (const memory_block<T, alignment> &other) = delete;

    memory_block(memory_block<T, alignment>&& other) noexcept
    {
      data_.swap(other.data_);
      std::swap(absoluteSize_, other.absoluteSize_);
      other.data_.reset();
    }

    memory_block& operator= (memory_block<T, alignment>&& other) noexcept
    {
      data_.swap(other.data_);
      std::swap(absoluteSize_, other.absoluteSize_);
      return *this;
    }

    void allocate(u64 newNumElements, bool initialiseToZero = false)
    {
      // custom deleter is supplied because we're using aligned memory alloc 
      // and on MSVC it is necessary to deallocate it with _aligned_free
      absoluteSize_ = newNumElements * alignment;
    #if defined(__GNUC__)
      data_.reset(static_cast<T *>(std::aligned_alloc(alignment, absoluteSize_)), [](T *memory) { deleter(memory); });
    #elif defined(_MSC_VER)
      data_.reset(static_cast<T *>(_aligned_malloc(absoluteSize_, alignment)), [](T *memory) { deleter(memory); });
    #endif
      if (initialiseToZero)
        std::memset(data_.get(), 0, absoluteSize_);
    }

    void realloc(u64 newNumElements)
    {
      if (!data_)
      {
        absoluteSize_ = newNumElements * alignment;
      #if defined(__GNUC__)
        data_.reset(static_cast<T *>(std::aligned_alloc(alignment, absoluteSize_)), [](T *memory) { deleter(memory); });
      #elif defined(_MSC_VER)
        data_.reset(static_cast<T *>(_aligned_malloc(absoluteSize_, alignment)), [](T *memory) { deleter(memory); });
      #endif
      }
      else
      {
      #if defined(__GNUC__)
        T *newPtr = static_cast<T *>(std::aligned_alloc(alignment, newNumElements * alignment));
        u64 copySize = ((newNumElements * alignment) < absoluteSize_) ?
          (newNumElements * alignment) : absoluteSize_;
        std::memcpy(newPtr, data_.get(), copySize);
        data_.reset(newPtr, [](T *memory) { deleter(memory); });
        absoluteSize_ = newNumElements * alignment;
      #elif defined(_MSC_VER)
        absoluteSize_ = newNumElements * alignment;
        T *ptr = data_.get();
        data_.reset(static_cast<T *>(_aligned_realloc(ptr, absoluteSize_, alignment)), [](T *memory) { deleter(memory); });
      #endif
      }
    }

    void free() noexcept
    {
      data_.reset();
      absoluteSize_ = 0;
    }

    void swap(memory_block<T, alignment>& other) noexcept
    {
      if (&other == this)
        return;
      data_.swap(other.data_);
      std::swap(absoluteSize_, other.absoluteSize_);
    }

    void clear() noexcept
    {
      std::memset(data_.get(), 0, absoluteSize_);
    }

    force_inline T read(u64 index) const noexcept
    { 
      COMPLEX_ASSERT(index < absoluteSize_);
      return data_[index];
    }

    force_inline void write(T value, u64 index) noexcept
    { 
      COMPLEX_ASSERT(index < absoluteSize_);
      data_[index] = value;
    }


    force_inline u64 getAbsoluteSize() const noexcept { return absoluteSize_; }
    force_inline std::weak_ptr<T[]> getData() const noexcept { return data_; }

    T& operator[] (u64 index) noexcept
    { 
      COMPLEX_ASSERT(index < absoluteSize_);
      return data_[index];
    }

    force_inline bool operator== (const T* otherPointer) const noexcept { return otherPointer == data_.get(); }
    force_inline bool operator!= (const T* otherPointer) const noexcept { return otherPointer != data_.get(); }

  private:
    // size in bytes
    u64 absoluteSize_ = 0;
    std::shared_ptr<T[]> data_{ nullptr };

    static force_inline void deleter(T *memory) noexcept
    {
    #if defined(__GNUC__)
      std::free(memory);
    #elif defined(_MSC_VER)
      _aligned_free(memory);
    #endif
    }
  };
}