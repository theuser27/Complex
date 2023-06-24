/*
  ==============================================================================

    load_save.h
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Third Party/json/json.hpp"
#include "Framework/common.h"

using json = nlohmann::json;

namespace Framework
{
  class LoadSave
  {
  public:
    static File getConfigFile();
    static json getConfigJson();
    static void saveConfigJson(json configState);

    static File getDefaultSkin();
    static String getVersion();

  	// returns absolute window dimensions
  	static std::pair<int, int> getWindowSize();
  	static double getWindowScale();

    static void saveWindowSize(int windowWidth, int windowHeight);
    static void saveWindowScale(double windowScale);
  };
}