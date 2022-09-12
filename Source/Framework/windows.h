/*
	==============================================================================

		windows.h
		Created: 5 Aug 2021 3:39:49am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "lookup.h"
#include "./Third Party/gcem/include/gcem.hpp"

namespace Framework
{
	namespace
	{
		// all functions are in range from 0.0f to 1.0f

		// static windows

		static constexpr strict_inline float createHannWindow(float position)
		{	return 0.5f * (1.0f - gcem::cos(common::k2Pi * position)); }

		// simplified version of the traditional hamming window
		static constexpr strict_inline float createHammingWindow(float position)
		{	return 0.46f * (25.0f - 21.0f * gcem::cos(common::k2Pi * position)); }

		static constexpr strict_inline float createTriangleWindow(float position)
		{	return 1 - 2 * gcem::abs(position - 0.5f); }

		static constexpr strict_inline float createSineWindow(float position)
		{	return gcem::sin(common::k2Pi * position); }

		// dynamic windows

		static constexpr strict_inline float createExponentialWindow(float position)
		{	return gcem::exp((-common::k2Pi) * gcem::abs(position - 0.5f)); }

		static constexpr strict_inline float createLanczosWindow(float position)
		{
			float adjustedPosition = position - 0.5f;
			return adjustedPosition == 0.0f ? 1.0f :
				gcem::sin(common::k2Pi * adjustedPosition) / common::k2Pi * adjustedPosition;
		}
	}

	class Window
	{
	public:
		Window(const Window &) = delete;
		Window& operator=(const Window &) = delete;
		Window(Window &&) = delete;
		Window &operator=(const Window &&) = delete;

		static Window *getInstance() { static Window instance; return &instance; }

	private:
		Window() = default;
		~Window() = default;

		static constexpr auto hannWindowLookup = Lookup<common::kWindowResolution>(Framework::createHannWindow);
		static constexpr auto hammingWindowLookup = Lookup<common::kWindowResolution>(Framework::createHammingWindow);
		static constexpr auto triangleWindowLookup = Lookup<common::kWindowResolution>(Framework::createTriangleWindow);
		static constexpr auto sineWindowLookup = Lookup<common::kWindowResolution>(Framework::createSineWindow);

		static constexpr auto exponentialWindowLookup = Lookup<common::kWindowResolution>(Framework::createExponentialWindow);
		static constexpr auto lanczosWindowLookup = Lookup<common::kWindowResolution>(Framework::createLanczosWindow);

	public:

		static perf_inline float getHannWindow(float position)
		{	return hannWindowLookup.linearLookup(position); }

		static perf_inline float getHammingWindow(float position)
		{	return hammingWindowLookup.linearLookup(position); }

		static perf_inline float getTriangleWindow(float position)
		{	return triangleWindowLookup.linearLookup(position); }

		static perf_inline float getSineWindow(float position)
		{	return sineWindowLookup.linearLookup(position); }


		static perf_inline float getExponentialWindow(float position, float alpha)
		{	return utils::pow((exponentialWindowLookup.linearLookup(position)), alpha); }

		static perf_inline float getHannExponentialWindow(float position, float alpha)
		{
			return (utils::pow((exponentialWindowLookup.linearLookup(position)), alpha)
				* hannWindowLookup.linearLookup(position));
		}

		static perf_inline float getLanczosWindow(float position, float alpha)
		{	return utils::pow((lanczosWindowLookup.linearLookup(position)), alpha); }

		perf_inline void applyWindow(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToProcess, u32 numSamples, WindowTypes type, float alpha)
		{
			if (type == WindowTypes::Custom)
				applyCustomWindows(buffer, numChannels, channelsToProcess, numSamples, type, alpha);
			else
				applyDefaultWindows(buffer, numChannels, channelsToProcess, numSamples, type, alpha);
		}

		perf_inline void applyDefaultWindows(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToCopy, u32 numSamples, WindowTypes type, float alpha)
		{
			if (type == WindowTypes::Rectangle)
				return;

			float increment = 1.0f / numSamples;

			// the windowing is periodic, therefore if we start one sample forward,
			// omit the centre sample and we scale both explicitly
			// we can take advantage of window symmetry and do 2 assignments with 1 lookup
			u32 halfLength = (numSamples - 2) / 2;

			float window = 1.0f;
			float position = 0.0f;

			// applying window to first sample and middle
			{
				float centerWindow = 1.0f;
				u32 centerSample = numSamples / 2;
				switch (type)
				{
				case Framework::WindowTypes::Hann:
					window = getHannWindow(position);
					centerWindow = getHannWindow(0.5f);
					break;
				case Framework::WindowTypes::Hamming:
					window = getHammingWindow(position);
					centerWindow = getHammingWindow(0.5f);
					break;
				case Framework::WindowTypes::Triangle:
					window = getTriangleWindow(position);
					centerWindow = getTriangleWindow(0.5f);
					break;
				case Framework::WindowTypes::Sine:
					window = getSineWindow(position);
					centerWindow = getSineWindow(0.5f);
					break;
				case Framework::WindowTypes::Exponential:
					window = getExponentialWindow(position, alpha);
					centerWindow = getExponentialWindow(0.5f, alpha);
					break;
				case Framework::WindowTypes::HannExponential:
					window = getHannExponentialWindow(position, alpha);
					centerWindow = getHannExponentialWindow(0.5f, alpha);
					break;
				case Framework::WindowTypes::Lanczos:
					window = getLanczosWindow(position, alpha);
					centerWindow = getLanczosWindow(0.5f, alpha);
					break;
				case Framework::WindowTypes::Blackman:
					break;
				case Framework::WindowTypes::BlackmanHarris:
					break;
				default:
					break;
				}
				for (u32 j = 0; j < numChannels; j++)
				{
					if (!channelsToCopy[j])
						continue;

					*buffer.getWritePointer(j, 0) *= window;
					*buffer.getWritePointer(j, centerSample) *= centerWindow;
				}
				position += increment;
			}

			for (u32 i = 1; i < halfLength; i++)
			{
				switch (type)
				{
				case Framework::WindowTypes::Hann:
					window = getHannWindow(position);
					break;
				case Framework::WindowTypes::Hamming:
					window = getHammingWindow(position);
					break;
				case Framework::WindowTypes::Triangle:
					window = getTriangleWindow(position);
					break;
				case Framework::WindowTypes::Sine:
					window = getSineWindow(position);
					break;
				case Framework::WindowTypes::Exponential:
					window = getExponentialWindow(position, alpha);
					break;
				case Framework::WindowTypes::HannExponential:
					window = getHannExponentialWindow(position, alpha);
					break;
				case Framework::WindowTypes::Lanczos:
					window = getLanczosWindow(position, alpha);
					break;
				case Framework::WindowTypes::Blackman:
					break;
				case Framework::WindowTypes::BlackmanHarris:
					break;
				default:
					break;
				}

				for (u32 j = 0; j < numChannels; j++)
				{
					if (!channelsToCopy[j])
						continue;

					*buffer.getWritePointer(j, i) *= window;
					*buffer.getWritePointer(j, numSamples - i) *= window;
				}
				position += increment;
			}
		}

		perf_inline void applyCustomWindows(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToCopy, u32 numSamples, WindowTypes type, float alpha)
		{
			// TODO: see into how to generate custom windows based on spectral properties
			// redirecting to the default types for now
			applyDefaultWindows(buffer, numChannels, channelsToCopy, numSamples, type, alpha);
		}
	};
}