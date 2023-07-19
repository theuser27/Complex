/*
  ==============================================================================

    Paths.h
    Created: 16 Nov 2022 6:47:05am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Framework/constants.h"

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

    static Path contrastIcon() { return fromSvgData(BinaryData::Icon_Contrast_svg, BinaryData::Icon_Contrast_svgSize); }
    static Path powerButtonIcon()
    {
      static Path powerButtonPath = []()
      {
        static constexpr float kAngle = 0.8f * k2Pi;
        static constexpr float kAngleStart = kPi - kAngle * 0.5f;
        Path path;

        path.startNewSubPath(5.5f, 0.0f);
        path.lineTo(5.5f, 5.0f);
        path.closeSubPath();

        path.addArc(0.0f, 2.0f, 11.0f, 11.0f, kAngleStart, kAngle + kAngleStart, true);

        return path;
      }();

      return powerButtonPath;
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