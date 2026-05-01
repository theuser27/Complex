
// Created: 2023-02-03 18:42:55

#include "EffectsLaneSection.hpp"

#include "Generation/Effects.hpp"
#include "Plugin/Complex.hpp"
#include "Plugin/Renderer.hpp"
#include "../Components/Control.hpp"
#include "MainInterface.hpp"
#include "EffectModuleSection.hpp"

namespace Interface
{
  bool 
  EffectsLaneSection::AddModulesButton::mouseEnter(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::PointingHand);
    return true;
  }

  bool 
  EffectsLaneSection::AddModulesButton::mouseExit(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
    return true;
  }

  bool
  EffectsLaneSection::AddModulesButton::mouseDown(const MouseEvent &e)
  {
    using namespace Framework;

    if (e.mods.test(ModifierKeys::altModifier) || e.mods.test(ModifierKeys::popupMenuClickModifier))
      return true;

    auto *selector = getPopupSelector();

    if (isDropdownOpen)
    {
      selector->resetState();
      isDropdownOpen = false;
      return true;
    }

    selector->resetState();
    isDropdownOpen = true;

    // find the module options in the state
    IndexedData *indexedData{};
    ProcessorMetadata::visit(laneSection->effectsLane->state->pluginStructure.metadata,
      [&indexedData](ProcessorMetadata &metadata)
      {
        if (metadata.id != Generation::Processors::EffectModule)
          return false;

        ParameterMetadata *parameter = metadata.parameters;
        for (usize i = 0; i < metadata.parametersCount; (++i), (parameter = parameter->next))
          if (parameter->details.id == Generation::EffectModule::ModuleType)
            break;

        indexedData = parameter->details.options;

        return true;
      });

    PopupList *list = anew(selector->arena, PopupList, { selector });
    list->parentSelector = selector;

    Framework::iterateOverIndexedData(indexedData,
      [list](Framework::IndexedData &option)
      {
        if (option.childrenCount || option.canBeChosen())
          list->addChildComponent(OptionPopupItem::createOption(list, option));

        return false;
      });

    selector->list = list;
    selector->skinOverride = getSkinOverride();
    selector->callback = [this](PopupSelector *, PopupItem *selectedItem)
    {
      isDropdownOpen = false;
      auto *option = (Framework::IndexedData *)selectedItem->extraData;
      COMPLEX_ASSERT(option->processorMetadata);

      auto *state = laneSection->effectsLane->state;
      auto *effectModule = (Generation::EffectModule *)state->createProcessor(
        Generation::Processors::EffectModule);
      effectModule->changeEffect(option);

      {
        auto *moduleTypeParameter = effectModule->getParameter(Generation::EffectModule::ModuleType);
        auto details = moduleTypeParameter->getParameterDetails();
        auto normalisedValue = (float)Framework::unscaleValue(getValueFromOptionId(option->id, details), details);
        moduleTypeParameter->updateNormalisedValue(&normalisedValue);
      }      

      auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
      state->plugin->undoManager.perform(anew(transactionArena, AddProcessorUpdate, { effectModule,
        laneSection->effectsLane->stateId, laneSection->effectsLane->childrenCount }));
    };
    selector->cancel = [this](PopupSelector *) { isDropdownOpen = false; };
    selector->summon(this, Placement::bottom, { 0, 4 });

    componentFlags.isClicked = false;

    return true;
  }

  bool
  EffectsLaneSection::AddModulesButton::render(OpenGlWrapper &openGl)
  {
    static constexpr auto kHoverIncrement = 0.1f;
    tickAnimation(animationValues,
      {{ componentFlags.isHovered || isDropdownOpen }},
      {{ kHoverIncrement }});
    
    strokeRect(openGl, getLocalBounds().toFloat(), 1.0f, 
      getColour(Skin::kBody, this).withAlpha(animationValues[0]), 
      scaleValue(kBorderRounding));

    return true;
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
    removeCommandMessageHandler(laneHandler);

    componentFlags.vertical = true;
    skinOverride = Skin::kEffectsLane;
    sizingFlags = Component::GrowableY;
    desiredSize = { kEffectsLaneWidth, 0, kEffectsLaneWidth, 0 };

    addCommandMessageHandler(laneHandler);
    laneHandler.object = [](Component *c, u64 commandId, void *extraData)
    {
      auto *self = (EffectsLaneSection *)c;
      switch (commandId)
      {
      case CommandMessages::HandleAutoscroll:
      {
        auto &data = *(CommandMessages::Autoscroll *)extraData;
        if (data.handleY)
        {
          data.handleY = false;

          Point<i32> position = data.position;
          auto offset = utils::max(0, kAutoScrollRegion - utils::min(position.y, self->bounds.h - position.y));
          self->autoScrollIncrements.y = (i8)((position.y < self->bounds.h - position.y) ? -offset : offset);
        }

        return true;
      }
      case CommandMessages::HandleProcessorInsertion:
      {
        auto *metadata = (CommandMessages::ProcessorInsertion *)extraData;
        auto *placeholderInsert = &getGui(uiRelated.renderer)->placeholderInsert;

        if (!CommandMessages::handleProcessorInsertion(self->effectsLane, 
          &self->moduleHolder, metadata, Placement::top))
          return false;

        auto section = (EffectModuleSection *)metadata->processor->component;

        placeholderInsert->source = section;

        section->laneSection = self;
        section->componentFlags.animateMovement = true;
        section->previousPosition = invalidPosition;
        if (metadata->useIndex)
          section->effectHolder.header.draggableBox.surfaceToLiftTo = 
            &self->soundEngineSection->effectsSection;
        (metadata->placeholder ? metadata->placeholder : section)->margin = 
          { 0, 0, 0, kVModuleToModuleMargin };
        
        // moving the button to the back
        self->moduleHolder.removeChildComponent(&self->addModulesButton);
        self->moduleHolder.addChildComponent(&self->addModulesButton);

        return true;
      }
      }

      return false;
    };

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
    effectsLane->name.copy(laneTitle.text);
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
    moduleHolder.componentFlags.vertical = true;
    moduleHolder.componentFlags.clickable = true;
    moduleHolder.padding = { kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin, kHVModuleToLaneMargin };
    moduleHolder.draw = [](OpenGlWrapper &openGl, Component *, Component *self, Point<i32>)
    {
      fillRect(openGl, self->getLocalBounds().toFloat(), 
        getColour(Skin::kBackground, self), scaleValue(kInsideRouding));

      self->renderScrollbars(openGl, 0.2f);

      return true;
    };

    moduleHolder.addChildComponent(&addModulesButton);
    addModulesButton.componentFlags.clickable = true;
    addModulesButton.sizingFlags = Component::GrowableX;
    addModulesButton.placement = Placement::top;
    addModulesButton.laneSection = this;
    addModulesButton.desiredSize = { 0, kAddModuleButtonHeight, 0, kAddModuleButtonHeight };

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

  bool
  EffectsLaneSection::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this), scaleValue(kInsideRouding));
    strokeRect(openGl, getLocalBounds().toFloat(), scaleValue(kEffectsLaneOutlineThickness),
      Colour{ 45, 45, 45 }, scaleValue(kInsideRouding));

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
  }
}
