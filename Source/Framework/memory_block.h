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
		MemoryBlock(u64 numElements, bool initialiseToZero = false) noexcept
		{
			allocate(numElements, initialiseToZero);
		}
		~MemoryBlock() = default;

		MemoryBlock(const MemoryBlock &other) = delete;
		MemoryBlock &operator= (const MemoryBlock &other) = delete;

		MemoryBlock(MemoryBlock&& other) noexcept
		{
			data_ = std::move(other.data_);
			absoluteSize_ = other.absoluteSize_;
		}

		MemoryBlock& operator= (MemoryBlock&& other) noexcept
		{
			if (this != &other)
			{
				data_ = std::move(other.data_);
				absoluteSize_ = other.absoluteSize_;
			}

			return *this;
		}

		void copy(const MemoryBlock &other)
		{
			if (absoluteSize_ < other.absoluteSize_)
				allocate(other.absoluteSize_ / sizeof(T));

			std::memcpy(data_.get(), other.data_.get(), other.absoluteSize_);
			absoluteSize_ = other.absoluteSize_;
		}

		void allocate(u64 newNumElements, bool initialiseToZero = false)
		{
			// custom deleter is supplied because we're using aligned memory alloc 
			// and on MSVC it is necessary to deallocate it with _aligned_free
			absoluteSize_ = newNumElements * sizeof(T);
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
				absoluteSize_ = newNumElements * sizeof(T);
			#if defined(__GNUC__)
				data_.reset(static_cast<T *>(std::aligned_alloc(alignment, absoluteSize_)), [](T *memory) { deleter(memory); });
			#elif defined(_MSC_VER)
				data_.reset(static_cast<T *>(_aligned_malloc(absoluteSize_, alignment)), [](T *memory) { deleter(memory); });
			#endif
			}
			else
			{
			#if defined(__GNUC__)
				u64 newSize = newNumElements * sizeof(T);
				T *newPtr = static_cast<T *>(std::aligned_alloc(alignment, newSize));
				u64 copySize = (newSize < absoluteSize_) ? newSize : absoluteSize_;
				std::memcpy(newPtr, data_.get(), copySize);
				data_.reset(newPtr, [](T *memory) { deleter(memory); });
				absoluteSize_ = newSize;
			#elif defined(_MSC_VER)
				absoluteSize_ = newNumElements * sizeof(T);
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

		perf_inline void swap(MemoryBlock& other) noexcept
		{
			if (&other == this)
				return;
			data_.swap(other.data_);
			std::swap(absoluteSize_, other.absoluteSize_);
		}

		void clear() noexcept { std::memset(data_.get(), 0, absoluteSize_); }

		T read(u64 index) const noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_ / sizeof(T));
			return data_[index];
		}

		void write(T value, u64 index) noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_ / sizeof(T));
			data_[index] = value;
		}

		T& operator[](u64 index) noexcept
		{ 
			COMPLEX_ASSERT(index < absoluteSize_ / sizeof(T));
			return data_[index];
		}

		const T& operator[](u64 index) const noexcept
		{
			COMPLEX_ASSERT(index < absoluteSize_ / sizeof(T));
			return data_[index];
		}

		bool operator== (const T* otherPointer) const noexcept { return otherPointer == data_.get(); }
		bool operator!= (const T* otherPointer) const noexcept { return otherPointer != data_.get(); }

		bool isEmpty() const noexcept { return data_.use_count(); }

		u64 getAbsoluteSize() const noexcept { return absoluteSize_; }
		std::shared_ptr<T[]> getData() noexcept { return data_; }

	private:
		// size in bytes
		u64 absoluteSize_ = 0;
		std::shared_ptr<T[]> data_{ nullptr };

		static void deleter(T *memory) noexcept
		{
		#if defined(__GNUC__)
			std::free(memory);
		#elif defined(_MSC_VER)
			_aligned_free(memory);
		#endif
		}

		template <typename T, u64 alignment = alignof(T)>
		friend class MemoryBlockView;
	};

	template <typename T, u64 alignment = alignof(T)>
	class MemoryBlockView
	{
	public:
		MemoryBlockView() = default;
		MemoryBlockView(const MemoryBlock<T, alignment> &block)
		{
			block_.data_ = block.data_;
			block_.absoluteSize_ = block.absoluteSize_;
		}

		MemoryBlockView &operator= (const MemoryBlock<T, alignment> &block) noexcept
		{
			if (block_.data_ != block.data_)
			{
				block_.data_ = block.data_;
				block_.absoluteSize_ = block.absoluteSize_;
			}
			return *this;
		}

		T read(u64 index) const noexcept { return block_.read(index); }
		const T& operator[](u64 index) const noexcept { return block_[index]; }

		bool isEmpty() const noexcept { return block_.isEmpty(); }

	private:
		MemoryBlock<T, alignment> block_{};
	};
}