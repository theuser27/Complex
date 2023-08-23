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

    static std::pair<Path, Path> filterIcon()
    {
      return { fromSvgData(BinaryData::Icon_Filter_svg, BinaryData::Icon_Filter_svgSize), {} };
    }
    
    static std::pair<Path, Path> contrastIcon()
    {
      float width = 14.0f;
      float height = 14.0f;
      float rounding = 6.0f;

      Path strokePath;
      Path fillPath;

      strokePath.startNewSubPath(width - rounding, 0.0f);
      strokePath.quadraticTo(width, 0.0f, width, rounding);
      strokePath.lineTo(width, height - rounding);
      strokePath.quadraticTo(width, height, width - rounding, height);
      strokePath.lineTo(rounding, height);
      strokePath.quadraticTo(0.0f, height, 0.0f, height - rounding);
      strokePath.lineTo(0.0f, rounding);
      strokePath.quadraticTo(0.0f, 0.0f, rounding, 0.0f);
      strokePath.closeSubPath();

      fillPath.startNewSubPath(width * 0.5f, 0.0f);
      fillPath.lineTo(width - rounding, 0.0f);
      fillPath.quadraticTo(width, 0.0f, width, rounding);
      fillPath.lineTo(width, height - rounding);
      fillPath.quadraticTo(width, height, width - rounding, height);
      fillPath.lineTo(width * 0.5f, height);
      fillPath.closeSubPath();

      return { strokePath, fillPath };
    }

    static std::pair<Path, Path> powerButtonIcon()
    {
      static Path strokePath = []()
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

      return { strokePath, {} };
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