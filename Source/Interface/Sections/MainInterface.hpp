/*
  ==============================================================================

    MainInterface.hpp
    Created: 10 Oct 2022 6:02:52pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "BaseSection.hpp"

namespace Interface
{
  class AboutSection;
  class DeleteSection;
  class ExtraModSection;
  class HeaderFooterSections;
  class EffectsStateSection;
  class MasterControlsInterface;
  class ModulationInterface;
  class ModulationManager;
  class PresetBrowser;
  class SaveSection;
  class PopupDisplay;
  class PopupSelector;

  class MainInterface final : public BaseSection, public juce::DragAndDropContainer
  {
  public:
    MainInterface();
    ~MainInterface() override;

    void resized() override;
    void parentHierarchyChanged() override { }

    void renderOpenGlComponents(OpenGlWrapper &openGl) override;

    void notifyChange();
    void notifyFresh();
    void popupSelector(const BaseComponent *source, juce::Point<int> position, PopupItems options,
      Skin::SectionOverride skinOverride, std::function<void(int)> callback, 
      std::function<void()> cancel = {}, int minWidth = kMinPopupWidth);
    void popupSelector(const BaseComponent *source, Placement placement, PopupItems options,
      Skin::SectionOverride skinOverride, std::function<void(int)> callback, 
      std::function<void()> cancel = {}, int minWidth = kMinPopupWidth);
    void hidePopupSelector();
    void popupDisplay(BaseComponent *source, juce::String text, Placement placement, bool primary, 
      Skin::SectionOverride sectionOverride = Skin::kPopupBrowser);
    void hideDisplay(bool primary);

    void reinstantiateUI();

  private:
    utils::up<HeaderFooterSections> headerFooter_;
    utils::up<EffectsStateSection> effectsStateSection_;
    utils::up<PopupSelector> popupSelector_;
    utils::up<PopupDisplay> popupDisplay1_;
    utils::up<PopupDisplay> popupDisplay2_;

    utils::shared_value<juce::Colour> backgroundColour_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainInterface)
  };
}
