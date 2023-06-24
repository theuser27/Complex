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

	class SpectralMaskComponent : public PinBoundsBox
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
		void sliderValueChanged(Slider *slider) override;

		void setListener(SpectralMaskListener *listener) noexcept { listener_ = listener; }

	private:
		// TODO: finish spectrogram and expanding behaviour
		Spectrogram spectrogram_{};
		std::unique_ptr<EmptySlider> shiftBounds_ = nullptr;
		SpectralMaskListener *listener_ = nullptr;
		bool isExpanded_ = false;
	};

	class EffectModuleSection : public ProcessorSection, public SpectralMaskComponent::SpectralMaskListener,
		public Generation::BaseProcessor::Listener
	{
	public:
		static constexpr int kEffectModuleWidth = 400;
		static constexpr int kEffectModuleHeight = 144;
		static constexpr int kSpectralMaskContractedHeight = 20;
		static constexpr int kSpectralMaskExpandedHeight = 92;
		static constexpr int kSpectralMaskMargin = 2;
		static constexpr int kTopMenuHeight = 28;
		static constexpr int kDraggableSectionWidth = 36;
		static constexpr int kIconSize = 14;
		static constexpr int kIconToTextSelectorMargin = 4;
		static constexpr int kDelimiterWidth = 1;
		static constexpr int kDelimiterToTextSelectorExtraMargin = 2;
		static constexpr int kNumberBoxSectionPadding = 6;
		static constexpr int kLabelToNumberBoxMargin = 4;
		static constexpr int kNumberBoxesMargin = 12;

		static constexpr int kOuterPixelRounding = 8;
		static constexpr int kInnerPixelRounding = 3;

		EffectModuleSection(Generation::EffectModule *effectModule);
		~EffectModuleSection() override;
		std::unique_ptr<EffectModuleSection> createCopy() const;

		void resized() override;
		void paintBackground(Graphics &g) override;
		void resizeForText(TextSelector *textSelector, int requestedWidthChange) override;
		void expansionChange(bool isExpanded) override;

		void sliderValueChanged(Slider *slider) override;

		// (re)initialises parameter to be whatever they need to be for the specific module type/effect mode
		void initialiseParameters()
		{
			COMPLEX_ASSERT(initialiseParametersFunction_ && "No initParametersFunction was provided");
			initialiseParametersFunction_(effectSliders_, effectModule_);
		}
		// sets positions and dimensions of contained UI elements for the specific module type/effect mode
		void arrangeUI()
		{
			COMPLEX_ASSERT(arrangeUIFunction_ && "No arrangeUIFunction was provided");
			arrangeUIFunction_(this);
		}
		// sets positions and dimensions of the module header
		void arrangeHeader();

		auto &getDraggableComponent() noexcept { return draggableBox_; }
		BaseSlider *getModuleSlider(Framework::EffectModuleParameters index) noexcept;
		BaseSlider *getEffectSlider(size_t index) noexcept;
		Rectangle<int> getUIBounds() const noexcept
		{ return getLocalBounds().withTop(getYMaskOffset() + kTopMenuHeight + 1); }

		void setEffectType(Framework::EffectTypes type);

	protected:
		int getYMaskOffset() const noexcept
		{
			return kSpectralMaskMargin + ((isMaskExpanded_) ? 
				kSpectralMaskExpandedHeight : kSpectralMaskContractedHeight);
		}

		DraggableComponent draggableBox_{};
		Path effectTypeIcon_{};
		std::unique_ptr<TextSelector> effectTypeSelector_ = nullptr;
		std::unique_ptr<NumberBox> mixNumberBox_ = nullptr;

		std::unique_ptr<TextSelector> effectModeSelector_ = nullptr;
		std::unique_ptr<SpectralMaskComponent> maskComponent_ = nullptr;

		Generation::EffectModule *effectModule_ = nullptr;
		std::array<Generation::baseEffect *, magic_enum::enum_count<Framework::EffectTypes>()> cachedEffects_{};
		std::vector<std::unique_ptr<BaseSlider>> effectSliders_{};

		bool isMaskExpanded_ = false;

		void (*initialiseParametersFunction_)(std::vector<std::unique_ptr<BaseSlider>> &effectSliders,
			Generation::EffectModule *effectModule) = nullptr;
		void (*arrangeUIFunction_)(EffectModuleSection *section) = nullptr;
	};	
}
