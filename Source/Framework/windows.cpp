/*
  ==============================================================================

    windows.cpp
    Created: 5 Sep 2023 04:07:49
    Author:  theuser27

  ==============================================================================
*/

#include "windows.hpp"

#include "Third Party/gcem/gcem.hpp"

#include "simd_utils.hpp"
#include "circular_buffer.hpp"

namespace Framework
{
  // all functions take a normalised value as position

  // static windows

  static constexpr float createHannWindow(float position)
  {
    return 0.5f * (1.0f - gcem::cos(k2Pi * position));
  }

  // an accurate version of the traditional hamming window
  static constexpr float createHammingWindow(float position)
  {
    return (25.0f / 46.0f) + ((-21.0f / 46.0f) * gcem::cos(k2Pi * position));
  }

  static constexpr float createTriangleWindow(float position)
  {
    return 1 - 2 * utils::abs(position - 0.5f);
  }

  static constexpr float createSineWindow(float position)
  {
    return gcem::sin(k2Pi * position);
  }

  // dynamic windows

  static constexpr float createExponentialWindow(float position)
  {
    return gcem::exp((-k2Pi) * utils::abs(position - 0.5f));
  }

  static constexpr float createLanczosWindow(float position)
  {
    float adjustedPosition = position - 0.5f;
    return adjustedPosition == 0.0f ? 1.0f :
      gcem::sin(k2Pi * adjustedPosition) / (k2Pi * adjustedPosition);
  }
  

  static constexpr auto hannWindowLookup = utils::Lookup<kWindowResolution>(createHannWindow);
  static constexpr auto hammingWindowLookup = utils::Lookup<kWindowResolution>(createHammingWindow);
  static constexpr auto triangleWindowLookup = utils::Lookup<kWindowResolution>(createTriangleWindow);
  static constexpr auto sineWindowLookup = utils::Lookup<kWindowResolution>(createSineWindow);

  static constexpr auto exponentialWindowLookup = utils::Lookup<kWindowResolution>(createExponentialWindow);
  static constexpr auto lanczosWindowLookup = utils::Lookup<kWindowResolution>(createLanczosWindow);

  float getHannWindow(float position) noexcept { return hannWindowLookup.linearLookup(position); }
  float getHammingWindow(float position) noexcept { return hammingWindowLookup.linearLookup(position); }
  float getTriangleWindow(float position) noexcept { return triangleWindowLookup.linearLookup(position); }
  float getSineWindow(float position) noexcept { return sineWindowLookup.linearLookup(position); }

  float getExponentialWindow(float position, float alpha) noexcept 
  { return utils::pow((exponentialWindowLookup.linearLookup(position)), alpha); }

  float getHannExponentialWindow(float position, float alpha) noexcept
  {
    return (utils::pow((exponentialWindowLookup.linearLookup(position)), alpha)
      * hannWindowLookup.linearLookup(position));
  }

  float getLanczosWindow(float position, float alpha) noexcept 
  { return utils::pow(utils::clamp(lanczosWindowLookup.linearLookup(position), 0.0f, 1.0f), alpha); }

  static void applyDefaultWindows(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
    u32 samples, uuid windowType, float alpha) noexcept
  {
    if (windowType == Window::Lerp || windowType == Window::Rectangle)
      return;

    float increment = 1.0f / (float)samples;

    // the windowing is periodic, therefore if we start one sample forward,
    // omit the centre sample and we scale both explicitly
    // we can take advantage of window symmetry and do 2 multiplications with 1 lookup
    u32 halfLength = (samples - 2) / 2;

    float window = 1.0f;
    float position = 0.0f;

    // applying window to first sample and middle
    {
      float centerWindow = 1.0f;
      u32 centerSample = samples / 2;
      switch (windowType)
      {
      case Window::Hann:
        window = getHannWindow(position);
        centerWindow = getHannWindow(0.5f);
        break;
      case Window::Hamming:
        window = getHammingWindow(position);
        centerWindow = getHammingWindow(0.5f);
        break;
      case Window::Triangle:
        window = getTriangleWindow(position);
        centerWindow = getTriangleWindow(0.5f);
        break;
      case Window::Sine:
        window = getSineWindow(position);
        centerWindow = getSineWindow(0.5f);
        break;
      case Window::Exponential:
        window = getExponentialWindow(position, alpha);
        centerWindow = getExponentialWindow(0.5f, alpha);
        break;
      case Window::HannExp:
        window = getHannExponentialWindow(position, alpha);
        centerWindow = getHannExponentialWindow(0.5f, alpha);
        break;
      case Window::Lanczos:
        window = getLanczosWindow(position, alpha);
        centerWindow = getLanczosWindow(0.5f, alpha);
        break;
      case Window::Rectangle:
      case Window::Lerp:
      default:
        break;
      }
      for (u32 j = 0; j < channels; j++)
      {
        if (!channelsToProcess[j])
          continue;

        auto data = buffer.get(j);
        data[0] *= window;
        data[centerSample] *= centerWindow;
      }
      position += increment;
    }

    for (u32 i = 1; i < halfLength; i++)
    {
      switch (windowType)
      {
      case Window::Hann:
        window = getHannWindow(position);
        break;
      case Window::Hamming:
        window = getHammingWindow(position);
        break;
      case Window::Triangle:
        window = getTriangleWindow(position);
        break;
      case Window::Sine:
        window = getSineWindow(position);
        break;
      case Window::Exponential:
        window = getExponentialWindow(position, alpha);
        break;
      case Window::HannExp:
        window = getHannExponentialWindow(position, alpha);
        break;
      case Window::Lanczos:
        window = getLanczosWindow(position, alpha);
        break;
      case Window::Rectangle:
      case Window::Lerp:
      default:
        break;
      }

      for (u32 j = 0; j < channels; j++)
      {
        if (!channelsToProcess[j])
          continue;

        auto data = buffer.get(j);
        data[i] *= window;
        data[samples - i] *= window;
      }
      position += increment;
    }
  }

  void Window::applyWindow(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
    u32 samples, uuid windowType, float alpha)
  {
    applyDefaultWindows(buffer, channels, channelsToProcess, samples, windowType, alpha);
  }

  void Window::addOverlap(CircularBuffer &destination, Buffer &source, u32 channels, 
    utils::span<bool> channelsToProcess, u32 samples, u32 destinationBegin, uuid windowType)
  {
    auto bufferSize = destination.size;
    if (windowType == Lerp)
    {
      Framework::applyToBuffer(
        [](float &destination, const float &source, float t) { destination = (1.0f - t) * destination + t * source; },
        destination, source, channels, samples, destinationBegin, 0, channelsToProcess);
    }
    else
    {
      // fading edges and overlapping
      u32 fadeSamples = samples / 4;

      // fade in overlap
      Framework::applyToBuffer(
        [](float &destination, const float &source, float t) { destination = (1.0f - t) * destination + t * (destination + source); },
        destination, source, channels, fadeSamples, destinationBegin, 0, channelsToProcess);

      // overlap
      Framework::applyToBuffer(
        [](float &destination, const float &source, float) { destination += source; },
        destination, source, channels, samples - 2 * fadeSamples,
        (destinationBegin + fadeSamples) % bufferSize, fadeSamples, channelsToProcess);

      // fade out overlap
      Framework::applyToBuffer(
        [](float &destination, const float &source, float t) { destination = (1.0f - t) * (destination + source) + t * source; },
        destination, source, channels, fadeSamples, (destinationBegin + samples - fadeSamples) % bufferSize,
        samples - fadeSamples, channelsToProcess);
    }
  }

  void Window::scaleDown(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
    u32 start, u32 samples, uuid windowType, float overlap, float alpha)
  {
    // TODO: use an extra overlap_ variable to store the overlap param 
    // from previous scaleDown run in order to apply extra attenuation 
    // when moving the overlap control (essentially becomes linear interpolation)

    float mult;
    switch (windowType)
    {
    case Lerp: return;
    case Rectangle:
      mult = 1.0f - overlap;
      break;
    case Hann:
    case Triangle:
      if (overlap <= 0.5f)
        return;

      mult = (1.0f - overlap) * 2.0f;
      break;
    case Hamming:
      if (overlap <= 0.5f)
        return;

      // https://www.desmos.com/calculator/z21xz7r2c9
      mult = (1.0f - overlap) * 1.84f;
      break;
    case Sine:
      if (overlap <= 0.33333333f)
        return;

      // https://www.desmos.com/calculator/mmjwlj0gqe
      mult = (1.0f - overlap) * 1.57f;
      break;
    case Exponential:
      if (overlap <= 0.1235f)
        return;

      // not optimal but it works somewhat ok
      // https://www.desmos.com/calculator/ozcckbnyvl
      mult = (1.0f - overlap) * 3.25f * sqrtf(alpha * overlap);
      break;
    case HannExp:
    case Lanczos:
      if (overlap <= 0.1235f)
        return;

      // TODO: add optimal scaling for these
      mult = (1.0f - overlap) * 3.25f * sqrtf(alpha * overlap);
      break;
    default:
      COMPLEX_ASSERT_FALSE("Missing case");
      return;
    }

    auto bufferSize = buffer.size;
    for (u32 i = 0; i < channels; i++)
    {
      if (!channelsToProcess[i])
        continue;

      auto channel = buffer.get(i);

      // TODO: optimise this with aligned simd multiply
      for (u32 j = 0; j < samples; j++)
      {
        u32 sampleIndex = (start + j) % bufferSize;
        channel[sampleIndex] *= mult;
      }
    }
  }

}
