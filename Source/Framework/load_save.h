/*
	==============================================================================

		load_save.h
		Created: 3 Dec 2022 1:46:31am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <any>
#include "AppConfig.h"
#include <juce_core/juce_core.h>

namespace Framework
{
	namespace LoadSave
	{
		juce::File getConfigFile();

		juce::File getDefaultSkin();
		juce::String getVersion();

		// returns absolute window dimensions
		std::pair<int, int> getWindowSize();
		double getWindowScale();

		void saveWindowSize(int windowWidth, int windowHeight);
		void saveWindowScale(double windowScale);

		void deserialiseSave(juce::File);
		void writeSave(std::any jsonData);
	};
}