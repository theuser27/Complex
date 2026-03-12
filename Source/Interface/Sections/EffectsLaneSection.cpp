
// Created: 2023-02-03 18:42:55

#include "EffectsLaneSection.hpp"

#include "Generation/EffectsLane.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../Components/BaseControl.hpp"
#include "MainInterface.hpp"
//#include "EffectModuleSection.hpp"

namespace Interface
{
  bool 
  EffectsLaneSection::ModuleHolder::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBackground, this), scaleValue(kInsideRouding));

    return true;
  }

  bool 
  EffectsLaneSection::AddModulesButton::mouseDown(const MouseEvent &e)
  {
    // TODO: create popup

    return true;
  }

  bool 
  EffectsLaneSection::AddModulesButton::render(OpenGlWrapper &openGl)
  {
    strokeRect(openGl, getLocalBounds().toFloat(), 1.0f, getColour(Skin::kBody), scaleValue(kBorderRounding));

    //renderText("Add Modules", FontId::InterType, );

    return true;
  }

  bool 
  EffectsLaneSection::AddModulesButton::handleCommandMessage(u64 commandId, utils::whatever<64> extraData)
  {
    switch (commandId)
    {
    case Component::HandleCustomPosition:

      // TODO:

      return true;
    default:
      break;
    }

    return false;
  }

  void EffectsLaneSection::reinitialise()
  {
    static constexpr auto kHeaderLeftPadding = 12;
    static constexpr auto kHeaderRightPadding = 4;
    static constexpr auto kFooterPadding = 8;

    moduleHolder.deleteAllChildComponents();
    header.removeAllChildComponents();
    footer.removeAllChildComponents();
    removeAllChildComponents();

    componentFlags.vertical = true;
    skinOverride = Skin::kEffectsLane;
    sizingFlags = Component::GrowableY;
    desiredSize = { kEffectsLaneWidth, 0, kEffectsLaneWidth, 0 };

    if (!arena)
      arena = utils::bumpArena::createNested(utils::bumpArena::fromAllocation(this), COMPLEX_KB(1));    

    addChildComponent(&header);
    header.desiredSize = { 0, kEffectsLaneTopBarHeight, 0, kEffectsLaneTopBarHeight };
    header.sizingFlags = Component::GrowableX;
    header.placement = Placement::top;
    header.padding = { kHeaderLeftPadding, 0, kHeaderRightPadding, 0 };

    header.addChildComponent(&laneTitle);
    laneTitle.placement = Placement::left;
    laneTitle.text = { arena, "Lane A" };
    laneTitle.textColour = Skin::kWidgetAccent1;
    header.addChildComponent(&inputSelector);
    inputSelector.placement = Placement::right;
    inputSelector.prefix = { arena, "From: " };
    inputSelector.arena = arena;
    inputSelector.text.font = FontId::InterType;
    inputSelector.changeLinkedParameter(*effectsLane->getParameter(Generation::EffectsLane::Input));
    header.addChildComponent(&laneActivator);
    laneActivator.placement = Placement::right;
    laneActivator.margin = { 4, 0, 0, 0 };
    laneActivator.desiredSize = { kDefaultActivatorSize, kDefaultActivatorSize, kDefaultActivatorSize, kDefaultActivatorSize };
    laneActivator.changeLinkedParameter(*effectsLane->getParameter(Generation::EffectsLane::LaneEnabled));


    addChildComponent(&moduleHolder);
    moduleHolder.placement = Placement::top;
    moduleHolder.margin = { kEffectsLaneOutlineThickness, 0, kEffectsLaneOutlineThickness, 0 };
    moduleHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY | Component::ScrollableWithBarY);
    moduleHolder.padding = { kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin };

    addModulesButton.placement = Placement::custom;
    addModulesButton.sizingFlags = Component::GrowableX;
    //moduleHolder.addChildComponent(&addModulesButton);


    addChildComponent(&footer);
    footer.desiredSize = { 0, kEffectsLaneBottomBarHeight, 0, kEffectsLaneBottomBarHeight };
    footer.padding = { kFooterPadding, 0, kFooterPadding, 0 };
    footer.sizingFlags = Component::GrowableX;
    footer.placement = Placement::bottom;

    footer.addChildComponent(&gainMatchingButton);
    gainMatchingButton.placement = Placement::left;
    gainMatchingButton.arena = arena;
    gainMatchingButton.changeLinkedParameter(*effectsLane->getParameter(Generation::EffectsLane::GainMatching));
    footer.addChildComponent(&gainMatchingButtonLabel);
    gainMatchingButtonLabel.placement = Placement::left;
    gainMatchingButtonLabel.control = &gainMatchingButton;
    gainMatchingButtonLabel.margin = { Control::kLabelMargin, 0, 0, 0 };
    footer.addChildComponent(&outputSelector);
    outputSelector.placement = Placement::right;
    outputSelector.popupPlacement = Placement::top;
    outputSelector.prefix = { arena, "To: " };
    outputSelector.arena = arena;
    outputSelector.text.font = FontId::InterType;
    outputSelector.changeLinkedParameter(*effectsLane->getParameter(Generation::EffectsLane::Output));
  }

  void EffectsLaneSection::destroy()
  {
    moduleHolder.deleteAllChildComponents();
    removeAllChildComponents();

    utils::bumpArena::destroy(arena);
    arena = nullptr;

    auto *state = effectsLane->state;
    auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
    state->plugin->undoManager.perform(anew(transactionArena, Framework::DeleteProcessorUpdate, { effectsLane }));
  }

  //EffectsLaneSection *
  //EffectsLaneSection::createCopy(utils::bumpArena *arenaToUse)
  //{
  //	Generation::BaseProcessor *processorCopy;
  //	{
  //		auto g = effectsLane->state->plugin->acquireProcessingLock(false);
  //		processorCopy = effectsLane->createCopy();
  //	}
  //	auto *newLaneSection = (EffectsLaneSection *)processorCopy->createUI();
  //	newLaneSection->laneTitle.text.append(" - Copy");
  //  
  //	return newLaneSection;
  //}

  bool 
  EffectsLaneSection::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this), scaleValue(kInsideRouding));
    //reinitialise();
    //fillRect(openGl, getLocalBounds().toFloat());

    doRenderChildren(openGl);

    if (!laneActivator.isOn())
    {
      fillRect(openGl, getLocalBounds().toFloat(),
        getColour(Skin::kBackground, this).withMultipliedAlpha(0.8f));
    }

    return false;
  }
}

namespace Generation
{
  Interface::Component *
  EffectsLane::createUI()
  {
    auto guiArena = Interface::getGui(Interface::uiRelated.renderer)->arena;
    auto *effectsLaneSection = anew(guiArena, Interface::EffectsLaneSection, {});
    effectsLaneSection->effectsLane = this;
    effectsLaneSection->reinitialise();
    return effectsLaneSection;
    //return nullptr;
  }
}
