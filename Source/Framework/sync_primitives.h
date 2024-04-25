/*
	==============================================================================

		sync_primitives.h
		Created: 10 Mar 2024 2:06:32am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <atomic>
#include <thread>
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

	// git blame for deadlocks
	template<typename T>
	struct LockBlame
	{
		std::atomic<T> lock{};
		std::thread::id lastLockId{};
	};

	void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
	void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept;
	i32 lockAtomic(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism,
		const clg::small_function<void()> &lambda = [](){}) noexcept;
	void unlockAtomic(LockBlame<i32> &atomic, bool wasExclusive, WaitMechanism mechanism);

	class ScopedLock
	{
	public:
		ScopedLock(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected = false) noexcept :
			boolAtomic_(&atomic), expectedOrExclusive_(expected), mechanism_(mechanism)
		{ lockAtomic(atomic, mechanism, expected); }

		ScopedLock(LockBlame<i32> &atomic, bool isExclusive, WaitMechanism mechanism,
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
			else if (boolAtomic_)
				unlockAtomic(*boolAtomic_, mechanism_, expectedOrExclusive_);

			i32Atomic_ = nullptr;
			boolAtomic_ = nullptr;
		}

	private:
		std::atomic<bool> *boolAtomic_ = nullptr;
		LockBlame<i32> *i32Atomic_ = nullptr;
		bool expectedOrExclusive_;
		WaitMechanism mechanism_;
	};

	template<typename T>
	class shared_value
	{
		struct atomic_holder
		{
			T load() const noexcept { return value.load(std::memory_order_relaxed); }
			void store(T newValue) noexcept { value.store(newValue, std::memory_order_relaxed); }

			std::atomic<T> value;
		};

		struct guard_holder
		{
			T load() const noexcept
			{
				utils::ScopedLock g{ guard, utils::WaitMechanism::Spin };
				return value;
			}
			void store(T newValue) noexcept
			{
				utils::ScopedLock g{ guard, utils::WaitMechanism::Spin };
				value = std::move(newValue);
			}

			mutable std::atomic<bool> guard = false;
			T value;
		};

		// if the type cannot fit into an atomic we can have an atomic_bool to guard it
		using holder = std::conditional_t<sizeof(T) <= sizeof(std::uintptr_t), atomic_holder, guard_holder>;  // NOLINT(bugprone-sizeof-expression)
	public:
		shared_value() = default;
		shared_value(const shared_value &other) noexcept { value.store(other.value.load()); }
		shared_value(T newValue) noexcept { value.store(std::move(newValue)); }
		shared_value &operator=(const shared_value &other) noexcept { return shared_value::operator=(other.value.load()); }
		shared_value &operator=(T newValue) noexcept { value.store(std::move(newValue)); return *this; }
		operator T() const noexcept { return get(); }

		[[nodiscard]] T get() const noexcept { return value.load(); }
	private:
		holder value{};
	};
}
