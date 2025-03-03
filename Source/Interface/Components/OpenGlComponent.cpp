/*
  ==============================================================================

    OpenGLComponent.cpp
    Created: 5 Dec 2022 11:55:29pm
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlComponent.hpp"

#include <juce_opengl/opengl/juce_gl.h>

#include "Framework/sync_primitives.hpp"
#include "Plugin/Renderer.hpp"
#include "../Sections/MainInterface.hpp"

namespace Interface
{
  using namespace juce::gl;

  bool setViewPort(const BaseComponent &target, const OpenGlComponent &renderSource, juce::Rectangle<int> viewportBounds,
    juce::Rectangle<int> scissorBounds, const OpenGlWrapper &openGl, const BaseComponent *ignoreClipIncluding)
  {
    auto findIndex = [](const std::vector<ViewportChange> &vector, const BaseComponent *target)
    {
      isize index;
      for (index = vector.size() - 1; index >= 0; --index)
        if (vector[index].component == target)
          break;
      return index;
    };

    isize startingIndex = (&target == static_cast<const BaseComponent *>(&renderSource)) ?
      (isize)openGl.parentStack.size() : findIndex(openGl.parentStack, &target);
    COMPLEX_ASSERT(startingIndex >= 0, "Render target is not a parent of the rendering component");

    // if target is MainInterface
    if (startingIndex != 0)
    {
      isize clippingIndex = (!ignoreClipIncluding) ?
        (isize)openGl.parentStack.size() : findIndex(openGl.parentStack, ignoreClipIncluding);
      COMPLEX_ASSERT(clippingIndex > 0, "Clipping target not found");

      viewportBounds += target.getPositionSafe();
      scissorBounds += target.getPositionSafe();

      for (isize i = startingIndex - 1; i > 0; --i)
      {
        auto [_, bounds, isClipping] = openGl.parentStack[i];

        if (isClipping && i < clippingIndex)
          bounds.withZeroOrigin().intersectRectangle(scissorBounds);

        viewportBounds = viewportBounds + bounds.getPosition();
        scissorBounds = scissorBounds + bounds.getPosition();
      }
    }

    if (scissorBounds.getWidth() <= 0 || scissorBounds.getHeight() <= 0)
      return false;

    glViewport(viewportBounds.getX(), openGl.topLevelHeight - viewportBounds.getBottom(),
      viewportBounds.getWidth(), viewportBounds.getHeight());

    glScissor(scissorBounds.getX(), openGl.topLevelHeight - scissorBounds.getBottom(),
      scissorBounds.getWidth(), scissorBounds.getHeight());

    return true;
  }

  void pushResourcesForDeletion(OpenGlAllocatedResource type, GLsizei n, GLuint id)
  { uiRelated.renderer->pushOpenGlResourceToDelete(type, n, id); }

  OpenGlComponent::OpenGlComponent(String name) : BaseComponent{ COMPLEX_MOVE(name) } { }
  OpenGlComponent::~OpenGlComponent() = default;

  void OpenGlComponent::doWorkOnComponent(OpenGlWrapper &openGl)
  {
    if (!isInitialised_.load(std::memory_order_acquire))
    {
      init(openGl);
      COMPLEX_ASSERT(isInitialised_.load(std::memory_order_relaxed), "Init method didn't set flag");
    }

    auto customRenderFunction = renderFunction_.get();
    if (customRenderFunction)
      customRenderFunction(openGl, *this);
    else
      render(openGl);

    COMPLEX_CHECK_OPENGL_ERROR;
  }

  float OpenGlComponent::getValue(Skin::ValueId valueId, bool isScaled) const
  {
    if (auto *skin = uiRelated.skin)
    {
      auto value = skin->getValue(valueId);
      return (isScaled) ? scaleValue(value) : value;
    }

    return 0.0f;
  }

  Colour OpenGlComponent::getColour(Skin::ColourId colourId) const
  {
    if (auto *skin = uiRelated.skin)
      return Colour{ skin->getColour(colourId) };

    return Colours::black;
  }

  void OpenGlComponent::setIgnoreClip(BaseComponent *ignoreClipIncluding) noexcept
  {
    COMPLEX_ASSERT(dynamic_cast<MainInterface *>(ignoreClipIncluding) == nullptr);
    ignoreClipIncluding_ = ignoreClipIncluding;
  }

  void Animator::tick(bool isAnimating) noexcept
  {
    utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin };

    if (isAnimating)
    {
      if (isHovered_)
        hoverValue_ = utils::min(1.0f, hoverValue_ + hoverIncrement_);
      else
        hoverValue_ = utils::max(0.0f, hoverValue_ - hoverIncrement_);

      if (isClicked_)
        clickValue_ = utils::min(1.0f, clickValue_ + clickIncrement_);
      else
        clickValue_ = utils::max(0.0f, clickValue_ - clickIncrement_);
    }
    else
    {
      hoverValue_ = isHovered_;
      clickValue_ = isClicked_;
    }
  }

  float Animator::getValue(EventType type, float min, float max) const noexcept
  {
    utils::ScopedLock g{ guard_, utils::WaitMechanism::Spin };

    float value;
    switch (type)
    {
    case Hover:
      value = hoverValue_;
      break;
    case Click:
      value = clickValue_;
      break;
    default:
      value = 1.0f;
      break;
    }

    value *= max - min;
    return value + min;
  }
}
