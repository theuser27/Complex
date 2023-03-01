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

namespace Interface
{
  class MainInterface;

  struct GuiData
  {
    GuiData(Plugin::ComplexPlugin &plugin) : plugin_(plugin),
  		parameterBridges_(plugin.getParameterBridges()),
  		parameterModulators_(plugin.getParameterModulators()) { }

    Plugin::ComplexPlugin &plugin_;
    std::vector<Framework::ParameterBridge *> &parameterBridges_;
    std::vector<Framework::ParameterModulator *> &parameterModulators_;
  };

  class InterfaceEngineLink : private juce::Timer
  {
  public:
    InterfaceEngineLink(Plugin::ComplexPlugin &plugin);
    ~InterfaceEngineLink() override { stopTimer(); }

    // Inherited from juce::Timer
    void timerCallback() override { updateParameterValues(); }

    auto *getPlugin() { return &plugin_; }
    auto *getGUI() { return gui_.get(); }

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

    void setFocus();
    void setGuiSize(float scale);

    // TODO: specify function body to update all parameters linked to a bridge
    void updateParameterValues();

  private:
    Plugin::ComplexPlugin &plugin_;
    std::unique_ptr<MainInterface> gui_ = nullptr;
  };
}
