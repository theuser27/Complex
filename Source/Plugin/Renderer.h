/*
  ==============================================================================

    Renderer.h
    Created: 20 Dec 2022 7:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <queue>
#include <memory>

namespace juce
{
	class AudioProcessorEditor;
}

namespace Plugin
{
  class ComplexPlugin;
}

namespace Interface
{
  class OpenGlComponent;
  class MainInterface;
  class Skin;

  class Renderer
  {
  public:
    static constexpr double kMinOpenGlVersion = 1.4;

    Renderer(Plugin::ComplexPlugin &plugin);
    ~Renderer() noexcept;

    void startUI();
    void stopUI();

    void updateFullGui();
    void setGuiScale(double scale);
    double clampScaleFactorToFit(double desiredScale, int windowWidth, int windowHeight) const;

    //void notifyModulationsChanged();
    //void notifyModulationValueChanged(int index);
    //void connectModulation(std::string source, std::string destination);
    //void connectModulation(vital::ModulationConnection *connection);
    //void setModulationValues(const std::string &source, const std::string &destination,
    //  vital::mono_float amount, bool bipolar, bool stereo, bool bypass);
    //void initModulationValues(const std::string &source, const std::string &destination);
    //void disconnectModulation(std::string source, std::string destination);
    //void disconnectModulation(vital::ModulationConnection *connection);

    auto &getPlugin() noexcept { return plugin_; }
    MainInterface *getGui() noexcept;
    Skin *getSkin() noexcept;

    void setEditor(juce::AudioProcessorEditor *editor) noexcept { topLevelComponent_ = editor; }
    void pushOpenGlComponentToDelete(OpenGlComponent *openGlComponent);

  private:
    class Pimpl;

    std::unique_ptr<Pimpl> pimpl_;
    std::unique_ptr<Skin> skinInstance_;

    Plugin::ComplexPlugin &plugin_;
    juce::AudioProcessorEditor *topLevelComponent_;
    std::unique_ptr<MainInterface> gui_;

    friend class MainInterface;
    friend class Pimpl;
  };
}
