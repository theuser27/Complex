/*
  ==============================================================================

    OpenGlContainer.hpp
    Created: 8 Feb 2024 9:42:09pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "../LookAndFeel/Shaders.hpp"
#include "../LookAndFeel/BaseComponent.hpp"
#include "../LookAndFeel/Skin.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"

namespace Interface
{
  class Renderer;
  class OpenGlComponent;

  class OpenGlContainer : public BaseComponent
  {
  public:
    OpenGlContainer(juce::String name = {});
    ~OpenGlContainer() noexcept override;

    virtual void renderOpenGlComponents(OpenGlWrapper &openGl);
    virtual void destroyAllOpenGlComponents();

    void addOpenGlComponent(OpenGlComponent *openGlComponent, bool toBeginning = false);
    void removeOpenGlComponent(OpenGlComponent *openGlComponent, bool removeChild = false);
    void removeAllOpenGlComponents(bool removeChild = false);

    virtual void addSubOpenGlContainer(OpenGlContainer *container, bool addChild = true);
    virtual void removeSubOpenGlContainer(OpenGlContainer *container, bool removeChild = false);
    void removeAllSubOpenGlContainers(bool removeChild = false);

    void showPopupSelector(const BaseComponent *source, juce::Point<int> position, PopupItems options,
      utils::dyn_fn<void(int)> callback, utils::dyn_fn<void()> cancel = {}, int minWidth = kMinPopupWidth) const;
    void showPopupSelector(const BaseComponent *source, Placement placement, PopupItems options,
      utils::dyn_fn<void(int)> callback, utils::dyn_fn<void()> cancel = {}, int minWidth = kMinPopupWidth) const;
    // ReSharper disable once CppHiddenFunction
    void hidePopupSelector() const;
    void showPopupDisplay(BaseComponent *source, juce::String text, Placement placement, bool primary) const;
    void hidePopupDisplay(bool primary) const;

    auto getSectionOverride() const noexcept { return skinOverride_; }
    float getValue(Skin::ValueId valueId) const noexcept;
    float getValue(Skin::SectionOverride skinOverride, Skin::ValueId valueId) const noexcept;
    juce::Colour getColour(Skin::ColourId colorId) const noexcept;
    juce::Colour getColour(Skin::SectionOverride skinOverride, Skin::ColourId colorId) const noexcept;

    virtual juce::Colour getThemeColour() const;
    virtual Shape getIcon() const { return {}; }

    virtual void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
  protected:
    std::vector<OpenGlComponent *> openGlComponents_{};
    std::vector<OpenGlContainer *> subContainers_{};

    utils::shared_value<Skin::SectionOverride> skinOverride_ = Skin::kNone;

    std::atomic<bool> isRendering_ = false;
  };
}
