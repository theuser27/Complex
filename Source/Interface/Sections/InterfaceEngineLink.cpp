/*
	==============================================================================

		InterfaceEngineLink.cpp
		Created: 20 Dec 2022 7:34:54pm
		Author:  theuser27

	==============================================================================
*/

#include "InterfaceEngineLink.h"

#include "MainInterface.h"

namespace Interface
{
	InterfaceEngineLink::InterfaceEngineLink(Plugin::ComplexPlugin &plugin) : plugin_(plugin)
	{
		gui_ = std::make_unique<MainInterface>(GuiData(plugin_));
	}

	void InterfaceEngineLink::updateParameterValues()
	{
		auto &parameterBridges = plugin_.getParameterBridges();

		for (auto &bridge : parameterBridges)
		{
			if (auto *link = bridge->parameterLinkPointer_.load(std::memory_order_acquire); link)
				link->UIControl->updateValue();
		}
	}

}
