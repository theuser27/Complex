
// Created: 2022-12-03 01:46:31

#pragma once

#include "platform.hpp"

namespace utils
{
  struct bumpArena;
  class string_view;
  class string;
  template<typename T>
  class vector;
}

namespace Framework
{
  struct IndexedData;

  namespace LoadSave
  {
    struct JsonTlsContext
    {
      // after finishing work with the arena and freeing it, reset this pointer
      utils::bumpArena *arena{};
      utils::string *errorPath{};
      utils::vector<Framework::IndexedData *> *dynamicOptionFixups{};
    };
    JsonTlsContext &getJsonContext();

    utils::string getConfigFilePath(utils::string_view file);
    // returns absolute window dimensions
    void getWindowSizeScale(u32 &windowWidth, u32 &windowHeight, float &windowScale);
    i32 getModuleWidth();
    void getStartupParameters(usize &parameterMappings, usize &inSidechains, usize &outSidechains, usize &undoSteps);

    void saveWindowSizeScale(u32 windowWidth, u32 windowHeight, float windowScale);
    void saveParameterMappings(usize parameterMappings);
    void saveUndoStepCount(usize undoStepCount);
  }
}
