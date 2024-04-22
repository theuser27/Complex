/*
	==============================================================================

		windows.h
		Created: 5 Aug 2021 3:39:49am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "nested_enum.h"
#include "platform_definitions.h"

namespace Framework
{
	class Buffer;

	NESTED_ENUM((WindowTypes, u32), (Lerp, Hann, Hamming, Triangle, Sine, Rectangle, Exp, HannExp, Lanczos))

	class Window
	{
	public:
		Window() = default;

		// all functions take a normalised value as position
		static float getHannWindow(float position) noexcept;
		static float getHammingWindow(float position) noexcept;
		static float getTriangleWindow(float position) noexcept;
		static float getSineWindow(float position) noexcept;

		static float getExponentialWindow(float position, float alpha) noexcept;
		static float getHannExponentialWindow(float position, float alpha) noexcept;
		static float getLanczosWindow(float position, float alpha) noexcept;

		void applyWindow(Framework::Buffer &buffer, size_t channels,
			const bool *channelsToProcess, size_t samples, WindowTypes type, float alpha);

		static void applyDefaultWindows(Framework::Buffer &buffer, size_t channels,
			const bool *channelsToCopy, size_t samples, WindowTypes type, float alpha) noexcept;

		void applyCustomWindows(Framework::Buffer &buffer, size_t channels,
			const bool *channelsToCopy, size_t samples, WindowTypes type, float alpha);
	};
}