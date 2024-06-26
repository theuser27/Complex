/*
	==============================================================================

		MainInterface.cpp
		Created: 10 Oct 2022 6:02:52pm
		Author:  theuser27

	==============================================================================
*/

#include "Generation/SoundEngine.h"
#include "Plugin/Complex.h"
#include "Plugin/Renderer.h"
#include "../LookAndFeel/DefaultLookAndFeel.h"
#include "Popups.h"
#include "MainInterface.h"
#include "HeaderFooterSections.h"
#include "EffectsStateSection.h"

namespace Interface
{
	MainInterface::MainInterface(Renderer *renderer) : BaseSection(typeid(MainInterface).name())
	{
		renderer_ = renderer;

		headerFooter_ = std::make_unique<HeaderFooterSections>(renderer->getPlugin().getSoundEngine());
		addSubSection(headerFooter_.get());

		effectsStateSection_ = std::make_unique<EffectsStateSection>(
			renderer->getPlugin().getSoundEngine().getEffectsState());
		addSubSection(effectsStateSection_.get());

		popupSelector_ = std::make_unique<SinglePopupSelector>();
		addSubSection(popupSelector_.get());
		popupSelector_->setVisible(false);
		popupSelector_->setAlwaysOnTop(true);
		popupSelector_->setWantsKeyboardFocus(true);

		dualPopupSelector_ = std::make_unique<DualPopupSelector>();
		addSubSection(dualPopupSelector_.get());
		dualPopupSelector_->setVisible(false);
		dualPopupSelector_->setAlwaysOnTop(true);
		dualPopupSelector_->setWantsKeyboardFocus(true);

		popupDisplay1_ = std::make_unique<PopupDisplay>();
		addSubSection(popupDisplay1_.get());
		popupDisplay1_->setVisible(false);
		popupDisplay1_->setAlwaysOnTop(true);
		popupDisplay1_->setWantsKeyboardFocus(false);

		popupDisplay2_ = std::make_unique<PopupDisplay>();
		addSubSection(popupDisplay2_.get());
		popupDisplay2_->setVisible(false);
		popupDisplay2_->setAlwaysOnTop(true);
		popupDisplay2_->setWantsKeyboardFocus(false);

		popupSelector_->toFront(true);
		dualPopupSelector_->toFront(true);
		popupDisplay1_->toFront(true);
		popupDisplay2_->toFront(true);

		setOpaque(false);
	}

	MainInterface::~MainInterface() = default;

	void MainInterface::reloadSkin(const Skin &skin)
	{
		skin.copyValuesToLookAndFeel(DefaultLookAndFeel::instance());
		juce::Rectangle<int> bounds = getBounds();
		// triggering repainting like a boss
		setBounds(0, 0, bounds.getWidth() / 4, bounds.getHeight() / 4);
		setBounds(bounds);
	}

	void MainInterface::resized()
	{
		int width = getWidth();
		int height = getHeight();

		headerFooter_->setBounds(0, 0, width, height);

		int effectsStateXStart = scaleValueRoundInt(kHorizontalWindowEdgeMargin);
		int effectsStateYStart = scaleValueRoundInt(kHeaderHeight +
			kMainVisualiserHeight + kVerticalGlobalMargin);
		int effectsStateWidth = scaleValueRoundInt(kEffectsStateMinWidth);
		int effectsStateHeight = getHeight() - effectsStateYStart - 
			scaleValueRoundInt(kLaneToBottomSettingsMargin + kFooterHeight);
		juce::Rectangle bounds{ effectsStateXStart, effectsStateYStart, effectsStateWidth, effectsStateHeight };

		effectsStateSection_->setBounds(bounds);
	}

	void MainInterface::reset()
	{
		BaseSection::reset();
		//modulationChanged();
		/*if (effects_interface_ && effects_interface_->isVisible())
			effects_interface_->redoBackgroundImage();*/
	}

	void MainInterface::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		utils::ScopedLock g{ renderLock_, utils::WaitMechanism::WaitNotify };
		BaseSection::renderOpenGlComponents(openGl, animate);
	}

	void MainInterface::popupSelector(const BaseComponent *source, juce::Point<int> position, PopupItems options, 
		Skin::SectionOverride skinOverride, std::function<void(int)> callback, std::function<void()> cancel)
	{
		popupSelector_->setPopupSkinOverride(skinOverride);
		popupSelector_->setCallback(std::move(callback));
		popupSelector_->setCancelCallback(std::move(cancel));
		popupSelector_->showSelections(std::move(options));
		juce::Rectangle bounds{ 0, 0, getWidth(), getHeight() };
		popupSelector_->setPosition(getLocalPoint(source, position), bounds);
		popupSelector_->setVisible(true);
	}

	void MainInterface::dualPopupSelector(BaseComponent *source, juce::Point<int> position, int width,
		const PopupItems &options, std::function<void(int)> callback)
	{
		dualPopupSelector_->setCallback(std::move(callback));
		dualPopupSelector_->showSelections(options);
		juce::Rectangle bounds{ 0, 0, getWidth(), getHeight() };
		dualPopupSelector_->setPosition(getLocalPoint(source, position), width, bounds);
		dualPopupSelector_->setVisible(true);
	}

	void MainInterface::popupDisplay(BaseComponent *source, juce::String text,
		BubblePlacement placement, bool primary, Skin::SectionOverride sectionOverride)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		display->setContent(std::move(text), getLocalArea(source, source->getLocalBounds()), placement, sectionOverride);
		display->setVisible(true);
	}

	void MainInterface::hideDisplay(bool primary)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		if (display)
			display->setVisible(false);
	}
}
