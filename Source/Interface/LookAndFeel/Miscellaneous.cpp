/*
  ==============================================================================

    Miscellaneous.cpp
    Created: 22 Jun 2024 7:00:15pm
    Author:  theuser27

  ==============================================================================
*/

#include "Miscellaneous.hpp"

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

#include "Framework/constants.hpp"
#include "Framework/utils.hpp"

namespace Interface
{
  thread_local InterfaceRelated uiRelated{};

  // initialising paths here because juce::Path is incomplete in the header 
  // and causes problems on clang with libstdc++
  Shape::Shape() : paths{} { }
  Shape::Shape(const Shape &) = default;
  Shape::Shape(Shape &&) noexcept = default;
  Shape::~Shape() noexcept = default;
  Shape &Shape::operator=(const Shape &) = default;
  Shape &Shape::operator=(Shape &&) noexcept = default;

  void Shape::drawAll(juce::Graphics &g, const juce::PathStrokeType &strokeType, 
    const juce::AffineTransform &transform, std::span<juce::Colour> colours) const
  {
    for (size_t i = 0; i < paths.size(); ++i)
    {
      auto &[path, type, predefinedColour] = paths[i];
      auto colour = (predefinedColour == juce::Colour{}) ? colours[i] : predefinedColour;

      switch (type)
      {
      case Stroke:
        g.setColour(colour);
        g.strokePath(path, strokeType, transform);
        break;
      case Fill:
        g.setColour(colour);
        g.fillPath(path, transform);
        break;
      default:
        COMPLEX_ASSERT_FALSE("Unhandled case");
        break;
      }
    }
  }

  PopupItems::PopupItems(Type type, int id, std::string name, std::string hint, Shape icon, bool active) :
    icon(std::move(icon)), name(std::move(name)), hint(std::move(hint)), type(type), id(id), isActive(active) { }

  PopupItems::PopupItems(const PopupItems &) = default;
  PopupItems::PopupItems(PopupItems &&) noexcept = default;
  PopupItems &PopupItems::operator=(const PopupItems &) = default;
  PopupItems &PopupItems::operator=(PopupItems &&) noexcept = default;

  namespace Paths
  {
    juce::Path fromSvgData(const void *data, size_t data_size)
    {
      return juce::Drawable::createFromImageData(data, data_size)->getOutlineAsPath();
    }

    Shape pasteValueIcon()
    {
      static const Shape shape = []()
      {
        juce::Path one;
        one.startNewSubPath({ 4.5f, 11.5f });
        one.quadraticTo({ 3.0f, 12.0f }, { 3.5f, 10.5f });
        one.lineTo({ 3.5f, 4.5f });
        one.quadraticTo({ 3.0f, 3.0f }, { 4.5f, 3.5f });
        one.lineTo({ 5.5f, 3.5f });

        one.startNewSubPath({ 6.0f, 4.5f });
        one.quadraticTo({ 5.0f, 5.0f }, { 5.5f, 4.0f });
        one.lineTo({ 5.5f, 3.0f });
        one.quadraticTo({ 5.0f, 2.0f }, { 6.0f, 2.5f });
        one.lineTo({ 8.0f, 2.5f });
        one.quadraticTo({ 9.0f, 2.0f }, { 8.5f, 3.0f });
        one.lineTo({ 8.5f, 4.0f });
        one.quadraticTo({ 9.0f, 5.0f }, { 8.0f, 4.5f });
        one.closeSubPath();

        one.startNewSubPath({ 8.5f, 3.5f });
        one.lineTo({ 9.5f, 3.5f });
        one.quadraticTo({ 11.0f, 3.0f }, { 10.5f, 4.5f });

        one.addRoundedRectangle(juce::Rectangle{ 6.5f, 6.5f, 6.0f, 7.0f }, 1.5f);

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(one), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape enterValueIcon()
    {
      static const Shape shape = []()
      {
        juce::Path one;
        one.startNewSubPath({ 3.5f, 12.5f });
        one.lineTo({ 3.5f, 11.0f });
        one.quadraticTo({ 3.0f, 10.0f }, { 4.0f, 10.0f });
        one.lineTo({ 8.5f, 5.5f });
        one.lineTo({ 10.5f, 7.5f });
        one.lineTo({ 6.0f, 12.0f });
        one.quadraticTo({ 6.0f, 13.0f }, { 5.0f, 12.5f });
        one.closeSubPath();

        juce::Path two;
        two.startNewSubPath({ 10.0f, 4.0f });
        two.quadraticTo({ 11.0f, 2.5f }, { 12.5f, 3.5f });
        two.quadraticTo({ 13.5f, 5.0f }, { 12.0f, 6.0f });
        two.closeSubPath();

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(one), Shape::Stroke, juce::Colour{});
        result.paths.emplace_back(COMPLEX_MOV(two), Shape::Fill, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape copyNormalisedValueIcon()
    {
      static const Shape shape = []()
      {
        juce::Path path;
        path.startNewSubPath({ 3.5f, 6.0f });
        path.lineTo({ 3.5f, 11.0f });
        path.quadraticTo({ 3.5f, 12.5f }, { 5.0f, 12.5f });
        path.lineTo({ 10.0f, 12.5f });

        path.addRoundedRectangle(juce::Rectangle{ 5.5f, 3.5f, 6.0f, 7.0f }, 1.5f);
        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(path), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape copyScaledValueIcon()
    {
      static const Shape shape = []()
      {
        static constexpr float fStartX = 9.5f;
        static constexpr float fStartY = 7.5f;
        static constexpr float fWidth = 2.0f;
        static constexpr float fHeight = 4.0f;
        static constexpr float fRounding = 1.0f;

        juce::Path one;
        one.startNewSubPath({ 3.5f, 6.0f });
        one.lineTo({ 3.5f, 11.0f });
        one.quadraticTo({ 3.5f, 12.5f }, { 5.0f, 12.5f });
        one.lineTo({ 10.0f, 12.5f });

        one.startNewSubPath({ 7.5f, 3.5f });
        one.lineTo({ 6.5f, 3.5f });
        one.quadraticTo({ 5.5f, 3.5f }, { 5.5f, 4.5f });
        one.lineTo({ 5.5f, 9.5f });
        one.quadraticTo({ 5.5f, 10.5f }, { 6.5f, 10.5f });
        one.lineTo({ 10.5f, 10.5f });
        one.quadraticTo({ 11.5f, 10.5f }, { 11.5f, 9.5f });
        one.lineTo({ 11.5f, 8.5f });

        juce::Path two;
        two.startNewSubPath({ fStartX, fStartY });
        two.lineTo({ fStartX, fStartY - (fHeight - fRounding) });
        two.quadraticTo({ fStartX, fStartY - fHeight }, { fStartX + fRounding, fStartY - fHeight });
        two.lineTo({ fStartX + fWidth, fStartY - fHeight });
        two.startNewSubPath({ fStartX, 5.5f });
        two.lineTo({ fStartX + fWidth, 5.5f });

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(one), Shape::Stroke, juce::Colour{});
        result.paths.emplace_back(COMPLEX_MOV(two), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape filterIcon()
    {
      static const auto shape = []()
      {
        Shape result;
        result.paths.emplace_back(fromSvgData(BinaryData::Icon_Filter_svg,
          BinaryData::Icon_Filter_svgSize), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape dynamicsIcon()
    {
      static const auto shape = []()
      {
        Shape result;
        result.paths.emplace_back(fromSvgData(BinaryData::Icon_Dynamics_svg,
          BinaryData::Icon_Dynamics_svgSize), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape phaseIcon()
    {
      static const auto shape = []()
      {
        Shape result;
        result.paths.emplace_back(fromSvgData(BinaryData::Icon_Phase_svg,
          BinaryData::Icon_Phase_svgSize), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape pitchIcon()
    {
      static const auto shape = []()
      {
        Shape result;
        result.paths.emplace_back(fromSvgData(BinaryData::Icon_Pitch_svg,
          BinaryData::Icon_Pitch_svgSize), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape destroyIcon()
    {
      static const auto shape = []()
      {
        static constexpr float width = 9.0f;
        static constexpr float height = 10.0f;
        static constexpr float centerElementWidth = width / 3.0f;
        static constexpr float bodyX = 1.5f;
        static constexpr float bodyY = 3.0f;
        static constexpr float bodyWidth = width - 2.0f * bodyX;
        static constexpr float bodyHeight = height - bodyY;

        juce::Path strokePath;

        // hat
        strokePath.startNewSubPath(0.0f, 1.0f);
        strokePath.lineTo(width, 1.0f);
        strokePath.closeSubPath();
        strokePath.startNewSubPath((width - centerElementWidth) * 0.5f, 0.0f);
        strokePath.lineTo((width - centerElementWidth) * 0.5f + centerElementWidth, 0.0f);
        strokePath.closeSubPath();

        // body outline
        strokePath.addRoundedRectangle(bodyX, bodyY, bodyWidth, bodyHeight, 2.0f, 2.0f, false, false, true, true);
        
        // vertical lines 
        strokePath.startNewSubPath((width - centerElementWidth + 1.0f) * 0.5f, bodyY + 2.0f);
        strokePath.lineTo((width - centerElementWidth + 1.0f) * 0.5f, bodyY + bodyHeight - 2.0f);
        strokePath.closeSubPath();
        strokePath.startNewSubPath((width + centerElementWidth + 1.0f) * 0.5f - 1.0f, bodyY + 2.0f);
        strokePath.lineTo((width + centerElementWidth + 1.0f) * 0.5f - 1.0f, bodyY + bodyHeight - 2.0f);
        strokePath.closeSubPath();

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(strokePath), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape contrastIcon()
    {
      static const auto shape = []()
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

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(strokePath), Shape::Stroke, juce::Colour{});
        result.paths.emplace_back(COMPLEX_MOV(fillPath), Shape::Fill, juce::Colour{});
        return result;
      }();

      return shape;
    }

    Shape powerButtonIcon()
    {
      static const auto shape = []()
      {
        static constexpr float kAngle = 0.8f * k2Pi;
        static constexpr float kAngleStart = kPi - kAngle * 0.5f;
        juce::Path path;

        path.startNewSubPath(5.5f, 0.0f);
        path.lineTo(5.5f, 5.0f);
        path.closeSubPath();

        path.addArc(0.0f, 2.0f, 11.0f, 11.0f, kAngleStart, kAngle + kAngleStart, true);

        Shape result;
        result.paths.emplace_back(COMPLEX_MOV(path), Shape::Stroke, juce::Colour{});
        return result;
      }();

      return shape;
    }
  }
}
