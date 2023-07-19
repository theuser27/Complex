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
			setUseQuad(false);
			setUseImage(false);
			setShouldShowPopup(true);
		}

		void mouseDown(const MouseEvent &e) override
		{
			// an unfortunate consequence
			if (e.mods.isCtrlDown() || e.mods.isCommandDown())
				getParentComponent()->mouseDown(e);
			else
				PinSlider::mouseDown(e);
		}

		void paint(Graphics &g) override { }
		void redoImage() override { }
		void setSliderBounds() override { }
	};

	SpectralMaskComponent::SpectralMaskComponent(Framework::ParameterValue *lowBound, 
		Framework::ParameterValue *highBound, Framework::ParameterValue *shiftBounds) : PinBoundsBox("Spectral Mask", lowBound, highBound)
	{
		using namespace Framework;

		setInterceptsMouseClicks(true, true);

		shiftBounds_ = std::make_unique<EmptySlider>(shiftBounds);
		addSlider(shiftBounds_.get());
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

		effectTypeIcon_.setAlwaysOnTop(true);
		addOpenGlComponent(&effectTypeIcon_);

		effectTypeSelector_ = std::make_unique<TextSelector>( 
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleType), 
			Fonts::instance()->getInterVFont().withStyle(Font::bold));
		effectTypeSelector_->setDrawArrow(true);
		effectTypeSelector_->setTextSelectorListener(this);
		addSlider(effectTypeSelector_.get());

		mixNumberBox_ = std::make_unique<NumberBox>(
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleMix));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		addSlider(mixNumberBox_.get());

		moduleActivator_ = std::make_unique<BaseButton>(
			effectModule->getParameterUnchecked((u32)EffectModuleParameters::ModuleEnabled));
		moduleActivator_->addButtonListener(this);
		setActivator(moduleActivator_.get());
		addButton(moduleActivator_.get());

		auto *baseEffect = effectModule->getEffect();
		auto effectIndex = (u32)effectTypeSelector_->getValueSafeScaled(effectModule->getProcessorTree()->getSampleRate());
		cachedEffects_[effectIndex] = baseEffect;

		effectAlgoSelector_ = std::make_unique<TextSelector>(
			baseEffect->getParameterUnchecked((u32)BaseEffectParameters::Algorithm),
			Fonts::instance()->getInterVFont());
		effectAlgoSelector_->setDrawArrow(true);
		effectAlgoSelector_->setTextSelectorListener(this);
		addSlider(effectAlgoSelector_.get());

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
		auto copiedModule = static_cast<Generation::EffectModule *>(effectModule_
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
		auto width = getWidth();

		// top
		auto spectralMaskHeight = (isMaskExpanded_) ? scaleValueRoundInt(kSpectralMaskExpandedHeight) :
			scaleValueRoundInt(kSpectralMaskContractedHeight);
		Rectangle currentBound = { 0, 0, width, spectralMaskHeight };
		maskComponent_->setBounds(currentBound);
		maskComponent_->setRounding(scaleValue(kOuterPixelRounding), scaleValue(kInnerPixelRounding));

		auto yOffset = getYMaskOffset();

		// left hand side
		auto topMenuHeight = scaleValueRoundInt(kTopMenuHeight);
		auto effectSelectorsHeight = scaleValueRoundInt(TextSelector::kDefaultTextSelectorHeight);
		currentBound = { 0, centerVertically(yOffset, effectSelectorsHeight, topMenuHeight),
			scaleValueRoundInt(kDraggableSectionWidth), topMenuHeight };
		draggableBox_.setBounds(currentBound);

		auto effectTypeSelectorIconDimensions = scaleValueRoundInt(kIconSize);
		auto effectTypeSelectorWidth = effectTypeSelector_->setHeight(effectSelectorsHeight);
		currentBound.setX(currentBound.getRight());
		currentBound.setWidth(effectTypeSelectorWidth + effectTypeSelectorIconDimensions);
		currentBound.setHeight(effectSelectorsHeight);
		/*effectTypeIcon_.setJustification(Justification::centred);
		effectTypeIcon_.setColor(getColour(Skin::kWidgetPrimary1));
		effectTypeIcon_.setBounds(currentBound.withTrimmedRight(effectTypeSelectorWidth));*/
		effectTypeSelector_->setBoundsAndDrawBounds(currentBound,
			currentBound.withPosition(0, 0).withTrimmedLeft(effectTypeSelectorIconDimensions));

		currentBound.setX(currentBound.getRight() + scaleValueRoundInt(kDelimiterWidth) +
			2 * scaleValueRoundInt(kDelimiterToTextSelectorMargin));
		auto effectAlgoSelectorWidth = effectAlgoSelector_->setHeight(effectSelectorsHeight);
		currentBound.setWidth(effectAlgoSelectorWidth);
		effectAlgoSelector_->setBounds(currentBound);

		// right hand side
		auto mixNumberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		auto mixNumberBoxWidth = mixNumberBox_->setHeight(mixNumberBoxHeight);
		currentBound.setX(moduleActivator_->getX() - mixNumberBoxWidth - scaleValueRoundInt(kNumberBoxToPowerButtonMargin));
		currentBound.setY(centerVertically(yOffset, mixNumberBoxHeight, topMenuHeight));
		currentBound.setHeight(mixNumberBoxHeight);
		currentBound.setWidth(mixNumberBoxWidth);
		mixNumberBox_->setBounds(currentBound);

		auto mixNumberBoxLabelWidth = mixNumberBox_->getLabelComponent()->getLabelTextWidth();
		currentBound.setX(currentBound.getX() - mixNumberBoxLabelWidth - scaleValueRoundInt(kLabelToNumberBoxMargin));
		currentBound.setWidth(mixNumberBoxLabelWidth);
		mixNumberBox_->getLabelComponent()->setBounds(currentBound);
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
		auto yOffset = getYMaskOffset();
		auto rectangleBounds = getLocalBounds().withTop(yOffset).toFloat();

		auto innerRounding = scaleValue(kInnerPixelRounding);
		auto outerRounding = scaleValue(kOuterPixelRounding);

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

		auto topMenuHeight = scaleValueRoundInt(kTopMenuHeight);
		auto delimiterToTextSelectorMargin = scaleValueRoundInt(kDelimiterToTextSelectorMargin);

		// drawing separator line between header and main body
		g.setColour(getColour(Skin::kBackgroundElement));
		g.fillRect(0.0f, rectangleBounds.getY() + (float)topMenuHeight, rectangleBounds.getRight(), 1.0f);

		// drawing separator line between type and algo
		auto lineX = effectTypeSelector_->getRight() + delimiterToTextSelectorMargin;
		auto lineY = centerVertically((int)rectangleBounds.getY(), topMenuHeight / 2, topMenuHeight);
		g.fillRect(lineX, lineY, 1, topMenuHeight / 2);

		drawSliderLabel(g, mixNumberBox_.get());

		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
		paintUIBackground(g);
	}

	void EffectModuleSection::repaintBackground()
	{
		auto backgroundImage = Image{ Image::ARGB, getWidth(), getHeight(), true };

		Graphics backgroundGraphics{ backgroundImage };
		paintBackground(backgroundGraphics);
		background_.setOwnImage(backgroundImage);
	}

	void EffectModuleSection::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		if (!isVisible() || !OpenGlComponent::setViewPort(this, openGl))
			return;

		background_.setTopLeft(-1.0f, 1.0f);
		background_.setTopRight(1.0f, 1.0f);
		background_.setBottomLeft(-1.0f, -1.0f);
		background_.setBottomRight(1.0f, -1.0f);

		background_.render();

		BaseSection::renderOpenGlComponents(openGl, animate);
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
			// SAFETY: we know what type BaseProcessor we'll be getting so static_cast is safe
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
		mixNumberBox_->resized();
		moduleActivator_->resized();
		maskComponent_->resized();

		// replacing the mapped out parameters, if there are any
		auto parametersBridgesToRelink = std::min(replacedEffect->getNumParameters(), newEffect->getNumParameters());
		for (size_t i = 0; i < parametersBridgesToRelink; i++)
		{
			auto controlVariant = replacedEffect->getParameterUnchecked(i)->changeControl(std::variant<ParameterUI *,
				ParameterBridge *>(std::in_place_index<1>, nullptr));
			// we don't check has_variant() because we always receive something from changeControl
			if (auto *bridge = std::get<1>(controlVariant))
				bridge->resetParameterLink(newEffect->getParameterUnchecked(i)->getParameterLink());
		}
		
		// replacing the parameters for the algo and mask sliders
		bool getValueFromParameter = true;
		for (size_t i = 0; i < magic_enum::enum_count<BaseEffectParameters>(); i++)
		{
			auto parameter = utils::as<BaseSlider *>(getEffectParameter(i));
			parameter->changeLinkedParameter(newEffect->getParameterUnchecked(i), getValueFromParameter);
			parameter->resized();
			getValueFromParameter = false;
		}
	}

	ParameterUI *EffectModuleSection::getEffectParameter(size_t index) noexcept
	{
		using namespace Framework;

		switch (index)
		{
		case (u32)BaseEffectParameters::Algorithm:
			return effectAlgoSelector_.get();
		case (u32)BaseEffectParameters::LowBound:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::LowBound].name);
		case (u32)BaseEffectParameters::HighBound:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::HighBound].name);
		case (u32)BaseEffectParameters::ShiftBounds:
			return maskComponent_->getSlider(baseEffectParameterList[(u32)BaseEffectParameters::ShiftBounds].name);
		default:
			return effectParameters_[index - magic_enum::enum_count<BaseEffectParameters>()].get();
		}
	}


	namespace
	{
		void initFilterParameters(std::vector<std::unique_ptr<ParameterUI>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			effectSliders.reserve(FilterEffectParameters::size);
			auto *baseEffect = section->getEffect();

			auto initNormal = [&]()
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<FilterEffectParameters::Normal>(); ++i)
				{
					effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getEffectParameter(i)));

					section->addSlider(utils::as<BaseSlider *>(effectSliders[i].get()));
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
				auto knobEdgeOffset = section->scaleValueRoundInt(32);
				auto knobTopOffset = section->scaleValueRoundInt(32);

				auto knobsWidth = RotarySlider::kDefaultWidthHeight;
				auto knobsHeight = RotarySlider::kDefaultWidthHeight;

				bounds = bounds.withTrimmedLeft(knobEdgeOffset).withTrimmedRight(knobEdgeOffset)
					.withTrimmedTop(knobTopOffset).withHeight(knobsHeight);

				// gain rotary and label
				auto *gainSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Gain));
				gainSlider->setBounds(bounds.getX(), bounds.getY(), knobsWidth, knobsHeight);

				auto *gainSliderLabel = gainSlider->getLabelComponent();
				auto labelBounds = Rectangle{ gainSlider->getRight() + RotarySlider::kLabelOffset,
					gainSlider->getY() + RotarySlider::kLabelVerticalPadding,
					gainSliderLabel->getTotalWidth(),
					gainSlider->getHeight() - 2 * RotarySlider::kLabelVerticalPadding };
				gainSliderLabel->setBounds(labelBounds);

				// cutoff rotary and label
				auto *cutoffSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Cutoff));
				cutoffSlider->setBounds((int)std::round((float)bounds.getX() + 1.0f / 3.0f * (float)bounds.getWidth()),
					bounds.getY(), knobsWidth, knobsHeight);

				auto *cutoffSliderLabel = cutoffSlider->getLabelComponent();
				labelBounds.setPosition(cutoffSlider->getRight() + RotarySlider::kLabelOffset, labelBounds.getY());
				labelBounds.setWidth(cutoffSliderLabel->getTotalWidth());
				cutoffSliderLabel->setBounds(labelBounds);

				// slope rotary and label
				auto *slopeSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Slope));
				slopeSlider->setBounds((int)std::round((float)bounds.getX() + 2.0f / 3.0f * (float)bounds.getWidth()),
					bounds.getY(), knobsWidth, knobsHeight);

				auto *slopeSliderLabel = slopeSlider->getLabelComponent();
				labelBounds.setPosition(slopeSlider->getRight() + RotarySlider::kLabelOffset, labelBounds.getY());
				labelBounds.setWidth(slopeSliderLabel->getTotalWidth());
				slopeSliderLabel->setBounds(labelBounds);
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

		void paintFilterBackground(Graphics &g, EffectModuleSection *section)
		{
			using namespace Framework;

			auto paintNormal = [&]()
			{
				auto *gainSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Gain));
				section->drawSliderLabel(g, gainSlider);

				auto *cutoffSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Cutoff));
				section->drawSliderLabel(g, cutoffSlider);

				auto *slopeSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)FilterEffectParameters::Normal::Slope));
				section->drawSliderLabel(g, slopeSlider);
			};

			auto paintRegular = [&]()
			{

			};

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)FilterModes::Normal:
				paintNormal();
				break;
			default:
			case (u32)FilterModes::Regular:
				paintRegular();
				break;
			}
		}

		void initDynamicsParameters(std::vector<std::unique_ptr<ParameterUI>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			effectSliders.reserve(FilterEffectParameters::size);
			auto *baseEffect = section->getEffect();

			auto initContrast = [&]()
			{
				for (size_t i = 0; i < (u32)magic_enum::enum_count<DynamicsEffectParameters::Contrast>(); i++)
				{
					effectSliders.emplace_back(std::make_unique<RotarySlider>(baseEffect->getEffectParameter(i)));

					section->addSlider(utils::as<BaseSlider *>(effectSliders[i].get()));
				}
			};

			auto initCompressor = [&]() { };

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)DynamicsModes::Contrast:
				initContrast();
				break;
			default:
			case (u32)DynamicsModes::Compressor:
				initCompressor();
				break;
			}
		}

		void arrangeDynamicsUI(EffectModuleSection *section, Rectangle<int> bounds)
		{
			using namespace Framework;

			auto arrangeContrast = [&]()
			{
				// TEST
				auto knobEdgeOffset = section->scaleValueRoundInt(32);
				auto knobTopOffset = section->scaleValueRoundInt(32);

				auto knobsWidth = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);
				auto knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				// depth rotary and label
				auto *depthSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)DynamicsEffectParameters::Contrast::Depth));
				depthSlider->setBounds(bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset, knobsWidth, knobsHeight);

				auto *depthSliderLabel = depthSlider->getLabelComponent();
				auto labelBounds = Rectangle{ depthSlider->getRight() + section->scaleValueRoundInt(RotarySlider::kLabelOffset),
					depthSlider->getY() + section->scaleValueRoundInt(RotarySlider::kLabelVerticalPadding),
					depthSliderLabel->getTotalWidth(),
					depthSlider->getHeight() - 2 * section->scaleValueRoundInt(RotarySlider::kLabelVerticalPadding) };
				depthSliderLabel->setBounds(labelBounds);
			};

			auto arrangeCompressor = [&]()
			{

			};

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)DynamicsModes::Contrast:
				arrangeContrast();
				break;
			default:
			case (u32)DynamicsModes::Compressor:
				arrangeCompressor();
				break;
			}
		}

		void paintDynamicsBackground(Graphics &g, EffectModuleSection *section)
		{
			using namespace Framework;

			auto paintContrast = [&]()
			{
				auto *depthSlider = utils::as<BaseSlider *>(section->getEffectParameter((u32)DynamicsEffectParameters::Contrast::Depth));
				section->drawSliderLabel(g, depthSlider);
			};

			auto paintCompressor = [&]()
			{

			};

			switch ((u32)section->getEffectParameter((u32)BaseEffectParameters::Algorithm)->getValueSafeScaled())
			{
			case (u32)DynamicsModes::Contrast:
				paintContrast();
				break;
			default:
			case (u32)DynamicsModes::Compressor:
				paintCompressor();
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
			paintBackgroundFunction_ = paintFilterBackground;

			setSkinOverride(Skin::kFilterModule);
			maskComponent_->setSkinOverride(Skin::kFilterModule);
			effectTypeIcon_.setShape(Path{});
		}
		else if (type == dynamicsEffect::getClassType())
		{
			initialiseParametersFunction_ = initDynamicsParameters;
			arrangeUIFunction_ = arrangeDynamicsUI;
			paintBackgroundFunction_ = paintDynamicsBackground;

			setSkinOverride(Skin::kDynamicsModule);
			maskComponent_->setSkinOverride(Skin::kDynamicsModule);
			effectTypeIcon_.setShape(Paths::contrastIcon());
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
