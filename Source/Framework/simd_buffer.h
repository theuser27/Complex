/*
	==============================================================================

		simd_buffer.h
		Created: 25 Oct 2021 11:14:40pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "sync_primitives.h"
#include "memory_block.h"
#include "utils.h"
#include "simd_utils.h"

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
		SimdBuffer(u32 numChannels, u32 size) noexcept { reserve(numChannels, size); }
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

		void reserve(u32 newNumChannels, u32 newSize, bool fitToSize = false) noexcept
		{
			// TODO: make it so the memory allocation happens on a different thread
			COMPLEX_ASSERT(newNumChannels > 0 && newSize > 0);
			if ((newNumChannels <= channels_) && (newSize <= size_) && !fitToSize)
				return;

			u32 oldSimdChannels = getTotalSimdChannels(channels_);
			u32 newSimdChannels = getTotalSimdChannels(newNumChannels);
			MemoryBlock<SIMD> newData(newSimdChannels * newSize, true);

			// checking if we even have any elements to move
			if (channels_ * size_ != 0)
			{
				u32 simdChannelsToCopy = (newSimdChannels < oldSimdChannels) ? newSimdChannels : oldSimdChannels;
				u32 capacityToCopy = (newSize < size_) ? newSize : size_;
				auto newBufferData = newData.getData();
				auto oldBufferData = data_.getData();
				for (u32 i = 0; i <= simdChannelsToCopy; i++)
					std::memcpy(&newBufferData[i * newSize], &oldBufferData[i * size_], capacityToCopy * sizeof(SIMD));
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
		static void applyToThis(SimdBuffer &thisBuffer, const SimdBufferView<T, SIMD> &otherBuffer, u32 numChannels,
			u32 numSamples, utils::MathOperations operation = utils::MathOperations::Assign, simd_mask mergeMask = kNoChangeMask,
			u32 thisStartChannel = 0, u32 otherStartChannel = 0, u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept;

		// same thing without the mask
		template<utils::MathOperations Operation>
		static void applyToThisNoMask(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> &otherBuffer,
			u32 numChannels, u32 numSamples, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
			u32 thisStartIndex = 0, u32 otherStartIndex = 0, SIMD scaleFactor = SIMD(1)) noexcept;


		void add(SIMD value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());
			
			auto indices = getAbsoluteIndices(channel, size_, index);
			data_[indices.first] += value;
		}

		// prefer the SIMD version
		void add(T value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto scalar = readValueAt(channel, index);
			writeValueAt(scalar + value, channel, index);
		}

		void addBuffer(const SimdBuffer &other, u32 numChannels, u32 numSamples,
			simd_mask mergeMask = kNoChangeMask, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
			 u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
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

		void multiply(SIMD value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			data_[indices.first] *= value;
		}

		// prefer the SIMD version
		void multiply(T value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto scalar = readValueAt(channel, index);
			writeValueAt(scalar * value, channel, index);
		}

		void multiplyBuffer(const SimdBuffer &other, u32 numChannels, u32 numSamples,
			simd_mask mergeMask = kNoChangeMask, u32 thisStartChannel = 0, u32 otherStartChannel = 0,
			 u32 thisStartIndex = 0, u32 otherStartIndex = 0) noexcept
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
		SIMD readSimdValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			return data_.read(getSimdIndex(channel, size_, index));
		}

		// returns a single value from the buffer
		T readValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			auto scalars = data_.read(indices.first).template getArrayOfValues<T>();

			return scalars[indices.second];
		}

		// writes packed values to the buffer
		void writeSimdValueAt(SIMD value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			data_.write(value, getSimdIndex(channel, size_, index));
		}

		void writeMaskedSimdValueAt(SIMD value, simd_mask mask, u32 channel, u32 index)
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			data_.write(utils::maskLoad(data_.read(indices.first), value, mask), indices.first);
		}

		// writes a single value to the buffer
		void writeValueAt(T value, u32 channel, u32 index) noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = getAbsoluteIndices(channel, size_, index);
			auto data = data_.getData();
			auto scalars = data_.read(indices.first).template getArrayOfValues<T>();
			scalars[indices.second] = value;
			data_.write(SIMD(scalars), indices.first);
		}

		bool isEmpty() const noexcept { return channels_ == 0 || size_ == 0; }
		u32 getSize() const noexcept {	return size_; }
		u32 getChannels() const noexcept { return channels_; }
		u32 getSimdChannels() const noexcept { return getTotalSimdChannels(channels_); }

		MemoryBlock<SIMD> &getData() noexcept { return data_; }
		const MemoryBlock<SIMD> &getData() const noexcept { return data_; }
		auto &getLock() const noexcept { return dataLock_; }

		// can get raw pointer if the context already has a shared_ptr
		static SIMD* getDataPointer(std::shared_ptr<SIMD[]>& dataPtr, u32 channel, u32 index, u32 size) noexcept
		{
			auto indices = getAbsoluteIndices(channel, size, index);
			return &dataPtr[indices.first];
		}

		static constexpr u32 getRelativeSize() noexcept { return sizeof(SIMD) / sizeof(T); }

	private:
		MemoryBlock<SIMD> data_;
		u32 channels_ = 0;
		u32 size_ = 0;

		mutable utils::LockBlame<i32> dataLock_{ 0, {} };

		static u32 getTotalSimdChannels(u32 numChannels)
		{	return (u32)std::ceil((double)numChannels / (double)getRelativeSize()); }

		// internal function for calculating SIMD and internal positions of an element
		// first - index to the **SIMD** element 
		// second - channel index of T value inside the SIMD element
		static constexpr std::pair<u32, u32> getAbsoluteIndices(u32 channel, u32 channelSize, u32 index) noexcept
		{ return { (channel / getRelativeSize()) * channelSize + index, channel % getRelativeSize() }; }

		static constexpr u32 getSimdIndex(u32 channel, u32 channelSize, u32 index) noexcept
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
		SimdBufferView(const SimdBuffer<T, SIMD> &buffer, u32 beginChannel = 0, u32 channels = 0) noexcept
		{
			COMPLEX_ASSERT(beginChannel + channels <= buffer.getChannels());

			dataView_ = buffer.data_;
			beginChannel_ = beginChannel;
			channels_ = (channels) ? channels : buffer.getChannels() - beginChannel;
			size_ = buffer.getSize();
			dataLock_ = &buffer.dataLock_;
		}

		SIMD readSimdValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			return dataView_.read(SimdBuffer<T, SIMD>::getSimdIndex(channel, size_, index));
		}

		T readValueAt(u32 channel, u32 index) const noexcept
		{
			COMPLEX_ASSERT(channel < getChannels());
			COMPLEX_ASSERT(index < getSize());

			auto indices = SimdBuffer<T, SIMD>::getAbsoluteIndices(channel, size_, index);
			auto scalars = dataView_.read(indices.first).template getArrayOfValues<T>();

			return scalars[indices.second];
		}

		auto &getLock() const noexcept { return *dataLock_; }

		bool isEmpty() const noexcept { return channels_ == 0 || size_ == 0; }

		u32 getSize() const noexcept { return size_; }
		u32 getChannels() const noexcept { return channels_; }
		u32 getSimdChannels() const noexcept { return SimdBuffer<T, SIMD>::getTotalSimdChannels(channels_); }

		static constexpr u32 getRelativeSize() noexcept { return SimdBuffer<T, SIMD>::getRelativeSize(); }

		MemoryBlockView<SIMD> getData() const noexcept { return dataView_; }

	private:
		MemoryBlockView<SIMD> dataView_{};
		u32 beginChannel_ = 0;
		u32 channels_ = 0;
		u32 size_ = 0;

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
	void SimdBuffer<T, SIMD>::applyToThis(SimdBuffer &thisBuffer, const SimdBufferView<T, SIMD> &otherBuffer, u32 numChannels,
		u32 numSamples, utils::MathOperations operation, simd_mask mergeMask, u32 thisStartChannel, u32 otherStartChannel, 
		u32 thisStartIndex, u32 otherStartIndex) noexcept
	{
		COMPLEX_ASSERT(thisBuffer.getChannels() >= thisStartChannel + numChannels);
		COMPLEX_ASSERT(otherBuffer.getChannels() >= otherStartChannel + numChannels);
		COMPLEX_ASSERT(thisBuffer.getSize() >= thisStartIndex + numSamples);
		COMPLEX_ASSERT(otherBuffer.getSize() >= otherStartIndex + numSamples);

		// defining the math operations
		SIMD(*function)(SIMD, SIMD, simd_mask);
		switch (operation)
		{
		case utils::MathOperations::Add:

			function = [](SIMD one, SIMD two, simd_mask mask) { return utils::maskLoad(one + two, one, mask); };
			break;

		case utils::MathOperations::Multiply:

			function = [](SIMD one, SIMD two, simd_mask mask) { return utils::maskLoad(one * two, one, mask); };
			break;

		default:
		case utils::MathOperations::Assign:

			function = [](SIMD one, SIMD two, simd_mask mask) { return utils::maskLoad(two, one, mask); };
			break;
		}

		// getting data and setting up variables
		auto thisDataBlock = thisBuffer.getData();
		auto otherDataBlock = otherBuffer.getData();

		auto thisBufferSize = thisBuffer.getSize();
		auto otherBufferSize = otherBuffer.getSize();
		u32 simdNumChannels = getTotalSimdChannels(numChannels);

		for (u32 i = 0; i < simdNumChannels; i++)
		{
			// getting indices to the beginning of the SIMD channel buffer blocks
			auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisBufferSize, thisStartIndex);
			auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherBufferSize, otherStartIndex);

			for (u32 k = 0; k < numSamples; k++)
			{
				thisDataBlock[thisChannelIndices.first + k] = function(thisDataBlock[thisChannelIndices.first + k],
					otherDataBlock[otherChannelIndices.first + k], mergeMask);
			}
		}
	}

	template<typename T, SimdValue SIMD>
	template<utils::MathOperations Operation>
	void SimdBuffer<T, SIMD>::applyToThisNoMask(SimdBuffer &thisBuffer, SimdBufferView<T, SIMD> &otherBuffer,
		u32 numChannels, u32 numSamples, u32 thisStartChannel, u32 otherStartChannel, 
		u32 thisStartIndex, u32 otherStartIndex, SIMD scaleFactor) noexcept
	{
		COMPLEX_ASSERT(thisBuffer.getChannels() >= thisStartChannel + numChannels);
		COMPLEX_ASSERT(otherBuffer.getChannels() >= otherStartChannel + numChannels);
		COMPLEX_ASSERT(thisBuffer.getSize() >= thisStartIndex + numSamples);
		COMPLEX_ASSERT(otherBuffer.getSize() >= otherStartIndex + numSamples);

		// getting data and setting up variables
		auto thisData = thisBuffer.getData().getData().get();
		auto otherData = otherBuffer.getData().getData().get();

		auto thisSize = thisBuffer.getSize();
		auto otherSize = otherBuffer.getSize();
		u32 simdNumChannels = getTotalSimdChannels(numChannels);

		if constexpr (Operation == utils::MathOperations::Add)
		{
			for (u32 i = 0; i < simdNumChannels; i++)
			{
				// getting indices to the beginning of the SIMD channel buffer blocks
				auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
				auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

				for (u32 k = 0; k < numSamples; k++)
					thisData[thisChannelIndices.first + k] += otherData[otherChannelIndices.first + k] * scaleFactor;
			}
		}
		else if constexpr (Operation == utils::MathOperations::Multiply)
		{
			for (u32 i = 0; i < simdNumChannels; i++)
			{
				// getting indices to the beginning of the SIMD channel buffer blocks
				auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
				auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

				for (u32 k = 0; k < numSamples; k++)
					thisData[thisChannelIndices.first + k] *= otherData[otherChannelIndices.first + k] * scaleFactor;
			}
		}
		else
		{
			for (u32 i = 0; i < simdNumChannels; i++)
			{
				// getting indices to the beginning of the SIMD channel buffer blocks
				auto thisChannelIndices = getAbsoluteIndices(thisStartChannel + i * getRelativeSize(), thisSize, thisStartIndex);
				auto otherChannelIndices = getAbsoluteIndices(otherStartChannel + i * getRelativeSize(), otherSize, otherStartIndex);

				COMPLEX_ASSERT(thisBuffer.getData().getData() != otherBuffer.getData().getData() && 
					"We're copying from a buffer to itself consider using something else if this is intentional");

				std::memcpy(&thisData[thisChannelIndices.first], &otherData[otherChannelIndices.first], numSamples * sizeof(SIMD));
			}
		}
	}

	template<typename T>
	struct complex { T values[2]{}; };

	struct ComplexDataSource
	{
		enum DataSourceType : u8 { Cartesian, Polar, Both };

		// scratch buffer for conversion between cartesian and polar data
		SimdBuffer<complex<float>, simd_float> conversionBuffer{ kNumChannels, kMaxFFTBufferLength };
		// is the data in sourceBuffer polar or cartesian
		DataSourceType dataType = Cartesian;
		SimdBufferView<complex<float>, simd_float> sourceBuffer;
	};
}
