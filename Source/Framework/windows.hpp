/*
  ==============================================================================

    windows.hpp
    Created: 5 Aug 2021 3:39:49am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "parameters.hpp"

namespace Framework
{
  class Buffer;

  struct Window
  {
    // all functions take a normalised value as position
    static float getHannWindow(float position) noexcept;
    static float getHammingWindow(float position) noexcept;
    static float getTriangleWindow(float position) noexcept;
    static float getSineWindow(float position) noexcept;

    static float getExponentialWindow(float position, float alpha) noexcept;
    static float getHannExponentialWindow(float position, float alpha) noexcept;
    static float getLanczosWindow(float position, float alpha) noexcept;

    void applyWindow(Buffer &buffer, usize channels, utils::span<char> channelsToProcess,
      usize samples, Processors::SoundEngine::WindowType::type type, float alpha);

    static void applyDefaultWindows(Buffer &buffer, usize channels, utils::span<char> channelsToProcess,
      usize samples, Processors::SoundEngine::WindowType::type type, float alpha) noexcept;

    void applyCustomWindows(Buffer &buffer, usize channels, utils::span<char> channelsToProcess,
      usize samples, Processors::SoundEngine::WindowType::type type, float alpha);
  };
}