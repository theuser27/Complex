
// Created: 2022-12-03 01:46:31

#pragma once

#include "platform_definitions.hpp"

namespace utils
{
  class string_view;
  class string;
}

namespace Framework
{
  namespace LoadSave
  {
    utils::string
    getConfigFilePath(utils::string_view file);
    // returns absolute window dimensions
    void getWindowSizeScale(u32 &windowWidth, u32 &windowHeight, float &windowScale);
    i32
    getModuleWidth();
    void getStartupParameters(usize &parameterMappings, usize &inSidechains, usize &outSidechains, usize &undoSteps);

    void saveWindowSizeScale(u32 windowWidth, u32 windowHeight, float windowScale);
    void saveParameterMappings(usize parameterMappings);
    void saveUndoStepCount(usize undoStepCount);
  }
}