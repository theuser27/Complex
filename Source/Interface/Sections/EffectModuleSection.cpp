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
	class EmptySlider : public PinSlider
	{
	public:
		EmptySlider(Framework::ParameterValue *parameter, String name = {}) :
			PinSlider(parameter, std::move(name))
		{
			setShouldShowPopup(true);
			components_.clear();
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
		void setComponentsBounds() override { }
	};

	SpectralMaskComponent::SpectralMaskComponent(Framework::ParameterValue *lowBound, 
		Framework::ParameterValue *highBound, Framework::ParameterValue *shiftBounds) : PinBoundsBox("Spectral Mask", lowBound, highBound)
	{
		using namespace Framework;

		setInterceptsMouseClicks(true, true);

		shiftBounds_ = std::make_unique<EmptySlider>(shiftBounds);
		addControl(shiftBounds_.get());
		shiftBounds_->toBack();
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
		auto shiftValue = Framework::scaleValue(shiftBounds_->getValue(), shiftBounds_->getParameterDetails());
		paintHighlightBox(g, (float)lowBound_->getValue(), (float)highBound_->getValue(), 
			getColour(Skin::kWidgetPrimary1).withAlpha(0.15f), (float)shiftValue);
	}

	void SpectralMaskComponent::resized()
	{
		shiftBounds_->setBounds(getLocalBounds());
		shiftBounds_->setTotalRange(getWidth());
		PinBoundsBox::resized();
	}

	void SpectralMaskComponent::sliderValueChanged(Slider *slider)
	{
		if (shiftBounds_.get() == slider)
		{
			highlight_->redrawImage();
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
		addControl(effectTypeSelector_.get());

		effectTypeIcon_ = makeOpenGlComponent<PlainShapeComponent>("Effect Type Icon");
		effectTypeIcon_->setJustification(Justification::centred);
		effectTypeSelector_->setExtraIcon(effectTypeIcon_.get());
		addOpenGlComponent(effectTypeIcon_);

		mixNumberBox_ = std::make_unique<NumberBox>(
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleMix));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		addControl(mixNumberBox_.get());

		moduleActivator_ = std::make_unique<BaseButton>(
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleEnabled));
		addControl(moduleActivator_.get());
		setActivator(moduleActivator_.get());

		auto *baseEffect = effectModule->getEffect();
		auto effectIndex = (u32)effectTypeSelector_->getValueSafeScaled();
		cachedEffects_[effectIndex] = baseEffect;

		effectAlgoSelector_ = std::make_unique<TextSelector>(
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::Algorithm),
			Fonts::instance()->getInterVFont());
		addControl(effectAlgoSelector_.get());

		maskComponent_ = std::make_unique<SpectralMaskComponent>(
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::LowBound),
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::HighBound),
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::ShiftBounds));
		maskComponent_->setListener(this);
		addSubSection(maskComponent_.get());

		setEffectType(baseEffect->getProcessorType());
		initialiseParameters();
	}

	EffectModuleSection::~EffectModuleSection()
	{
		for (auto cachedEffect : cachedEffects_)
			if (cachedEffect && cachedEffect != effectModule_->getSubProcessor(0))
				effectModule_->getProcessorTree()->deleteProcessor(cachedEffect->getProcessorId());
	}

	std::unique_ptr<EffectModuleSection> EffectModuleSection::createCopy() const
	{
		auto copiedModule = utils::as<Generation::EffectModule *>(effectModule_
			->getProcessorTree()->copyProcessor(effectModule_));
		return std::make_unique<EffectModuleSection>(copiedModule);
	}

	void EffectModuleSection::resized()
	{
		BaseSection::resized();

		arrangeHeader();
		arrangeUI();
		repaintBackground();
	}

	void EffectModuleSection::arrangeHeader()
	{
		// top
		int spectralMaskHeight = (isMaskExpanded_) ? scaleValueRoundInt(kSpectralMaskExpandedHeight) :
			scaleValueRoundInt(kSpectralMaskContractedHeight);
		maskComponent_->setBounds({ 0, 0, getWidth(), spectralMaskHeight });
		maskComponent_->setRounding(scaleValue(kOuterPixelRounding), scaleValue(kInnerPixelRounding));

		int yOffset = getYMaskOffset();

		// left hand side
		int topMenuHeight = scaleValueRoundInt(kTopMenuHeight);
		int effectSelectorsHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);
		draggableBox_.setBounds({ 0, yOffset, scaleValueRoundInt(kDraggableSectionWidth), topMenuHeight });

		int effectTypeSelectorIconDimensions = scaleValueRoundInt(kIconSize);
		effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
		effectTypeIcon_->setSize(effectTypeSelectorIconDimensions, effectTypeSelectorIconDimensions);

		auto currentPoint = Point{ draggableBox_.getRight(), centerVertically(yOffset, effectSelectorsHeight, topMenuHeight) };
		auto effectTypeSelectorBounds = effectTypeSelector_->getOverallBoundsForHeight(effectSelectorsHeight);
		effectTypeSelector_->setOverallBounds(currentPoint);

		currentPoint.x += effectTypeSelectorBounds.getWidth() + scaleValueRoundInt(kDelimiterWidth) +
			2 * scaleValueRoundInt(kDelimiterToTextSelectorMargin);
		std::ignore = effectAlgoSelector_->getOverallBoundsForHeight(effectSelectorsHeight);
		effectAlgoSelector_->setOverallBounds(currentPoint);

		// right hand side
		int mixNumberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		auto mixNumberBoxBounds = mixNumberBox_->getOverallBoundsForHeight(mixNumberBoxHeight);
		mixNumberBox_->setOverallBounds({ moduleActivator_->getX() - mixNumberBoxBounds.getRight() - scaleValueRoundInt(kNumberBoxToPowerButtonMargin),
			centerVertically(yOffset, mixNumberBoxHeight, topMenuHeight) });
	}

	void EffectModuleSection::resizeForText(TextSelector *textSelector, int requestedWidthChange)
	{
		BaseSection::resizeForText(textSelector, requestedWidthChange);
		if (textSelector != effectTypeSelector_.get())
			return;

		auto position = effectAlgoSelector_->getPosition();
		effectAlgoSelector_->setTopLeftPosition(position.x + requestedWidthChange, position.y);

		repaintBackground();
	}

	void EffectModuleSection::expansionChange(bool isExpanded)
	{
		if (isMaskExpanded_ == isExpanded)
			return;

		isMaskExpanded_ = isExpanded;
		if (isMaskExpanded_)
			setBounds(getBounds().withHeight(kSpectralMaskExpandedHeight + kSpectralMaskMargin + kMinHeight));
		else
			setBounds(getBounds().withHeight(kSpectralMaskContractedHeight + kSpectralMaskMargin + kMinHeight));
	}

	void EffectModuleSection::paintBackground(Graphics &g)
	{
		maskComponent_->setRoundedCornerColour(parent_->getColour(Skin::kBackground));

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
		int lineY = centerVertically((int)rectangleBounds.getY(), topMenuHeight / 2, topMenuHeight);
		g.fillRect(lineX, lineY, 1, topMenuHeight / 2);

		BaseSection::paintBackground(g);

		paintUIBackground(g);
	}

	void EffectModuleSection::sliderValueChanged(Slider *slider)
	{
		if (slider == effectTypeSelector_.get() && effectModule_)
		{
			changeEffect();
			initialiseParameters();
			arrangeUI();
			repaintBackground();
		}
		else if (slider == effectAlgoSelector_.get() && effectModule_)
		{
			initialiseParameters();
			arrangeUI();
			repaintBackground();
		}
	}

	void EffectModuleSection::changeEffect()
	{
		using namespace Framework;

		auto effectIndex = (u32)Framework::scaleValue(effectTypeSelector_->getValue(),
			effectModuleParameterList[(u32)EffectModuleParameters::ModuleType]);

		// getting the desired effect if it's cached
		// if not, create and cache it
		Generation::baseEffect *newEffect = cachedEffects_[effectIndex];
		if (!newEffect)
		{
			newEffect = utils::as<Generation::baseEffect *>(effectModule_
				->createSubProcessor(Generation::effectsTypes[effectIndex]));
			cachedEffects_[effectIndex] = newEffect;
		}

		auto replacedEffect = effectModule_->getEffect();

		auto replace = [newEffect, this]() { effectModule_->updateSubProcessor(0, newEffect); };
		effectModule_->getProcessorTree()->executeOutsideProcessing(replace);

		// resetting the UI
		setEffectType(newEffect->getProcessorType());
		effectTypeSelector_->resized();
		effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
		mixNumberBox_->resized();
		moduleActivator_->resized();
		maskComponent_->resized();

		// replacing the mapped out parameters, if there are any
		auto parametersBridgesToRelink = std::min(replacedEffect->getNumParameters(), newEffect->getNumParameters());
		for (size_t i = 0; i < parametersBridgesToRelink; i++)
		{
			auto controlVariant = replacedEffect->getParameterUnchecked(i)->changeControl(std::variant<BaseControl *,
				ParameterBridge *>(std::in_place_index<1>, nullptr));
			// we don't check has_variant() because we always receive something from changeControl
			if (auto *bridge = std::get<1>(controlVariant))
				bridge->resetParameterLink(newEffect->getParameterUnchecked(i)->getParameterLink());
		}
		
		// replacing the parameters for the algo and mask sliders
		bool getValueFromParameter = true;
		for (size_t i = 0; i < magic_enum::enum_count<BaseEffectParameters>(); i++)
		{
			auto parameter = getEffectParameter(i);
			parameter->changeLinkedParameter(*newEffect->getParameterUnchecked(i), getValueFromParameter);
			parameter->refresh();
			getValueFromParameter = false;
		}
	}

	BaseControl *EffectModuleSection::getEffectParameter(size_t index) noexcept
	{
		using namespace Framework;

		switch (index)
		{
		case (u32)BaseEffectParameters::Algorithm:
			return effectAlgoSelector_.get();
		case (u32)BaseEffectParameters::LowBound:
			return maskComponent_->getControl(baseEffectParameterList[(u32)BaseEffectParameters::LowBound].id);
		case (u32)BaseEffectParameters::HighBound:
			return maskComponent_->getControl(baseEffectParameterList[(u32)BaseEffectParameters::HighBound].id);
		case (u32)BaseEffectParameters::ShiftBounds:
			return maskComponent_->getControl(baseEffectParameterList[(u32)BaseEffectParameters::ShiftBounds].id);
		default:
			return effectControls_[index - magic_enum::enum_count<BaseEffectParameters>()].get();
		}
	}


	namespace
	{
		void initFilterParameters(std::vector<std::unique_ptr<BaseControl>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			effectSliders.reserve(FilterEffectParameters::size);
			auto *baseEffect = section->getEffect();

			auto initNormal = [&]()
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<FilterEffectParameters::Normal>(); ++i)
				{
					effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getEffectParameter(i)));

					section->addControl(utils::as<BaseSlider *>(effectSliders[i].get()));
				}
			};

			auto initRegular = [&]() { };


			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)FilterModes::Normal:
				initNormal();
				break;
			default:
			case (u32)FilterModes::Regular:
				initRegular();
				break;
			}
		}

		void arrangeFilterUI(EffectModuleSection *section, Rectangle<int> bounds)
		{
			using namespace Framework;

			auto arrangeNormal = [&]()
			{
				int knobEdgeOffset = section->scaleValueRoundInt(32);
				int knobTopOffset = section->scaleValueRoundInt(32);

				int knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
					.withTrimmedTop(knobTopOffset).withHeight(knobsHeight);
				int rotaryInterval = (int)std::round((float)bounds.getWidth() / 3.0f);

				// gain rotary
				auto *gainSlider = section->getEffectParameter((u32)FilterEffectParameters::Normal::Gain);
				std::ignore = gainSlider->getOverallBoundsForHeight(knobsHeight);
				gainSlider->setOverallBounds({ bounds.getX(), bounds.getY() });

				// cutoff rotary
				auto *cutoffSlider = section->getEffectParameter((u32)FilterEffectParameters::Normal::Cutoff);
				std::ignore = cutoffSlider->getOverallBoundsForHeight(knobsHeight);
				cutoffSlider->setOverallBounds({ bounds.getX() + rotaryInterval, bounds.getY() });

				// slope rotary
				auto *slopeSlider = section->getEffectParameter((u32)FilterEffectParameters::Normal::Slope);
				std::ignore = slopeSlider->getOverallBoundsForHeight(knobsHeight);
				slopeSlider->setOverallBounds({ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
			};

			auto arrangeRegular = [&]()
			{

			};

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)FilterModes::Normal:
				arrangeNormal();
				break;
			default:
			case (u32)FilterModes::Regular:
				arrangeRegular();
				break;
			}
		}

		void initDynamicsParameters(std::vector<std::unique_ptr<BaseControl>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			effectSliders.reserve(DynamicsEffectParameters::size);
			auto *baseEffect = section->getEffect();

			auto initContrast = [&]()
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<DynamicsEffectParameters::Contrast>(); i++)
				{
					effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getEffectParameter(i)));
					section->addControl(effectSliders.back().get());
				}
			};

			auto initClip = [&]()
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<DynamicsEffectParameters::Clip>(); i++)
				{
					effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getEffectParameter(i)));
					section->addControl(effectSliders.back().get());
				}
			};
			
			auto initCompressor = [&]()
			{

			};

			auto algorithmIndex = (u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled();
			baseEffect->updateParameterDetails(dynamicsEffectParameterList[algorithmIndex]);
			switch (algorithmIndex)
			{
			case (u32)DynamicsModes::Contrast:
				initContrast();
				break;
			case (u32)DynamicsModes::Clip:
				initClip();
				break;
			case (u32)DynamicsModes::Compressor:
				initCompressor();
				break;
			default:
				break;
			}
		}

		void arrangeDynamicsUI(EffectModuleSection *section, Rectangle<int> bounds)
		{
			using namespace Framework;

			auto arrangeContrast = [&]()
			{
				// TEST
				int knobEdgeOffset = section->scaleValueRoundInt(32);
				int knobTopOffset = section->scaleValueRoundInt(32);

				int knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				// depth rotary and label
				auto *depthSlider = section->getEffectParameter((u32)DynamicsEffectParameters::Contrast::Depth);
				std::ignore = depthSlider->getOverallBoundsForHeight(knobsHeight);
				depthSlider->setOverallBounds({ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeClip = [&]()
			{
				// TEST
				int knobEdgeOffset = section->scaleValueRoundInt(32);
				int knobTopOffset = section->scaleValueRoundInt(32);

				int knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				// depth rotary and label
				auto *thresholdSlider = section->getEffectParameter((u32)DynamicsEffectParameters::Clip::Threshold);
				std::ignore = thresholdSlider->getOverallBoundsForHeight(knobsHeight);
				thresholdSlider->setOverallBounds({ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeCompressor = [&]()
			{

			};

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)DynamicsModes::Contrast:
				arrangeContrast();
				break;
			case (u32)DynamicsModes::Clip:
				arrangeClip();
				break;
			case (u32)DynamicsModes::Compressor:
				arrangeCompressor();
				break;
			default:
				break;
			}
		}
	}

	void EffectModuleSection::setEffectType(std::string_view type)
	{
		using namespace Generation;

		paintBackgroundFunction_ = nullptr;

		if (type == utilityEffect::getClassType())
		{
			
		}
		else if (type == filterEffect::getClassType())
		{
			initialiseParametersFunction_ = initFilterParameters;
			arrangeUIFunction_ = arrangeFilterUI;

			setSkinOverride(Skin::kFilterModule);
			maskComponent_->setSkinOverride(Skin::kFilterModule);
			effectTypeIcon_->setShapes(Paths::filterIcon());
		}
		else if (type == dynamicsEffect::getClassType())
		{
			initialiseParametersFunction_ = initDynamicsParameters;
			arrangeUIFunction_ = arrangeDynamicsUI;

			setSkinOverride(Skin::kDynamicsModule);
			maskComponent_->setSkinOverride(Skin::kDynamicsModule);
			effectTypeIcon_->setShapes(Paths::contrastIcon());
		}
		else if (type == phaseEffect::getClassType())
		{
			
		}
		else if (type == pitchEffect::getClassType())
		{
			
		}
		else if (type == stretchEffect::getClassType())
		{
			
		}
		else if (type == warpEffect::getClassType())
		{
			
		}
		else if (type == destroyEffect::getClassType())
		{
			
		}
	}
}
