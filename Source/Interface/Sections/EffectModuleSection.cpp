/*
	==============================================================================

		EffectModuleSection.cpp
		Created: 14 Feb 2023 2:29:16am
		Author:  theuser27

	==============================================================================
*/

#include "EffectModuleSection.h"

#include "Plugin/ProcessorTree.h"
#include "Framework/parameter_value.h"
#include "Framework/parameter_bridge.h"
#include "Generation/EffectModules.h"
#include "../LookAndFeel/Paths.h"
#include "../LookAndFeel/Fonts.h"
#include "../Components/OpenGlImageComponent.h"
#include "../Components/BaseButton.h"
#include "../Components/BaseSlider.h"
#include "../Components/Spectrogram.h"
#include "../Components/PinBoundsBox.h"
#include "EffectsLaneSection.h"

namespace Interface
{
	class EmptySlider final : public PinSlider
	{
	public:
		EmptySlider(Framework::ParameterValue *parameter) : PinSlider(parameter)
		{
			setShouldShowPopup(true);
			for (auto &component : openGlComponents_)
				removeOpenGlComponent(component.get());
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

		void mouseDown(const MouseEvent &e) override
		{
			if (e.mods.isAnyModifierKeyDown())
			{
				isExpanded_ = !isExpanded_;
				COMPLEX_ASSERT(listener_ && "This spectral mask was not given a pointer to owner to notify for an expansion change");
				listener_->expansionChange(isExpanded_);
			}
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
			shiftBounds_.setTotalRange(getWidth());
			PinBoundsBox::resized();
		}
		void sliderValueChanged(BaseSlider *slider) override
		{
			if (&shiftBounds_ == slider)
			{
				highlight_->redrawImage();
				//simd_float shiftValues = shiftBounds_->getInternalValue<simd_float>(kDefaultSampleRate);
				//simd_float lowValues = simd_float::clamp(lowBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
				//simd_float highValues = simd_float::clamp(highBound_->getInternalValue<simd_float>(kDefaultSampleRate, true) + shiftValues, 0.0f, 1.0f);
				//highlight_->setStaticValues(lowValues[0], 0);
				//highlight_->setStaticValues(lowValues[2], 1);
				//highlight_->setStaticValues(highValues[0], 2);
				//highlight_->setStaticValues(highValues[2], 3);
				return;
			}

			PinBoundsBox::sliderValueChanged(slider);
		}

		void setListener(SpectralMaskListener *listener) noexcept { listener_ = listener; }

	private:
		// TODO: finish spectrogram and expanding behaviour
		EmptySlider shiftBounds_;
		SpectralMaskListener *listener_ = nullptr;
		bool isExpanded_ = false;
	};

	EffectModuleSection::EffectModuleSection(Generation::EffectModule *effectModule, EffectsLaneSection *laneSection) :
		ProcessorSection(typeid(EffectModuleSection).name(), effectModule), laneSection_(laneSection), effectModule_(effectModule)
	{
		using namespace Framework;

		setInterceptsMouseClicks(true, true);

		draggableBox_.setDraggedComponent(this);
		addAndMakeVisible(draggableBox_);

		effectTypeSelector_ = std::make_unique<TextSelector>(
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleType::name()),
			Fonts::instance()->getInterVFont().withStyle(Font::bold));
		addControl(effectTypeSelector_.get());

		effectTypeIcon_ = makeOpenGlComponent<PlainShapeComponent>("Effect Type Icon");
		effectTypeIcon_->setJustification(Justification::centred);
		effectTypeIcon_->setAlwaysOnTop(true);
		effectTypeSelector_->setExtraIcon(effectTypeIcon_.get());
		addOpenGlComponent(effectTypeIcon_);

		mixNumberBox_ = std::make_unique<NumberBox>(
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleMix::name()));
		mixNumberBox_->setMaxTotalCharacters(5);
		mixNumberBox_->setMaxDecimalCharacters(2);
		addControl(mixNumberBox_.get());

		moduleActivator_ = std::make_unique<PowerButton>(
			effectModule->getParameter(BaseProcessors::EffectModule::ModuleEnabled::name()));
		addControl(moduleActivator_.get());
		setActivator(moduleActivator_.get());

		auto *baseEffect = effectModule->getEffect();
		auto effectIndex = (u32)effectTypeSelector_->getValueSafeScaled();
		cachedEffects_[effectIndex] = baseEffect;

		effectAlgoSelector_ = std::make_unique<TextSelector>(
			baseEffect->getParameter(BaseProcessors::BaseEffect::Algorithm::name()),
			Fonts::instance()->getInterVFont());
		addControl(effectAlgoSelector_.get());

		maskComponent_ = std::make_unique<SpectralMaskComponent>(
			baseEffect->getParameter(BaseProcessors::BaseEffect::LowBound::name()),
			baseEffect->getParameter(BaseProcessors::BaseEffect::HighBound::name()),
			baseEffect->getParameter(BaseProcessors::BaseEffect::ShiftBounds::name()));
		maskComponent_->setListener(this);
		addSubSection(maskComponent_.get());

		setEffectType(baseEffect->getProcessorType());
		initialiseParameters();
	}

	EffectModuleSection::~EffectModuleSection()
	{
		for (Generation::baseEffect *cachedEffect : cachedEffects_)
			if (cachedEffect && cachedEffect != effectModule_->getSubProcessor(0))
				effectModule_->getProcessorTree()->deleteProcessor(cachedEffect->getProcessorId());
	}

	std::unique_ptr<EffectModuleSection> EffectModuleSection::createCopy() const
	{
		auto copiedModule = utils::as<Generation::EffectModule>(effectModule_
			->getProcessorTree()->copyProcessor(effectModule_));
		return std::make_unique<EffectModuleSection>(copiedModule, laneSection_);
	}

	void EffectModuleSection::resized()
	{
		BaseSection::resized();

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
		Rectangle dropdownHitbox{ 0, yOffset, getWidth(), topMenuHeight };

		if (!dropdownHitbox.contains(e.getPosition()) && !e.mods.isPopupMenu())
			return;

		PopupItems options = createPopupMenu();
		showPopupSelector(this, e.getPosition(), std::move(options),
			[this](int selection) { handlePopupResult(selection); });
	}

	void EffectModuleSection::initialiseParameters()
	{
		COMPLEX_ASSERT(initialiseParametersFunction_ && "No initParametersFunction was provided");

		for (auto &control : effectControls_)
			removeControl(control.get());

		effectControls_ = decltype(effectControls_){};

		initialiseParametersFunction_(effectControls_, this);
		for (auto &control : effectControls_)
			addControl(control.get());
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
		draggableBox_.setBounds(Rectangle{ 0, yOffset, scaleValueRoundInt(kDraggableSectionWidth), topMenuHeight });

		int effectTypeSelectorIconDimensions = scaleValueRoundInt(kIconSize);
		effectTypeIcon_->setColor(getColour(Skin::kWidgetPrimary1));
		effectTypeIcon_->setSize(effectTypeSelectorIconDimensions, effectTypeSelectorIconDimensions);

		auto currentPoint = Point{ draggableBox_.getRight(), centerVertically(yOffset, effectSelectorsHeight, topMenuHeight) };
		auto effectTypeSelectorBounds = effectTypeSelector_->setBoundsForSizes(effectSelectorsHeight);
		effectTypeSelector_->setPosition(currentPoint);

		currentPoint.x += effectTypeSelectorBounds.getWidth() + scaleValueRoundInt(kDelimiterWidth) +
			2 * scaleValueRoundInt(kDelimiterToTextSelectorMargin);
		std::ignore = effectAlgoSelector_->setBoundsForSizes(effectSelectorsHeight);
		effectAlgoSelector_->setPosition(currentPoint);

		// right hand side
		int mixNumberBoxHeight = scaleValueRoundInt(NumberBox::kDefaultNumberBoxHeight);
		auto mixNumberBoxBounds = mixNumberBox_->setBoundsForSizes(mixNumberBoxHeight);
		mixNumberBox_->setPosition(Point{ moduleActivator_->getX() - mixNumberBoxBounds.getRight() - scaleValueRoundInt(kNumberBoxToPowerButtonMargin),
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

	void EffectModuleSection::automationMappingChanged(BaseSlider *slider)
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

	Generation::baseEffect *EffectModuleSection::getEffect() noexcept { return effectModule_->getEffect(); }
	u64 EffectModuleSection::getAlgorithm() const noexcept { return (u64)effectAlgoSelector_->getValueSafeScaled(); }

	BaseControl *EffectModuleSection::getEffectControl(std::string_view name)
	{
		using namespace Framework;

		static constexpr auto baseEffectIds = BaseProcessors::BaseEffect::enum_names<nested_enum::OuterNodes>();
		if (std::ranges::find(baseEffectIds, name) != baseEffectIds.end())
		{
			if (name == BaseProcessors::BaseEffect::Algorithm::name())
				return effectAlgoSelector_.get();

			return maskComponent_->getControl(name);
		}

		for (auto &control : effectControls_)
			if (control->getParameterDetails().pluginName == name)
				return control.get();

		COMPLEX_ASSERT_FALSE("Parameter could not be found");
		return nullptr;
	}

	void EffectModuleSection::handlePopupResult(int result) const noexcept
	{
		if (result == kDeleteInstance)
		{
			laneSection_->deleteModule(this, true);
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
		PopupItems options{ getName().toStdString() };

		options.addItem(kDeleteInstance, "Delete");
		options.addItem(kCopyInstance, "Copy (TODO)");
		options.addItem(kInitInstance, "Re-initialise");

		return options;
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
			newEffect = utils::as<Generation::baseEffect>(effectModule_
				->createSubProcessor(BaseProcessors::BaseEffect::enum_ids<nested_enum::InnerNodes>()[effectIndex].value()));
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
			newEffect->getParameter(BaseProcessors::BaseEffect::Algorithm::name())->getInternalValue<u32>()).value();

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
		for (std::string_view value : BaseProcessors::BaseEffect::enum_names<nested_enum::OuterNodes>())
		{
			auto *control = getEffectControl(value);
			control->changeLinkedParameter(*newEffect->getParameter(value), value == BaseProcessors::BaseEffect::Algorithm::name());
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
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Gain::name())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Cutoff::name())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Filter::Normal::Slope::name())));
			};

			auto initRegular = [&]() { };


			switch (section->getAlgorithm())
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
				auto *gainSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Gain::name());
				std::ignore = gainSlider->setBoundsForSizes(knobsHeight);
				gainSlider->setPosition(Point{ bounds.getX(), bounds.getY() });

				// cutoff rotary
				auto *cutoffSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Cutoff::name());
				std::ignore = cutoffSlider->setBoundsForSizes(knobsHeight);
				cutoffSlider->setPosition(Point{ bounds.getX() + rotaryInterval, bounds.getY() });

				// slope rotary
				auto *slopeSlider = section->getEffectControl(BaseProcessors::BaseEffect::Filter::Normal::Slope::name());
				std::ignore = slopeSlider->setBoundsForSizes(knobsHeight);
				slopeSlider->setPosition(Point{ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
			};

			auto arrangeRegular = [&]()
			{

			};

			switch (section->getAlgorithm())
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
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Dynamics::Contrast::Depth::name())));
			};

			auto initClip = [&]()
			{
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Dynamics::Clip::Threshold::name())));
			};
			
			auto initCompressor = [&]() { };

			switch (section->getAlgorithm())
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
				auto *depthSlider = section->getEffectControl(BaseProcessors::BaseEffect::Dynamics::Contrast::Depth::name());
				std::ignore = depthSlider->setBoundsForSizes(knobsHeight);
				depthSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeClip = [&]()
			{
				// TEST
				int knobEdgeOffset = section->scaleValueRoundInt(32);
				int knobTopOffset = section->scaleValueRoundInt(32);

				int knobsHeight = section->scaleValueRoundInt(RotarySlider::kDefaultWidthHeight);

				// depth rotary and label
				auto *thresholdSlider = section->getEffectControl(BaseProcessors::BaseEffect::Dynamics::Clip::Threshold::name());
				std::ignore = thresholdSlider->setBoundsForSizes(knobsHeight);
				thresholdSlider->setPosition(Point{ bounds.getX() + knobEdgeOffset, bounds.getY() + knobTopOffset });
			};

			auto arrangeCompressor = [&]() { };

			switch (section->getAlgorithm())
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

		void initPhaseParameters(std::vector<std::unique_ptr<BaseControl>> &effectSliders, EffectModuleSection *section)
		{
			using namespace Framework;

			auto *baseEffect = section->getEffect();

			auto initShift = [&]()
			{
				effectSliders.reserve(BaseProcessors::BaseEffect::Phase::Shift::enum_count(nested_enum::OuterNodes));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Phase::Shift::PhaseShift::name())));
				effectSliders.emplace_back(std::make_unique<TextSelector>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Phase::Shift::Slope::name())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Phase::Shift::Interval::name())));
				effectSliders.emplace_back(std::make_unique<RotarySlider>
					(baseEffect->getParameter(BaseProcessors::BaseEffect::Phase::Shift::Offset::name())));
			};

			switch (section->getAlgorithm())
			{
			case BaseProcessors::BaseEffect::Phase::Shift:
				initShift();
				break;
			default:
			case BaseProcessors::BaseEffect::Phase::Transform:
				break;
			}
		}

		void arrangePhaseUI(EffectModuleSection *section, Rectangle<int> bounds)
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

				auto *slopeDropdown = static_cast<TextSelector *>(section->getEffectControl(BaseProcessors::BaseEffect::Phase::Shift::Slope::name()));

				auto *shiftSlider = static_cast<RotarySlider *>(section->getEffectControl(BaseProcessors::BaseEffect::Phase::Shift::PhaseShift::name()));
				shiftSlider->setModifier(slopeDropdown);
				shiftSlider->setLabelPlacement(BubbleComponent::right);
				std::ignore = shiftSlider->setBoundsForSizes(knobsHeight);
				shiftSlider->setPosition({ bounds.getX(), bounds.getY() });


				auto *intervalSlider = section->getEffectControl(BaseProcessors::BaseEffect::Phase::Shift::Interval::name());
				std::ignore = intervalSlider->setBoundsForSizes(knobsHeight);
				intervalSlider->setPosition({ bounds.getX() + rotaryInterval, bounds.getY() });

				auto *offsetSlider = section->getEffectControl(BaseProcessors::BaseEffect::Phase::Shift::Offset::name());
				std::ignore = offsetSlider->setBoundsForSizes(knobsHeight);
				offsetSlider->setPosition({ bounds.getX() + 2 * rotaryInterval, bounds.getY() });
			};

			switch (section->getAlgorithm())
			{
			case BaseProcessors::BaseEffect::Phase::Shift:
				arrangeNormal();
				break;
			default:
			case BaseProcessors::BaseEffect::Phase::Transform:
				break;
			}
		}
	}

	void EffectModuleSection::setEffectType(std::string_view type)
	{
		using namespace Framework;

		paintBackgroundFunction_ = nullptr;

		if (type == BaseProcessors::BaseEffect::Utility::id().value())
		{
			
		}
		else if (type == BaseProcessors::BaseEffect::Filter::id().value())
		{
			initialiseParametersFunction_ = initFilterParameters;
			arrangeUIFunction_ = arrangeFilterUI;

			setSkinOverride(Skin::kFilterModule);
			maskComponent_->setSkinOverride(Skin::kFilterModule);
			effectTypeIcon_->setShapes(Paths::filterIcon());
		}
		else if (type == BaseProcessors::BaseEffect::Dynamics::id().value())
		{
			initialiseParametersFunction_ = initDynamicsParameters;
			arrangeUIFunction_ = arrangeDynamicsUI;

			setSkinOverride(Skin::kDynamicsModule);
			maskComponent_->setSkinOverride(Skin::kDynamicsModule);
			effectTypeIcon_->setShapes(Paths::dynamicsIcon());
		}
		else if (type == BaseProcessors::BaseEffect::Phase::id().value())
		{
			initialiseParametersFunction_ = initPhaseParameters;
			arrangeUIFunction_ = arrangePhaseUI;

			setSkinOverride(Skin::kPhaseModule);
			maskComponent_->setSkinOverride(Skin::kPhaseModule);
			effectTypeIcon_->setShapes(Paths::phaseIcon());
		}
		else if (type == BaseProcessors::BaseEffect::Pitch::id().value())
		{
			
		}
		else if (type == BaseProcessors::BaseEffect::Stretch::id().value())
		{
			
		}
		else if (type == BaseProcessors::BaseEffect::Warp::id().value())
		{
			
		}
		else if (type == BaseProcessors::BaseEffect::Destroy::id().value())
		{
			
		}
	}
}
