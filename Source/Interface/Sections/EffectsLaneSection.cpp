
// Created: 2023-02-03 18:42:55

#include "EffectsLaneSection.hpp"

#include "Generation/Effects.hpp"
#include "Plugin/Complex.hpp"
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

  static bool
  laneMessageHandler(Component *c, u64 commandId, void *extraData)
  {
    auto *self = (EffectsLaneSection *)c;
    switch (commandId)
    {
    case CommandMessages::HandleAutoscroll:
    {
      auto &data = *(CommandMessages::Autoscroll *)extraData;
      if (data.handleY)
      {
        static constexpr float kAutoscrollMultiplier = 10.0f;

        data.handleY = false;

        Point<i32> position = data.position;
        i32 offset = utils::max(0, EffectsLaneSection::kAutoScrollRegion - utils::min(position.y, self->bounds.h - position.y));
        offset = utils::min(offset, (i32)utils::int_max<i8>);
        auto yOffset = (i8)((position.y < self->bounds.h - position.y) ? offset : -offset);

        offsetScroll(&self->moduleHolder, 0.0f, yOffset * getUiRelated()->deltaTime * kAutoscrollMultiplier, false);
      }

      //COMPLEX_DEBUG_LOG("Position: %d, %d\n", data.position.x, data.position.y);
      //COMPLEX_DEBUG_LOG("Autoscroll: %d\n", (int)self->moduleHolder.autoScrollIncrements.y);

      return true;
    }
    case CommandMessages::HandleProcessorInsertion:
    {
      auto *metadata = (CommandMessages::ProcessorInsertion *)extraData;

      if (!CommandMessages::handleProcessorInsertion(self->effectsLane,
        &self->moduleHolder, metadata, Placement::top))
        return false;

      auto section = (EffectModuleSection *)metadata->processor->component;
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
  }

  static void laneInputChangeCallback(Control *c, double newValue, double oldValue)
  {
    auto &selector = *(TextSelector *)c;

    bool direction = oldValue < newValue;
    double oldScaled = Framework::scaleValue(oldValue, selector.details);
    double newScaled = Framework::scaleValue(newValue, selector.details);

    if (oldScaled == newScaled)
      return;

    auto *parentProcessor = selector.parameterLink->parameter->parentProcessor;
    auto [option, _] = Framework::getOptionFromValue(newScaled, selector.details);

    // disallow feedback until we figure out a good UX for that
    // TODO: fix lane feedback UX
    outer:
    {
      auto *next = option;
      while (true)
      {
        // have we reached a primary source? (main or sidechain inputs)
        if (next->flags != Framework::IndexedData::StateIdFlag)
          break;

        // if we've reached our starting lane change initial option
        if (next->stateId == parentProcessor->stateId)
        {
          if (direction)
            --newScaled;
          else
            ++newScaled;

          option = Framework::getOptionFromValue(newScaled, selector.details).first;
          goto outer; // no labelled break in this language...
        }

        // continue down the chain until we find a primary source or our starting lane
        auto *nextControl = parentProcessor->state
          ->getProcessorParameter(next->stateId, Generation::EffectsLane::Input)
          ->getParameterLink()->UIControl;
        next = Framework::getOptionFromValue(Framework::scaleValue(nextControl->getValue(),
          nextControl->details), nextControl->details).first;
      }
    }

    newValue = Framework::unscaleValue(Framework::getValueFromOption(option, selector.details), selector.details);

    (void)selector.value.exchange(newValue, satomi::memory_order_relaxed);
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
    componentFlags.animateMovement = true;
    skinOverride = Skin::kEffectsLane;
    sizingFlags = Component::GrowableY;
    desiredSize = { kEffectsLaneWidth, 0, kEffectsLaneWidth, 0 };

    addCommandMessageHandler(laneHandler);
    laneHandler.object = laneMessageHandler;

    if (!arena)
      arena = utils::bumpArena::createNested(utils::bumpArena::fromAllocation(this), COMPLEX_KB(1));

    addChildComponent(&header);
    header.desiredSize = { 0, kEffectsLaneTopBarHeight, 0, kEffectsLaneTopBarHeight };
    header.sizingFlags = Component::GrowableX;
    header.placement = Placement::top;
    header.padding = { kHeaderLeftPadding, 0, kHeaderRightPadding, 0 };

    header.addChildComponent(&laneTitle);
    laneTitle.placement = Placement::left;
    laneTitle.text = { arena, effectsLane->name };
    laneTitle.textColour = Skin::kWidgetAccent1;
    header.addChildComponent(&inputSelector);
    inputSelector.placement = Placement::right;
    inputSelector.prefix = { arena, "From: " };
    inputSelector.arena = arena;
    inputSelector.text.font = FontId::InterType;
    inputSelector.changeLinkedParameter(*effectsLane->getParameter(Generation::EffectsLane::Input));
    inputSelector.controlFlags.handleSetValueInCallback = true;
    inputSelector.valueChangedCallback = laneInputChangeCallback;
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

    miniView.processor = effectsLane;
    miniView.draggedComponent = &miniView;
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
  EffectsLaneSection::ModuleHolder::mouseEnter(const MouseEvent &)
  {
    // this algorithm will ALMOST work in mouseMove
    // unfortunately if we add a module and the cursor happens to be where the next add module highlight appears
    // there is a pseudo race for where the next mouse hover refresh will happen:
    //  1. if it happens from the rendering procedure,
    //      the module's bounds will have been set and everything works or,
    //  2. the windowing system might send a bogus mouse event before that,
    //      so the module will have bounds of { 0, 0, 0, 0 },
    //      meaning that the next highlight bounds will be where they already were
    //      and if the user doesn't move the cursor, it will not recheck that

    COMPLEX_INVARIANT_ASSERT(
      (
        return getUiRelated()->renderer->callbacks.find(this) ==
          getUiRelated()->renderer->callbacks.data.end();
      )
    );

    getUiRelated()->renderer->callbacks.add(this, [](Component *c)
      {
        auto self = (EffectsLaneSection::ModuleHolder *)c;
        auto laneSection = (EffectsLaneSection *)self->parent;
        auto scaledPadding = scaleValueRoundInt(self->padding.toInt());
        auto localBounds = self->getLocalBounds().withTrimLeft(scaledPadding.x).withTrimRight(scaledPadding.w);

        if (auto *lastChild = Generation::Processor::getChild(
          laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
        {
          auto newBottom = lastChild->component->bounds.getBottom() +
            scaleValueRoundInt((float)lastChild->component->margin.h + kEffectModuleMinHeight);
          localBounds.h = utils::min(newBottom, localBounds.getBottom()) - localBounds.y;
        }
        else
        {
          localBounds.y = scaledPadding.y;
          localBounds.h = scaleValueRoundInt(kEffectModuleMinHeight);
        }

        if (localBounds.contains(Point{ self->lastMouseMove.x, self->lastMouseMove.y }))
        {
          if (!self->hasEnteredHover)
          {
            self->enterHoverTime = getUiRelated()->steadyTime;
            self->hasEnteredHover = true;
          }

          // get the module below cursor
          auto *laneSeciton = (EffectsLaneSection *)self->parent;
          self->hoveredBeforeModuleIndex = 0;
          self->hoveredBeforeModule = laneSeciton->effectsLane->children;
          for (; self->hoveredBeforeModule && self->hoveredBeforeModuleIndex < laneSeciton->effectsLane->childrenCount;
            (++self->hoveredBeforeModuleIndex), (self->hoveredBeforeModule = self->hoveredBeforeModule->next))
          {
            if (self->hoveredBeforeModule->component->bounds.y > self->lastMouseMove.y)
              break;
          }
        }
        else if (self->hasEnteredHover)
        {
          getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::Normal);
          self->hasEnteredHover = false;
        }
      });

    return true;
  }

  bool
  EffectsLaneSection::ModuleHolder::mouseExit(const MouseEvent &)
  {
    getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::Normal);
    hasEnteredHover = false;

    getUiRelated()->renderer->callbacks.erase(this);

    COMPLEX_INVARIANT_ASSERT(
      (
        return getUiRelated()->renderer->callbacks.find(this) == 
          getUiRelated()->renderer->callbacks.data.end();
      )
    );

    return true;
  }

  bool
  EffectsLaneSection::ModuleHolder::mouseMove(const MouseEvent &e)
  {
    auto laneSection = (EffectsLaneSection *)parent;
    if (!laneSection->isDropdownOpen)
      lastMouseMove = e;
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

    double time = getUiRelated()->steadyTime;
    if (time - enterHoverTime < kTimeout)
      return true;

    if (e.mods.test(ModifierKeys::altModifier) || e.mods.test(ModifierKeys::popupMenuClickModifier))
      return true;

    // need to get rid of the callback here manually
    // because the renderer will not call our mouseUp
    // since we allow to be handled by the popup
    // which can result in more than 1 callback being present
    // if the mouse continues being inside the lane
    // because it will trigger an immediate mouseEnter
    getUiRelated()->renderer->callbacks.erase(this);

    summonModulePopup(getPopupSelector(), this, laneSection,
      Placement::custom, { e.x, e.y }, hoveredBeforeModuleIndex);

    COMPLEX_INVARIANT_ASSERT(
      (
        return getUiRelated()->renderer->callbacks.find(this) == 
          getUiRelated()->renderer->callbacks.data.end();
      )
    );

    return true;
  }

  static void renderInsertHint(EffectsLaneSection::ModuleHolder *holder, Graphics &g)
  {
    static constexpr auto kHoverIncrement = 0.05f;

    auto laneSection = (EffectsLaneSection *)holder->parent;
    bool isEmptyAndHovered = holder->componentFlags.isHovered && laneSection->effectsLane->childrenCount == 0;
    tickAnimation(holder->animationValues,
      { { isEmptyAndHovered || laneSection->isDropdownOpen ||
        (holder->hasEnteredHover && getUiRelated()->steadyTime - holder->enterHoverTime >= EffectsLaneSection::kTimeout) } },
      { { kHoverIncrement } });

    if (isEmptyAndHovered || laneSection->isDropdownOpen ||
      (holder->hasEnteredHover && getUiRelated()->steadyTime - holder->enterHoverTime >= EffectsLaneSection::kTimeout))
    {
      if (holder->hasEnteredHover)
        getUiRelated()->renderer->setMouseCursor(MouseCursorTypes::PointingHand);

      auto e = getUiRelated()->renderer->getMouseInteractions().mouseState.getEventRelativeTo(holder);
      auto scaledPadding = scaleValueRoundInt(holder->padding.toInt());
      auto drawBounds = holder->getLocalBounds().withTrimLeft(scaledPadding.x).withTrimRight(scaledPadding.w);

      // the childrenCount check is necessary because we don't know if have outdated info
      // because callback hadn't run since we weren't hovered over
      if (holder->hoveredBeforeModule && laneSection->effectsLane->childrenCount)
      {
        if (holder->hoveredBeforeModule->previous->next)
          drawBounds.y = holder->hoveredBeforeModule->previous->component->bounds.getBottom();
        drawBounds.h = holder->hoveredBeforeModule->component->bounds.y - drawBounds.y;

        drawBounds = drawBounds.withY(drawBounds.getCentreY()).withHeight(0);
      }
      else
      {
        drawBounds = drawBounds.withTrimTop(scaledPadding.y).withTrimBottom(scaledPadding.h);
        if (auto *lastChild = Generation::Processor::getChild(
          laneSection->effectsLane->children, laneSection->effectsLane->childrenCount - 1))
        {
          auto newY = lastChild->component->bounds.getBottom() + scaleValueRoundInt((float)holder->padding.h);
          drawBounds = drawBounds.withTop(newY);
        }

        drawBounds.h = utils::min(drawBounds.h, scaleValueRoundInt(kEffectModuleMinHeight));
      }

      auto colour = getColour(Skin::kBorder, holder).withMultipliedAlpha(holder->animationValues[0]);
      strokeRect(g, drawBounds.toFloat(), scaleValue(1.0f), colour, scaleValue(EffectsLaneSection::kBorderRounding));

      static constexpr int kPlusSize = 16;
      auto plusSize = scaleValueRound(kPlusSize);

      if (drawBounds.h >= 2 * plusSize)
      {
        auto plusBounds = Rectangle{ (float)drawBounds.getCentreX(),
          (float)drawBounds.getCentreY(), 0.0f, 0.0f }.withExpand(plusSize * 0.5f);

        strokePlus(g, plusBounds, scaleValue(2.0f), colour);
      }
    }
  }

  bool
  EffectsLaneSection::ModuleHolder::render(Graphics &g)
  {
    fillRect(g, getLocalBounds().toFloat(),
      getColour(Skin::kBackground, this), scaleValue(kInsideRouding));

    renderScrollbars(g, 0.2f);

    renderInsertHint(this, g);

    return true;
  }

  bool
  EffectsLaneSection::render(Graphics &g)
  {
    fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kBody, this), scaleValue(kInsideRouding));
    strokeRect(g, getLocalBounds().toFloat(), scaleValue(kEffectsLaneOutlineThickness),
      Colour{ 45, 45, 45 }, scaleValue(kInsideRouding));

    doRenderChildren(g);

    if (!laneActivator.isOn())
    {
      fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kOverlayScreen, this));
    }

    return false;
  }

  EffectsLaneSection::LaneMiniView::LaneMiniView()
  {
    sizingFlags = Component::GrowableX;
    padding = { 4, 0, 4, 0 };
    desiredSize = { kMinWidth, kMinHeight, utils::int_max<i32>, kMinHeight };
  }

  bool
  EffectsLaneSection::LaneMiniView::mouseDown(const MouseEvent &e)
  {
    if (e.mods.test(ModifierKeys::middleButtonModifier))
    {
      // LaneSelector -> EffectsSection
      auto *effectsSection = (EffectsSection *)parent->parent;
      effectsSection->removeLane((EffectsLaneSection *)processor->component);

      return true;
    }

    return DraggableComponent::mouseDown(e);
  }

  bool
  EffectsLaneSection::LaneMiniView::render(Graphics &g)
  {
    auto localBounds = getLocalBounds().toFloat();
    fillRect(g, localBounds, getColour(Skin::kBody, this), EffectsSection::LaneSelector::kLaneMiniViewRounding);

    auto textBounds = localBounds.withTrim(scaleValueRound(padding.toFloat()))
      .withY(localBounds.getCentreY()).withHeight(0.0f)
      .withExpand(0.0f, scaleValue(kPrimaryTextLineHeight / 2));

    renderText(((EffectsLaneSection *)processor->component)->laneTitle.text,
      FontId::DDinType, textBounds, g, getColour(Skin::kHeadingText, this));

    return true;
  }
}

Interface::Component *
Generation::EffectsLane::createUI()
{
  auto guiArena = Interface::getUiRelated()->renderer->gui->arena;
  auto *effectsLaneSection = anew(guiArena, Interface::EffectsLaneSection, {});
  effectsLaneSection->effectsLane = this;
  effectsLaneSection->reinitialise();
  return effectsLaneSection;
}
