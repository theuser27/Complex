/*
  ==============================================================================

    OpenGlContainer.cpp
    Created: 8 Feb 2024 9:42:09pm
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlContainer.hpp"

#include "Framework/sync_primitives.hpp"
#include "Plugin/Renderer.hpp"
#include "OpenGlComponent.hpp"
#include "../Sections/MainInterface.hpp"

namespace Interface
{
  OpenGlContainer::OpenGlContainer(juce::String name) : BaseComponent{ COMPLEX_MOVE(name) } { }
  OpenGlContainer::~OpenGlContainer() noexcept = default;

  void OpenGlContainer::renderOpenGlComponents(OpenGlWrapper &openGl)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::SpinNotify };
    ScopedBoundsEmplace b{ openGl.parentStack, this };

    for (auto &openGlComponent : openGlComponents_)
      if (openGlComponent->isVisibleSafe() && !openGlComponent->isAlwaysOnTopSafe())
        openGlComponent->doWorkOnComponent(openGl);

    for (auto &openGlComponent : openGlComponents_)
      if (openGlComponent->isVisibleSafe() && openGlComponent->isAlwaysOnTopSafe())
        openGlComponent->doWorkOnComponent(openGl);
  }

  void OpenGlContainer::destroyAllOpenGlComponents()
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    for (auto &openGlComponent : openGlComponents_)
      openGlComponent->destroy();
  }

  void OpenGlContainer::addOpenGlComponent(OpenGlComponent *openGlComponent, bool toBeginning)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
    
    if (!openGlComponent)
      return;

    COMPLEX_ASSERT(std::ranges::find(openGlComponents_, openGlComponent) == openGlComponents_.end()
      && "We're adding a component that is already a child of this container");

    openGlComponent->setParentSafe(this);
    if (toBeginning)
      openGlComponents_.insert(openGlComponents_.begin(), openGlComponent);
    else
      openGlComponents_.emplace_back(openGlComponent);
    addAndMakeVisible(openGlComponent);
  }

  void OpenGlContainer::removeOpenGlComponent(OpenGlComponent *openGlComponent, bool removeChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
    
    if (openGlComponent == nullptr)
      return;

    if (std::erase_if(openGlComponents_, [&](const auto &value) { return value == openGlComponent; }))
    {
      removeChildComponent(openGlComponent);
      if (removeChild)
        openGlComponent->setParentSafe(nullptr);
    }
  }

  void OpenGlContainer::removeAllOpenGlComponents(bool removeChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    for (usize i = openGlComponents_.size(); i > 0; --i)
    {
      removeChildComponent(openGlComponents_[i - 1]);
      if (removeChild)
        openGlComponents_[i - 1]->setParentSafe(nullptr);
    }

    openGlComponents_.clear();
  }

  void OpenGlContainer::addSubOpenGlContainer(OpenGlContainer *container, bool addChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    container->setParentSafe(this);

    if (addChild)
      addAndMakeVisible(container);

    subContainers_.emplace_back(container);
  }

  void OpenGlContainer::removeSubOpenGlContainer(OpenGlContainer *container, bool removeChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    auto location = std::ranges::find(subContainers_, container);
    if (location != subContainers_.end())
      subContainers_.erase(location);

    if (removeChild)
      removeChildComponent(container);
  }

  void OpenGlContainer::removeAllSubOpenGlContainers(bool removeChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    for (usize i = subContainers_.size(); i > 0; --i)
    {
      removeChildComponent(subContainers_[i - 1]);
      if (removeChild)
        subContainers_[i - 1]->setParentSafe(nullptr);
    }

    subContainers_.clear();
  }

  void OpenGlContainer::showPopupSelector(const BaseComponent *source, Point<int> position,
    PopupItems options, std::function<void(int)> callback, std::function<void()> cancel, int minWidth) const
  {
    uiRelated.renderer->getGui()->popupSelector(source, position, COMPLEX_MOVE(options), getSectionOverride(),
      COMPLEX_MOVE(callback), COMPLEX_MOVE(cancel), minWidth);
  }

  void OpenGlContainer::showPopupSelector(const BaseComponent *source, Placement placement,
    PopupItems options, std::function<void(int)> callback, std::function<void()> cancel, int minWidth) const
  {
    uiRelated.renderer->getGui()->popupSelector(source, placement, COMPLEX_MOVE(options), getSectionOverride(),
      COMPLEX_MOVE(callback), COMPLEX_MOVE(cancel), minWidth);
  }

  void OpenGlContainer::hidePopupSelector() const { uiRelated.renderer->getGui()->hidePopupSelector(); }

  void OpenGlContainer::showPopupDisplay(BaseComponent *source,
    String text, Placement placement, bool primary) const
  {
    uiRelated.renderer->getGui()->popupDisplay(source, COMPLEX_MOVE(text), placement, primary, getSectionOverride());
  }

  void OpenGlContainer::hidePopupDisplay(bool primary) const { uiRelated.renderer->getGui()->hideDisplay(primary); }


  float OpenGlContainer::getValue(Skin::ValueId valueId) const noexcept { return uiRelated.skin->getValue(this, valueId); }
  float OpenGlContainer::getValue(Skin::SectionOverride skinOverride, Skin::ValueId valueId) const noexcept 
  { return uiRelated.skin->getValue(skinOverride, valueId); }
  juce::Colour OpenGlContainer::getColour(Skin::ColourId colorId) const noexcept { return juce::Colour{ uiRelated.skin->getColour(this, colorId) }; }
  juce::Colour OpenGlContainer::getColour(Skin::SectionOverride skinOverride, Skin::ColourId colorId) const noexcept 
  { return juce::Colour{ uiRelated.skin->getColour(skinOverride, colorId) }; }


  juce::Colour OpenGlContainer::getThemeColour() const
  { return Colour{ uiRelated.skin->getColour(this, Skin::kWidgetPrimary1) }; }
}
