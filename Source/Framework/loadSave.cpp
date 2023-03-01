/*
  ==============================================================================

    loadSave.cpp
    Created: 3 Dec 2022 1:46:31am
    Author:  theuser27

  ==============================================================================
*/

#include "loadSave.h"

namespace Framework
{
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
}