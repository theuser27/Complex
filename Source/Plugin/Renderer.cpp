/*
  ==============================================================================

    Renderer.cpp
    Created: 20 Dec 2022 7:34:54pm
    Author:  theuser27

  ==============================================================================
*/

#include "Renderer.hpp"

#include <vector>
#include <juce_opengl/juce_opengl.h>

#include "Framework/load_save.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Plugin/Complex.hpp"
#include "Interface/Components/OpenGlComponent.hpp"
#include "Interface/Sections/MainInterface.hpp"

namespace Interface
{
  class Renderer::Pimpl final : public OpenGLRenderer, Timer
  {
    static constexpr auto parentStackSpace = 64;
  public:
    Pimpl(Renderer &renderer, Plugin::ComplexPlugin &plugin) : renderer_(renderer), plugin_(plugin) 
    {
      openGl_.parentStack.reserve(parentStackSpace);
    }

    void newOpenGLContextCreated() override
    {
      double versionSupported = OpenGLShaderProgram::getLanguageVersion();
      unsupported_ = versionSupported < kMinOpenGlVersion;
      if (unsupported_)
      {
        NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Unsupported OpenGl Version",
          String{ CharPointer_UTF8{ JucePlugin_Name " requires OpenGL version: " } } + String(kMinOpenGlVersion) +
          String("\nSupported version: ") + String(versionSupported));
        return;
      }

      shaders_ = utils::up<Shaders>::create(openGlContext_);
      openGl_.shaders = shaders_.get();
      uiRelated.renderer = &renderer_;
    }

    void renderOpenGL() override
    {
      if (unsupported_)
        return;

      doCleanupWork();

      openGl_.animate = animate_;
      utils::ScopedLock g{ renderLock_, utils::WaitMechanism::WaitNotify };

      renderer_.gui_->renderOpenGlComponents(openGl_);

      // calling swapBuffers inside the critical section in case 
      // we're resizing because a glFinish is necessary in order to
      // not get frame tearing/overlap with previous frames
      // https://community.khronos.org/t/swapbuffers-and-synchronization/107667/5
      openGl_.context.swapBuffers();
      if (isResizing_.load(std::memory_order_acquire))
        juce::gl::glFinish();
    }

    void openGLContextClosing() override
    {
      renderer_.gui_->destroyAllOpenGlComponents();
      doCleanupWork();

      openGl_.shaders = nullptr;
      shaders_ = nullptr;
    }

    void timerCallback() override
    {
      for (auto *bridge : plugin_.getParameterBridges())
        bridge->updateUIParameter();

      openGlContext_.triggerRepaint();
    }

    void startUI()
    {
      openGlContext_.setContinuousRepainting(false);
      openGlContext_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
      openGlContext_.setRenderer(this);
      openGlContext_.setComponentPaintingEnabled(false);
      // attaching the context to an empty component so that we can activate it
      // and also take advantage of componentRendering to lock the message manager
      openGlContext_.attachTo(*renderer_.gui_);

      startTimerHz(kParameterUpdateIntervalHz);
    }

    void stopUI()
    {
      stopTimer();

      openGlContext_.detach();
      openGlContext_.setRenderer(nullptr);
    }

    auto &getRenderLock() noexcept { return renderLock_; }
    void setIsResizing(bool isResizing) noexcept { isResizing_.store(isResizing, std::memory_order_release); }
    
    void pushOpenGlResourceToDelete(OpenGlAllocatedResource type, GLsizei n, GLuint id)
    {
      utils::ScopedLock g{ cleanupQueueLock_, utils::WaitMechanism::Spin };
      cleanupQueue_.emplace_back(type, n, id);
    }

    void doCleanupWork()
    {
      using namespace juce::gl;

      utils::ScopedLock g{ cleanupQueueLock_, utils::WaitMechanism::Spin };
      while (!cleanupQueue_.empty())
      {
        auto &[resourceType, n, id] = cleanupQueue_.back();
        switch (resourceType)
        {
        case OpenGlAllocatedResource::Buffer:
          glDeleteBuffers(n, &id);
          break;
        case OpenGlAllocatedResource::Texture:
          glDeleteTextures(n, &id);
          break;
        default:
          COMPLEX_ASSERT_FALSE("Missing resource type");
          break;
        }
        cleanupQueue_.erase(cleanupQueue_.end() - 1);
      }
    }

  private:
    bool unsupported_ = false;
    utils::shared_value<bool> animate_ = true;
    std::atomic<bool> renderLock_ = false;
    std::atomic<bool> isResizing_ = false;

    Renderer &renderer_;
    Plugin::ComplexPlugin &plugin_;
    OpenGLContext openGlContext_;
    OpenGlWrapper openGl_{ openGlContext_ };
    utils::up<Shaders> shaders_;
    std::vector<std::tuple<OpenGlAllocatedResource, GLsizei, GLuint>> cleanupQueue_{};
    std::atomic<bool> cleanupQueueLock_{};
  };

  Renderer::Renderer(Plugin::ComplexPlugin &plugin) : 
    skinInstance_{ utils::up<Skin>::create() }, plugin_{ plugin }
  {
    // if this fails, make pimplStorage_ big/aligned enough so that the implementation can fit
    // if only there were some way of backpropagating sizeof and alignof at compile time
    static_assert(COMPLEX_IS_ALIGNED_TO(Renderer, pimplStorage_, alignof(Pimpl)) && sizeof(Pimpl) <= sizeof(pimplStorage_));

    pimpl_ = new(pimplStorage_) Pimpl{ *this, plugin };

    uiRelated.renderer = this;
    uiRelated.skin = skinInstance_.get();
    gui_ = utils::up<MainInterface>::create();
  }

  Renderer::~Renderer() noexcept
  {
    pimpl_->~Pimpl();
  }

  void Renderer::startUI()
  {
    pimpl_->startUI();
  }

  void Renderer::stopUI()
  {
    pimpl_->stopUI();

    topLevelComponent_ = nullptr;
  }

  void Renderer::reloadSkin(utils::up<Skin> skin)
  {
    skinInstance_ = COMPLEX_MOV(skin);
    uiRelated.skin = skinInstance_.get();
    juce::Rectangle<int> bounds = gui_->getBounds();
    // TODO: trigger repaint in a more uhhh efficient manner
    gui_->setBounds(0, 0, bounds.getWidth() / 4, bounds.getHeight() / 4);
    gui_->setBounds(bounds);
  }

  void Renderer::updateFullGui()
  {
    if (gui_ == nullptr)
      return;

    gui_->updateAllValues();
  }

  void Renderer::setGuiScale(double scale)
  {
    if (gui_ == nullptr || topLevelComponent_ == nullptr)
      return;

    auto windowWidth = gui_->getWidth();
    auto windowHeight = gui_->getHeight();
    clampScaleWidthHeight(scale, windowWidth, windowHeight);

    Framework::LoadSave::saveWindowScale(scale);
    uiRelated.scale = (float)scale;
    topLevelComponent_->setSize((int)std::round(windowWidth * scale),
      (int)std::round(windowHeight * scale));
  }

  void Renderer::clampScaleWidthHeight(double &desiredScale, int &windowWidth, int &windowHeight) const
  {
    // the available display area on screen for the window - border thickness
    juce::Rectangle<int> displayArea = Desktop::getInstance().getDisplays().getTotalBounds(true);
    if (auto *peer = gui_->getPeer())
      if (auto frame = peer->getFrameSizeIfPresent())
        frame->subtractFrom(displayArea);

    desiredScale = std::floor(std::clamp(desiredScale, 
      (double)kMinWindowScaleFactor, (double)kMaxWindowScaleFactor) * 4.0) * 0.25;

    auto clampScaleToDisplay = [&](double min, double display)
    {
      while (min * desiredScale > display)
      {
        if (desiredScale == kMinWindowScaleFactor)
          break;
        desiredScale -= kWindowScaleIncrements;
      }
    };

    double displayWidth = displayArea.getWidth();
    clampScaleToDisplay(kMinWidth, displayWidth);

    while (desiredScale * windowWidth > displayWidth)
    {
      if (windowWidth <= kMinWidth)
      {
        windowWidth = kMinWidth;
        break;
      }
      windowWidth -= kAddedWidthPerLane;
    }

    double displayHeight = displayArea.getHeight();
    clampScaleToDisplay(kMinHeight, displayHeight);

    windowHeight = std::clamp(windowHeight, kMinHeight, (int)std::floor(displayHeight / desiredScale));
  }

  MainInterface *Renderer::getGui() noexcept { return gui_.get(); }
  Skin *Renderer::getSkin() noexcept { return skinInstance_.get(); }
  std::atomic<bool> &Renderer::getRenderLock() noexcept { return pimpl_->getRenderLock(); }
  
  void Renderer::pushOpenGlResourceToDelete(OpenGlAllocatedResource type, GLsizei n, GLuint id)
  { pimpl_->pushOpenGlResourceToDelete(type, n, id); }
  void Renderer::setIsResizing(bool isResizing) noexcept { pimpl_->setIsResizing(isResizing); }
}
