/*
  ==============================================================================

    Paths.h
    Created: 16 Nov 2022 6:47:05am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"
#include "Framework/constants.h"

namespace Interface
{
  namespace Paths
  {
    inline juce::Path fromSvgData(const void *data, size_t data_size)
    {
      std::unique_ptr drawable = juce::Drawable::createFromImageData(data, data_size);
      return drawable->getOutlineAsPath();
    }

    inline std::pair<juce::Path, juce::Path> filterIcon()
    {
      return { fromSvgData(BinaryData::Icon_Filter_svg, BinaryData::Icon_Filter_svgSize), {} };
    }

    inline std::pair<juce::Path, juce::Path> dynamicsIcon()
    {
      return { fromSvgData(BinaryData::Icon_Dynamics_svg, BinaryData::Icon_Dynamics_svgSize), {} };
    }

    inline std::pair<juce::Path, juce::Path> phaseIcon()
    {
      return { fromSvgData(BinaryData::Icon_Phase_svg, BinaryData::Icon_Phase_svgSize), {} };
    }
    
    inline std::pair<juce::Path, juce::Path> contrastIcon()
    {
      float width = 14.0f;
      float height = 14.0f;
      float rounding = 6.0f;

      juce::Path strokePath;
      juce::Path fillPath;

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

    inline std::pair<juce::Path, juce::Path> powerButtonIcon()
    {
      static juce::Path strokePath = []()
      {
        static constexpr float kAngle = 0.8f * k2Pi;
        static constexpr float kAngleStart = kPi - kAngle * 0.5f;
        juce::Path path;

        path.startNewSubPath(5.5f, 0.0f);
        path.lineTo(5.5f, 5.0f);
        path.closeSubPath();

        path.addArc(0.0f, 2.0f, 11.0f, 11.0f, kAngleStart, kAngle + kAngleStart, true);

        return path;
      }();

      return { strokePath, {} };
    }

    inline juce::Path downTriangle()
    {
      juce::Path path;

      path.startNewSubPath(0.33f, 0.4f);
      path.lineTo(0.66f, 0.4f);
      path.lineTo(0.5f, 0.6f);
      path.closeSubPath();

      path.addLineSegment(juce::Line{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.2f);
      path.addLineSegment(juce::Line{ 1.0f, 1.0f, 1.0f, 1.0f }, 0.2f);
      return path;
    }

  };
}