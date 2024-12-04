/*
  ==============================================================================

    load_save.hpp
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <tuple>

#include "platform_definitions.hpp"

namespace juce
{
  class File;
}

namespace Framework
{
  namespace LoadSave
  {
    // returns absolute window dimensions
    std::pair<int, int> getWindowSize();
    double getWindowScale();
    std::tuple<usize, usize, usize> getParameterMappingsAndSidechains();

    void saveWindowSize(int windowWidth, int windowHeight);
    void saveWindowScale(double windowScale);

    void deserialiseSave(const juce::File &);
    void writeSave(void *jsonData);
  };
}