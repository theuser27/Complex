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
		EmptySlider(Framework::ParameterValue *parameter) : PinSlider(parameter)
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

	void SpectralMaskComponent::sliderValueChanged(BaseSlider *slider)
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
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleType::self()),
			Fonts::instance()->getInterVFont().withStyle(Font::bold));
		addControl(effectTypeSelector_.get());

		effectTypeIcon_ = makeOpenGlComponent<PlainShapeComponent>("Effect Type Icon");
		effectTypeIcon_->setJustification(Justification::centred);
		effectTypeSelector_->setExtraIcon(effectTypeIcon_.get());
		addOpenGlComponent(effectTypeIcon_);

		mixNumberBox_ = std::make_unique<NumberBox>(
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleMix::self()));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		addControl(mixNumberBox_.get());

		moduleActivator_ = std::make_unique<PowerButton>(
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleEnabled::self()));
		addControl(moduleActivator_.get());
		setActivator(moduleActivator_.get());

		auto *baseEffect = effectModule->getEffect();
		auto effectIndex = (u32)effectTypeSelector_->getValueSafeScaled();
		cachedEffects_[effectIndex] = baseEffect;

		effectAlgoSelector_ = std::make_unique<TextSelector>(
			baseEffect->getParameter(BaseProcessors::BaseEffect::Algorithm::self()),
			Fonts::instance()->getInterVFont());
		addControl(effectAlgoSelector_.get());

		maskComponent_ = std::make_unique<SpectralMaskComponent>(
			baseEffect->getParameter(BaseProcessors::BaseEffect::LowBound::self()),
			baseEffect->getParameter(BaseProcessors::BaseEffect::HighBound::self()),
			baseEffect->getParameter(BaseProcessors::BaseEffect::ShiftBounds::self()));
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
		auto effectTypeSelectorBounds = effectTypeSelector_->getBoundsForSizes(effectSelectorsHeight);
		effectTypeSelector_->setOverallBounds(currentPoint);

		currentPoint.x += effectTypeSelectorBounds.getWidth() + scaleValueRoundInt(kDelimiterWidth) +
			2 * scaleValueRoundInt(kDelimiterToTextSelectorMargin);
		std::ignore = effectAlgoSelector_->getBoundsForSizes(effectSelectorsHeight);
		effectAlgoSelector_->setOverallBounds(currentPoint);

		// right hand side
		int mixNumberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		auto mixNumberBoxBounds = mixNumberBox_->getBoundsForSizes(mixNumberBoxHeight);
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

	void EffectModuleSection::sliderValueChanged(BaseSlider *slider)
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

	void EffectModuleSection::automationMappingChanged([[maybe_unused]] BaseSlider *slider)
	{
		auto controlIter = std::ranges::find_if(effectControls_, [&](auto &&control) { return control.get() == slider; });
		if (effectControls_.end() == controlIter)
			return;

		auto index = controlIter - effectControls_.begin();
		auto parameterIter = parameterMappings.find(index);
		if (parameterMappings.data.end() == parameterIter)
			parameterMappings.add(index, (*controlIter)->getParameterLink()->hostControl);
		else
			parameterMappings.erase(parameterIter);
	}

	void EffectModuleSection::changeEffect()
	{
		using namespace Framework;

		auto effectIndex = (u32)effectTypeSelector_->getValueSafeScaled();

		// getting the desired effect if it's cached
		// if not, create and cache it
		Generation::baseEffect *newEffect = cachedEffects_[effectIndex];
		if (!newEffect)
		{
			newEffect = utils::as<Generation::baseEffect *>(effectModule_
				->createSubProcessor(Generation::effectsTypes[effectIndex]));
			cachedEffects_[effectIndex] = newEffect;
		}

		auto replace = [newEffect, this]() { effectModule_->updateSubProcessor(0, newEffect); };
		effectModule_->getProcessorTree()->executeOutsideProcessing(replace);

		// resetting UI
		setEffectType(newEffect->getProcessorType());
		effectTypeSelector_->resized();
		effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
		mixNumberBox_->resized();
		moduleActivator_->resized();
		maskComponent_->resized();

		// replacing mapped out parameters, if there are any
		auto [parametersStart, parametersCount] = Parameters::getIndexAndCountForEffect(BaseProcessors::BaseEffect::make_enum(effectIndex).value(),
			newEffect->getParameter(BaseProcessors::BaseEffect::Algorithm::self())->getInternalValue<u32>()).value();

		for (auto &[mappingIndex, mappedParameter] : parameterMappings.data)
		{
			if (auto *link = mappedParameter->getParameterLink(); link && link->parameter)
				link->parameter->changeControl({ (ParameterBridge *)nullptr });

			if (mappingIndex >= parametersCount)
			{
				String name = "Module P";
				name += mappingIndex;
				mappedParameter->setCustomName(name);
				continue;
			}
			
			mappedParameter->resetParameterLink(newEffect->getParameterUnchecked(parametersStart + mappingIndex)->getParameterLink(), false);
		}
		
		// replacing the parameters for algorithm and mask sliders
		for (BaseProcessors::BaseEffect::type value : BaseProcessors::BaseEffect::enum_values<nested_enum::OuterNodes>())
		{
			auto *control = getEffectControl(value);
			control->changeLinkedParameter(*newEffect->getParameter(value), value == BaseProcessors::BaseEffect::Algorithm);
			control->resized();
		}
	}


	namespace
	{
		void initFilterParameters(std::vector<std::unique_ptr<BaseControl>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			auto *baseEffect = section->getEffect();

			auto initNormal = [&]()
			{
				effectSliders.reserve(BaseProcessors::BaseEffect::Filter::Normal::enum_count(nested_enum::OuterNodes));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Gain::self())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Cutoff::self())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Slope::self())));
			};

			auto initRegular = [&]() { };


			switch (section->getAlgorithm<BaseProcessors::BaseEffect::Filter::type>())
			{
			case BaseProcessors::BaseEffect::Filter::Normal:
				initNormal();
				break;
			default:
			case BaseProcessors::BaseEffect::Filter::Regular:
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
				auto *gainSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Gain::self());
				std::ignore = gainSlider->getBoundsForSizes(knobsHeight);
				gainSlider->setOverallBounds({ bounds.getX(), bounds.getY() });

				// cutoff rotary
				auto *cutoffSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Cutoff::self());
				std::ignore = cutoffSlider->getBoundsForSizes(knobsHeight);
				cutoffSlider->setOverallBounds({ bounds.getX() + rotaryInterval, bounds.getY() });

				// slope rotary
				auto *slopeSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Slope::self());
				std::ignore = slopeSlider->getBoundsForSizes(knobsHeight);
				slopeSlider->setOverallBounds({ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
			};

			auto arrangeRegular = [&]()
			{

			};

			switch (section->getAlgorithm<BaseProcessors::BaseEffect::Filter::type>())
			{
			case BaseProcessors::BaseEffect::Filter::Normal:
				arrangeNormal();
				break;
			default:
			case BaseProcessors::BaseEffect::Filter::Regular:
				arrangeRegular();
				break;
			}
		}

		void initDynamicsParameters(std::vector<std::unique_ptr<BaseControl>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			auto *baseEffect = section->getEffect();

			auto initContrast = [&]()
			{
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Dynamics::Contrast::Depth::self())));
			};

			auto initClip = [&]()
			{
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Dynamics::Clip::Threshold::self())));
			};
			
			auto initCompressor = [&]() { };

			switch (section->getAlgorithm<BaseProcessors::BaseEffect::Dynamics::type>())
			{
			case BaseProcessors::BaseEffect::Dynamics::Contrast:
				initContrast();
				break;
			case BaseProcessors::BaseEffect::Dynamics::Clip:
				initClip();
				break;
			case BaseProcessors::BaseEffect::Dynamics::Compressor:
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
				auto *depthSlider = section->getEffectControl(BaseProcessors::BaseEffect::Dynamics::Contrast::Depth::self());
				std::ignore = depthSlider->getBoundsForSizes(knobsHeight);
				depthSlider->setOverallBounds({ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeClip = [&]()
			{
				// TEST
				int knobEdgeOffset = section->scaleValueRoundInt(32);
				int knobTopOffset = section->scaleValueRoundInt(32);

				int knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				// depth rotary and label
				auto *thresholdSlider = section->getEffectControl(BaseProcessors::BaseEffect::Dynamics::Clip::Threshold::self());
				std::ignore = thresholdSlider->getBoundsForSizes(knobsHeight);
				thresholdSlider->setOverallBounds({ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeCompressor = [&]() { };

			switch (section->getAlgorithm<BaseProcessors::BaseEffect::Dynamics::type>())
			{
			case BaseProcessors::BaseEffect::Dynamics::Contrast:
				arrangeContrast();
				break;
			case BaseProcessors::BaseEffect::Dynamics::Clip:
				arrangeClip();
				break;
			case BaseProcessors::BaseEffect::Dynamics::Compressor:
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
