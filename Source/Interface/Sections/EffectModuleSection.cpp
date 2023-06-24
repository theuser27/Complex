/*
	==============================================================================

		EffectModuleSection.cpp
		Created: 14 Feb 2023 2:29:16am
		Author:  theuser27

	==============================================================================
*/

#include "Plugin/ProcessorTree.h"
#include "Generation/EffectModules.h"
#include "Interface/LookAndFeel/Paths.h"
#include "EffectModuleSection.h"

namespace Interface
{
	static Path getEffectIcon(Framework::EffectTypes type)
	{
		using namespace Framework;

		switch (type)
		{
		case EffectTypes::Utility: return Paths::contrastIcon();
		case EffectTypes::Filter: return Paths::contrastIcon();
		case EffectTypes::Dynamics: return Paths::contrastIcon();
		case EffectTypes::Phase:
		case EffectTypes::Pitch:
		case EffectTypes::Stretch:
		case EffectTypes::Warp:
		case EffectTypes::Destroy:
		default:
			return {};
		}
	}

	class EmptySlider : public BaseSlider
	{
	public:
		EmptySlider(Framework::ParameterValue *parameter, String name = {}) : BaseSlider(parameter, std::move(name)) { }

		void mouseDown(const MouseEvent &e) override
		{
			// an unfortunate consequence
			if (e.mods.isCtrlDown() || e.mods.isCommandDown())
				getParentComponent()->mouseDown(e);
			else
				BaseSlider::mouseDown(e);
		}

		void redoImage() override { }
		void setSliderBounds() override { }
		Colour getSelectedColor() const override { return Colours::transparentBlack; }
		Colour getThumbColor() const override { return Colours::transparentBlack; }
	};

	SpectralMaskComponent::SpectralMaskComponent(Framework::ParameterValue *lowBound, 
		Framework::ParameterValue *highBound, Framework::ParameterValue *shiftBounds) : PinBoundsBox("Spectral Mask", lowBound, highBound)
	{
		using namespace Framework;

		setInterceptsMouseClicks(true, true);

		shiftBounds_ = std::make_unique<EmptySlider>(shiftBounds);
		addSlider(shiftBounds_.get());
		shiftBounds_->toBehind(lowBound_.get());
		shiftBounds_->toBehind(highBound_.get());

		std::function shiftingFunction = [shiftBoundsPointer = shiftBounds_.get()](const BaseSlider *slider)
		{
			auto value = shiftBoundsPointer->getValue() * 2.0 - 1.0;
			return std::clamp(value + slider->getValue(), 0.0, 1.0);
		};

		lowBound_->setValueForDrawingFunction(shiftingFunction);
		highBound_->setValueForDrawingFunction(shiftingFunction);
	}

	void SpectralMaskComponent::mouseDown(const MouseEvent &e)
	{
		if (e.mods.isAnyModifierKeyDown())
		{
			isExpanded_ = !isExpanded_;
			COMPLEX_ASSERT(listener_ && "This spectral mask was not given a pointer to owner to notify for an expansion change");
			listener_->expansionChange(isExpanded_);
		}
	}

	void SpectralMaskComponent::paint(Graphics& g)
	{
		auto shiftBoundsValue = Framework::scaleValue(shiftBounds_->getValue(), shiftBounds_->getParameterDetails());
		auto lowBoundValue = (float)std::clamp(lowBound_->getValue() + shiftBoundsValue, 0.0, 1.0);
		auto highBoundValue = (float)std::clamp(lowBound_->getValue() + shiftBoundsValue, 0.0, 1.0);
		paintHighlightBox(g, lowBoundValue, highBoundValue, findColour(Skin::kPinSlider, true).withAlpha(0.15f));
	}

	void SpectralMaskComponent::resized()
	{
		shiftBounds_->setBounds(getLocalBounds());
		PinBoundsBox::resized();
	}

	void SpectralMaskComponent::sliderValueChanged(Slider *slider)
	{
		if (shiftBounds_.get() == slider)
		{
			repaintBackground();
			return;
		}

		PinBoundsBox::sliderValueChanged(slider);
	}

	EffectModuleSection::EffectModuleSection(Generation::EffectModule *effectModule) :
		ProcessorSection(typeid(EffectModuleSection).name(), effectModule), effectModule_(effectModule)
	{
		using namespace Framework;

		setInterceptsMouseClicks(false, true);

		draggableBox_.setDraggedComponent(this);
		addAndMakeVisible(draggableBox_);

		effectTypeSelector_ = std::make_unique<TextSelector>( 
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleType), 
			Fonts::instance()->getInterVFont().withStyle(Font::bold));
		effectTypeSelector_->setDrawArrow(true);
		effectTypeSelector_->setTextSelectorListener(this);
		addSlider(effectTypeSelector_.get());

		mixNumberBox_ = std::make_unique<NumberBox>(
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleMix));
		mixNumberBox_->setMaxDisplayCharacters(4);
		mixNumberBox_->setMaxDecimalPlaces(2);
		addSlider(mixNumberBox_.get());

		auto *baseEffect = effectModule->getSubProcessor(0);
		auto effectType = magic_enum::enum_cast<EffectTypes>(baseEffect->getProcessorType()).value();
		effectTypeIcon_ = getEffectIcon(effectType);

		effectModeSelector_ = std::make_unique<TextSelector>(
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::Mode),
			Fonts::instance()->getInterVFont());

		maskComponent_ = std::make_unique<SpectralMaskComponent>(
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::LowBound),
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::HighBound),
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::ShiftBounds));
		maskComponent_->setListener(this);
		addSubSection(maskComponent_.get());

		setEffectType(effectType);
	}

	EffectModuleSection::~EffectModuleSection()
	{
		for (auto cachedEffect : cachedEffects_)
			if (cachedEffect && cachedEffect != effectModule_->getSubProcessor(0))
				effectModule_->getProcessorTree()->deleteProcessor(cachedEffect->getProcessorId());
	}

	std::unique_ptr<EffectModuleSection> EffectModuleSection::createCopy() const
	{
		auto copiedModule = static_cast<Generation::EffectModule *>(effectModule_
			->getProcessorTree()->copyProcessor(effectModule_));
		return std::make_unique<EffectModuleSection>(copiedModule);
	}

	void EffectModuleSection::resized()
	{
		arrangeHeader();
		arrangeUI();
		repaintBackground();
	}

	void EffectModuleSection::paintBackground(Graphics &g)
	{
		auto yOffset = getYMaskOffset();
		auto rectangleBounds = getLocalBounds().withTop(yOffset).toFloat();

		Path rectangle;
		rectangle.startNewSubPath(rectangleBounds.getCentreX(), rectangleBounds.getY());

		rectangle.lineTo(rectangleBounds.getRight() - kInnerPixelRounding, rectangleBounds.getY());
		rectangle.quadraticTo(rectangleBounds.getRight(), rectangleBounds.getY(), 
			rectangleBounds.getRight(), rectangleBounds.getY() + kInnerPixelRounding);

		rectangle.lineTo(rectangleBounds.getRight(), rectangleBounds.getBottom() - kOuterPixelRounding);
		rectangle.quadraticTo(rectangleBounds.getRight(), rectangleBounds.getBottom(),
			rectangleBounds.getRight() - kOuterPixelRounding, rectangleBounds.getBottom());

		rectangle.lineTo(rectangleBounds.getX() + kOuterPixelRounding, rectangleBounds.getBottom());
		rectangle.quadraticTo(rectangleBounds.getX(), rectangleBounds.getBottom(),
			rectangleBounds.getX(), rectangleBounds.getBottom() - kOuterPixelRounding);

		rectangle.lineTo(rectangleBounds.getX(), rectangleBounds.getY() + kInnerPixelRounding);
		rectangle.quadraticTo(rectangleBounds.getX(), rectangleBounds.getY(),
			rectangleBounds.getX() + kInnerPixelRounding, rectangleBounds.getY());

		rectangle.closeSubPath();

		g.setColour(findColour(Skin::kBackground));
		g.fillPath(rectangle);

		g.saveState();
		draggableBox_.paint(g);
		g.restoreState();

		g.setColour(findColour(Skin::kBody).withAlpha(0.08f));
		g.drawLine(rectangleBounds.getX(), rectangleBounds.getY() + kTopMenuHeight, 
			rectangleBounds.getRight(), rectangleBounds.getY() + kTopMenuHeight, 1.0f);

		auto lineX = rectangleBounds.getX() + kDraggableSectionWidth + 
			(float)effectTypeSelector_->getTotalDrawWidth() + kDelimiterToTextSelectorExtraMargin;
		g.drawLine(lineX, kTopMenuHeight / 4.0f, lineX, 3.0f * kTopMenuHeight / 4.0f, 1.0f);

		drawNumberBoxBackground(g, mixNumberBox_.get());
		drawSliderLabel(g, mixNumberBox_.get());

		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
	}

	void EffectModuleSection::resizeForText(TextSelector *textSelector, int requestedWidthChange)
	{
		BaseSection::resizeForText(textSelector, requestedWidthChange);
		if (textSelector != effectTypeSelector_.get())
			return;

		auto position = effectModeSelector_->getPosition();
		effectModeSelector_->setTopLeftPosition(position.x + requestedWidthChange, position.y);

		repaintBackground();
	}

	void EffectModuleSection::expansionChange(bool isExpanded)
	{
		if (isMaskExpanded_ == isExpanded)
			return;

		isMaskExpanded_ = isExpanded;
		if (isMaskExpanded_)
			setBounds(getBounds().withHeight(kSpectralMaskExpandedHeight + kSpectralMaskMargin + kEffectModuleHeight));
		else
			setBounds(getBounds().withHeight(kSpectralMaskContractedHeight + kSpectralMaskMargin + kEffectModuleHeight));
	}

	void EffectModuleSection::sliderValueChanged(Slider *slider)
	{
		using namespace Framework;

		if (slider == effectTypeSelector_.get() && effectModule_)
		{
			auto effectIndex = (u32)scaleValue(slider->getValue(),
				effectModuleParameterList[(u32)EffectModuleParameters::ModuleType]);

			// getting the desired effect if it's cached
			// if not, create and cache it
			Generation::baseEffect *newEffect = cachedEffects_[effectIndex];
			if (newEffect)
			{
				// SAFETY: we know what type BaseProcessor we'll be getting so static_cast is safe
				newEffect = static_cast<Generation::baseEffect *>(effectModule_
					->createSubProcessor(Generation::effectsTypes[effectIndex]));
				cachedEffects_[effectIndex] = newEffect;
			}

			auto replacedEffect = effectModule_->getSubProcessor(0);

			auto replace = [newEffect, this]() { effectModule_->updateSubProcessor(0, newEffect); };
			effectModule_->getProcessorTree()->executeOutsideProcessing(replace);

			// replacing the mapped out parameters, if there are any
			auto parametersToReplace = std::min(replacedEffect->getNumParameters(), newEffect->getNumParameters());
			for (size_t i = 0; i < parametersToReplace; i++)
			{
				// we don't check has_variant() because we receive the same element type we're replacing
				if (auto *bridge = std::get<1>(replacedEffect->getParameterUnchecked(i)->changeControl(std::variant<ParameterUI *,
					ParameterBridge *>(std::in_place_index<1>, nullptr))))
					bridge->resetParameterLink(newEffect->getParameterUnchecked(i)->getParameterLink());
			}

			// resetting the UI
			setEffectType((EffectTypes)effectIndex);
			initialiseParameters();
			arrangeUI();
		}
		else if (slider == effectModeSelector_.get() && effectModule_)
		{
			initialiseParameters();
			arrangeUI();
		}
	}

	void EffectModuleSection::arrangeHeader()
	{
		auto width = getWidth();

		// top
		Rectangle currentBound = { 0, 0, width, (isMaskExpanded_) ? 
			kSpectralMaskExpandedHeight : kSpectralMaskContractedHeight };
		maskComponent_->setBounds(currentBound);
		maskComponent_->setRounding(kOuterPixelRounding, kInnerPixelRounding);

		auto yOffset = getYMaskOffset();

		// left hand side
		currentBound = { 0, yOffset, kDraggableSectionWidth, kTopMenuHeight };
		draggableBox_.setBounds(currentBound);

		currentBound.setX(currentBound.getRight());
		currentBound.setWidth(effectTypeSelector_->getTotalDrawWidth());
		effectTypeSelector_->setBounds(currentBound);
		effectTypeSelector_->setDrawBox(currentBound.withLeft(kIconSize));

		currentBound.setX(currentBound.getRight() + 2 * kDelimiterToTextSelectorExtraMargin + kDelimiterWidth);
		currentBound.setWidth(effectModeSelector_->getTotalDrawWidth());
		effectModeSelector_->setBounds(currentBound);

		// right hand side
		currentBound.setX(width);
		currentBound.setY(yOffset + kNumberBoxSectionPadding);
		currentBound.setHeight(kTopMenuHeight - 2 * kNumberBoxSectionPadding);

		auto mixNumberBoxWidth = mixNumberBox_->setHeight(kTopMenuHeight - 2 * kNumberBoxSectionPadding);
		currentBound.setX(currentBound.getX() - mixNumberBoxWidth - kNumberBoxSectionPadding);
		currentBound.setWidth(mixNumberBoxWidth);
		mixNumberBox_->setBounds(currentBound);

		auto mixNumberBoxLabelWidth = mixNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - mixNumberBoxLabelWidth - kLabelToNumberBoxMargin);
		currentBound.setWidth(mixNumberBoxLabelWidth);
		mixNumberBox_->getLabelComponent()->setBounds(currentBound);
	}

	BaseSlider *EffectModuleSection::getModuleSlider(Framework::EffectModuleParameters index) noexcept
	{
		using namespace Framework;

		switch (index)
		{
		case EffectModuleParameters::ModuleEnabled:
			return nullptr;
		case EffectModuleParameters::ModuleType:
			return effectTypeSelector_.get();
		case EffectModuleParameters::ModuleMix:
			return mixNumberBox_.get();
		default:
			return nullptr;
		}
	}

	BaseSlider *EffectModuleSection::getEffectSlider(size_t index) noexcept
	{
		using namespace Framework;

		switch (index)
		{
		case (u32)BaseEffectParameters::Mode:
			return effectModeSelector_.get();
		case (u32)BaseEffectParameters::LowBound:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::LowBound].name);
		case (u32)BaseEffectParameters::HighBound:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::HighBound].name);
		case (u32)BaseEffectParameters::ShiftBounds:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::ShiftBounds].name);
		default:
			return effectSliders_.at(index).get();
		}
	}

	namespace
	{
		void initFilterParameters(std::vector<std::unique_ptr<BaseSlider>> &effectSliders, Generation::EffectModule *effectModule)
		{
			using namespace Framework;

			auto initNormal = [](std::vector<std::unique_ptr<BaseSlider>> &effectSliders, Generation::BaseProcessor *baseEffect)
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<FilterEffectParameters::Normal>(); i++)
				{
					if (effectSliders[i])
						effectSliders[i] = std::make_unique<RotarySlider>(baseEffect->getParameterUnchecked(i));
					else
						effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getParameterUnchecked(i)));
				}
			};

			auto initRegular = []([[maybe_unused]] std::vector<std::unique_ptr<BaseSlider>> &effectSliders, 
				[[maybe_unused]] Generation::BaseProcessor *baseEffect) { };

			effectSliders.reserve((u32)FilterEffectParameters::size);
			// it's safe to static_cast because by design effectModules can only contain some derived type of baseEffect
			auto *baseEffect = static_cast<Generation::baseEffect *>(effectModule->getSubProcessor(0));
			switch (baseEffect->getEffectMode())
			{
			case (u32)FilterModes::Normal:
				initNormal(effectSliders, baseEffect);
				break;
			case (u32)FilterModes::Regular:
				initRegular(effectSliders, baseEffect);
				break;
			default:
				initNormal(effectSliders, baseEffect);
				break;
			}
		}

		void arrangeFilterUI(EffectModuleSection *section)
		{
			using namespace Framework;

			// TEST
			static constexpr int kKnobEdgeOffset = 16;
			static constexpr int kKnobTopOffset = 32;

			auto knobsWidth = RotarySlider::kDefaultWidthHeight;
			auto knobsHeight = RotarySlider::kDefaultWidthHeight;

			auto bounds = section->getUIBounds().withLeft(kKnobEdgeOffset).withRight(kKnobEdgeOffset)
				.withTop(kKnobTopOffset).withHeight(knobsHeight);

			// gain rotary and label
			auto *gainSlider = section->getEffectSlider((u32)FilterEffectParameters::Normal::Gain);
			gainSlider->setBounds(bounds.getX(), bounds.getY(), knobsWidth, knobsHeight);

			auto *gainSliderLabel = gainSlider->getLabelComponent();
			auto labelBounds = Rectangle{ gainSlider->getRight() + RotarySlider::kLabelOffset,
				gainSlider->getY() + RotarySlider::kLabelVerticalPadding,
				gainSliderLabel->getTotalWidth(),
				gainSlider->getHeight() - 2 * RotarySlider::kLabelVerticalPadding };
			gainSliderLabel->setBounds(labelBounds);

			// cutoff rotary and label
			auto *cutoffSlider = section->getEffectSlider((u32)FilterEffectParameters::Normal::Cutoff);
			cutoffSlider->setBounds((int)std::round((float)bounds.getX() + 1.0f / 3.0f * (float)bounds.getWidth()), 
				bounds.getY(), knobsWidth, knobsHeight);

			auto *cutoffSliderLabel = cutoffSlider->getLabelComponent();
			labelBounds.setPosition(cutoffSlider->getRight() + RotarySlider::kLabelOffset, labelBounds.getY());
			labelBounds.setWidth(cutoffSliderLabel->getTotalWidth());
			cutoffSliderLabel->setBounds(labelBounds);

			// slope rotary and label
			auto *slopeSlider = section->getEffectSlider((u32)FilterEffectParameters::Normal::Slope);
			slopeSlider->setBounds((int)std::round((float)bounds.getX() + 2.0f / 3.0f * (float)bounds.getWidth()), 
				bounds.getY(), knobsWidth, knobsHeight);

			auto *slopeSliderLabel = slopeSlider->getLabelComponent();
			labelBounds.setPosition(slopeSlider->getRight() + RotarySlider::kLabelOffset, labelBounds.getY());
			labelBounds.setWidth(slopeSliderLabel->getTotalWidth());
			slopeSliderLabel->setBounds(labelBounds);
		}
	}

	void EffectModuleSection::setEffectType(Framework::EffectTypes type)
	{
		using namespace Framework;

		switch (type)
		{
		case EffectTypes::Utility:
			break;
		case EffectTypes::Filter:
			initialiseParametersFunction_ = initFilterParameters;
			arrangeUIFunction_ = arrangeFilterUI;
			break;
		case EffectTypes::Dynamics:
			break;
		case EffectTypes::Phase:
			break;
		case EffectTypes::Pitch:
			break;
		case EffectTypes::Stretch:
			break;
		case EffectTypes::Warp:
			break;
		case EffectTypes::Destroy:
			break;
		default:
			break;
		}
	}
}
