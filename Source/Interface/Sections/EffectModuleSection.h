/*
	==============================================================================

		EffectModuleSection.h
		Created: 14 Feb 2023 2:29:16am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Generation/EffectModules.h"
#include "../Components/DraggableComponent.h"
#include "../Components/Spectrogram.h"
#include "../Components/PinBoundsBox.h"

namespace Interface
{
	class EmptySlider;

	class SpectralMaskComponent final : public PinBoundsBox
	{
	public:
		class SpectralMaskListener
		{
		public:
			virtual ~SpectralMaskListener() = default;
			virtual void expansionChange(bool isExpanded) = 0;
		};

		SpectralMaskComponent(Framework::ParameterValue *lowBound, 
			Framework::ParameterValue *highBound, Framework::ParameterValue *shiftBounds);

		void mouseDown(const MouseEvent &e) override;

		void paint(Graphics &g) override;
		void resized() override;
		void sliderValueChanged(BaseSlider *slider) override;

		void setListener(SpectralMaskListener *listener) noexcept { listener_ = listener; }

	private:
		// TODO: finish spectrogram and expanding behaviour
		Spectrogram spectrogram_{};
		std::unique_ptr<EmptySlider> shiftBounds_ = nullptr;
		SpectralMaskListener *listener_ = nullptr;
		bool isExpanded_ = false;
	};

	class EffectsLaneSection;

	class EffectModuleSection final : public ProcessorSection, public SpectralMaskComponent::SpectralMaskListener,
		public Generation::BaseProcessor::Listener
	{
	public:
		enum MenuId
		{
			kCancel = 0,
			kDeleteInstance,
			kCopyInstance,
			kInitInstance
		};


		static constexpr int kSpectralMaskContractedHeight = 20;
		static constexpr int kSpectralMaskExpandedHeight = 92;
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

		static constexpr int kWidth = 400;
		static constexpr int kMainBodyHeight = 144;
		static constexpr int kMinHeight = kSpectralMaskContractedHeight + kSpectralMaskMargin + kMainBodyHeight;

		EffectModuleSection(Generation::EffectModule *effectModule, EffectsLaneSection *laneSection);
		~EffectModuleSection() override;
		std::unique_ptr<EffectModuleSection> createCopy() const;

		void resized() override;

		void mouseDown(const MouseEvent &e) override;

		void paintBackground(Graphics &g) override;

		void resizeForText(TextSelector *textSelector, int requestedWidthChange) override;
		void expansionChange(bool isExpanded) override;
		void sliderValueChanged(BaseSlider *slider) override;
		void automationMappingChanged(BaseSlider *slider) override;

		Rectangle<int> getPowerButtonBounds() const noexcept override
		{
			auto widthHeight = (int)std::round(scaleValue(kDefaultActivatorSize));
			return { getWidth() - scaleValueRoundInt(kPowerButtonPadding) - widthHeight,
				getYMaskOffset() + centerVertically(0, widthHeight, scaleValueRoundInt(kTopMenuHeight)),
				widthHeight, widthHeight };
		}

		// (re)initialises parameter to be whatever they need to be for the specific module type/effect mode
		void initialiseParameters()
		{
			COMPLEX_ASSERT(initialiseParametersFunction_ && "No initParametersFunction was provided");

			for (auto &control : effectControls_)
				removeControl(control.get());
			
			effectControls_ = decltype(effectControls_){};

			initialiseParametersFunction_(effectControls_, this);
			for (auto &control : effectControls_)
				addControl(control.get());
		}

		// sets positions and dimensions of the module header
		void arrangeHeader();

		// sets positions and dimensions of contained UI elements for the specific module type/effect mode
		void arrangeUI()
		{
			COMPLEX_ASSERT(arrangeUIFunction_ && "No arrangeUIFunction was provided");
			arrangeUIFunction_(this, getUIBounds());
		}
		// paints static/background elements of the specific module
		void paintUIBackground(Graphics &g)
		{
			if (paintBackgroundFunction_)
				paintBackgroundFunction_(g, this);
		}

		auto &getDraggableComponent() noexcept { return draggableBox_; }
		auto *getEffect() noexcept { return effectModule_->getEffect(); }
		template<nested_enum::NestedEnum E>
		E getAlgorithm() { return E::make_enum(effectAlgoSelector_->getValueSafeScaled()).value(); }
		
		BaseControl *getEffectControl(nested_enum::NestedEnum auto enumValue) noexcept
		{
			using namespace Framework;

			if constexpr (std::is_same_v<decltype(enumValue), BaseProcessors::BaseEffect::type>)
			{
				switch (enumValue)
				{
				case BaseProcessors::BaseEffect::Algorithm:
					return effectAlgoSelector_.get();
				default:
					COMPLEX_ASSERT_FALSE("Parameter could not be found");
				case BaseProcessors::BaseEffect::LowBound:
				case BaseProcessors::BaseEffect::HighBound:
				case BaseProcessors::BaseEffect::ShiftBounds:
					return maskComponent_->getControl(enumValue);
				}
			}
			else
			{
				auto enumId = enumValue.enum_id();
				for (auto &control : effectControls_)
					if (control->getParameterDetails().id == enumId)
						return control.get();
				
				COMPLEX_ASSERT_FALSE("Parameter could not be found");
				return nullptr;
			}
		}

		Rectangle<int> getUIBounds() const noexcept
		{ return getLocalBounds().withTop(getYMaskOffset() + scaleValueRoundInt(kTopMenuHeight) + 1); }

		void handlePopupResult(int result) const noexcept;
	protected:
		PopupItems createPopupMenu() const noexcept;

		void changeEffect();
		void setEffectType(std::string_view type);

		int getYMaskOffset() const noexcept
		{
			auto offset = scaleValueRoundInt(kSpectralMaskMargin) + ((isMaskExpanded_) ? 
				scaleValueRoundInt(kSpectralMaskExpandedHeight) : scaleValueRoundInt(kSpectralMaskContractedHeight));
			return scaleValueRoundInt((float)offset);
		}

		DraggableComponent draggableBox_{};
		gl_ptr<PlainShapeComponent> effectTypeIcon_ = nullptr;
		std::unique_ptr<TextSelector> effectTypeSelector_ = nullptr;
		std::unique_ptr<NumberBox> mixNumberBox_ = nullptr;
		std::unique_ptr<PowerButton> moduleActivator_ = nullptr;

		std::unique_ptr<TextSelector> effectAlgoSelector_ = nullptr;
		std::unique_ptr<SpectralMaskComponent> maskComponent_ = nullptr;

		EffectsLaneSection *laneSection_ = nullptr;
		Generation::EffectModule *effectModule_ = nullptr;
		std::array<Generation::baseEffect *, Framework::BaseProcessors::BaseEffect::enum_count(nested_enum::InnerNodes)> cachedEffects_{};
		std::vector<std::unique_ptr<BaseControl>> effectControls_{};
		Framework::VectorMap<size_t, Framework::ParameterBridge *> parameterMappings{};

		bool isMaskExpanded_ = false;

		void (*initialiseParametersFunction_)(std::vector<std::unique_ptr<BaseControl>> &effectSliders,
			EffectModuleSection *section) = nullptr;
		void (*arrangeUIFunction_)(EffectModuleSection *section, Rectangle<int> bounds) = nullptr;
		void (*paintBackgroundFunction_)(Graphics &g, EffectModuleSection *section) = nullptr;
	};	
}
