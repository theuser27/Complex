/*
  ==============================================================================

    MainInterface.cpp
    Created: 10 Oct 2022 6:02:52pm
    Author:  theuser27

  ==============================================================================
*/

#include "MainInterface.hpp"

#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "Popups.hpp"
#include "HeaderFooterSections.hpp"
#include "EffectsStateSection.hpp"

namespace Interface
{
  MainInterface::MainInterface() : BaseSection{ "Main Interface" }
  {
    reinstantiateUI();

    setOpaque(false);
  }

  MainInterface::~MainInterface() = default;

  void MainInterface::resized()
  {
    backgroundColour_ = getColour(Skin::kBackground);

    int width = getWidth();
    int height = getHeight();

    popupSelector_->setBounds(0, 0, width, height);
    headerFooter_->setBounds(0, 0, width, height);

    int effectsStateXStart = scaleValueRoundInt(kHWindowEdgeMargin);
    int effectsStateYStart = scaleValueRoundInt(kHeaderHeight +
      kMainVisualiserHeight + kVGlobalMargin);
    int effectsStateWidth = scaleValueRoundInt(kEffectsStateMinWidth);
    int effectsStateHeight = getHeight() - effectsStateYStart - 
      scaleValueRoundInt(kLaneToBottomSettingsMargin + kFooterHeight);
    auto bounds = juce::Rectangle{ effectsStateXStart, effectsStateYStart,
      effectsStateWidth, effectsStateHeight };

    effectsStateSection_->setBounds(bounds);
  }

  void MainInterface::renderOpenGlComponents(OpenGlWrapper &openGl)
  {
    auto backgroundColour = backgroundColour_.get();
    juce::gl::glClearColor(backgroundColour.getFloatRed(), backgroundColour.getFloatGreen(), 
      backgroundColour.getFloatBlue(), 1.0f);
    juce::gl::glClear(juce::gl::GL_COLOR_BUFFER_BIT);

    COMPLEX_ASSERT(openGl.parentStack.empty());

    openGl.parentStack.emplace_back(this, getBoundsSafe(), true);
    openGl.parentStack.emplace_back(ScopedBoundsEmplace::doNotAddFlag);
    openGl.topLevelHeight = getHeightSafe();

    BaseSection::renderOpenGlComponents(openGl);

    openGl.parentStack.pop_back();
  }

  void MainInterface::popupSelector(const BaseComponent *source, juce::Point<int> position, PopupItems options, 
    Skin::SectionOverride skinOverride, utils::dyn_fn<void(int)> callback, utils::dyn_fn<void()> cancel, int minWidth)
  {
    popupSelector_->setPopupSkinOverride(skinOverride);
    popupSelector_->setComponent(source);
    popupSelector_->setCallback(COMPLEX_MOVE(callback));
    popupSelector_->setCancelCallback(COMPLEX_MOVE(cancel));
    popupSelector_->setItems(COMPLEX_MOVE(options), minWidth);
    popupSelector_->positionList(getLocalPoint(source, position));
    popupSelector_->setVisible(true);
  }

  void MainInterface::popupSelector(const BaseComponent *source, Placement placement, PopupItems options,
    Skin::SectionOverride skinOverride, utils::dyn_fn<void(int)> callback, utils::dyn_fn<void()> cancel, int minWidth)
  {
    popupSelector_->setPopupSkinOverride(skinOverride);
    popupSelector_->setComponent(source);
    popupSelector_->setCallback(COMPLEX_MOVE(callback));
    popupSelector_->setCancelCallback(COMPLEX_MOVE(cancel));
    popupSelector_->setItems(COMPLEX_MOVE(options), minWidth);
    popupSelector_->positionList(getLocalArea(source, source->getLocalBounds()), placement);
    popupSelector_->setVisible(true);
  }

  void MainInterface::hidePopupSelector()
  {
    popupSelector_->setVisible(false);
    popupSelector_->setPopupSkinOverride(Skin::kNone);
    popupSelector_->setComponent(nullptr);
    popupSelector_->setCallback({});
    popupSelector_->setCancelCallback({});
    popupSelector_->resetState();
  }

  void MainInterface::popupDisplay(BaseComponent *source, juce::String text,
    Placement placement, bool primary, Skin::SectionOverride sectionOverride)
  {
    PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
    display->setContent(COMPLEX_MOVE(text), getLocalArea(source, source->getLocalBounds()),
      getBoundsSafe(), placement, sectionOverride);
    display->setVisible(true);
  }

  void MainInterface::hideDisplay(bool primary)
  {
    PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
    display->setVisible(false);
  }

  void MainInterface::reinstantiateUI()
  {
    removeAllSubOpenGlContainers(false);

    headerFooter_ = utils::up<HeaderFooterSections>::create(uiRelated.renderer->getPlugin().getSoundEngine());
    addSubOpenGlContainer(headerFooter_.get());

    effectsStateSection_ = utils::up<EffectsStateSection>::create(uiRelated.renderer->getPlugin().getEffectsState());
    addSubOpenGlContainer(effectsStateSection_.get());

    popupSelector_ = utils::up<PopupSelector>::create();
    addSubOpenGlContainer(popupSelector_.get());
    popupSelector_->setVisible(false);
    popupSelector_->setAlwaysOnTop(true);
    popupSelector_->setWantsKeyboardFocus(true);

    popupDisplay1_ = utils::up<PopupDisplay>::create();
    addSubOpenGlContainer(popupDisplay1_.get());
    popupDisplay1_->setVisible(false);
    popupDisplay1_->setAlwaysOnTop(true);
    popupDisplay1_->setWantsKeyboardFocus(false);

    popupDisplay2_ = utils::up<PopupDisplay>::create();
    addSubOpenGlContainer(popupDisplay2_.get());
    popupDisplay2_->setVisible(false);
    popupDisplay2_->setAlwaysOnTop(true);
    popupDisplay2_->setWantsKeyboardFocus(false);

    popupSelector_->toFront(true);
    popupDisplay1_->toFront(true);
    popupDisplay2_->toFront(true);
  }
}
