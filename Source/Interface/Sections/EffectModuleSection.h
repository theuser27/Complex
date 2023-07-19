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

		EffectModuleSection(Generation::EffectModule *effectModule);
		~EffectModuleSection() override;
		std::unique_ptr<EffectModuleSection> createCopy() const;

		void resized() override;

		void paintBackground(Graphics &g) override;
		void repaintBackground() override;
		void initOpenGlComponents(OpenGlWrapper &openGl) override
		{ background_.init(openGl); BaseSection::initOpenGlComponents(openGl); }
		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) override;
		void destroyOpenGlComponents(OpenGlWrapper &openGl) override
		{ background_.destroy(); BaseSection::destroyOpenGlComponents(openGl); }

		void resizeForText(TextSelector *textSelector, int requestedWidthChange) override;
		void expansionChange(bool isExpanded) override;
		void sliderValueChanged(Slider *slider) override;

		Rectangle<int> getPowerButtonBounds() const noexcept override
		{
			auto widthHeight = (int)std::round(scaleValue(kDefaultActivatorSize));
			return { getWidth() - (int)std::round(scaleValue(kPowerButtonPadding)) - widthHeight,
				getYMaskOffset() + centerVertically(0, widthHeight, (int)std::round(scaleValue(kTopMenuHeight))),
				widthHeight, widthHeight };
		}

		// (re)initialises parameter to be whatever they need to be for the specific module type/effect mode
		void initialiseParameters()
		{
			COMPLEX_ASSERT(initialiseParametersFunction_ && "No initParametersFunction was provided");

			for (auto &parameter : effectParameters_)
			{
				if (auto *slider = dynamic_cast<BaseSlider *>(parameter.get()))
					removeSlider(slider);
				else
					removeButton(static_cast<BaseButton *>(parameter.get()));

				parameter->setParameterLink(nullptr);
			}
			effectParameters_.clear();
			initialiseParametersFunction_(effectParameters_, this);
		}
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
		ParameterUI *getEffectParameter(size_t index) noexcept;
		Rectangle<int> getUIBounds() const noexcept
		{ return getLocalBounds().withTop(getYMaskOffset() + scaleValueRoundInt(kTopMenuHeight) + 1); }

		// sets positions and dimensions of the module header
		void arrangeHeader();

	protected:
		void changeEffect();
		void setEffectType(std::string_view type);

		int getYMaskOffset() const noexcept
		{
			auto offset = kSpectralMaskMargin + ((isMaskExpanded_) ? 
				kSpectralMaskExpandedHeight : kSpectralMaskContractedHeight);
			return (int)std::round(scaleValue((float)offset));
		}

		OpenGlImage background_{};

		DraggableComponent draggableBox_{};
		PlainShapeComponent effectTypeIcon_{ "Effect Type Icon" };
		std::unique_ptr<TextSelector> effectTypeSelector_ = nullptr;
		std::unique_ptr<NumberBox> mixNumberBox_ = nullptr;
		std::unique_ptr<BaseButton> moduleActivator_ = nullptr;

		std::unique_ptr<TextSelector> effectAlgoSelector_ = nullptr;
		std::unique_ptr<SpectralMaskComponent> maskComponent_ = nullptr;

		Generation::EffectModule *effectModule_ = nullptr;
		std::array<Generation::baseEffect *, magic_enum::enum_count<Framework::EffectTypes>()> cachedEffects_{};
		std::vector<std::unique_ptr<ParameterUI>> effectParameters_{};

		bool isMaskExpanded_ = false;

		void (*initialiseParametersFunction_)(std::vector<std::unique_ptr<ParameterUI>> &effectSliders,
			EffectModuleSection *section) = nullptr;
		void (*arrangeUIFunction_)(EffectModuleSection *section, Rectangle<int> bounds) = nullptr;
		void (*paintBackgroundFunction_)(Graphics &g, EffectModuleSection *section) = nullptr;
	};	
}
