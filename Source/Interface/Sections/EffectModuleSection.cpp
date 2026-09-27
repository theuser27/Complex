
// Created: 2023=02-14 02:29:16

#include "EffectModuleSection.hpp"

#include "Plugin/Complex.hpp"
#include "Framework/parameters.hpp"
#include "Generation/Effects.hpp"
#include "Generation/Algorithms.hpp"
#include "../Components/Control.hpp"
#include "Popups.hpp"
#include "EffectsLaneSection.hpp"
#include "MainInterface.hpp"

EffectModuleSection::SpectralMaskComponent::EmptySlider::EmptySlider()
{
  sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  controlFlags.shouldShowPopup = true;
  controlFlags.shouldUsePlusMinusPrefix = true;
  overridePosition = [](Component *c) { c->bounds.x = c->bounds.y = 0; return true; };
  // because of the bipolarity we need to lower the sensitivity
  // so that the shift follows the mouse exactly
  sensitivity = 0.5f;
}

bool
EffectModuleSection::SpectralMaskComponent::EmptySlider::mouseDown(const MouseEvent &e)
{
  if (e.mods.test(ModifierKeys::ctrlModifier))
  {
    auto event = e;
    event.mods = event.mods.withoutFlags(ModifierKeys::ctrlModifier);
    return PinSlider::mouseDown(event);
  }

  return false;
}

EffectModuleSection::SpectralMaskComponent::SpectralMaskComponent()
{
  shiftBounds.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  addChildComponent(&shiftBounds, children);
}

bool
EffectModuleSection::SpectralMaskComponent::render(Graphics &g)
{
  fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kBody, this),
    scaleValue(rounding[0]), scaleValue(rounding[1]), scaleValue(rounding[2]), scaleValue(rounding[3]));

  auto shiftValue = scaleValue(shiftBounds.getValue(), shiftBounds.details);
  paintHighlightBox(this, g, (float)(shiftValue + lowBound.getValue()),
    (float)(shiftValue + highBound.getValue()),
    getColour(Skin::kWidgetPrimary1, this).withAlpha(0.15f), backgroundColour);

  renderChildren(g);

  return false;
}

void EffectModuleSection::EffectHolder::Header::reinitialise()
{
  removeAllChildComponents();

  addChildComponent(&draggableBox);
  draggableBox.placement = Placement::left;
  draggableBox.desiredSize = { kDraggableSectionWidth, kDraggableSectionWidth,
    kDraggableSectionWidth, kDraggableSectionWidth };

  effectTypeIcon.desiredSize = { kIconSize, kIconSize, kIconSize, kIconSize };
  effectTypeIcon.placement = Placement::left;
  effectTypeIcon.margin = { 0, 0, 4, 0 };
  effectTypeIcon.reference = this;
  effectTypeIcon.draw = [](Graphics &g, Component *reference, Component *self, Point<i32>)
  {
    auto *header = (Header *)reference;
    if (header->icon)
    {
      drawSVG(header->icon, g, getColour(Skin::kWidgetPrimary1, reference),
        self->bounds.withZeroOrigin().toFloat(), getUiRelated()->scale, scaleValue(1.0f));
    }

    return true;
  };

  addChildComponent(&effectTypeSelector);
  effectTypeSelector.addChildComponent(&effectTypeIcon, (usize)0);
  effectTypeSelector.arena = arena;
  effectTypeSelector.placement = Placement::left;
  effectTypeSelector.dropdownOffset = { 0, 4 };
  effectTypeSelector.valueChangedCallback = [](Control *c, float newValue, float)
  {
    // selector -> header -> effectHolder -> effectModuleSection
    auto *section = (EffectModuleSection *)c->parent->parent->parent;
    auto [option, _] = getOptionFromValue(
      scaleValue(newValue, c->details), c->details);

    // make icon component invisible and only show it if an icon exist
    // selector -> header
    auto *header = (Header *)c->parent;
    header->icon = nullptr;
    header->effectTypeIcon.componentFlags.isVisible = false;

    for (; option && (option->flags != IndexedData::SVGData || !option->svgData);
      option = option->parent) { }
    if (option)
    {
      header->icon = option->svgData;
      header->effectTypeIcon.componentFlags.isVisible = true;
    }

    section->restartEffectUI();
  };

  addChildComponent(&mixNumberBox);
  mixNumberBox.maxDecimalCharacters = 2;
  mixNumberBox.arena = arena;
  mixNumberBox.placement = Placement::right;

  addChildComponent(&moduleActivator);
  moduleActivator.placement = Placement::right;
  moduleActivator.margin = { kPowerButtonMargin, 0, kPowerButtonMargin, 0 };
  moduleActivator.desiredSize = { kDefaultActivatorSize, kDefaultActivatorSize,
    kDefaultActivatorSize, kDefaultActivatorSize };

  effectTypeSelector.valueChangedCallback(&effectTypeSelector, effectTypeSelector.getValue(), 0.0f);
}

void EffectModuleSection::EffectHolder::reinitialise()
{
  removeChildComponent(&header);
  deleteAllChildComponents();

  componentFlags.vertical = true;

  addChildComponent(&header);
  header.arena = arena;
  header.sizingFlags = Component::GrowableX;
  header.placement = Placement::top;
  header.desiredSize = { 0, kTopMenuHeight, 0, kTopMenuHeight };
  header.reinitialise();
}

bool
EffectModuleSection::EffectHolder::render(Graphics &g)
{
  nvgBeginPath(g);
  float width = scaleValue(kDelimiterWidth);
  float y = (float)header.bounds.getBottom() + width * 0.5f;
  nvgMoveTo(g, 0.0f, y);
  nvgLineTo(g, (float)bounds.w, y);
  nvgStrokeWidth(g, width);
  nvgStrokeColor(g, getColour(Skin::kBackgroundElement, this));
  nvgStroke(g);

  return true;
}

bool
EffectModuleSection::render(Graphics &g)
{
  float rounding = scaleValue(kOuterPixelRounding);
  fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kBackground, this),
    rounding, rounding, rounding, rounding);

  renderChildren(g);

  if (!effectHolder.header.moduleActivator.isOn())
  {
    fillRect(g, getLocalBounds().toFloat(), getColour(Skin::kOverlayScreen, this));
  }

  return false;
}

void EffectModuleSection::reinitialise()
{
  removeAllChildComponents();

  componentFlags.vertical = true;
  componentFlags.clickable = true;
  desiredSize = { kEffectModuleWidth, 0, kEffectModuleWidth, utils::int_max<i32> };

  if (!arena)
    arena = utils::Pool::createNested(utils::Pool::fromAllocation(this), COMPLEX_KB(64));
  if (!effectArena)
    effectArena = utils::Pool::createNested(arena, COMPLEX_KB(32));


  addChildComponent(&maskComponent);
  maskComponent.sizingFlags = Component::GrowableX;
  maskComponent.desiredSize = { 0, kSpectralMaskContractedHeight, 0, kSpectralMaskContractedHeight };
  utils::copy(utils::span{ maskComponent.rounding }, {{ (float)kOuterPixelRounding, (float)kOuterPixelRounding, 0.0f, 0.0f }});
  maskComponent.lowBound.arena = arena;
  maskComponent.lowBound.changeLinkedParameter(*effectModule->getParameter(EffectModule::LowBound));
  maskComponent.highBound.arena = arena;
  maskComponent.highBound.changeLinkedParameter(*effectModule->getParameter(EffectModule::HighBound));
  maskComponent.shiftBounds.arena = arena;
  maskComponent.shiftBounds.changeLinkedParameter(*effectModule->getParameter(EffectModule::ShiftBounds));


  addChildComponent(&effectHolder);
  effectHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::SnapToMinY);
  effectHolder.desiredSize = { 0, kEffectModuleMainBodyHeight, 0, utils::int_max<i32> };
  effectHolder.arena = arena;

  effectHolder.header.effectTypeSelector.changeLinkedParameter(*effectModule->getParameter(EffectModule::ModuleType));
  effectHolder.header.mixNumberBox.changeLinkedParameter(*effectModule->getParameter(EffectModule::ModuleMix));
  effectHolder.header.moduleActivator.changeLinkedParameter(*effectModule->getParameter(EffectModule::ModuleEnabled));
  effectHolder.header.draggableBox.draggedComponent = this;
  effectHolder.header.draggableBox.processor = effectModule;
  effectHolder.header.draggableBox.copyingDraggedComponent = [](Component *c)
  {
    auto *self = (EffectModuleSection *)c;
    auto *effectModuleCopy = (EffectModule *)self->effectModule->createCopy();
    auto *effectModuleSectionCopy = (EffectModuleSection *)effectModuleCopy->createUI();
    effectModuleSectionCopy->margin = self->margin;
    effectModuleCopy->component = effectModuleSectionCopy;
    return &effectModuleSectionCopy->effectHolder.header.draggableBox;
  };
  effectHolder.reinitialise();
}


bool
EffectModuleSection::mouseDown(const MouseEvent &e)
{
  if (e.mods.test(ModifierKeys::middleButtonModifier))
  {
    effectModule->state->plugin->undoManager.perform(anew(
      effectModule->state->plugin->undoManager.beginNewTransaction(),
      DeleteProcessorUpdate, { effectModule }));

    return true;
  }

  if (e.mods.test(ModifierKeys::popupMenuClickModifier))
  {
    enum MenuId
    {
      kCancel = 0,
      kDeleteInstance,
      kCopyInstance,
      kInitInstance
    };

    auto *selector = getPopupSelector();
    auto *itemArena = selector->arena;
    PopupList *list = anew(itemArena, PopupList, { selector });
    list->desiredSize = { kPopupMinWidth, 0, utils::int_max<i32>, utils::int_max<i32> };
    list->padding = { 0, 4, 0, 4 };

    list->addChildComponent(OptionPopupItem::createTitle(list, "Module Options"));
    auto insertItem = [&](MenuId id, utils::string_view text, i32 shortcutKeyCode)
    {
      auto *item = anew(itemArena, OptionPopupItem, {});
      item->id = id;
      item->dataTag = OptionPopupItem::StringData;
      item->extraData = anew(itemArena, utils::stringnd, { itemArena, text });
      item->associatedList = list;
      item->shortcutKeyCode = shortcutKeyCode;
      list->addChildComponent(item);
    };

    insertItem(kDeleteInstance, "D" COMPLEX_UNDERSCORE_LITERAL "elete", 'd');
    insertItem(kCopyInstance, "C" COMPLEX_UNDERSCORE_LITERAL "opy (TODO)", 'c');
    insertItem(kInitInstance, "I" COMPLEX_UNDERSCORE_LITERAL "nitialise", 'i');

    selector->list = list;
    selector->toggleable = false;
    selector->skinOverride = getSkinOverride();
    selector->callback = [this](PopupSelector *, PopupItem *item)
    {
      if (item->id == kDeleteInstance)
      {
        // make sure nothing touches this after the call runs
        effectModule->state->plugin->undoManager.perform(anew(
          effectModule->state->plugin->undoManager.beginNewTransaction(),
          DeleteProcessorUpdate, { effectModule }));
      }
      else if (item->id == kCopyInstance)
      {
        // TODO: copy right click option on EffectModuleSection
      }
      else if (item->id == kInitInstance)
      {
        // TODO: initialisation right click option on EffectModuleSection
      }
    };
    selector->summon(this, Placement::custom, { e.x, e.y });

    componentFlags.isClicked = false;

    return true;
  }

  return true;
}

void EffectModuleSection::restartEffectUI()
{
  effectHolder.removeChildComponent(&effectHolder.header);
  effectHolder.removeAllChildComponents();
  utils::Pool::clear(effectArena);
  effectHolder.addChildComponent(&effectHolder.header);
  effectControls = {};

  auto activeEffect = effectModule->currentEffect.load(satomi::memory_order_acquire);
  skinOverride = (Skin::Override)activeEffect->metadata->userFlags;

  if (auto *createUIPointer = ((EffectVtable *)activeEffect->metadata->vtable)->createUI)
    effectControls = createUIPointer(effectArena, this,
      effectModule->currentEffect.load(satomi::memory_order_acquire));
}

#define findParameterWithId(parameters, idToCheck) utils::findIf(parameters, [](Parameter *item) { return item->details.id == idToCheck; })

static CombinationRotarySlider *
createRotary(utils::Pool *arena, Parameter *parameter)
{
  auto *rotary = anew(arena, CombinationRotarySlider, {});
  rotary->placement = Placement::justifyX;
  rotary->rotary.arena = arena;
  rotary->rotary.changeLinkedParameter(*parameter);
  return rotary;
}

static utils::span<Control *>
genericKnobUI(utils::Pool *arena, EffectModuleSection *section,
  EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  utils::vectornd<Control *> controls{ arena, effectData->parameterCount };
  auto *parameter = effectData->parameters;
  for (usize i = 0; i < effectData->parameterCount; (++i), (parameter = parameter->next))
  {
    auto *rotary = createRotary(arena, parameter);
    controls.emplaceBack(&rotary->rotary);
    holder->addChildComponent(rotary);
  }

  section->effectHolder.addChildComponent(holder);

  return controls;
}



utils::span<Control *>
Filter::createUINormal(utils::Pool *arena, EffectModuleSection *section,
  EffectData *effectData)
{
  return genericKnobUI(arena, section, effectData);
}

utils::span<Control *>
Filter::createUIGate(utils::Pool *arena, EffectModuleSection *section,
  EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  auto *gainRotary = createRotary(arena, findParameterWithId(effectData->parameters, Gate::Gain));
  holder->addChildComponent(gainRotary);

  auto *thresholdRotary = createRotary(arena, findParameterWithId(effectData->parameters, Gate::Threshold));
  holder->addChildComponent(thresholdRotary);

  auto *tiltRotary = createRotary(arena, findParameterWithId(effectData->parameters, Gate::Tilt));
  holder->addChildComponent(tiltRotary);

  section->effectHolder.addChildComponent(holder);

  utils::vectornd<Control *> controls{ arena,
    {{ &gainRotary->rotary, &thresholdRotary->rotary, &tiltRotary->rotary }} };
  return controls;
}

utils::span<Control *>
Dynamics::createUIContrast(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  return genericKnobUI(arena, section, effectData);
}

utils::span<Control *>
Dynamics::createUIClip(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  return genericKnobUI(arena, section, effectData);
}

utils::span<Control *>
Phase::createUIShift(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  auto *shiftRotary = createRotary(arena, findParameterWithId(effectData->parameters, Shift::PhaseShift));
  holder->addChildComponent(shiftRotary);

  auto *intervalRotary = createRotary(arena, findParameterWithId(effectData->parameters, Shift::Interval));
  holder->addChildComponent(intervalRotary);

  auto *offsetRotary = createRotary(arena, findParameterWithId(effectData->parameters, Shift::Offset));
  holder->addChildComponent(offsetRotary);

  auto *selector = anew(arena, TextSelector, {});
  selector->arena = arena;
  selector->changeLinkedParameter(*findParameterWithId(effectData->parameters, Shift::Slope));
  shiftRotary->setModifier(selector);

  section->effectHolder.addChildComponent(holder);

  utils::vectornd<Control *> controls{ arena,
    {{ &shiftRotary->rotary, &intervalRotary->rotary, &offsetRotary->rotary, selector }} };
  return controls;
}

utils::span<Control *>
Pitch::createUIResample(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  auto *shiftRotary = createRotary(arena, findParameterWithId(effectData->parameters, Resample::Shift));
  holder->addChildComponent(shiftRotary);

  section->effectHolder.addChildComponent(holder);

  utils::vectornd<Control *> controls{ arena, {{ &shiftRotary->rotary }} };
  return controls;
}

utils::span<Control *>
Pitch::createUIFrequencyShift(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  return genericKnobUI(arena, section, effectData);
}

utils::span<Control *> 
Freeze::createUIRolling(utils::Pool *arena, 
  EffectModuleSection *section, EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  auto *shiftRotary = createRotary(arena, findParameterWithId(effectData->parameters, Rolling::Rate));
  holder->addChildComponent(shiftRotary);

  section->effectHolder.addChildComponent(holder);

  utils::vectornd<Control *> controls{ arena, {{ &shiftRotary->rotary }} };
  return controls;
}

utils::span<Control *>
Destroy::createUIReinterpret(utils::Pool *arena,
  EffectModuleSection *section, EffectData *effectData)
{
  Component *holder = anew(arena, Component, {});
  holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
  holder->padding = { 32, 0, 32, 0 };

  auto *attenuationRotary = createRotary(arena, findParameterWithId(effectData->parameters, Reinterpret::Attenuation));
  holder->addChildComponent(attenuationRotary);

  auto *mappingSelector = anew(arena, TextSelector, {});
  mappingSelector->changeLinkedParameter(*findParameterWithId(effectData->parameters, Reinterpret::Transform));
  mappingSelector->arena = arena;
  mappingSelector->placement = Placement::left;
  auto *label = anew(arena, Label, {});
  label->control = mappingSelector;
  label->textPlacement = Placement::right;
  label->placement = Placement::right;
  label->overrideSize = [](Component *c, bool isCalculatingVertical)
  {
    c->padding = c->next->padding;
    return Label::getSizeMetrics(c, isCalculatingVertical);
  };
  Component *mappingHolder = anew(arena, Component, {});
  mappingHolder->componentFlags.vertical = true;
  mappingHolder->placement = Placement::justifyX;
  mappingHolder->addChildComponent(label);
  mappingHolder->addChildComponent(mappingSelector);
  holder->addChildComponent(mappingHolder);

  section->effectHolder.addChildComponent(holder);

  utils::vectornd<Control *> controls{ arena, {{ &attenuationRotary->rotary, mappingSelector }} };
  return controls;
}

#undef findParameterWithId

Component *
createUIEffectModule(Processor *processor)
{
  auto guiArena = getUiRelated()->renderer->gui->arena;
  auto *effectModuleSection = anew(guiArena, EffectModuleSection, {});
  effectModuleSection->effectModule = (EffectModule *)processor;
  effectModuleSection->reinitialise();
  return effectModuleSection;
}
