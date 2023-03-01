/*
	==============================================================================

		fifo_queue.h
		Created: 10 Oct 2022 6:47:43pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"

namespace Framework
{
	template <typename T>
	class FIFOQueue
	{
	public:
		FIFOQueue() = default;

		FIFOQueue(u32 capacity) noexcept : capacity_(capacity), begin_(0), end_(0)
		{ data_ = std::make_unique<T[]>(capacity_); }

		void reserve(u32 capacity) noexcept
		{
			if (capacity < capacity_)
				return;

			std::unique_ptr<T[]> tmp = std::make_unique<T[]>(capacity);

			if (getSize())
			{
				end_ = getSize() - 1;
				for (u32 i = 0; i <= end_; ++i)
					tmp[i] = std::move(at(i));
			}

			data_ = std::move(tmp);
			capacity_ = capacity;
			begin_ = 0;
		}

		T &at(std::size_t index) noexcept
		{
			COMPLEX_ASSERT(index >= 0 && index < getSize());
			return data_[(begin_ + static_cast<u32>(index)) % capacity_];
		}

		void push_back(T entry) noexcept
		{
			data_[end_] = std::move(entry);
			end_ = (end_ + 1) % capacity_;
			COMPLEX_ASSERT(end_ != begin_);
		}

		T pop_front() noexcept
		{
			COMPLEX_ASSERT(end_ != begin_);
			T element = std::move(data_[begin_]);
			data_[begin_].~T();
			begin_ = (begin_ + 1) % capacity_;
			return element;
		}

		u32 getSize() const noexcept { return (capacity_ > 0) ? (capacity_ + end_ - begin_) % capacity_ : 0; }
		u32 getCapacity() const noexcept { return capacity_; }

	private:
		std::unique_ptr<T[]> data_ = nullptr;
		u32 capacity_ = 0;
		u32 begin_ = 0;
		u32 end_ = 0;
	};
}