/*
  ==============================================================================

    Paths.h
    Created: 16 Nov 2022 6:47:05am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

namespace Interface
{
  class Paths
  {
    static Path fromSvgData(const void *data, size_t data_size)
    {
      std::unique_ptr drawable = Drawable::createFromImageData(data, data_size);
      return drawable->getOutlineAsPath();
    }

  public:
    Paths() = delete;

    static Path contrastIcon()
    {
      return fromSvgData(BinaryData::Icon_Contrast_svg, BinaryData::Icon_Contrast_svgSize);
    }

    static Path downTriangle()
    {
      Path path;

      path.startNewSubPath(0.33f, 0.4f);
      path.lineTo(0.66f, 0.4f);
      path.lineTo(0.5f, 0.6f);
      path.closeSubPath();

      path.addLineSegment(Line{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.2f);
      path.addLineSegment(Line{ 1.0f, 1.0f, 1.0f, 1.0f }, 0.2f);
      return path;
    }

  };
}