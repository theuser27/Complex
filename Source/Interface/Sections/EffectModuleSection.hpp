
// Created: 2023-02-14 02:29:16

#pragma once

#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "../Components/DraggableComponent.hpp"
#include "../Components/BaseControl.hpp"

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
	class DrawComponent;
	class EmptySlider;
	class NumberBox;
	class TextSelector;
	class SpectralMaskComponent;
	class EffectsLaneSection;

	class EffectModuleSection final : public Component,
		public Framework::ParameterBridge::Listener
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
		auto createCopy() const -> utils::up<EffectModuleSection>;

		bool render(OpenGlWrapper &openGl) override;

		void resized() override;
		void mouseDown(const MouseEvent &e) override;
		void controlValueChanged(Control *control) override;

		// unfortunately we need both callbacks in order to 
		// handle unmapping of parameters that don't have a UI control, 
		//  automationMappingChanged for mapping
		//  parameterLinkReset for unmapping
		void automationMappingChanged(Control *control, bool isUnmapping) override;
		void parameterLinkReset(Framework::ParameterBridge *bridge,
			Framework::ParameterLink *newLink, Framework::ParameterLink *oldLink) override;

		// (re)initialises parameter to be whatever they need to be for the specific module type/effect mode
		void initialiseParameters();

		// sets positions and dimensions of the module header
		void arrangeHeader();

		// sets positions and dimensions of contained UI elements for the specific module type/effect mode
		void arrangeUI();

		// paints static/background elements of the specific module
		void paintUIBackground(Graphics &g)
		{
			if (paintBackgroundFunction_)
				paintBackgroundFunction_(g, this);
		}

		void handlePopupResult(int result) const noexcept;

		auto createPopupMenu() const noexcept -> PopupItem;

		void setEffectType(utils::string_view type);

		utils::up<SpectralMaskComponent> maskComponent_;

		DraggableComponent draggableBox_{};
		utils::up<DrawComponent> effectTypeIcon_;
		utils::up<TextSelector> effectTypeSelector_;
		utils::up<TextSelector> effectAlgoSelector_;

		utils::up<NumberBox> mixNumberBox_;
		utils::up<PowerButton> moduleActivator_;

		EffectsLaneSection *laneSection_ = nullptr;
		Generation::EffectModule *effectModule_ = nullptr;
		utils::vector<Control *> effectControls_;
		utils::vector_map<usize, Framework::ParameterBridge *> parameterMappings{};

		auto (*initialiseParametersFunction_)(EffectModuleSection *section, 
			utils::string_view type) -> std::vector<utils::up<Control>> = nullptr;
		void (*arrangeUIFunction_)(EffectModuleSection *section, 
			Rectangle<int> bounds, utils::string_view type) = nullptr;
		void (*paintBackgroundFunction_)(Graphics &g, EffectModuleSection *section) = nullptr;
		utils::span<const utils::pair<utils::string_view, usize>> effectParameterCounts_{};
	};	
}
