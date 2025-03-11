/*
  ==============================================================================

    load_save.hpp
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "platform_definitions.hpp"
#include "stl_utils.hpp"

namespace juce
{
  class File;
}

namespace Framework
{
  namespace LoadSave
  {
    // returns absolute window dimensions
    utils::pair<int, int> getWindowSize();
    double getWindowScale();
    void getStartupParameters(usize &parameterMappings, usize &inSidechains, usize &outSidechains, usize &undoSteps);

    void saveWindowSize(int windowWidth, int windowHeight);
    void saveWindowScale(double windowScale);
    void saveParameterMappings(usize parameterMappings);
    void saveUndoStepCount(usize undoStepCount);

    void deserialiseSave(const juce::File &);
    void writeSave(void *jsonData);
  }
}