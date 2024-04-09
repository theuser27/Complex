/*
	==============================================================================

		sync_primitives.h
		Created: 10 Mar 2024 2:06:32am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <atomic>
#include "Third Party/clog/small_function.hpp"
#include "platform_definitions.h"

namespace utils
{
	strict_inline void pause() noexcept
	{
	#if COMPLEX_SSE4_1
		_mm_pause();
	#elif COMPLEX_NEON
		__yield();
	#endif
	}

	strict_inline void longPause(size_t iterations) noexcept
	{
		for (size_t i = 0; i < iterations; i++)
		{
		#if COMPLEX_SSE4_1
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
		#elif COMPLEX_NEON
			__yield();
			__yield();
			__yield();
			__yield();
			__yield();
		#endif
		}
	}

	void loadLibraries();
	void unloadLibraries();

	void millisleep() noexcept;

	enum class WaitMechanism : u32 { Spin = 0, Wait, Sleep, SpinNotify = 4, WaitNotify, SleepNotify };

	void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
	void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
	i32 lockAtomic(std::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism, 
		const clg::small_function<void()> &lambda = [](){}) noexcept;
	void unlockAtomic(std::atomic<i32> &atomic, bool wasExclusive, WaitMechanism mechanism);

	class ScopedLock
	{
	public:
		ScopedLock(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
			boolAtomic_(&atomic), expectedOrExclusive_(expected), mechanism_(mechanism)
		{ lockAtomic(atomic, mechanism, expected); }

		ScopedLock(std::atomic<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
			const clg::small_function<void()> &lambda = [](){}) noexcept : i32Atomic_(&atomic),
			expectedOrExclusive_(isExclusive), mechanism_(mechanism)
		{ lockAtomic(atomic, isExclusive, mechanism, lambda); }

		ScopedLock(ScopedLock &&other) noexcept = default;
		ScopedLock &operator=(ScopedLock &&other) noexcept = default;
		ScopedLock(const ScopedLock &) = delete;
		ScopedLock operator=(const ScopedLock &) = delete;

		~ScopedLock() noexcept
		{
			if (i32Atomic_)
				unlockAtomic(*i32Atomic_, expectedOrExclusive_, mechanism_);
			else
				unlockAtomic(*boolAtomic_, mechanism_, expectedOrExclusive_);
		}

	private:
		std::atomic<bool> *boolAtomic_ = nullptr;
		std::atomic<i32> *i32Atomic_ = nullptr;
		bool expectedOrExclusive_;
		WaitMechanism mechanism_;
	};
}
