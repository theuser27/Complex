/*
  ==============================================================================

    EffectModuleSection.hpp
    Created: 14 Feb 2023 2:29:16am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/parameters.hpp"
#include "Framework/parameter_bridge.hpp"
#include "../Components/DraggableComponent.hpp"
#include "../Components/BaseControl.hpp"
#include "BaseSection.hpp"

namespace Framework
{
  class ParameterBridge;
}

namespace Generation
{
  class BaseEffect;
  class EffectModule;
}

namespace Interface
{
  class PlainShapeComponent;
  class EmptySlider;
  class NumberBox;
  class TextSelector;
  class SpectralMaskComponent;
  class EffectsLaneSection;

  class EffectModuleSection final : public ProcessorSection,
    public Generation::BaseProcessorListener, public Framework::ParameterBridge::Listener
  {
  public:
    enum MenuId
    {
      kCancel = 0,
      kDeleteInstance,
      kCopyInstance,
      kInitInstance
    };

    static constexpr int kSpectralMaskMargin = 2;
    static constexpr int kTopMenuHeight = 28;
    static constexpr int kDraggableSectionWidth = 36;
    static constexpr int kIconSize = 14;
    static constexpr int kIconToTextSelectorMargin = 4;
    static constexpr int kDelimiterWidth = 1;
    static constexpr int kDelimiterToTextSelectorMargin = 2;
    static constexpr int kNumberBoxToPowerButtonMargin = 6;
    static constexpr int kLabelToNumberBoxMargin = 4;
    static constexpr int kPowerButtonPadding = 8;

    static constexpr int kOuterPixelRounding = 8;
    static constexpr int kInnerPixelRounding = 3;

    EffectModuleSection(Generation::EffectModule *effectModule, EffectsLaneSection *laneSection);
    ~EffectModuleSection() override;
    auto createCopy() const -> utils::up<EffectModuleSection>;

    void resized() override;
    void renderOpenGlComponents(OpenGlWrapper &openGl) override 
    {
      ScopedIgnoreClip ic{ openGl.parentStack, ignoreClipIncluding_ };
      ProcessorSection::renderOpenGlComponents(openGl);
    }
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDown([[maybe_unused]] BaseControl *slider) override { }
    void paintBackground(juce::Graphics &g) override;
    void controlValueChanged(BaseControl *control) override;
    auto getPowerButtonBounds() const noexcept -> juce::Rectangle<int> override
    {
      auto widthHeight = (int)std::round(scaleValue(kDefaultActivatorSize));
      return { getWidth() - scaleValueRoundInt(kPowerButtonPadding) - widthHeight,
        getYMaskOffset() + utils::centerAxis(widthHeight, scaleValueRoundInt(kTopMenuHeight)),
        widthHeight, widthHeight };
    }

    // unfortunately we need both callbacks in order to 
    // handle unmapping of parameters that don't have a UI control, 
    //  automationMappingChanged for mapping
    //  parameterLinkReset for unmapping
    void automationMappingChanged(BaseControl *control, bool isUnmapping) override;
    void parameterLinkReset(Framework::ParameterBridge *bridge,
      Framework::ParameterLink *newLink, Framework::ParameterLink *oldLink) override;

    // (re)initialises parameter to be whatever they need to be for the specific module type/effect mode
    void initialiseParameters();

    // sets positions and dimensions of the module header
    void arrangeHeader();

    // sets positions and dimensions of contained UI elements for the specific module type/effect mode
    void arrangeUI();

    // paints static/background elements of the specific module
    void paintUIBackground(juce::Graphics &g)
    {
      if (paintBackgroundFunction_)
        paintBackgroundFunction_(g, this);
    }

    auto getDraggableComponent() noexcept -> DraggableComponent & { return draggableBox_; }
    auto getLaneSection() const noexcept -> EffectsLaneSection * { return laneSection_; }
    auto getEffect() noexcept -> Generation::BaseEffect *;
    auto getAlgorithmSelector() const noexcept -> TextSelector &;
    auto getEffectControl(utils::string_view name) -> BaseControl *;

    auto getUIBounds() const noexcept -> juce::Rectangle<int>
    { return getLocalBounds().withTop(getYMaskOffset() + scaleValueRoundInt(kTopMenuHeight) + 1); }

    void handlePopupResult(int result) const noexcept;
    void setIgnoreClip(BaseComponent *ignoreClipIncluding) noexcept { ignoreClipIncluding_ = ignoreClipIncluding; }
  private:
    auto createPopupMenu() const noexcept -> PopupItems;

    void setEffectType(utils::string_view type);

    auto getYMaskOffset() const noexcept -> int
    {
      auto offset = scaleValueRoundInt(kSpectralMaskMargin) + 
        scaleValueRoundInt(kSpectralMaskContractedHeight);
      return scaleValueRoundInt((float)offset);
    }

    utils::shared_value<BaseComponent *> ignoreClipIncluding_ = nullptr;

    utils::up<SpectralMaskComponent> maskComponent_;

    DraggableComponent draggableBox_{};
    utils::up<PlainShapeComponent> effectTypeIcon_;
    utils::up<TextSelector> effectTypeSelector_;
    utils::up<TextSelector> effectAlgoSelector_;
    ControlContainer topLeftContainer_{};

    utils::up<NumberBox> mixNumberBox_;
    utils::up<PowerButton> moduleActivator_;

    EffectsLaneSection *laneSection_ = nullptr;
    Generation::EffectModule *effectModule_ = nullptr;
    std::vector<utils::up<BaseControl>> effectControls_;
    utils::VectorMap<usize, Framework::ParameterBridge *> parameterMappings{};

    auto (*initialiseParametersFunction_)(EffectModuleSection *section, 
      utils::string_view type) -> std::vector<utils::up<BaseControl>> = nullptr;
    void (*arrangeUIFunction_)(EffectModuleSection *section, 
      juce::Rectangle<int> bounds, utils::string_view type) = nullptr;
    void (*paintBackgroundFunction_)(juce::Graphics &g, EffectModuleSection *section) = nullptr;
    utils::span<const utils::pair<utils::string_view, usize>> effectParameterCounts_{};
  };	
}
