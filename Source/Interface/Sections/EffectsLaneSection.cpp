
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
  static bool 
  summonModulePopup(PopupSelector *selector, Component *summoner, 
    EffectsLaneSection *laneSection, Placement placement, Point<i32> offset, usize insertionIndex)
  {
    using namespace Framework;

    if (laneSection->isDropdownOpen)
    {
      selector->resetState();
      laneSection->isDropdownOpen = false;
      return false;
    }

    laneSection->isDropdownOpen = true;
    selector->resetState();

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
    selector->skinOverride = summoner->getSkinOverride();
    selector->callback = [laneSection, insertionIndex](PopupSelector *, PopupItem *selectedItem)
    {
      laneSection->isDropdownOpen = false;
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
        laneSection->effectsLane->stateId, insertionIndex }));
    };
    selector->cancel = [laneSection](PopupSelector *) { laneSection->isDropdownOpen = false; };
    selector->summon(summoner, placement, offset);

    summoner->componentFlags.isClicked = false;

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
        (metadata->placeholder ? metadata->placeholder : section)->margin = { 0, 0, 0, kVModuleToModuleMargin };

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
  EffectsLaneSection::ModuleHolder::mouseExit(const MouseEvent &)
  {
    setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
    hasEnteredHover = false;

    return true;
  }

  bool
  EffectsLaneSection::ModuleHolder::mouseMove(const MouseEvent &e)
  {
    auto laneSection = (EffectsLaneSection *)parent;
    auto localBounds = getLocalBounds();

    if (auto *lastChild = Generation::Processor::getChild(
      laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
    {
      auto newBottom = lastChild->component->bounds.getBottom() + 
        scaleValueRoundInt((float)lastChild->component->margin.h + kEffectModuleMinHeight);
      localBounds.h = utils::min(newBottom, localBounds.getBottom()) - localBounds.y;
    }
    else
    {
      localBounds.y = scaleValueRoundInt((float)padding.y);
      localBounds.h = scaleValueRoundInt(kEffectModuleMinHeight);
    }

    if (localBounds.contains(Point{ e.x, e.y }))
    {
      if (!hasEnteredHover)
      {
        enterHoverTime = uiRelated.steadyTime;
        hasEnteredHover = true;
      }

      // get the module below cursor
      auto *laneSeciton = (EffectsLaneSection *)parent;
      hoveredBeforeModuleIndex = 0;
      hoveredBeforeModule = laneSeciton->effectsLane->children;
      for (; hoveredBeforeModule && hoveredBeforeModuleIndex < laneSeciton->effectsLane->childrenCount;
        (++hoveredBeforeModuleIndex), (hoveredBeforeModule = hoveredBeforeModule->next))
      {
        if (hoveredBeforeModule->component->bounds.y > e.y)
          break;
      }
    }
    else if (hasEnteredHover)
    {
      setMouseCursor(uiRelated.renderer, MouseCursorTypes::Normal);
      hasEnteredHover = false;
    }

    return true;
  }

  bool 
  EffectsLaneSection::ModuleHolder::mouseDown(const MouseEvent &e)
  {
    if (Component::mouseDown(e))
      return true;

    auto laneSection = (EffectsLaneSection *)parent;
    if (!hasEnteredHover)
    {
      if (laneSection->isDropdownOpen)
        getPopupSelector()->resetState();

      return true;
    }

    double time = uiRelated.steadyTime;
    if (time - enterHoverTime < kTimeout)
      return true;

    if (e.mods.test(ModifierKeys::altModifier) || e.mods.test(ModifierKeys::popupMenuClickModifier))
      return true;

    summonModulePopup(getPopupSelector(), this, laneSection,
      Placement::custom, { e.x, e.y }, hoveredBeforeModuleIndex);

    return true;
  }

  bool 
  EffectsLaneSection::ModuleHolder::render(OpenGlWrapper &openGl)
  {
    static constexpr auto kHoverIncrement = 0.1f;

    fillRect(openGl, getLocalBounds().toFloat(),
      getColour(Skin::kBackground, this), scaleValue(kInsideRouding));

    renderScrollbars(openGl, 0.2f);

    auto laneSection = (EffectsLaneSection *)parent;
    tickAnimation(animationValues, 
      {{ laneSection->isDropdownOpen || (hasEnteredHover && uiRelated.steadyTime - enterHoverTime >= kTimeout) }},
      {{ kHoverIncrement }});

    if (laneSection->isDropdownOpen || (hasEnteredHover && uiRelated.steadyTime - enterHoverTime >= kTimeout))
    {
      if (hasEnteredHover)
        setMouseCursor(uiRelated.renderer, MouseCursorTypes::PointingHand);

      auto e = getMouseInteractions(uiRelated.renderer).mouseState.getEventRelativeTo(this);
      auto scaledPadding = scaleValueRoundInt(padding.toInt());
      auto drawBounds = getLocalBounds().withTrimLeft(scaledPadding.x).withTrimRight(scaledPadding.w);
      if (hoveredBeforeModule)
      {
        if (hoveredBeforeModule->previous->next)
          drawBounds.y = hoveredBeforeModule->previous->component->bounds.getBottom();
        drawBounds.h = hoveredBeforeModule->component->bounds.y - drawBounds.y;

        strokeRect(openGl, drawBounds.withY(drawBounds.getCentreY()).withHeight(0).toFloat(),
          scaleValue(1.0f), getColour(Skin::kBorder, this).withMultipliedAlpha(animationValues[0]), scaleValue(kBorderRounding));
      }
      else
      {
        drawBounds = drawBounds.withTrimTop(scaledPadding.y).withTrimBottom(scaledPadding.h);
        if (auto *lastChild = Generation::Processor::getChild(
          laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
        {
          auto newY = lastChild->component->bounds.getBottom() + scaleValueRoundInt((float)padding.h);
          drawBounds = drawBounds.withTop(newY);
        }

        drawBounds.h = utils::min(drawBounds.h, kEffectModuleMinHeight);

        strokeRect(openGl, drawBounds.toFloat(), scaleValue(1.0f),
          getColour(Skin::kBorder, this).withMultipliedAlpha(animationValues[0]), scaleValue(kBorderRounding));
      }
    }

    return true;
  }

  bool
  EffectsLaneSection::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this), scaleValue(kInsideRouding));
    strokeRect(openGl, getLocalBounds().toFloat(), scaleValue(kEffectsLaneOutlineThickness),
      Colour{ 45, 45, 45 }, scaleValue(kInsideRouding));

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
