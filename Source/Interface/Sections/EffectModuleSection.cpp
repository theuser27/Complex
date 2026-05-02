
// Created: 2023=02-14 02:29:16

#include "EffectModuleSection.hpp"

#include "Plugin/Complex.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Generation/Effects.hpp"
#include "Generation/Algorithms.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/Control.hpp"
#include "Popups.hpp"
#include "EffectsLaneSection.hpp"
#include "MainInterface.hpp"

namespace Interface
{
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
  EffectModuleSection::SpectralMaskComponent::render(OpenGlWrapper &openGl)
  {
    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this),
      scaleValue(rounding[0]), scaleValue(rounding[1]), scaleValue(rounding[2]), scaleValue(rounding[3]));

    auto shiftValue = Framework::scaleValue(shiftBounds.getValue(), shiftBounds.details);
    paintHighlightBox(this, *openGl.cache, (float)(shiftValue + lowBound.getValue()),
      (float)(shiftValue + highBound.getValue()),
      getColour(Skin::kWidgetPrimary1, this).withAlpha(0.15f), backgroundColour);

    doRenderChildren(openGl);

    return false;
  }

  void EffectModuleSection::EffectHolder::Header::reinitialise()
  {
    removeAllChildComponents();

    addChildComponent(&draggableBox);
    draggableBox.placement = Placement::left;
    draggableBox.desiredSize = { kDraggableSectionWidth, kDraggableSectionWidth, 
      kDraggableSectionWidth, kDraggableSectionWidth };
    
    addChildComponent(&effectTypeSelector);
    effectTypeSelector.arena = arena;
    effectTypeSelector.placement = Placement::left;
    effectTypeSelector.dropdownOffset = { 0, 4 };
    effectTypeSelector.valueChangedCallback = [](Control *c, double newValue, double)
    {
      // selector -> header -> effectHolder -> effectModuleSection
      auto *section = (EffectModuleSection *)c->parent->parent->parent;
      (void)section->effectModule->changeEffect(Framework::getOptionFromValue(
        Framework::scaleValue(newValue, c->details), c->details).first);
      
      section->restartEffectUI();
    };

    addChildComponent(&mixNumberBox);
    mixNumberBox.maxDecimalCharacters = 2;
    mixNumberBox.arena = arena;
    mixNumberBox.placement = Placement::right;

    addChildComponent(&moduleActivator);
    moduleActivator.placement = Placement::right;
    moduleActivator.margin = { kNumberBoxToPowerButtonMargin, 0, 0, 0 };
    moduleActivator.desiredSize = { kDefaultActivatorSize, kDefaultActivatorSize, 
      kDefaultActivatorSize, kDefaultActivatorSize };
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
  EffectModuleSection::EffectHolder::render(OpenGlWrapper &openGl)
  {
    float topRounding = scaleValue(kInnerPixelRounding);
    float bottomRounding = scaleValue(kOuterPixelRounding);

    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBackground, this),
      topRounding, topRounding, bottomRounding, bottomRounding);

    nvgBeginPath(openGl);
    float y = (float)header.bounds.getBottom();
    nvgMoveTo(openGl, 0.0f, y);
    nvgLineTo(openGl, (float)bounds.w, y);
    nvgStrokeWidth(openGl, scaleValue(1.0f));
    nvgStrokeColor(openGl, getColour(Skin::kBackgroundElement, this));
    nvgStroke(openGl);

    return true;
  }

  bool 
  EffectModuleSection::render(OpenGlWrapper &openGl)
  {
    doRenderChildren(openGl);
    
    if (!effectHolder.header.moduleActivator.isOn())
    {
      fillRect(openGl, getLocalBounds().toFloat(),
        getColour(Skin::kBackground, this).withMultipliedAlpha(0.8f));
    }

    return false;
  }

  void EffectModuleSection::reinitialise()
  {
    using namespace Framework;

    removeAllChildComponents();

    componentFlags.vertical = true;
    componentFlags.clickable = true;
    //sizingFlags = Component::SnapToMinY;
    desiredSize = { kEffectModuleWidth, 0, kEffectModuleWidth, utils::max_limit<i32> };

    if (!arena)
      arena = utils::bumpArena::createNested(utils::bumpArena::fromAllocation(this), COMPLEX_KB(64));
    if (!effectArena)
      effectArena = utils::bumpArena::createNested(arena, COMPLEX_KB(32));


    addChildComponent(&maskComponent);
    maskComponent.sizingFlags = Component::GrowableX;
    maskComponent.desiredSize = { 0, kSpectralMaskContractedHeight, 0, kSpectralMaskContractedHeight };
    maskComponent.margin = { 0, 0, 0, kSpectralMaskMargin };
    utils::copy(utils::span{ maskComponent.rounding }, {{ (float)kOuterPixelRounding, (float)kOuterPixelRounding,
      (float)kInnerPixelRounding, (float)kInnerPixelRounding }});
    maskComponent.lowBound.arena = arena;
    maskComponent.lowBound.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::LowBound));
    maskComponent.highBound.arena = arena;
    maskComponent.highBound.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::HighBound));
    maskComponent.shiftBounds.arena = arena;
    maskComponent.shiftBounds.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::ShiftBounds));


    addChildComponent(&effectHolder);
    effectHolder.sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::SnapToMinY);
    effectHolder.desiredSize = { 0, kEffectModuleMainBodyHeight, 0, utils::max_limit<i32> };
    effectHolder.arena = arena;
    effectHolder.reinitialise();

    effectHolder.header.effectTypeSelector.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::ModuleType));
    effectHolder.header.mixNumberBox.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::ModuleMix));
    effectHolder.header.moduleActivator.changeLinkedParameter(*effectModule->getParameter(Generation::EffectModule::ModuleEnabled));
    effectHolder.header.draggableBox.draggedComponent = this;
    effectHolder.header.draggableBox.processor = effectModule;
    effectHolder.header.draggableBox.copyingDraggedComponent = [](Component *c)
    {
      auto *self = (EffectModuleSection *)c;
      auto *effectModuleCopy = (Generation::EffectModule *)self->effectModule->createCopy();
      auto *effectModuleSectionCopy = (EffectModuleSection *)effectModuleCopy->createUI();
      return &effectModuleSectionCopy->effectHolder.header.draggableBox;
    };

    restartEffectUI();
  }


  bool
  EffectModuleSection::mouseDown(const MouseEvent &e)
  {
    if (!e.mods.test(ModifierKeys::popupMenuClickModifier))
      return false;

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
    list->desiredSize = { kPopupMinWidth, 0, utils::max_limit<i32>, utils::max_limit<i32> };
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

    insertItem(kDeleteInstance, "D" COMPLEX_UNDERSCORE_LITERAL "elete", 'D');
    insertItem(kCopyInstance, "C" COMPLEX_UNDERSCORE_LITERAL "opy (TODO)", 'C');
    insertItem(kInitInstance, "I" COMPLEX_UNDERSCORE_LITERAL "nitialise", 'I');

    selector->list = list;
    selector->toggleable = false;
    selector->skinOverride = getSkinOverride();
    selector->callback = [this](PopupSelector *, PopupItem *item)
    {
      if (item->id == kDeleteInstance)
      {
        // make sure nothing touches this after the call runs
        auto *state = effectModule->state;
        auto *transactionArena = state->plugin->undoManager.beginNewTransaction();
        state->plugin->undoManager.perform(anew(transactionArena,
          Framework::DeleteProcessorUpdate, { effectModule }));
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

  void EffectModuleSection::restartEffectUI()
  {
    effectHolder.removeChildComponent(&effectHolder.header);
    effectHolder.removeAllChildComponents();
    utils::bumpArena::clear(effectArena);
    effectHolder.addChildComponent(&effectHolder.header);
    effectControls = {};

    auto activeEffect = effectModule->currentEffect.load(satomi::memory_order_acquire);
    skinOverride = (Interface::Skin::Override)activeEffect->metadata->userFlags;

    if (auto *createUIPointer = activeEffect->metadata->vtable[Generation::EffectData::CreateUIVtableIndex])
      effectControls = ((Generation::EffectData::CreateUIFn *)createUIPointer)(effectArena, this,
        effectModule->currentEffect.load(satomi::memory_order_acquire));
    
    // TODO: icon
  }
}

namespace Generation
{
#define findParameterWithId(parameters, id) utils::findIf(parameters, [](Framework::ParameterValue *item) { return item->getParameterId() == id; })

  static Interface::CombinationRotarySlider *
  createRotary(utils::bumpArena *arena, Framework::ParameterValue *parameter)
  {
    using namespace Interface;

    auto *rotary = anew(arena, CombinationRotarySlider, {});
    rotary->placement = Placement::justifyX;
    rotary->rotary.arena = arena;
    rotary->rotary.changeLinkedParameter(*parameter);
    return rotary;
  }

  static utils::span<Interface::Control *>
  genericKnobUI(utils::bumpArena *arena, Interface::EffectModuleSection *section,
    EffectData *effectData)
  {
    using namespace Interface;

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



  utils::span<Interface::Control *> 
  Filter::createUINormal(utils::bumpArena *arena, Interface::EffectModuleSection *section,
    EffectData *effectData)
  {
    return genericKnobUI(arena, section, effectData);
  }

  utils::span<Interface::Control *>
  Filter::createUIGate(utils::bumpArena *arena, Interface::EffectModuleSection *section,
    EffectData *effectData)
  {
    using namespace Interface;
    
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

  utils::span<Interface::Control *> 
  Dynamics::createUIContrast(utils::bumpArena *arena, 
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    return genericKnobUI(arena, section, effectData);
  }

  utils::span<Interface::Control *> 
  Dynamics::createUIClip(utils::bumpArena *arena,
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    return genericKnobUI(arena, section, effectData);
  }

  utils::span<Interface::Control *> 
  Phase::createUIShift(utils::bumpArena *arena, 
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    using namespace Interface;

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

  utils::span<Interface::Control *> 
  Pitch::createUIResample(utils::bumpArena *arena, 
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    using namespace Interface;

    Component *holder = anew(arena, Component, {});
    holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
    holder->padding = { 32, 0, 32, 0 };

    auto *shiftRotary = createRotary(arena, findParameterWithId(effectData->parameters, Resample::Shift));
    holder->addChildComponent(shiftRotary);

    section->effectHolder.addChildComponent(holder);

    utils::vectornd<Control *> controls{ arena, {{ &shiftRotary->rotary }} };
    return controls;
  }

  utils::span<Interface::Control *>
  Pitch::createUIFrequencyShift(utils::bumpArena *arena,
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    return genericKnobUI(arena, section, effectData);
  }

  utils::span<Interface::Control *> 
  Destroy::createUIReinterpret(utils::bumpArena *arena, 
    Interface::EffectModuleSection *section, EffectData *effectData)
  {
    using namespace Interface;

    Component *holder = anew(arena, Component, {});
    holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
    holder->padding = { 32, 0, 32, 0 };

    auto *attenuationRotary = createRotary(arena, findParameterWithId(effectData->parameters, Reinterpret::Attenuation));
    holder->addChildComponent(attenuationRotary);

    auto *mappingSelector = anew(arena, TextSelector, {});
    mappingSelector->changeLinkedParameter(*findParameterWithId(effectData->parameters, Reinterpret::Mapping));
    mappingSelector->arena = arena;
    mappingSelector->placement = Placement::left;
    auto *label = anew(arena, Label, {});
    label->control = mappingSelector;
    label->textPlacement = Placement::left;
    label->placement = Placement::left;
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
}

namespace Generation
{
  Interface::Component *
  EffectModule::createUI()
  {
    auto guiArena = Interface::getGui(Interface::uiRelated.renderer)->arena;
    auto *effectModuleSection = anew(guiArena, Interface::EffectModuleSection, {});
    effectModuleSection->effectModule = this;
    effectModuleSection->reinitialise();
    return effectModuleSection;
  }
}
