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
#include <Third Party/gcem/gcem.hpp>

namespace Framework
{
	namespace
	{
		// all functions take a normalised value as position

		// static windows

		inline constexpr strict_inline float createHannWindow(float position)
		{	return 0.5f * (1.0f - gcem::cos(common::k2Pi * position)); }

		// an accurate version of the traditional hamming window
		inline constexpr strict_inline float createHammingWindow(float position)
		{	return (25.0f / 46.0f) + ((-21.0f / 46.0f) * gcem::cos(k2Pi * position)); }

		inline constexpr strict_inline float createTriangleWindow(float position)
		{	return 1 - 2 * gcem::abs(position - 0.5f); }

		inline constexpr strict_inline float createSineWindow(float position)
		{	return gcem::sin(common::k2Pi * position); }

		// dynamic windows

		inline constexpr strict_inline float createExponentialWindow(float position)
		{	return gcem::exp((-common::k2Pi) * gcem::abs(position - 0.5f)); }

		inline constexpr strict_inline float createLanczosWindow(float position)
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

		// all functions take a normalised value as position
		static float getHannWindow(float position)
		{	return hannWindowLookup.linearLookup(position); }

		static float getHammingWindow(float position)
		{	return hammingWindowLookup.linearLookup(position); }

		static float getTriangleWindow(float position)
		{	return triangleWindowLookup.linearLookup(position); }

		static float getSineWindow(float position)
		{	return sineWindowLookup.linearLookup(position); }


		static float getExponentialWindow(float position, float alpha)
		{	return utils::pow((exponentialWindowLookup.linearLookup(position)), alpha); }

		static float getHannExponentialWindow(float position, float alpha)
		{
			return (utils::pow((exponentialWindowLookup.linearLookup(position)), alpha)
				* hannWindowLookup.linearLookup(position));
		}

		static float getLanczosWindow(float position, float alpha)
		{	return utils::pow((lanczosWindowLookup.linearLookup(position)), alpha); }

		void applyWindow(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToProcess, u32 numSamples, WindowTypes type, float alpha)
		{
			applyDefaultWindows(buffer, numChannels, channelsToProcess, numSamples, type, alpha);
		}

		static void applyDefaultWindows(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToCopy, u32 numSamples, WindowTypes type, float alpha)
		{
			if (type == WindowTypes::Lerp || type == WindowTypes::Rectangle)
				return;

			float increment = 1.0f / (float)numSamples;

			// the windowing is periodic, therefore if we start one sample forward,
			// omit the centre sample and we scale both explicitly
			// we can take advantage of window symmetry and do 2 multiplications with 1 lookup
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

		void applyCustomWindows(AudioBuffer<float> &buffer, u32 numChannels, 
			const bool* channelsToCopy, u32 numSamples, WindowTypes type, float alpha)
		{
			// TODO: see into how to generate custom windows based on spectral properties
			// redirecting to the default types for now
			applyDefaultWindows(buffer, numChannels, channelsToCopy, numSamples, type, alpha);
		}
	};
}