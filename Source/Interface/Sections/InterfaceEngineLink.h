/*
  ==============================================================================

    InterfaceEngineLink.h
    Created: 20 Dec 2022 7:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Plugin/Complex.h"
#include "MainInterface.h"

namespace Interface
{
  class InterfaceEngineLink
  {
  public:
    InterfaceEngineLink(Plugin::ComplexPlugin &plugin);
    virtual ~InterfaceEngineLink() = default;

    virtual void updateFullGui();

    //void notifyModulationsChanged();
    //void notifyModulationValueChanged(int index);
    //void connectModulation(std::string source, std::string destination);
    //void connectModulation(vital::ModulationConnection *connection);
    //void setModulationValues(const std::string &source, const std::string &destination,
    //  vital::mono_float amount, bool bipolar, bool stereo, bool bypass);
    //void initModulationValues(const std::string &source, const std::string &destination);
    //void disconnectModulation(std::string source, std::string destination);
    //void disconnectModulation(vital::ModulationConnection *connection);

  	void setGuiScale(double scale);

  protected:
    double clampScaleFactorToFit(double desiredScale, int windowWidth, int windowHeight) const;

    Plugin::ComplexPlugin &plugin_;
    std::unique_ptr<MainInterface> gui_ = nullptr;
  };
}
