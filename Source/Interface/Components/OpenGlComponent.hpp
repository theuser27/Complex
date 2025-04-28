/*
  ==============================================================================

    OpenGlComponent.hpp
    Created: 5 Dec 2022 11:55:29pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.hpp"
#include "../LookAndFeel/Shaders.hpp"
#include "../LookAndFeel/Skin.hpp"

namespace Interface
{
  using namespace juce;

  class OpenGlContainer;

  // NoWork - skip rendering on component
  // Dirty - render and update flag to NoWork
  // Realtime - render every frame
  enum class RenderFlag : u32 { NoWork = 0, Dirty = 1, Realtime = 2 };

  class Animator
  {
  public:
    enum EventType { Hover, Click };

    void tick(bool isAnimating = true) noexcept;
    float getValue(EventType type, float min = 0.0f, float max = 1.0f) const noexcept;

    void setHoverIncrement(float increment) noexcept
    { COMPLEX_ASSERT(increment > 0.0f); utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; hoverIncrement_ = increment; }

    void setClickIncrement(float increment) noexcept
    { COMPLEX_ASSERT(increment > 0.0f); utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; clickIncrement_ = increment; }

    void setIsHovered(bool isHovered) noexcept { utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; isHovered_ = isHovered; }
    void setIsClicked(bool isClicked) noexcept { utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin }; isClicked_ = isClicked; }

  private:
    mutable std::atomic<bool> guard_ = false;

    float hoverValue_ = 0.0f;
    float clickValue_ = 0.0f;

    float hoverIncrement_ = 1.0f;
    float clickIncrement_ = 1.0f;

    bool isHovered_ = false;
    bool isClicked_ = false;
  };

  void pushResourcesForDeletion(OpenGlAllocatedResource type, GLsizei n, GLuint id);

  class OpenGlComponent : public BaseComponent
  {
  public:
    OpenGlComponent(String name = "OpenGlComponent");
    ~OpenGlComponent() override;

    void setBounds(int x, int y, int width, int height) final { BaseComponent::setBounds(x, y, width, height); }
    using BaseComponent::setBounds;

    virtual void init(OpenGlWrapper &openGl) = 0;
    virtual void render(OpenGlWrapper &openGl) = 0;
    virtual void destroy() = 0;

    void doWorkOnComponent(OpenGlWrapper &openGl);

    Animator &getAnimator() noexcept { return animator_; }
    RenderFlag getRefreshFrequency() const noexcept { return renderFlag_; }
    
    // do not call these functions anywhere else but on the message thread
    float getValue(Skin::ValueId valueId, bool isScaled = true) const;
    Colour getColour(Skin::ColourId colourId) const;

    void setRefreshFrequency(RenderFlag frequency) noexcept { renderFlag_ = frequency; }
    void setRenderFunction(utils::small_fn<void(OpenGlWrapper &, OpenGlComponent &)> function) noexcept
    { renderFunction_ = COMPLEX_MOVE(function); }
    void setIgnoreClip(BaseComponent *ignoreClipIncluding) noexcept;

  protected:
    Animator animator_{};
    utils::shared_value<utils::small_fn<void(OpenGlWrapper &, OpenGlComponent &)>> renderFunction_{};
    utils::shared_value<RenderFlag> renderFlag_ = RenderFlag::Dirty;
    utils::shared_value<BaseComponent *> ignoreClipIncluding_ = nullptr;
    std::atomic<bool> isInitialised_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
  };
}
