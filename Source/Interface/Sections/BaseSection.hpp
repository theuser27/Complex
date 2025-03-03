/*
  ==============================================================================

    BaseSection.hpp
    Created: 9 Oct 2022 8:28:36pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../Components/OpenGlContainer.hpp"

namespace Generation
{
  class BaseProcessor;
}

namespace Interface
{
  class BaseControl;
  class Renderer;
  class PowerButton;
  class OpenGlBackground;
  class OffOverlayQuad;

  class BaseSection : public OpenGlContainer, public ControlListener
  {
  public:
    BaseSection(utils::string_view name);
    ~BaseSection() noexcept override;

    void setBounds(int x, int y, int width, int height) final { BaseComponent::setBounds(x, y, width, height); }
    using OpenGlContainer::setBounds;
    void resized() override;
    void controlValueChanged(BaseControl *) override { }

    // paint anything that doesn't move/is static
    virtual void paintBackground(juce::Graphics &) { }
    virtual void repaintBackground(juce::Rectangle<int> redrawArea = {});
    
    // main opengl render loop
    void renderOpenGlComponents(OpenGlWrapper &openGl) override;
    void destroyAllOpenGlComponents() final;

    virtual void updateAllValues();

    void addControl(BaseControl *control);
    void removeControl(BaseControl *control, bool removeChild = false);
    BaseControl *getControl(utils::string_view id) { return controls_.at(id); }

    void setSkinOverride(Skin::SectionOverride skinOverride) noexcept final;

  protected:
    void createBackground();

    utils::up<OpenGlBackground> background_;

    std::map<utils::string_view, BaseControl *> controls_{};
  };

  class ProcessorSection : public BaseSection
  {
  public:
    static constexpr int kDefaultActivatorSize = 12;

    ProcessorSection(utils::string_view name, Generation::BaseProcessor *processor);
    ~ProcessorSection() noexcept override;

    void resized() override;

    void controlValueChanged(BaseControl *control) override;

    std::optional<u64> getProcessorId() const noexcept;
    auto getProcessor() const noexcept { return processor_; }
    bool isActive() const noexcept { return active_; }

    virtual void setActive(bool active);
    virtual juce::Rectangle<int> getPowerButtonBounds() const noexcept
    {
      int activatorSize = scaleValueRoundInt(kDefaultActivatorSize);
      return { 0, 0, activatorSize, activatorSize };
    }

  protected:
    void setActivator(PowerButton *activator);
    void createOffOverlay();

    Generation::BaseProcessor *processor_ = nullptr;
    utils::up<OffOverlayQuad> offOverlayQuad_;
    PowerButton *activator_ = nullptr;
    bool active_ = true;
  };
}
