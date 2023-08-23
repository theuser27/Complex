/*
  ==============================================================================

    Renderer.h
    Created: 20 Dec 2022 7:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Complex.h"
#include "Interface/LookAndFeel/Shaders.h"
#include "Interface/Components/OpenGlComponent.h"

namespace Interface
{
  class OpenGlComponent;
  class MainInterface;
  class Skin;

  class Renderer : public OpenGLRenderer, Timer
  {
  public:
    static constexpr double kMinOpenGlVersion = 1.4;

    Renderer(Plugin::ComplexPlugin &plugin, AudioProcessorEditor &topLevelComponent);
    ~Renderer() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void timerCallback() override { plugin_.updateGUIParameters(); openGlContext_.triggerRepaint(); }

    void startUI(Component &dummyComponent);
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
    void pushOpenGlComponentToDelete(OpenGlComponent *openGlComponent) noexcept 
    { openCleanupQueue_.emplace(openGlComponent); }
    MainInterface *getGui() noexcept;
    Skin *getSkin() noexcept;

  private:
    void doCleanupWork()
    {
      while (!openCleanupQueue_.empty())
      {
        auto *openGlComponent = openCleanupQueue_.front();
        openGlComponent->destroy();
        delete openGlComponent;
        openCleanupQueue_.pop();
      }
    }

    bool unsupported_ = false;
    bool animate_ = true;
    bool enableRedoBackground_ = true;
    std::unique_ptr<Shaders> shaders_;
    std::queue<OpenGlComponent *> openCleanupQueue_{};
    std::unique_ptr<Skin> skinInstance_;
    OpenGLContext openGlContext_;
    OpenGlWrapper openGl_{ openGlContext_ };

    Plugin::ComplexPlugin &plugin_;
    AudioProcessorEditor &topLevelComponent_;
    std::unique_ptr<MainInterface> gui_;

    friend class MainInterface;
  };
}
