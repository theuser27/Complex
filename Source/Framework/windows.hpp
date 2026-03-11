
// Created: 2021-08-05 03:39:49

#pragma once

#include "stl_utils.hpp"

namespace Framework
{
  struct Buffer;
  struct CircularBuffer;

  struct Window
  {
    enum WindowFlags { HasAlpha = 1 };

    COMPLEX_ENUM_LOCAL(Types,
      (       Lerp, 1757856220258),
      (       Hann, 1757856231973),
      (    Hamming, 1757856234278),
      (   Triangle, 1757856253138),
      (       Sine, 1757856261326),
      (  Rectangle, 1757856269202),
      (Exponential, 1757856275556),
      (    HannExp, 1757856284388),
      (    Lanczos, 1757856295720),
    )

    void applyWindow(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
      u32 samples, uuid windowType, float alpha);

    void addOverlap(CircularBuffer &destination, Buffer &source, u32 channels, 
      utils::span<bool> channelsToProcess, u32 samples, u32 destinationBegin, uuid windowType);

    void scaleDown(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
      u32 start, u32 samples, uuid windowType, float overlap, float alpha);
  };
}
