/*
	==============================================================================

		simd_buffer.h
		Created: 25 Oct 2021 11:14:40pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "memory_block.h"
#include "utils.h"
#include "simd_utils.h"

namespace Framework
{
	// T - base type
	// SIMD - simd type
	// intended for use of simd types
	template<typename T, commonConcepts::SimdValue SIMD, u32 alignment = alignof(SIMD)>
		/*requires commonConcepts::Addable<SIMD> && commonConcepts::Multipliable<SIMD> &&
			commonConcepts::OperatorParen<SIMD> && commonConcepts::OperatorBracket<SIMD>*/
	class SimdBuffer 
	{
	public:
		static_assert(alignment % alignof(T) == 0);

		SimdBuffer() = default;
		~SimdBuffer() = default;

		SimdBuffer(u32 numChannels, u32 size)
		{
			COMPLEX_ASSERT(numChannels > 0 && size > 0);
			reserve(numChannels, size);
		}

		SimdBuffer(const SimdBuffer &other, bool doDataCopy = false)
		{
			COMPLEX_ASSERT(other.getNumChannels() > 0 && other.getSize() > 0);
			reserve(other.getNumChannels(), other.getSize());

			if (doDataCopy)
				copyToThis(*this, other, other.getNumChannels(), other.getSize());
		}

		SimdBuffer &operator=(const SimdBuffer &other) = delete;
		SimdBuffer(SimdBuffer &&other) = delete;
		SimdBuffer &operator=(const SimdBuffer &&other) = delete;

		perf_inline void swap(SimdBuffer<T, SIMD> &other)
		{
			if (&other == this)
				return;

			u32 newChannels = other.channels_;
			u32 newSize = other.size_;
			MemoryBlock<SIMD> newData;
			newData.swap(other.data_);

			other.data_.swap(data_);
			other.channels_ = channels_;
			other.size_ = size_;

			data_.swap(newData);
			channels_ = newChannels;
			size_ = newSize;
		}

		void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false)
		{
			// TODO: make it so the memory allocation happens on a different thread
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= channels_) && (newSize <= size_) && !fitToSize)
				return;

			u32 newSimdChannels = calculateNumSimdChannels(newNumChannels);
			MemoryBlock<SIMD> newData(newSimdChannels * newSize, true);

			// checking if we even have any elements to move
			if (channels_ * size_ != 0)
			{
				u32 simdChannelsToCopy = (newSimdChannels < simdChannels_) ? newSimdChannels : simdChannels_;
				u32 capacityToCopy = (newSize < size_) ? newSize : size_;
				auto newBufferData = newData.getData().lock();
				auto oldBufferData = getData().lock();
				for (u32 i = 0; i <= simdChannelsToCopy; i++)
					std::memcpy(&newBufferData[i * newSize], &oldBufferData[i * size_], capacityToCopy * sizeof(SIMD));
			}

			data_.swap(newData);

			channels_ = newNumChannels;
			simdChannels_ = newSimdChannels;
			size_ = newSize;
		}

		perf_inline void clear()
		{ data_.clear(); }

		// copies/does math operation on samples from "otherBuffer" to "thisBuffer"
		// specified by starting channels/indices
		// result is shifted by shiftMask and filtered with mergeMask
		// note: starting channels need to be congruent to kNumChannels
		static void copyToThis(SimdBuffer &thisBuffer, const SimdBuffer &otherBuffer, u32 numChannels, 
			u32 numSamples, utils::MathOperations operation = utils::MathOperations::Assign, simd_mask mergeMask = kNoChangeMask,
			u32 thisStartChannel = 0, u32 otherStartChannel = 0, u32 thisStartIndex = 0, u32 otherStartIndex = 0)
		{
			COMPLEX_ASSERT(thisBuffer.getNumChannels() >= thisStartChannel + numChannels);
			COMPLEX_ASSERT(otherBuffer.getNumChannels() >= otherStartChannel + numChannels);
			COMPLEX_ASSERT(thisBuffer.getSize() >= numSamples);
			COMPLEX_ASSERT(otherBuffer.getSize() >= numSamples);

			// defining the math operations
			SIMD (*opFunction)(SIMD, SIMD, simd_mask);
			switch (operation)
			{
			case utils::MathOperations::Add:

				opFunction = [](SIMD one, SIMD two, simd_mask mask) { return utils::maskLoad(one + two, one, mask); };
				break;

			case utils::MathOperations::Multiply:

				opFunction = [](SIMD one, SIMD two, simd_mask mask) { return utils::maskLoad(one * two, one, mask); };
				break;

			default:
			case utils::MathOperations::Assign:
				
				opFunction = [](SIMD one, SIMD two, simd_mask mask)	{ return utils::maskLoad(two, one, mask); };
				break;
			}

			// getting data and setting up variables
			auto otherDataPointer = otherBuffer.getData().lock();
			auto thisDataPointer = thisBuffer.getData().lock();

			auto thisBufferSize = thisBuffer.getSize();
			auto otherBufferSize = otherBuffer.getSize();
			u32 simdNumChannels = calculateNumSimdChannels(numChannels);

			for (u32 i = 0; i < simdNumChannels; i++)
			{
				// getting indices to the beginning of the SIMD channel buffer blocks
				auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, 0);
				auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, 0);

				for (u32 k = 0; k < numSamples; k++)
				{
					thisDataPointer[thisChannelIndices.first + thisStartIndex + k]
						= opFunction(thisDataPointer[thisChannelIndices.first + thisStartIndex + k],
							otherDataPointer[otherChannelIndices.first + otherStartIndex + k], mergeMask);
				}
			}
		}

		static void copyToThisNoMask(SimdBuffer &thisBuffer, const SimdBuffer &otherBuffer,
			u32 numChannels, u32 numSamples, utils::MathOperations operation = utils::MathOperations::Assign,
			u32 thisStartChannel = 0, u32 otherStartChannel = 0, u32 thisStartIndex = 0, u32 otherStartIndex = 0)
		{
			COMPLEX_ASSERT(thisBuffer.getNumChannels() >= thisStartChannel + numChannels);
			COMPLEX_ASSERT(otherBuffer.getNumChannels() >= otherStartChannel + numChannels);
			COMPLEX_ASSERT(thisBuffer.getSize() >= numSamples);
			COMPLEX_ASSERT(otherBuffer.getSize() >= numSamples);

			// getting data and setting up variables
			auto otherDataPointer = otherBuffer.getData().lock();
			auto thisDataPointer = thisBuffer.getData().lock();

			auto thisBufferSize = thisBuffer.getSize();
			auto otherBufferSize = otherBuffer.getSize();
			u32 simdNumChannels = calculateNumSimdChannels(numChannels);
			switch (operation)
			{
			case utils::MathOperations::Add:

				for (u32 i = 0; i < simdNumChannels; i++)
				{
					// getting indices to the beginning of the SIMD channel buffer blocks
					auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, 0);
					auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, 0);

					for (u32 k = 0; k < numSamples; k++)
					{
						thisDataPointer[thisChannelIndices.first + thisStartIndex + k] +=
							otherDataPointer[otherChannelIndices.first + otherStartIndex + k];
					}
				}
				break;

			case utils::MathOperations::Multiply:

				for (u32 i = 0; i < simdNumChannels; i++)
				{
					// getting indices to the beginning of the SIMD channel buffer blocks
					auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, 0);
					auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, 0);

					for (u32 k = 0; k < numSamples; k++)
					{
						thisDataPointer[thisChannelIndices.first + thisStartIndex + k] *=
							otherDataPointer[otherChannelIndices.first + otherStartIndex + k];
					}
				}
				break;

			default:
			case utils::MathOperations::Assign:

				for (u32 i = 0; i < simdNumChannels; i++)
				{
					// getting indices to the beginning of the SIMD channel buffer blocks
					auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, 0);
					auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, 0);

					for (u32 k = 0; k < numSamples; k++)
					{
						thisDataPointer[thisChannelIndices.first + thisStartIndex + k] =
							otherDataPointer[otherChannelIndices.first + otherStartIndex + k];
					}
				}
				break;
			}
		}


		perf_inline void add(SIMD value, u32 channel, u32 index)
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());
			
			auto indices = getAbsoluteIndices(channel, size_, index);
			data_[indices.first] += value;
		}

		// prefer the SIMD version
		perf_inline void add(T value, u32 channel, u32 index)
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto scalar = getValueAt(channel, index);
			writeValueAt(scalar + value, channel, index);
		}

		perf_inline void addBuffer(SimdBuffer<T, SIMD, alignment> &other, u32 numChannels, u32 numSamples,
			simd_mask mergeMask = kNoChangeMask, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
			 u32 thisStartIndex = 0, u32 otherStartIndex = 0)
		{
			COMPLEX_ASSERT(numChannels <= other.getNumChannels());
			COMPLEX_ASSERT(numChannels <= getNumChannels());

			if (simd_mask::notEqual(mergeMask, kNoChangeMask).sum() == 0)
				copyToThisNoMask(*this, other, numChannels, numSamples, utils::MathOperations::Add,
					thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
			else
				copyToThis(*this, other, numChannels, numSamples, utils::MathOperations::Add, 
					mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
		}

		perf_inline void multiply(SIMD value, u32 channel, u32 index)
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			data_[indices.first] *= value;
		}

		// prefer the SIMD version
		perf_inline void multiply(T value, u32 channel, u32 index)
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto scalar = getValueAt(channel, index);
			writeValueAt(scalar * value, channel, index);
		}

		perf_inline void multiplyBuffer(SimdBuffer<T, SIMD, alignment> &other, u32 numChannels, u32 numSamples,
			simd_mask mergeMask = kNoChangeMask, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
			 u32 thisStartIndex = 0, u32 otherStartIndex = 0)
		{
			COMPLEX_ASSERT(numChannels <= other.getNumChannels());
			COMPLEX_ASSERT(numChannels <= getNumChannels());

			if (simd_mask::notEqual(mergeMask, kNoChangeMask).sum() == 0)
				copyToThisNoMask(*this, other, numChannels, numSamples, utils::MathOperations::Multiply,
					thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
			else
				this->copyToThis(*this, other, numChannels, numSamples, utils::MathOperations::Multiply, 
					mergeMask, thisStartChannel, otherStartChannel, thisStartIndex, otherStartIndex);
		}
		

		// returns packed values from the buffer
		perf_inline SIMD getSIMDValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			return data_.read(indices.first);
		}

		// returns a single value from the buffer
		perf_inline T getValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			auto scalars = data_.read(indices.first).getArrayOfValues<T>();

			return scalars[indices.second];
		}

		// writes packed values to the buffer
		perf_inline void writeSIMDValueAt(SIMD value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			data_.write(value, indices.first);
		}

		// writes a single value to the buffer
		perf_inline void writeValueAt(T value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getNumChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			auto data = data_.getData().lock();
			auto scalars = data_.read(indices.first).getArrayOfValues<T>();
			scalars[indices.second] = value;
			data_.write(SIMD(scalars), indices.first);
		}


		strict_inline u32 getSize() const noexcept
		{	return size_; }

		strict_inline u32 getNumChannels() const noexcept
		{ return channels_; }

		strict_inline u32 getNumSimdChannels() const noexcept
		{ return simdChannels_; }

		perf_inline std::weak_ptr<SIMD[]> getData() const noexcept
		{ return data_.getData(); }

		// can get raw pointer if the context already has a shared_ptr
		perf_inline static SIMD* getDataPointer(std::shared_ptr<SIMD[]>& dataPtr, u32 channel, u32 index, u32 size) noexcept
		{
			auto indices = getAbsoluteIndices(channel, size, index);
			return &dataPtr[indices.first];
		}

		strict_inline static constexpr u32 getRelativeSize() noexcept
		{ return sizeof(SIMD) / sizeof(T); }

	private:
		u32 channels_ = 0;
		u32 size_ = 0;
		u32 simdChannels_ = 0;
		MemoryBlock<SIMD> data_;

		strict_inline static u32 calculateNumSimdChannels(u32 numChannels)
		{	return (u32)std::ceil((double)numChannels / (double)getRelativeSize()); }

		// internal function for calculating SIMD and internal positions of an element
		// first - index to the **SIMD** element 
		// second - channel index of T value inside the SIMD element
		strict_inline static constexpr auto getAbsoluteIndices(u32 channel, u32 channelSize, u32 index) noexcept
		{	return std::pair<u32, u32>((channel / getRelativeSize()) * channelSize + index, channel % getRelativeSize()); }
	};
}
