
// Created: 2023=02-14 02:29:16

#include "EffectModuleSection.hpp"

#include "Plugin/Complex.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Generation/Effects.hpp"
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
    overridePosition = [](Component *c) { c->bounds.setPosition({}); return true; };
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

    fillRect(openGl, getLocalBounds().toFloat(), getColour(Skin::kBody, this),
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

    if (auto *createUIPointer = activeEffect->metadata->vtable[Generation::EffectModule::CreateUIVtableIndex])
      effectControls = ((Generation::EffectModule::CreateUIFn *)createUIPointer)(effectArena, this,
        effectModule->currentEffect.load(satomi::memory_order_acquire));
    
    // TODO: icon
  }



  /*namespace
  {
    std::vector<utils::up<Control>> initFilterParameters(EffectModuleSection *section, utils::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<Control>> parameters;

      auto initNormal = [&]()
      {
        parameters.reserve(Processors::BaseEffect::Filter::Normal::enum_count_filter(Framework::kGetParameterPredicate));
        auto gain = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Gain::id().value()));
        gain->setShouldUsePlusMinusPrefix(true);
        parameters.emplace_back(COMPLEX_MOVE(gain));
        auto cutoff = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Cutoff::id().value()));
        cutoff->setMaxTotalCharacters(6);
        cutoff->setMaxDecimalCharacters(4);
        parameters.emplace_back(COMPLEX_MOVE(cutoff));
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Slope::id().value())));
      };

      auto initGate = [&]()
      {
        parameters.reserve(Processors::BaseEffect::Filter::Gate::enum_count_filter(Framework::kGetParameterPredicate));
        auto gain = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Gate::Gain::id().value()));
        gain->setShouldUsePlusMinusPrefix(true);
        parameters.emplace_back(COMPLEX_MOVE(gain));
        auto threshold = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Gate::Threshold::id().value()));
        threshold->setMaxTotalCharacters(6);
        threshold->setMaxDecimalCharacters(4);
        threshold->setShouldCheckDbInfinities(true);
        parameters.emplace_back(COMPLEX_MOVE(threshold));
        auto tilt = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Gate::Tilt::id().value()));
        tilt->setShouldUsePlusMinusPrefix(true);
        parameters.emplace_back(COMPLEX_MOVE(tilt));
      };

      auto initRegular = [&]() { };

      switch (Processors::BaseEffect::Filter::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Filter::Normal:
        initNormal();
        break;
      default:
      case Processors::BaseEffect::Filter::Gate:
        initGate();
        break;
      case Processors::BaseEffect::Filter::Regular:
        initRegular();
        break;
      }

      return parameters;
    }

    void arrangeFilterUI(EffectModuleSection *section, juce::Rectangle<int> bounds, utils::string_view type)
    {
      using namespace Framework;

      auto arrangeNormal = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
          .withTrimmedTop(knobTopOffset).withHeight(knobsHeight);
        int rotaryInterval = (int)std::round((float)bounds.getWidth() / 3.0f);

        // gain rotary
        auto *gainSlider = section->getEffectControl(Processors::BaseEffect::Filter::Normal::Gain::id().value());
        utils::ignore = gainSlider->setSizes(knobsHeight);
        gainSlider->setPosition(Point{ bounds.getX(), bounds.getY() });

        // cutoff rotary
        auto *cutoffSlider = section->getEffectControl(Processors::BaseEffect::Filter::Normal::Cutoff::id().value());
        utils::ignore = cutoffSlider->setSizes(knobsHeight);
        cutoffSlider->setPosition(Point{ bounds.getX() + rotaryInterval, bounds.getY() });

        // slope rotary
        auto *slopeSlider = section->getEffectControl(Processors::BaseEffect::Filter::Normal::Slope::id().value());
        utils::ignore = slopeSlider->setSizes(knobsHeight);
        slopeSlider->setPosition(Point{ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
      };

      auto arrangeGate = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
          .withTrimmedTop(knobTopOffset).withHeight(knobsHeight);
        int rotaryInterval = (int)std::round((float)bounds.getWidth() / 3.0f);

        // gain rotary
        auto *gainSlider = section->getEffectControl(Processors::BaseEffect::Filter::Gate::Gain::id().value());
        utils::ignore = gainSlider->setSizes(knobsHeight);
        gainSlider->setPosition(Point{ bounds.getX(), bounds.getY() });

        // threshold rotary
        auto *thresholdSlider = section->getEffectControl(Processors::BaseEffect::Filter::Gate::Threshold::id().value());
        utils::ignore = thresholdSlider->setSizes(knobsHeight);
        thresholdSlider->setPosition(Point{ bounds.getX() + rotaryInterval, bounds.getY() });

        // tilt rotary
        auto *tiltSlider = section->getEffectControl(Processors::BaseEffect::Filter::Gate::Tilt::id().value());
        utils::ignore = tiltSlider->setSizes(knobsHeight);
        tiltSlider->setPosition(Point{ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
      };

      auto arrangeRegular = [&]() { };

      switch (Processors::BaseEffect::Filter::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Filter::Normal:
        arrangeNormal();
        break;
      default:
      case Processors::BaseEffect::Filter::Gate:
        arrangeGate();
        break;
      case Processors::BaseEffect::Filter::Regular:
        arrangeRegular();
        break;
      }
    }

    std::vector<utils::up<Control>> initDynamicsParameters(EffectModuleSection *section, utils::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<Control>> parameters;

      auto initContrast = [&]()
      {
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Dynamics::Contrast::Depth::id().value())));
      };

      auto initClip = [&]()
      {
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Dynamics::Clip::Threshold::id().value())));
      };
      
      auto initCompressor = [&]() { };

      switch (Processors::BaseEffect::Dynamics::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Dynamics::Contrast:
        initContrast();
        break;
      case Processors::BaseEffect::Dynamics::Clip:
        initClip();
        break;
      case Processors::BaseEffect::Dynamics::Compressor:
        initCompressor();
        break;
      default:
        break;
      }

      return parameters;
    }

    void arrangeDynamicsUI(EffectModuleSection *section, juce::Rectangle<int> bounds, utils::string_view type)
    {
      using namespace Framework;

      auto arrangeContrast = [&]()
      {
        // TEST
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        // depth rotary and label
        auto *depthSlider = section->getEffectControl(Processors::BaseEffect::Dynamics::Contrast::Depth::id().value());
        utils::ignore = depthSlider->setSizes(knobsHeight);
        depthSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
      };

      auto arrangeClip = [&]()
      {
        // TEST
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        // depth rotary and label
        auto *thresholdSlider = section->getEffectControl(Processors::BaseEffect::Dynamics::Clip::Threshold::id().value());
        utils::ignore = thresholdSlider->setSizes(knobsHeight);
        thresholdSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
      };

      auto arrangeCompressor = [&]() { };

      switch (Processors::BaseEffect::Dynamics::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Dynamics::Contrast:
        arrangeContrast();
        break;
      case Processors::BaseEffect::Dynamics::Clip:
        arrangeClip();
        break;
      case Processors::BaseEffect::Dynamics::Compressor:
        arrangeCompressor();
        break;
      default:
        break;
      }
    }

    std::vector<utils::up<Control>> initPhaseParameters(EffectModuleSection *section, utils::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<Control>> parameters;

      auto initShift = [&]()
      {
        parameters.reserve(Processors::BaseEffect::Phase::Shift::enum_count_filter(Framework::kGetParameterPredicate));
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Phase::Shift::PhaseShift::id().value())));
        parameters.emplace_back(utils::up<TextSelector>::create(
          baseEffect->getParameter(Processors::BaseEffect::Phase::Shift::Slope::id().value())));
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Phase::Shift::Interval::id().value())));
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Phase::Shift::Offset::id().value())));
      };

      switch (Processors::BaseEffect::Phase::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Phase::Shift:
        initShift();
        break;
      default:
      case Processors::BaseEffect::Phase::Transform:
        break;
      }

      return parameters;
    }

    void arrangePhaseUI(EffectModuleSection *section, juce::Rectangle<int> bounds, utils::string_view type)
    {
      using namespace Framework;

      auto arrangeNormal = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
          .withTrimmedTop(knobTopOffset).withHeight(knobsHeight);
        int rotaryInterval = (int)std::round((float)bounds.getWidth() / 3.0f);

        auto *slopeDropdown = utils::as<TextSelector>(section->getEffectControl(Processors::BaseEffect::Phase::Shift::Slope::id().value()));
        auto *shiftSlider = utils::as<RotarySlider>(section->getEffectControl(Processors::BaseEffect::Phase::Shift::PhaseShift::id().value()));
        shiftSlider->setModifier(slopeDropdown);
        shiftSlider->setLabelPlacement(Placement::right);
        utils::ignore = shiftSlider->setSizes(knobsHeight);
        shiftSlider->setPosition({ bounds.getX(), bounds.getY() });


        auto *intervalSlider = section->getEffectControl(Processors::BaseEffect::Phase::Shift::Interval::id().value());
        utils::as<RotarySlider>(intervalSlider)->setMaxDecimalCharacters(3);
        utils::ignore = intervalSlider->setSizes(knobsHeight);
        intervalSlider->setPosition({ bounds.getX() + rotaryInterval, bounds.getY() });

        auto *offsetSlider = section->getEffectControl(Processors::BaseEffect::Phase::Shift::Offset::id().value());
        utils::as<RotarySlider>(offsetSlider)->setMaxTotalCharacters(6);
        utils::as<RotarySlider>(offsetSlider)->setMaxDecimalCharacters(4);
        utils::ignore = offsetSlider->setSizes(knobsHeight);
        offsetSlider->setPosition({ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
      };

      switch (Processors::BaseEffect::Phase::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Phase::Shift:
        arrangeNormal();
        break;
      default:
      case Processors::BaseEffect::Phase::Transform:
        break;
      }
    }

    std::vector<utils::up<Control>> initPitchParameters(EffectModuleSection *section, utils::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<Control>> parameters;

      auto initResample = [&]()
      {
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Pitch::Resample::Shift::id().value())));
      };

      auto initConstShift = [&]()
      {
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Pitch::ConstShift::Shift::id().value())));
      };

      switch (Processors::BaseEffect::Pitch::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Pitch::Resample:
        initResample();
        break;
      case Processors::BaseEffect::Pitch::ConstShift:
        initConstShift();
        break;
      default:
        break;
      }

      return parameters;
    }

    void arrangePitchUI(EffectModuleSection *section, juce::Rectangle<int> bounds, utils::string_view type)
    {
      using namespace Framework;

      auto arrangeResample = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        auto *shiftSlider = section->getEffectControl(Processors::BaseEffect::Pitch::Resample::Shift::id().value());
        utils::ignore = shiftSlider->setSizes(knobsHeight);
        shiftSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
      };

      auto arrangeConstShift = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

        auto *shiftSlider = section->getEffectControl(Processors::BaseEffect::Pitch::ConstShift::Shift::id().value());
        utils::ignore = shiftSlider->setSizes(knobsHeight);
        shiftSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
      };

      switch (Processors::BaseEffect::Pitch::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Pitch::Resample:
        arrangeResample();
        break;
      case Processors::BaseEffect::Pitch::ConstShift:
        arrangeConstShift();
        break;
      default:
        break;
      }
    }

    std::vector<utils::up<Control>> initDestroyParameters(EffectModuleSection *section, utils::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<Control>> parameters;

      auto initReinterpret = [&]()
      {
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Destroy::Reinterpret::Attenuation::id().value())));
        parameters.emplace_back(utils::up<TextSelector>::create(
          baseEffect->getParameter(Processors::BaseEffect::Destroy::Reinterpret::Mapping::id().value())));
      };

      switch (Processors::BaseEffect::Destroy::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Destroy::Reinterpret:
        initReinterpret();
        break;
      default:
        break;
      }

      return parameters;
    }

    void arrangeDestroyUI(EffectModuleSection *section, juce::Rectangle<int> bounds, utils::string_view type)
    {
      using namespace Framework;

      auto arrangeReinterpret = [&]()
      {
        int knobEdgeOffset = scaleValueRoundInt(32);
        int knobTopOffset = scaleValueRoundInt(32);

        int knobsHeight = scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);
        int dropdownHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);

        int sliderToDropdownMargin = scaleValueRoundInt(16);

        bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
          .withTrimmedTop(knobTopOffset).withHeight(knobsHeight);

        auto *attenuationSlider = utils::as<RotarySlider>(section->getEffectControl(Processors::BaseEffect::Destroy::Reinterpret::Attenuation::id().value()));
        attenuationSlider->setLabelPlacement(Placement::right);
        auto sliderBounds = attenuationSlider->setSizes(knobsHeight);
        attenuationSlider->setPosition({ bounds.getX(), bounds.getY() });

        auto *mappingDropdown = utils::as<TextSelector>(section->getEffectControl(Processors::BaseEffect::Destroy::Reinterpret::Mapping::id().value()));
        mappingDropdown->setLabelPlacement(Placement::above | Placement::left);
        mappingDropdown->addLabel();
        auto dropdownBounds = mappingDropdown->setSizes(dropdownHeight);
        mappingDropdown->setPosition({ 
          bounds.getX() + sliderBounds.getRight() - dropdownBounds.getX() + sliderToDropdownMargin,
          bounds.getY() + sliderBounds.getHeight() / 2 });
      };

      switch (Processors::BaseEffect::Destroy::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Destroy::Reinterpret:
        arrangeReinterpret();
        break;
      default:
        break;
      }
    }
  }*/

}

namespace Generation
{
  namespace Filter
  {
    utils::span<Interface::Control *> 
    createUINormal(utils::bumpArena *arena, Interface::EffectModuleSection *section, 
      EffectModule::EffectData *effectData)
    {
      using namespace Interface;

      Component *holder = anew(arena, Component, {});
      holder->sizingFlags = (Component::SizingFlags)(Component::GrowableX | Component::GrowableY);
      holder->padding = { 32, 0, 32, 0 };

      utils::vector<Control *> controls{ arena, effectData->parameterCount };
      auto *parameter = effectData->parameters;
      for (usize i = 0; i < effectData->parameterCount; (++i), (parameter = parameter->next))
      {
        auto *rotary = anew(arena, CombinationRotarySlider, {});
        rotary->placement = Placement::justifyX;
        rotary->rotary.arena = arena;
        rotary->rotary.changeLinkedParameter(*parameter);
        controls.emplaceBack(&rotary->rotary);
        holder->addChildComponent(rotary);
      }

      section->effectHolder.addChildComponent(holder);

      return controls;
    }

    utils::span<Interface::Control *>
    createUIGate(utils::bumpArena *arena, Interface::EffectModuleSection *section,
      EffectModule::EffectData *effectData)
    {
      using namespace Interface;


      return {};
    }
  }
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
