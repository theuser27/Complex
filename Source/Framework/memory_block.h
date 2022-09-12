/*
	==============================================================================

		memory_block.h
		Created: 30 Dec 2021 12:29:35pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <cstdlib>
#include "common.h"

namespace Framework
{
	// used for allocating raw memory and using it for whatever purpose
	template <typename T, u64 alignment = alignof(T)>
	class MemoryBlock
	{
	public:
		MemoryBlock() = default;
		MemoryBlock(u64 numElements, bool initialiseToZero = false)
		{
			allocate(numElements, initialiseToZero);
		}
		~MemoryBlock() = default;

		MemoryBlock(const MemoryBlock &other) = delete;
		MemoryBlock &operator= (const MemoryBlock<T, alignment> &other) = delete;

		MemoryBlock(MemoryBlock&& other) noexcept
		{
			data_ = std::move(other.data_);
			absoluteSize_ = other.absoluteSize_;
		}

		MemoryBlock& operator= (MemoryBlock<T, alignment>&& other) noexcept
		{
			if (this != &other)
			{
				data_ = std::move(other.data_);
				absoluteSize_ = other.absoluteSize_;
			}

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

		perf_inline void swap(MemoryBlock<T, alignment>& other) noexcept
		{
			if (&other == this)
				return;
			data_.swap(other.data_);
			std::swap(absoluteSize_, other.absoluteSize_);
		}

		perf_inline void clear() noexcept
		{ std::memset(data_.get(), 0, absoluteSize_); }

		perf_inline T read(u64 index) const noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_);
			return data_[index];
		}

		perf_inline void write(T value, u64 index) noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_);
			data_[index] = value;
		}


		strict_inline u64 getAbsoluteSize() const noexcept { return absoluteSize_; }
		perf_inline std::weak_ptr<T[]> getData() const noexcept { return data_; }

		perf_inline T& operator[] (u64 index) noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_);
			return data_[index];
		}

		strict_inline bool operator== (const T* otherPointer) const noexcept { return otherPointer == data_.get(); }
		strict_inline bool operator!= (const T* otherPointer) const noexcept { return otherPointer != data_.get(); }

	private:
		// size in bytes
		u64 absoluteSize_ = 0;
		std::shared_ptr<T[]> data_{ nullptr };

		static strict_inline void deleter(T *memory) noexcept
		{
		#if defined(__GNUC__)
			std::free(memory);
		#elif defined(_MSC_VER)
			_aligned_free(memory);
		#endif
		}
	};
}