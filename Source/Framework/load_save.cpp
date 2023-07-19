/*
  ==============================================================================

    load_save.cpp
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#include "load_save.h"
#include "Interface/Sections/MainInterface.h"

namespace Framework
{
  File LoadSave::getConfigFile()
  {
  #if defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
    PropertiesFile::Options config_options;
    config_options.applicationName = "Complex";
    config_options.osxLibrarySubFolder = "Application Support";
    config_options.filenameSuffix = "config";

  #ifdef LINUX
    config_options.folderName = "." + String(ProjectInfo::projectName).toLowerCase();
  #else
    config_options.folderName = String(ProjectInfo::projectName).toLowerCase();
  #endif

    return config_options.getDefaultFile();
  #else
    return File();
  #endif
  }

  json LoadSave::getConfigJson()
  {
    File config_file = getConfigFile();
    if (!config_file.exists())
      return json();

    try
    {
      json parsed = json::parse(config_file.loadFileAsString().toStdString(), nullptr, false);
      if (parsed.is_discarded())
        return json();
      return parsed;
    }
    catch (const json::exception &)
    {
      return json();
    }
  }

  void LoadSave::saveConfigJson(json configState)
  {
    File config_file = getConfigFile();

    if (!config_file.exists())
      config_file.create();
    config_file.replaceWithText(configState.dump());
  }

  File LoadSave::getDefaultSkin()
  {
#if defined(JUCE_DATA_STRUCTURES_H_INCLUDED)
    PropertiesFile::Options config_options;
    config_options.applicationName = "Complex";
    config_options.osxLibrarySubFolder = "Application Support";
    config_options.filenameSuffix = "skin";

#ifdef LINUX
    config_options.folderName = "." + String(ProjectInfo::projectName).toLowerCase();
#else
    config_options.folderName = String(ProjectInfo::projectName).toLowerCase();
#endif

    return config_options.getDefaultFile();
#else
    return File();
#endif
  }

  String LoadSave::getVersion()
  {
    json data = getConfigJson();

    if (!data.count("pluginVersion"))
      return "0.0.0";

    std::string version = data["pluginVersion"];
    return version;
  }

  std::pair<int, int> LoadSave::getWindowSize()
  {
    json data = getConfigJson();

    int width;
    int height;

    if (!data.count("windowWidth"))
      width = kDefaultWindowWidth;
    else
      width = std::max<int>(Interface::MainInterface::kMinWidth, data["windowWidth"]);

    if (!data.count("windowHeight"))
      height = kDefaultWindowHeight;
    else
      height = std::max<int>(Interface::MainInterface::kMinHeight, data["windowHeight"]);
    
    return { width, height };
  }

  double LoadSave::getWindowScale()
  {
    json data = getConfigJson();
    double scale;

    if (!data.count("windowScale"))
      scale = 1.0;
    else
      scale = data["windowScale"];

    return scale;
  }

  void LoadSave::saveWindowSize(int windowWidth, int windowHeight)
  {
    json data = getConfigJson();
    data["windowWidth"] = windowWidth;
    data["windowHeight"] = windowHeight;
    saveConfigJson(data);
  }

  void LoadSave::saveWindowScale(double windowScale)
  {
    json data = getConfigJson();
    data["windowScale"] = windowScale;
    saveConfigJson(data);
  }

}