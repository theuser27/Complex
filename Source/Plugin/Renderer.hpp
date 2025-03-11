/*
  ==============================================================================

    Renderer.hpp
    Created: 20 Dec 2022 7:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/utils.hpp"
#include "Interface/LookAndFeel/Shaders.hpp"

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

    void reloadSkin(utils::up<Skin> skin);
    void updateFullGui();
    void setGuiScale(double scale);
    void clampScaleWidthHeight(double &desiredScale, int &windowWidth, int &windowHeight) const;

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
    std::atomic<bool> &getRenderLock() noexcept;
    Skin *getSkin() noexcept;

    void setEditor(juce::AudioProcessorEditor *editor) noexcept { topLevelComponent_ = editor; }
    void pushOpenGlResourceToDelete(OpenGlAllocatedResource type, GLsizei n, GLuint id);
    void setIsResizing(bool isResizing) noexcept;

  private:
    class Pimpl;

    utils::up<Skin> skinInstance_;
    utils::up<MainInterface> gui_;

    Plugin::ComplexPlugin &plugin_;
    juce::AudioProcessorEditor *topLevelComponent_ = nullptr;

    Pimpl *pimpl_ = nullptr;
    static constexpr usize kPimplAlignment = 8;
    alignas(kPimplAlignment) unsigned char pimplStorage_[460]{};

    friend class Pimpl;
  };
}
