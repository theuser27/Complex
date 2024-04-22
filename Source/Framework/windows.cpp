/*
	==============================================================================

		windows.cpp
		Created: 5 Sep 2023 4:07:49am
		Author:  theuser27

	==============================================================================
*/

#include "windows.h"

#include <Third Party/gcem/gcem.hpp>
#include "lookup.h"
#include "circular_buffer.h"

namespace Framework
{
	namespace
	{
		// all functions take a normalised value as position

		// static windows

		constexpr strict_inline float createHannWindow(float position)
		{
			return 0.5f * (1.0f - gcem::cos(k2Pi * position));
		}

		// an accurate version of the traditional hamming window
		constexpr strict_inline float createHammingWindow(float position)
		{
			return (25.0f / 46.0f) + ((-21.0f / 46.0f) * gcem::cos(k2Pi * position));
		}

		constexpr strict_inline float createTriangleWindow(float position)
		{
			return 1 - 2 * gcem::abs(position - 0.5f);
		}

		constexpr strict_inline float createSineWindow(float position)
		{
			return gcem::sin(k2Pi * position);
		}

		// dynamic windows

		constexpr strict_inline float createExponentialWindow(float position)
		{
			return gcem::exp((-k2Pi) * gcem::abs(position - 0.5f));
		}

		constexpr strict_inline float createLanczosWindow(float position)
		{
			float adjustedPosition = position - 0.5f;
			return adjustedPosition == 0.0f ? 1.0f :
				gcem::sin(k2Pi * adjustedPosition) / k2Pi * adjustedPosition;
		}
	}

	static constexpr auto hannWindowLookup = Lookup<kWindowResolution>(createHannWindow);
	static constexpr auto hammingWindowLookup = Lookup<kWindowResolution>(createHammingWindow);
	static constexpr auto triangleWindowLookup = Lookup<kWindowResolution>(createTriangleWindow);
	static constexpr auto sineWindowLookup = Lookup<kWindowResolution>(createSineWindow);

	static constexpr auto exponentialWindowLookup = Lookup<kWindowResolution>(createExponentialWindow);
	static constexpr auto lanczosWindowLookup = Lookup<kWindowResolution>(createLanczosWindow);

	float Window::getHannWindow(float position) noexcept {	return hannWindowLookup.linearLookup(position); }
	float Window::getHammingWindow(float position) noexcept { return hammingWindowLookup.linearLookup(position); }
	float Window::getTriangleWindow(float position) noexcept { return triangleWindowLookup.linearLookup(position); }
	float Window::getSineWindow(float position) noexcept { return sineWindowLookup.linearLookup(position); }

	float Window::getExponentialWindow(float position, float alpha) noexcept 
	{ return utils::pow((exponentialWindowLookup.linearLookup(position)), alpha); }

	float Window::getHannExponentialWindow(float position, float alpha) noexcept
	{
		return (utils::pow((exponentialWindowLookup.linearLookup(position)), alpha)
			* hannWindowLookup.linearLookup(position));
	}

	float Window::getLanczosWindow(float position, float alpha) noexcept 
	{ return utils::pow((lanczosWindowLookup.linearLookup(position)), alpha); }

	void Window::applyWindow(Buffer &buffer, size_t channels, const bool *channelsToProcess, size_t samples, WindowTypes type, float alpha)
	{
		applyDefaultWindows(buffer, channels, channelsToProcess, samples, type, alpha);
	}

	void Window::applyDefaultWindows(Buffer &buffer, size_t channels, 
		const bool *channelsToCopy, size_t samples, WindowTypes type, float alpha) noexcept
	{
		if (type == WindowTypes::Lerp || type == WindowTypes::Rectangle)
			return;

		float increment = 1.0f / (float)samples;

		// the windowing is periodic, therefore if we start one sample forward,
		// omit the centre sample and we scale both explicitly
		// we can take advantage of window symmetry and do 2 multiplications with 1 lookup
		size_t halfLength = (samples - 2) / 2;

		float window = 1.0f;
		float position = 0.0f;

		auto data = buffer.getData().getData().get();
		auto size = buffer.getSize();

		// applying window to first sample and middle
		{
			float centerWindow = 1.0f;
			size_t centerSample = samples / 2;
			switch (type)
			{
			case WindowTypes::Hann:
				window = getHannWindow(position);
				centerWindow = getHannWindow(0.5f);
				break;
			case WindowTypes::Hamming:
				window = getHammingWindow(position);
				centerWindow = getHammingWindow(0.5f);
				break;
			case WindowTypes::Triangle:
				window = getTriangleWindow(position);
				centerWindow = getTriangleWindow(0.5f);
				break;
			case WindowTypes::Sine:
				window = getSineWindow(position);
				centerWindow = getSineWindow(0.5f);
				break;
			case WindowTypes::Exp:
				window = getExponentialWindow(position, alpha);
				centerWindow = getExponentialWindow(0.5f, alpha);
				break;
			case WindowTypes::HannExp:
				window = getHannExponentialWindow(position, alpha);
				centerWindow = getHannExponentialWindow(0.5f, alpha);
				break;
			case WindowTypes::Lanczos:
				window = getLanczosWindow(position, alpha);
				centerWindow = getLanczosWindow(0.5f, alpha);
				break;
			default:
				break;
			}
			for (size_t j = 0; j < channels; j++)
			{
				if (!channelsToCopy[j])
					continue;

				data[j * size] *= window;
				data[j * size + centerSample] *= centerWindow;
			}
			position += increment;
		}

		for (size_t i = 1; i < halfLength; i++)
		{
			switch (type)
			{
			case WindowTypes::Hann:
				window = getHannWindow(position);
				break;
			case WindowTypes::Hamming:
				window = getHammingWindow(position);
				break;
			case WindowTypes::Triangle:
				window = getTriangleWindow(position);
				break;
			case WindowTypes::Sine:
				window = getSineWindow(position);
				break;
			case WindowTypes::Exp:
				window = getExponentialWindow(position, alpha);
				break;
			case WindowTypes::HannExp:
				window = getHannExponentialWindow(position, alpha);
				break;
			case WindowTypes::Lanczos:
				window = getLanczosWindow(position, alpha);
				break;
			default:
				break;
			}

			for (size_t j = 0; j < channels; j++)
			{
				if (!channelsToCopy[j])
					continue;

				data[j * size + i] *= window;
				data[j * size + samples - i] *= window;
			}
			position += increment;
		}
	}

	void Window::applyCustomWindows(Buffer &buffer, size_t channels, const bool *channelsToCopy, size_t samples, WindowTypes type, float alpha)
	{
		// TODO: see into how to generate custom windows based on spectral properties
		// redirecting to the default types for now
		applyDefaultWindows(buffer, channels, channelsToCopy, samples, type, alpha);
	}

}
