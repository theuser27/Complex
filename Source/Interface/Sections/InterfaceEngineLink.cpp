/*
	==============================================================================

		InterfaceEngineLink.cpp
		Created: 20 Dec 2022 7:34:54pm
		Author:  theuser27

	==============================================================================
*/

#include "InterfaceEngineLink.h"

#include "MainInterface.h"
#include "Framework/load_save.h"

namespace Interface
{
	InterfaceEngineLink::InterfaceEngineLink(Plugin::ComplexPlugin &plugin) : plugin_(plugin)
	{
		gui_ = std::make_unique<MainInterface>(plugin);
	}

	void InterfaceEngineLink::updateFullGui()
	{
		if (gui_ == nullptr)
			return;

		gui_->updateAllValues();
		gui_->reset();
	}

	void InterfaceEngineLink::setGuiScale(double scale)
	{
		if (gui_ == nullptr)
			return;

		auto windowWidth = gui_->getWidth();
		auto windowHeight = gui_->getHeight();
		auto clampedScale = clampScaleFactorToFit(scale, windowWidth, windowHeight);

		Framework::LoadSave::saveWindowScale(scale);
		gui_->setScaling((float)scale);
		// we're calling *this through getParentComponent because the class doesn't inherit from Component
		gui_->getParentComponent()->setSize((int)std::round(windowWidth * clampedScale), 
			(int)std::round(windowHeight * clampedScale));

		gui_->redoBackground();
	}

	double InterfaceEngineLink::clampScaleFactorToFit(double desiredScale,
		int windowWidth, int windowHeight) const
	{
		// the available display area on screen for the window - border thickness
		Rectangle<int> displayArea = Desktop::getInstance().getDisplays().getTotalBounds(true);
		if (auto *peer = gui_->getPeer())
			peer->getFrameSize().subtractFrom(displayArea);

		auto displayWidth = (double)displayArea.getWidth();
		auto displayHeight = (double)displayArea.getHeight();

		double scaledWindowWidth = desiredScale * windowWidth;
		double scaledWindowHeight = desiredScale * windowHeight;

		if (scaledWindowWidth > displayWidth)
		{
			desiredScale *= displayWidth / scaledWindowWidth;
			scaledWindowHeight = desiredScale * windowHeight;
		}

		if (scaledWindowHeight > displayHeight)
			desiredScale *= displayHeight / scaledWindowHeight;

		return desiredScale;
	}

}
