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
	NESTED_ENUM((WindowTypes, u32), (, Lerp, Hann, Hamming, Triangle, Sine, Rectangle, Exp, HannExp, Lanczos))

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

		void applyWindow(float *const *buffers, u32 numChannels, 
			const bool* channelsToProcess, u32 numSamples, WindowTypes type, float alpha)
		{
			applyDefaultWindows(buffers, numChannels, channelsToProcess, numSamples, type, alpha);
		}

		static void applyDefaultWindows(float *const *buffers, u32 numChannels,
			const bool *channelsToCopy, u32 numSamples, WindowTypes type, float alpha) noexcept;

		void applyCustomWindows(float *const *buffers, u32 numChannels,
			const bool* channelsToCopy, u32 numSamples, WindowTypes type, float alpha)
		{
			// TODO: see into how to generate custom windows based on spectral properties
			// redirecting to the default types for now
			applyDefaultWindows(buffers, numChannels, channelsToCopy, numSamples, type, alpha);
		}
	};
}