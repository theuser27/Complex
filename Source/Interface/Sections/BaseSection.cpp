/*
  ==============================================================================

    BaseSection.cpp
    Created: 9 Oct 2022 8:28:36pm
    Author:  theuser27

  ==============================================================================
*/

#include "BaseSection.hpp"

#include "Generation/BaseProcessor.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/OpenGlQuad.hpp"
#include "../Components/BaseButton.hpp"
#include "../Components/BaseSlider.hpp"

namespace Interface
{
  BaseSection::BaseSection(utils::string_view name) : OpenGlContainer({ name.data(), name.size() })
  {
    setWantsKeyboardFocus(true);
  }

  BaseSection::~BaseSection() noexcept = default;

  void BaseSection::resized()
  {
    if (background_)
      background_->setBounds(getLocalBounds());
  }

  void BaseSection::repaintBackground(juce::Rectangle<int> redrawArea)
  {
    if (!background_)
      createBackground();

    background_->setComponentToRedraw(this);
    background_->redrawImage(redrawArea);
  }

  void BaseSection::renderOpenGlComponents(OpenGlWrapper &openGl)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::SpinNotify };
    ScopedBoundsEmplace b{ openGl.parentStack, this };
    
    if (background_)
      background_->doWorkOnComponent(openGl);
    
    for (auto *subContainer : subContainers_)
      if (subContainer->isVisibleSafe() && !subContainer->isAlwaysOnTopSafe())
        subContainer->renderOpenGlComponents(openGl);

    for (auto &openGlComponent : openGlComponents_)
      if (openGlComponent->isVisibleSafe() && !openGlComponent->isAlwaysOnTopSafe())
        openGlComponent->doWorkOnComponent(openGl);

    for (auto &[controlName, control] : controls_)
      if (control->isVisibleSafe() && !control->isAlwaysOnTopSafe())
        control->renderOpenGlComponents(openGl);

    for (auto *subContainer : subContainers_)
      if (subContainer->isVisibleSafe() && subContainer->isAlwaysOnTopSafe())
        subContainer->renderOpenGlComponents(openGl);

    for (auto &openGlComponent : openGlComponents_)
      if (openGlComponent->isVisibleSafe() && openGlComponent->isAlwaysOnTopSafe())
        openGlComponent->doWorkOnComponent(openGl);

    for (auto &[controlName, control] : controls_)
      if (control->isVisibleSafe() && control->isAlwaysOnTopSafe())
        control->renderOpenGlComponents(openGl);
  }

  void BaseSection::destroyAllOpenGlComponents()
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
    
    if (background_)
      background_->destroy();

    for (auto &openGlComponent : openGlComponents_)
      openGlComponent->destroy();

    for (auto &[controlName, control] : controls_)
      control->destroyAllOpenGlComponents();

    for (auto *subContainer : subContainers_)
      subContainer->destroyAllOpenGlComponents();
  }

  void BaseSection::addControl(BaseControl *control)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
    
    // juce String is a copy-on-write type so the internal data is constant across all instances of the same string
    // this means that so long as the control's name isn't changed the string_view will be safe to access 
    if (control->getParameterDetails().id.empty())
    {
      COMPLEX_ASSERT(!control->getName().isEmpty() && control->getName() != "" && "Every control must have a name");
      controls_[control->getName().toRawUTF8()] = control;
    }
    else
      controls_[control->getParameterDetails().id] = control;
    
    control->setParentSafe(this);
    control->addListener(this);
    control->setSkinOverride(skinOverride_);

    addAndMakeVisible(control);
  }

  void BaseSection::removeControl(BaseControl *control, bool removeChild)
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
    
    removeChildComponent(control);
    if (removeChild)
      control->setParentSafe(nullptr);
    control->removeListener(this);

    if (control->getParameterDetails().id.empty())
      controls_.erase(control->getName().toRawUTF8());
    else
      controls_.erase(control->getParameterDetails().id);
  }

  void BaseSection::setSkinOverride(Skin::SectionOverride skinOverride) noexcept
  {
    skinOverride_ = skinOverride;
    for (auto &control : controls_)
      control.second->setSkinOverride(skinOverride);
  }

  void BaseSection::createBackground()
  {
    utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

    background_ = utils::up<OpenGlBackground>::create();
    background_->setTargetComponent(this);
    background_->setParentSafe(this);
    addAndMakeVisible(background_.get());
    background_->setBounds({ 0, 0, getWidth(), getHeight() });
  }

  void BaseSection::updateAllValues()
  {
    for (auto &control : controls_)
      control.second->setValueFromParameter();

    for (auto *subContainer : subContainers_)
      if (auto subSection = dynamic_cast<BaseSection *>(subContainer))
        subSection->updateAllValues();
  }

  class OffOverlayQuad final : public OpenGlMultiQuad
  {
  public:
    OffOverlayQuad() : OpenGlMultiQuad{ 1, Shaders::kColorFragment, "OffOverlayQuad" }
    { getQuadData().setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f); }
  };

  ProcessorSection::ProcessorSection(utils::string_view name, Generation::BaseProcessor *processor) :
    BaseSection{ name }, processor_{ processor } { }

  ProcessorSection::~ProcessorSection() noexcept = default;

  void ProcessorSection::resized()
  {
    if (offOverlayQuad_)
    {
      offOverlayQuad_->setBounds(getLocalBounds());
      offOverlayQuad_->setColor(getColour(Skin::kBackground).withMultipliedAlpha(0.8f));
    }
    if (activator_)
      activator_->setBounds(getPowerButtonBounds());

    BaseSection::resized();
  }

  void ProcessorSection::controlValueChanged(BaseControl *control)
  {
    if (control == activator_)
    {
      bool active = activator_->getValue();
      if (offOverlayQuad_)
        offOverlayQuad_->setVisible(!active);

      active_ = active;
      for (auto &[name, controlPointer] : controls_)
        if (auto *slider = dynamic_cast<BaseSlider *>(controlPointer))
          slider->setActive(active);

      if (background_)
        repaintBackground();
    }
  }

  void ProcessorSection::setActivator(PowerButton *activator)
  {
    createOffOverlay();

    activator_ = activator;
    activator->addListener(this);
    setActive(activator->getValue());
  }

  void ProcessorSection::createOffOverlay()
  {
    if (offOverlayQuad_)
      return;

    offOverlayQuad_ = utils::up<OffOverlayQuad>::create();
    addOpenGlComponent(offOverlayQuad_.get());
    offOverlayQuad_->setVisible(false);
    offOverlayQuad_->setAlwaysOnTop(true);
    offOverlayQuad_->setInterceptsMouseClicks(false, false);
  }

  std::optional<u64> ProcessorSection::getProcessorId() const noexcept
  { return (processor_) ? processor_->getProcessorId() : std::optional<u64>{}; }

  void ProcessorSection::setActive(bool active)
  {
    if (active_ == active)
      return;

    if (offOverlayQuad_)
      offOverlayQuad_->setVisible(!active);

    active_ = active;
    for (auto &control : controls_)
      if (auto *slider = dynamic_cast<BaseSlider *>(control.second))
        slider->setActive(active);

    if (background_)
      repaintBackground();
  }

}
