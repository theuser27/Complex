/*
	==============================================================================

		sync_primitives.cpp
		Created: 10 Mar 2024 2:06:32am
		Author:  theuser27

	==============================================================================
*/

#include <thread>
#include "sync_primitives.h"

#ifdef COMPLEX_WINDOWS
	#include <windows.h>

extern "C"
{
	typedef NTSTATUS __stdcall NtDelayExecution_t(unsigned char Alertable, PLARGE_INTEGER Interval);
}

namespace
{
	HMODULE winmmLibrary = nullptr;
	HMODULE ntdllLibrary = nullptr;

	decltype(&timeBeginPeriod) timeBeginPeriodFunction = nullptr;
	decltype(&timeEndPeriod) timeEndPeriodFunction = nullptr;
	NtDelayExecution_t *ntDelayExecutionFunction = nullptr;
}
#endif

namespace utils
{
	void loadLibraries()
	{
	#ifdef COMPLEX_WINDOWS
		winmmLibrary = LoadLibraryA("Winmm.dll");
		if (!winmmLibrary)
			throw std::exception("Couldn't open Winmm.dll");

		ntdllLibrary = LoadLibraryA("ntdll.dll");
		if (!ntdllLibrary)
		{
			FreeLibrary(winmmLibrary);
			throw std::exception("Couldn't open ntdll.dll");
		}

		timeBeginPeriodFunction = reinterpret_cast<decltype(timeBeginPeriodFunction)>(GetProcAddress(winmmLibrary, "timeBeginPeriod"));
		timeEndPeriodFunction = reinterpret_cast<decltype(timeEndPeriodFunction)>(GetProcAddress(winmmLibrary, "timeEndPeriod"));
		ntDelayExecutionFunction = reinterpret_cast<decltype(ntDelayExecutionFunction)>(GetProcAddress(ntdllLibrary, "NtDelayExecution"));
	#endif
	}

	void unloadLibraries()
	{
	#ifdef COMPLEX_WINDOWS
		FreeLibrary(ntdllLibrary);
		FreeLibrary(winmmLibrary);
	#endif
	}

	void millisleep() noexcept
	{
	#ifdef COMPLEX_WINDOWS
		// taking advantage of undocumented ntdll function in combination with timeBegin/timeEndPeriod to reach ~1ms sleep
		// https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FThread%2FNtDelayExecution.html
		// after some experimentation i discovered that 600us most consistently yielded a real delay of ~1ms **on my machine**
		LONGLONG delay = 600 * -10;
		timeBeginPeriodFunction(1);
		ntDelayExecutionFunction(0, (PLARGE_INTEGER)&delay);
		timeEndPeriodFunction(1);
	#elif
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	#endif
	}

	void lockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept
	{
		bool state = expected;
		while (!atomic.compare_exchange_weak(state, !expected, std::memory_order_acq_rel, std::memory_order_relaxed))
		{
			// protecting against spurious failures
			if (state == expected)
				continue;

			switch (mechanism)
			{
			case WaitMechanism::Spin:
			case WaitMechanism::SpinNotify:
				// taking advantage of the MESI protocol where we only want shared access to read until the value is changed,
				// instead of read/write with exclusive access thereby slowing down other threads trying to read as well
				while (atomic.load(std::memory_order_relaxed) != expected) { pause(); }
				break;
			case WaitMechanism::Wait:
			case WaitMechanism::WaitNotify:
				atomic.wait(state, std::memory_order_relaxed);
				break;
			case WaitMechanism::Sleep:
			case WaitMechanism::SleepNotify:
				// sleeping should take ideally around a milisecond, may change later
				millisleep();
				break;
			default:
				COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
				break;
			}

			state = expected;
		}
	}

	i32 lockAtomic(LockBlame<i32> &lock, bool isExclusive, WaitMechanism mechanism, const clg::small_function<void()> &lambda) noexcept
	{
		i32 state, desired;
		if (isExclusive)
		{
			static constexpr i32 expected = 0;
			state = expected;
			desired = state - 1;
			while (!lock.lock.compare_exchange_weak(state, desired, std::memory_order_acq_rel, std::memory_order_relaxed))
			{
				// protecting against spurious failures
				if (state == expected)
					continue;

				if (state <= expected)
				{
					COMPLEX_ASSERT_FALSE(lock.lastLockId != std::this_thread::get_id() && 
						"Guess who forgot to unlock this atomic");
				}

				lambda();

				if (mechanism == WaitMechanism::Spin || mechanism == WaitMechanism::SpinNotify)
					while (lock.lock.load(std::memory_order_relaxed) != expected) { pause(); }
				else
				{
					do
					{
						if (mechanism == WaitMechanism::Sleep || mechanism == WaitMechanism::SleepNotify)
							millisleep();
						else
							lock.lock.wait(state, std::memory_order_relaxed);
						state = lock.lock.load(std::memory_order_relaxed);
					} while (state != expected);
				}
			}

			lock.lastLockId = std::this_thread::get_id();
		}
		else
		{
			state = lock.lock.load(std::memory_order_relaxed);
			bool isLambdaRun = false;

			while (true)
			{
				while (state < 0)
				{
					if (!isLambdaRun)
					{
						lambda();
						isLambdaRun = true;
					}

					switch (mechanism)
					{
					case WaitMechanism::Spin:
					case WaitMechanism::SpinNotify:
						while (lock.lock.load(std::memory_order_relaxed) < 0) { pause(); }
						break;
					case WaitMechanism::Wait:
					case WaitMechanism::WaitNotify:
						lock.lock.wait(state, std::memory_order_relaxed);
						break;
					case WaitMechanism::Sleep:
					case WaitMechanism::SleepNotify:
						while (lock.lock.load(std::memory_order_relaxed) < 0) { millisleep(); }
						break;
					default:
						COMPLEX_ASSERT_FALSE("Invalid wait mechanism");
						break;
					}

					state = lock.lock.load(std::memory_order_relaxed);
				}

				desired = state + 1;
				lock.lastLockId = {};
				if (lock.lock.compare_exchange_weak(state, desired, std::memory_order_acq_rel, std::memory_order_relaxed))
					break;
			}
		}

		return state;
	}

	void unlockAtomic(std::atomic<bool> &atomic, WaitMechanism mechanism, bool expected) noexcept
	{
		atomic.store(expected, std::memory_order_release);
		if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
			atomic.notify_all();
	}

	void unlockAtomic(LockBlame<i32> &atomic, bool wasExclusive, WaitMechanism mechanism)
	{
		if (wasExclusive)
		{
			[[maybe_unused]] auto value = atomic.lock.fetch_add(1, std::memory_order_release);
			COMPLEX_ASSERT(value == -1);
		}
		else
		{
			[[maybe_unused]] auto value = atomic.lock.fetch_sub(1, std::memory_order_release);
			COMPLEX_ASSERT(value > 0);
		}

		if ((u32)mechanism & (u32)WaitMechanism::SpinNotify)
			atomic.lock.notify_all();
	}
}
