
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
      (       Lerp, 1757856220258303000),
      (       Hann, 1757856231973283300),
      (    Hamming, 1757856234278114900),
      (   Triangle, 1757856253138693000),
      (       Sine, 1757856261326615000),
      (  Rectangle, 1757856269202469800),
      (Exponential, 1757856275556662800),
      (    HannExp, 1757856284388080900),
      (    Lanczos, 1757856295720547400),
    )

    void applyWindow(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
      u32 samples, uuid windowType, float alpha);

    void addOverlap(CircularBuffer &destination, Buffer &source, u32 channels, 
      utils::span<bool> channelsToProcess, u32 samples, u32 destinationBegin, uuid windowType);

    void scaleDown(Buffer &buffer, u32 channels, utils::span<bool> channelsToProcess,
      u32 start, u32 samples, uuid windowType, float overlap, float alpha);
  };
}
