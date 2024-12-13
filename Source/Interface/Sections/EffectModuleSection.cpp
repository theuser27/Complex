/*
  ==============================================================================

    EffectModuleSection.cpp
    Created: 14 Feb 2023 2:29:16am
    Author:  theuser27

  ==============================================================================
*/

#include "EffectModuleSection.hpp"

#include "Plugin/ProcessorTree.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "Generation/EffectModules.hpp"
#include "../LookAndFeel/Miscellaneous.hpp"
#include "../LookAndFeel/Fonts.hpp"
#include "../Components/OpenGlImage.hpp"
#include "../Components/BaseButton.hpp"
#include "../Components/BaseSlider.hpp"
#include "../Components/Spectrogram.hpp"
#include "../Components/PinBoundsBox.hpp"
#include "EffectsLaneSection.hpp"

namespace Interface
{
  class EmptySlider final : public PinSlider
  {
  public:
    EmptySlider(Framework::ParameterValue *parameter) : PinSlider(parameter)
    {
      setShouldShowPopup(true);
      removeAllOpenGlComponents();
    }

    void mouseDown(const MouseEvent &e) override
    {
      // an unfortunate consequence
      if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        getParentComponent()->mouseDown(e);
      else
        PinSlider::mouseDown(e);
    }

    void paint(Graphics &) override { }
    void redoImage() override { }
    void setComponentsBounds(bool) override { }
  };

  class SpectralMaskComponent final : public PinBoundsBox
  {
  public:
    SpectralMaskComponent(Framework::ParameterValue *lowBound, Framework::ParameterValue *highBound, 
      Framework::ParameterValue *shiftBounds) : PinBoundsBox("Spectral Mask", lowBound, highBound), shiftBounds_(shiftBounds)
    {
      using namespace Framework;

      /*highlight_->setCustomRenderFunction([this](OpenGlWrapper &openGl, bool animate)
        {
          simd_float shiftValues = shiftBounds_->getInternalValue<simd_float>(kDefaultSampleRate);
          simd_float lowValues = simd_float::clamp(lowBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
          simd_float highValues = simd_float::clamp(highBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
          highlight_->setStaticValues(lowValues[0], 0);
          highlight_->setStaticValues(lowValues[2], 1);
          highlight_->setStaticValues(highValues[0], 2);
          highlight_->setStaticValues(highValues[2], 3);

          highlight_->render(openGl, animate);
        });*/

      setInterceptsMouseClicks(true, true);

      addControl(&shiftBounds_);
      shiftBounds_.toBack();
    }

    void paint(Graphics &g) override
    {
      auto shiftValue = Framework::scaleValue(shiftBounds_.getValue(), shiftBounds_.getParameterDetails());
      paintHighlightBox(g, (float)lowBound_->getValue(), (float)highBound_->getValue(),
        getColour(Skin::kWidgetPrimary1).withAlpha(0.15f), (float)shiftValue);
    }
    void resized() override
    {
      shiftBounds_.setBounds(getLocalBounds());
      shiftBounds_.setTotalRange(2 * getWidth());
      PinBoundsBox::resized();
    }
    void controlValueChanged(BaseControl *control) override
    {
      if (&shiftBounds_ == control)
      {
        highlight_.redrawImage();
        //simd_float shiftValues = shiftBounds_->getInternalValue<simd_float>(kDefaultSampleRate);
        //simd_float lowValues = simd_float::clamp(lowBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
        //simd_float highValues = simd_float::clamp(highBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
        //highlight_->setStaticValues(lowValues[0], 0);
        //highlight_->setStaticValues(lowValues[2], 1);
        //highlight_->setStaticValues(highValues[0], 2);
        //highlight_->setStaticValues(highValues[2], 3);
        return;
      }

      PinBoundsBox::controlValueChanged(control);
    }

  private:
    EmptySlider shiftBounds_;
  };

  EffectModuleSection::EffectModuleSection(Generation::EffectModule *effectModule, EffectsLaneSection *laneSection) :
    ProcessorSection("Effect Module Section", effectModule), laneSection_(laneSection), effectModule_(effectModule)
  {
    using namespace Framework;

    setInterceptsMouseClicks(true, true);

    draggableBox_.setDraggedComponent(this);
    addAndMakeVisible(draggableBox_);

    effectTypeSelector_ = utils::up<TextSelector>::create(
      effectModule->getParameter(Processors::EffectModule::ModuleType::id().value()),
      Fonts::instance()->getInterVFont().withStyle(Font::bold));
    effectTypeSelector_->setOptionsTitle("Change Module");
    addControl(effectTypeSelector_.get());

    effectTypeIcon_ = utils::up<PlainShapeComponent>::create("Effect Type Icon");
    effectTypeIcon_->setJustification(Justification::centred);
    effectTypeIcon_->setAlwaysOnTop(true);
    effectTypeSelector_->setExtraIcon(effectTypeIcon_.get());
    addOpenGlComponent(effectTypeIcon_.get());

    mixNumberBox_ = utils::up<NumberBox>::create(
      effectModule->getParameter(Processors::EffectModule::ModuleMix::id().value()));
    mixNumberBox_->setMaxTotalCharacters(5);
    mixNumberBox_->setMaxDecimalCharacters(2);
    addControl(mixNumberBox_.get());

    moduleActivator_ = utils::up<PowerButton>::create(
      effectModule->getParameter(Processors::EffectModule::ModuleEnabled::id().value()));
    addControl(moduleActivator_.get());
    setActivator(moduleActivator_.get());

    auto *baseEffect = effectModule->getEffect();

    effectAlgoSelector_ = utils::up<TextSelector>::create(
      baseEffect->getParameter(Processors::BaseEffect::Algorithm::id().value()),
      Fonts::instance()->getInterVFont());
    effectAlgoSelector_->setOptionsTitle("Change Algorithm");
    addControl(effectAlgoSelector_.get());

    topLeftContainer_.addControl(effectTypeSelector_.get());
    topLeftContainer_.addControl(effectAlgoSelector_.get());
    topLeftContainer_.setAnchor(Placement::left);

    maskComponent_ = utils::up<SpectralMaskComponent>::create(
      baseEffect->getParameter(Processors::BaseEffect::LowBound::id().value()),
      baseEffect->getParameter(Processors::BaseEffect::HighBound::id().value()),
      baseEffect->getParameter(Processors::BaseEffect::ShiftBounds::id().value()));
    addSubOpenGlContainer(maskComponent_.get());

    setEffectType(baseEffect->getProcessorType());
    initialiseParameters();
  }

  EffectModuleSection::~EffectModuleSection() = default;

  utils::up<EffectModuleSection> EffectModuleSection::createCopy() const
  {
    auto *copiedModule = effectModule_->getProcessorTree()->copyProcessor(effectModule_);
    return utils::up<EffectModuleSection>::create(copiedModule, laneSection_);
  }

  void EffectModuleSection::resized()
  {
    ProcessorSection::resized();

    arrangeHeader();
    arrangeUI();
    repaintBackground();
  }

  void EffectModuleSection::mouseDown(const MouseEvent &e)
  {
    if (!e.mods.isPopupMenu())
      return;

    int topMenuHeight = scaleValueRoundInt(kTopMenuHeight);
    int yOffset = getYMaskOffset();
    juce::Rectangle dropdownHitbox{ 0, yOffset, getWidth(), topMenuHeight };

    if (!dropdownHitbox.contains(e.getPosition()) && !e.mods.isPopupMenu())
      return;

    PopupItems options = createPopupMenu();
    showPopupSelector(this, e.getPosition(), COMPLEX_MOV(options),
      [this](int selection) { handlePopupResult(selection); });
  }

  void EffectModuleSection::initialiseParameters()
  {
    COMPLEX_ASSERT(initialiseParametersFunction_ && "No initParametersFunction was provided");

    for (auto &control : effectControls_)
      removeControl(control.get());

    effectControls_.clear();

    effectControls_ = initialiseParametersFunction_(this, effectAlgoSelector_->getTextValue());
    for (auto &control : effectControls_)
      addControl(control.get());
  }

  void EffectModuleSection::arrangeHeader()
  {
    // top
    int spectralMaskHeight = scaleValueRoundInt(kSpectralMaskContractedHeight);
    maskComponent_->setBounds({ 0, 0, getWidth(), spectralMaskHeight });
    maskComponent_->setRounding(scaleValue(kOuterPixelRounding), scaleValue(kInnerPixelRounding));

    int yOffset = getYMaskOffset();
    int topMenuHeight = scaleValueRoundInt(kTopMenuHeight);

    // right hand side
    int mixNumberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
    auto mixNumberBoxBounds = mixNumberBox_->setSizes(mixNumberBoxHeight);
    mixNumberBox_->setPosition(Point{ moduleActivator_->getX() - mixNumberBoxBounds.getRight() - scaleValueRoundInt(kNumberBoxToPowerButtonMargin),
      yOffset + utils::centerAxis(mixNumberBoxHeight, topMenuHeight) });

    // left hand side
    draggableBox_.setBounds(juce::Rectangle{ 0, yOffset, scaleValueRoundInt(kDraggableSectionWidth), topMenuHeight });

    int effectTypeSelectorIconDimensions = scaleValueRoundInt(kIconSize);
    effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
    effectTypeIcon_->setSize(effectTypeSelectorIconDimensions, effectTypeSelectorIconDimensions);

    int effectSelectorsHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);
    topLeftContainer_.setParentAndBounds(this, { draggableBox_.getRight(),
      yOffset, getWidth() - draggableBox_.getRight(), topMenuHeight });
    topLeftContainer_.setControlSizes(effectTypeSelector_.get(), effectSelectorsHeight);
    topLeftContainer_.setControlSizes(effectAlgoSelector_.get(), effectSelectorsHeight);
    topLeftContainer_.setControlSpacing(scaleValueRoundInt(kDelimiterWidth) +
      2 * scaleValueRoundInt(kDelimiterToTextSelectorMargin));
    topLeftContainer_.repositionControls();
  }

  void EffectModuleSection::arrangeUI()
  {
    COMPLEX_ASSERT(arrangeUIFunction_ && "No arrangeUIFunction was provided");
    arrangeUIFunction_(this, getUIBounds(), effectAlgoSelector_->getTextValue());
  }

  void EffectModuleSection::paintBackground(Graphics &g)
  {
    maskComponent_->setRoundedCornerColour(getColour(Skin::kBackground));

    // drawing body
    int yOffset = getYMaskOffset();
    auto rectangleBounds = getLocalBounds().withTop(yOffset).toFloat();

    float innerRounding = scaleValue(kInnerPixelRounding);
    float outerRounding = scaleValue(kOuterPixelRounding);

    Path rectangle;
    rectangle.startNewSubPath(rectangleBounds.getCentreX(), rectangleBounds.getY());

    rectangle.lineTo(rectangleBounds.getRight() - innerRounding, rectangleBounds.getY());
    rectangle.quadraticTo(rectangleBounds.getRight(), rectangleBounds.getY(), 
      rectangleBounds.getRight(), rectangleBounds.getY() + innerRounding);

    rectangle.lineTo(rectangleBounds.getRight(), rectangleBounds.getBottom() - outerRounding);
    rectangle.quadraticTo(rectangleBounds.getRight(), rectangleBounds.getBottom(),
      rectangleBounds.getRight() - outerRounding, rectangleBounds.getBottom());

    rectangle.lineTo(rectangleBounds.getX() + outerRounding, rectangleBounds.getBottom());
    rectangle.quadraticTo(rectangleBounds.getX(), rectangleBounds.getBottom(),
      rectangleBounds.getX(), rectangleBounds.getBottom() - outerRounding);

    rectangle.lineTo(rectangleBounds.getX(), rectangleBounds.getY() + innerRounding);
    rectangle.quadraticTo(rectangleBounds.getX(), rectangleBounds.getY(),
      rectangleBounds.getX() + innerRounding, rectangleBounds.getY());

    rectangle.closeSubPath();

    g.setColour(getColour(Skin::kBody));
    g.fillPath(rectangle);

    // drawing draggable box
    g.saveState();
    g.setOrigin(0, yOffset);
    draggableBox_.paint(g);
    g.restoreState();

    int topMenuHeight = scaleValueRoundInt(kTopMenuHeight);
    int delimiterToTextSelectorMargin = scaleValueRoundInt(kDelimiterToTextSelectorMargin);

    // drawing separator line between header and main body
    g.setColour(getColour(Skin::kBackgroundElement));
    g.fillRect(0.0f, rectangleBounds.getY() + (float)topMenuHeight, rectangleBounds.getRight(), 1.0f);

    // drawing separator line between type and algo
    int lineX = effectTypeSelector_->getRight() + delimiterToTextSelectorMargin;
    int lineY = (int)rectangleBounds.getY() + utils::centerAxis(topMenuHeight / 2, topMenuHeight);
    g.fillRect(lineX, lineY, 1, topMenuHeight / 2);

    paintUIBackground(g);
  }

  void EffectModuleSection::controlValueChanged(BaseControl *control)
  {
    if (control == effectTypeSelector_.get() && effectModule_)
    {
      if (!changeEffectOrAlgo(false))
        return;

      repaintBackground();
    }
    else if (control == effectAlgoSelector_.get() && effectModule_)
    {
      changeEffectOrAlgo(true);
      /*initialiseParameters();
      arrangeUI();*/
      repaintBackground();
    }
    else
      ProcessorSection::controlValueChanged(control);
  }

  static constexpr auto kCommonEffectParameters = Framework::Processors::BaseEffect::enum_ids_filter<Framework::kGetParameterPredicate, true>();
  static constexpr auto kEffectModuleParameters = Framework::Processors::EffectModule::enum_ids_filter<Framework::kGetParameterPredicate, true>();

  void EffectModuleSection::automationMappingChanged(BaseControl *control, bool isUnmapping)
  {
    if (!effectModule_ || isUnmapping)
      return;

    if (std::ranges::find(kCommonEffectParameters, control->getParameterDetails().id) != 
      kCommonEffectParameters.end())
      return;

    if (std::ranges::find(kEffectModuleParameters, control->getParameterDetails().id) !=
      kEffectModuleParameters.end())
      return;

    auto algoId = effectAlgoSelector_->getTextValue();
    usize parametersStart = 0;
    for (auto &[id, count] : effectParameterCounts_)
    {
      if (id == algoId)
        break;
      parametersStart += count;
    }
    auto parameterLink = control->getParameterLink();
    auto index = effectModule_->getEffect()->getParameterIndex(parameterLink->parameter);
    index -= kCommonEffectParameters.size();
    index -= parametersStart;

    parameterMappings.add(index, parameterLink->hostControl);
    parameterLink->hostControl->addListener(this);
  }

  void EffectModuleSection::parameterLinkReset(Framework::ParameterBridge *bridge,
    Framework::ParameterLink *newLink, Framework::ParameterLink *)
  {
    if (newLink)
      return;

    std::erase_if(parameterMappings.data, [&](const auto &element) { return element.second == bridge; });
    bridge->removeListener(this);
  }

  Generation::BaseEffect *EffectModuleSection::getEffect() noexcept { return effectModule_->getEffect(); }
  TextSelector &EffectModuleSection::getAlgorithmSelector() const noexcept { return *effectAlgoSelector_; }

  BaseControl *EffectModuleSection::getEffectControl(std::string_view id)
  {
    using namespace Framework;

    static constexpr auto baseEffectIds = Processors::BaseEffect::enum_ids_filter<kGetParameterPredicate, true>();
    if (std::ranges::find(baseEffectIds, id) != baseEffectIds.end())
    {
      if (id == Processors::BaseEffect::Algorithm::id().value())
        return effectAlgoSelector_.get();

      return maskComponent_->getControl(id);
    }

    for (auto &control : effectControls_)
      if (control->getParameterDetails().id == id)
        return control.get();

    COMPLEX_ASSERT_FALSE("Parameter could not be found");
    return nullptr;
  }

  void EffectModuleSection::handlePopupResult(int result) const noexcept
  {
    if (result == kDeleteInstance)
    {
      // make sure nothing touches this after the call runs
      utils::ignore = laneSection_->deleteModule(this, true);
    }
    else if (result == kCopyInstance)
    {
      // TODO: copy right click option on EffectModuleSection
    }
    else if (result == kInitInstance)
    {
      // TODO: initialisation right click option on EffectModuleSection
    }
  }

  PopupItems EffectModuleSection::createPopupMenu() const noexcept
  {
    PopupItems options{};
    options.addDelimiter("Module Settings");
    auto &deleteOption = options.addEntry(kDeleteInstance, "D" COMPLEX_UNDERSCORE_LITERAL "elete");
    deleteOption.shortcut = 'D';
    auto &copyOption = options.addEntry(kCopyInstance, "C" COMPLEX_UNDERSCORE_LITERAL "opy (TODO)");
    copyOption.shortcut = 'C';
    auto &initialiseOption = options.addEntry(kInitInstance, "I" COMPLEX_UNDERSCORE_LITERAL "nitialise");
    initialiseOption.shortcut = 'I';

    return options;
  }

  bool EffectModuleSection::changeEffectOrAlgo(bool changeAlgo)
  {
    using namespace Framework;

    Generation::BaseEffect *newEffect;
    if (changeAlgo)
      newEffect = getEffect();
    else
    {
      auto effectType = effectTypeSelector_->getTextValue(false);
      auto effectIndex = Processors::BaseEffect::enum_value_by_id(effectType);
      if (!effectIndex.has_value())
        return false;

      newEffect = effectModule_->changeEffect(effectType);

      // resetting UI
      setEffectType(newEffect->getProcessorType());

      // replacing the parameters for algorithm and mask sliders
      for (auto &id : kCommonEffectParameters)
      {
        auto *control = getEffectControl(id);
        auto *newEffectParameter = newEffect->getParameter(id);

        if (auto parameterBridge = control->getParameterLink()->hostControl)
          parameterBridge->resetParameterLink(newEffectParameter->getParameterLink(), false);

        control->changeLinkedParameter(*newEffectParameter,
          id == Processors::BaseEffect::Algorithm::id().value());
        control->resized();
      }

      effectTypeSelector_->resized();
      effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
      mixNumberBox_->resized();
      moduleActivator_->resized();
      maskComponent_->resized();
    }

    initialiseParameters();

    // replacing mapped out parameters, if there are any
    usize parametersCount = 0;
    usize parametersStart = 0;
    auto algoId = effectAlgoSelector_->getTextValue();
    for (auto &[id, count] : effectParameterCounts_)
    {
      if (id == algoId)
      {
        parametersCount = count;
        break;
      }
      parametersStart += count;
    }

    static constexpr auto kReservedString = std::string_view{ " (Reserved)" };

    for (auto &[mappingIndex, parameterBridge] : parameterMappings.data)
    {
      if (auto *link = parameterBridge->getParameterLink(); link && link->parameter)
        link->parameter->changeBridge(nullptr);

      if (mappingIndex >= parametersCount)
      {
        String name = parameterBridge->getName();
        name += { kReservedString.data(), kReservedString.size() };
        parameterBridge->setCustomName(name);
        continue;
      }

      auto newParameterLink = newEffect->getParameterUnchecked(
        kCommonEffectParameters.size() + parametersStart + mappingIndex)->getParameterLink();

      // if the new parameter is the old one, just add the bridge and fix name
      if (parameterBridge->getParameterLink() == newParameterLink)
      {
        parameterBridge->setCustomName(parameterBridge->getName()
          .dropLastCharacters((int)kReservedString.size()));
        newParameterLink->parameter->changeBridge(parameterBridge);
      }
      else
      {
        parameterBridge->resetParameterLink(newEffect->getParameterUnchecked(
          kCommonEffectParameters.size() + parametersStart + mappingIndex)->getParameterLink(), false);
      }
    }

    arrangeUI();

    return true;
  }


  namespace
  {
    std::vector<utils::up<BaseControl>> initFilterParameters(EffectModuleSection *section, std::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<BaseControl>> parameters;

      auto initNormal = [&]()
      {
        parameters.reserve(Processors::BaseEffect::Filter::Normal::enum_count_filter(Framework::kGetParameterPredicate));
        auto gain = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Gain::id().value()));
        gain->setShouldUsePlusMinusPrefix(true);
        parameters.emplace_back(COMPLEX_MOV(gain));
        auto cutoff = utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Cutoff::id().value()));
        cutoff->setMaxTotalCharacters(6);
        cutoff->setMaxDecimalCharacters(4);
        parameters.emplace_back(COMPLEX_MOV(cutoff));
        parameters.emplace_back(utils::up<RotarySlider>::create(
          baseEffect->getParameter(Processors::BaseEffect::Filter::Normal::Slope::id().value())));
      };

      auto initRegular = [&]() { };

      switch (Processors::BaseEffect::Filter::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Filter::Normal:
        initNormal();
        break;
      default:
      case Processors::BaseEffect::Filter::Regular:
        initRegular();
        break;
      }

      return parameters;
    }

    void arrangeFilterUI(EffectModuleSection *section, juce::Rectangle<int> bounds, std::string_view type)
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

      auto arrangeRegular = [&]() { };

      switch (Processors::BaseEffect::Filter::enum_value_by_id(type).value())
      {
      case Processors::BaseEffect::Filter::Normal:
        arrangeNormal();
        break;
      default:
      case Processors::BaseEffect::Filter::Regular:
        arrangeRegular();
        break;
      }
    }

    std::vector<utils::up<BaseControl>> initDynamicsParameters(EffectModuleSection *section, std::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<BaseControl>> parameters;

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

    void arrangeDynamicsUI(EffectModuleSection *section, juce::Rectangle<int> bounds, std::string_view type)
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

    std::vector<utils::up<BaseControl>> initPhaseParameters(EffectModuleSection *section, std::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<BaseControl>> parameters;

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

    void arrangePhaseUI(EffectModuleSection *section, juce::Rectangle<int> bounds, std::string_view type)
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

    std::vector<utils::up<BaseControl>> initPitchParameters(EffectModuleSection *section, std::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<BaseControl>> parameters;

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

    void arrangePitchUI(EffectModuleSection *section, juce::Rectangle<int> bounds, std::string_view type)
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

    std::vector<utils::up<BaseControl>> initDestroyParameters(EffectModuleSection *section, std::string_view type)
    {
      using namespace Framework;

      auto *baseEffect = section->getEffect();
      std::vector<utils::up<BaseControl>> parameters;

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

    void arrangeDestroyUI(EffectModuleSection *section, juce::Rectangle<int> bounds, std::string_view type)
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
  }

  void EffectModuleSection::setEffectType(std::string_view type)
  {
    using namespace Framework;

    auto getEffectParameterCounts = []<nested_enum::NestedEnum Type>()
    {
      static constexpr auto typeCounts = []<typename ... Ts>(const std::tuple<nested_enum::type_identity<Ts>...> &)
      {
        return utils::array{ utils::pair{ Ts::id().value(), Ts::enum_count(nested_enum::All) }... };
      }(Type::template enum_subtypes_filter<Framework::kGetActiveAlgoPredicate>());

      return std::span{ typeCounts };
    };

    paintBackgroundFunction_ = nullptr;

    if (type == Processors::BaseEffect::Utility::id().value())
    {
      
    }
    else if (type == Processors::BaseEffect::Filter::id().value())
    {
      initialiseParametersFunction_ = initFilterParameters;
      arrangeUIFunction_ = arrangeFilterUI;
      effectParameterCounts_ = getEffectParameterCounts.template operator()<Processors::BaseEffect::Filter::type>();

      setSkinOverride(Skin::kFilterModule);
      maskComponent_->setSkinOverride(Skin::kFilterModule);
      effectTypeIcon_->setShapes(Paths::filterIcon());
    }
    else if (type == Processors::BaseEffect::Dynamics::id().value())
    {
      initialiseParametersFunction_ = initDynamicsParameters;
      arrangeUIFunction_ = arrangeDynamicsUI;
      effectParameterCounts_ = getEffectParameterCounts.template operator()<Processors::BaseEffect::Dynamics::type>();

      setSkinOverride(Skin::kDynamicsModule);
      maskComponent_->setSkinOverride(Skin::kDynamicsModule);
      effectTypeIcon_->setShapes(Paths::dynamicsIcon());
    }
    else if (type == Processors::BaseEffect::Phase::id().value())
    {
      initialiseParametersFunction_ = initPhaseParameters;
      arrangeUIFunction_ = arrangePhaseUI;
      effectParameterCounts_ = getEffectParameterCounts.template operator()<Processors::BaseEffect::Phase::type>();

      setSkinOverride(Skin::kPhaseModule);
      maskComponent_->setSkinOverride(Skin::kPhaseModule);
      effectTypeIcon_->setShapes(Paths::phaseIcon());
    }
    else if (type == Processors::BaseEffect::Pitch::id().value())
    {
      initialiseParametersFunction_ = initPitchParameters;
      arrangeUIFunction_ = arrangePitchUI;
      effectParameterCounts_ = getEffectParameterCounts.template operator()<Processors::BaseEffect::Pitch::type>();

      setSkinOverride(Skin::kPitchModule);
      maskComponent_->setSkinOverride(Skin::kPitchModule);
      effectTypeIcon_->setShapes(Paths::pitchIcon());
    }
    else if (type == Processors::BaseEffect::Stretch::id().value())
    {
      
    }
    else if (type == Processors::BaseEffect::Warp::id().value())
    {
      
    }
    else if (type == Processors::BaseEffect::Destroy::id().value())
    {
      initialiseParametersFunction_ = initDestroyParameters;
      arrangeUIFunction_ = arrangeDestroyUI;
      effectParameterCounts_ = getEffectParameterCounts.template operator()<Processors::BaseEffect::Destroy::type>();

      setSkinOverride(Skin::kDestroyModule);
      maskComponent_->setSkinOverride(Skin::kDestroyModule);
      effectTypeIcon_->setShapes(Paths::destroyIcon());
    }
  }
}
