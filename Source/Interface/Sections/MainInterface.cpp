/*
	==============================================================================

		MainInterface.cpp
		Created: 10 Oct 2022 6:02:52pm
		Author:  theuser27

	==============================================================================
*/

#include "MainInterface.h"

#include "../LookAndFeel/DefaultLookAndFeel.h"
#include "Popups.h"

namespace Interface
{
	MainInterface::MainInterface(Plugin::ComplexPlugin &plugin) :
		BaseSection(typeid(MainInterface).name()), plugin_(plugin)
	{
		skinInstance_ = std::make_unique<Skin>();
		skinInstance_->copyValuesToLookAndFeel(DefaultLookAndFeel::instance());
		skin_ = skinInstance_.get();

		headerFooter_ = std::make_unique<HeaderFooterSections>(plugin.getSoundEngine());
		addSubSection(headerFooter_.get());

		effectsStateSection_ = std::make_unique<EffectsStateSection>(plugin.getSoundEngine().getEffectsState());
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

		setOpaque(true);
		setSkin(skin_);

		openGlContext_.setContinuousRepainting(false);
		openGlContext_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
		openGlContext_.setSwapInterval(1);
		openGlContext_.setRenderer(this);
		openGlContext_.setComponentPaintingEnabled(true);
		openGlContext_.attachTo(*this);

		startTimerHz(kParameterUpdateIntervalHz);
	}

	MainInterface::~MainInterface()
	{
		stopTimer();
		openGlContext_.detach();
		openGlContext_.setRenderer(nullptr);
	}

	void MainInterface::paintBackground(Graphics &g)
	{
		g.fillAll(getColour(Skin::kBackground));
		paintChildrenShadows(g);

		/*int padding = getPadding();
		int bar_width = 6 * padding;
		g.setColour(headerFooter_->getColour(Skin::kBody));
		int y = header_->getBottom();
		int height = keyboard_interface_->getY() - y;
		int x1 = extra_mod_section_->getRight() + padding;
		g.fillRect(x1, y, bar_width, height);*/

		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
	}

	void MainInterface::copySkinValues(const Skin &skin)
	{
		ScopedLock open_gl_lock(openGlCriticalSection_);
		skin.copyValuesToLookAndFeel(DefaultLookAndFeel::instance());
	}

	void MainInterface::reloadSkin(const Skin &skin)
	{
		copySkinValues(skin);
		Rectangle<int> bounds = getBounds();
		// triggering repainting like a boss
		setBounds(0, 0, bounds.getWidth() / 4, bounds.getHeight() / 4);
		setBounds(bounds);
	}

	void MainInterface::repaintChildBackground(BaseSection *child)
	{
		if (!backgroundImage_.isValid() || setting_all_values_)
			return;

		/*if (effects_interface_ != nullptr && effects_interface_->isParentOf(child))
			child = effects_interface_.get();*/

		background_.lock();
		Graphics g(backgroundImage_);
		paintChildBackground(g, child);
		background_.updateBackgroundImage(backgroundImage_);
		background_.unlock();
	}

	void MainInterface::repaintOpenGlBackground(OpenGlComponent *component)
	{
		if (!backgroundImage_.isValid())
			return;

		background_.lock();
		Graphics g(backgroundImage_);
		paintOpenGlBackground(g, component);
		background_.updateBackgroundImage(backgroundImage_);
		background_.unlock();
	}

	void MainInterface::redoBackground()
	{
		int width = std::ceil(displayScale_ * getWidth());
		int height = std::ceil(displayScale_ * getHeight());
		if (width < kMinWidth || height < kMinHeight)
			return;

		ScopedLock open_gl_lock(openGlCriticalSection_);

		background_.lock();
		backgroundImage_ = Image(Image::RGB, width, height, true);
		Graphics g(backgroundImage_);
		paintBackground(g);
		background_.updateBackgroundImage(backgroundImage_);
		background_.unlock();
	}

	void MainInterface::checkShouldReposition(bool resize)
	{
		float old_scale = displayScale_;
		displayScale_ = getDisplayScale();

		if (resize && (old_scale != displayScale_))
			resized();
	}

	void MainInterface::resized()
	{
		checkShouldReposition(false);

		if (!enableRedoBackground_)
			return;

		ScopedLock lock(openGlCriticalSection_);

		int width = getWidth();
		int height = getHeight();

		headerFooter_->setBounds(0, 0, width, height);

		int effectsStateXStart = (int)std::round(scaleValue(kHorizontalWindowEdgeMargin));
		int effectsStateYStart = (int)std::round(scaleValue(HeaderFooterSections::kHeaderHeight +
			kMainVisualiserHeight + kVerticalGlobalMargin));
		int effectsStateWidth = (int)std::round(scaleValue(EffectsStateSection::kMinWidth));
		int effectsStateHeight = getHeight() - effectsStateYStart - 
			(int)std::round(scaleValue(kLaneToBottomSettingsMargin + HeaderFooterSections::kFooterHeight));
		Rectangle bounds{ effectsStateXStart, effectsStateYStart, effectsStateWidth, effectsStateHeight };

		effectsStateSection_->setBounds(bounds);

		if (getWidth() && getHeight())
			redoBackground();
	}

	void MainInterface::reset()
	{
		ScopedLock lock(openGlCriticalSection_);

		setting_all_values_ = true;
		BaseSection::reset();
		//modulationChanged();
		/*if (effects_interface_ && effects_interface_->isVisible())
			effects_interface_->redoBackgroundImage();*/

		setting_all_values_ = false;
	}

	void MainInterface::updateAllValues()
	{
		ScopedLock lock(openGlCriticalSection_);
		setting_all_values_ = true;
		BaseSection::updateAllValues();
		setting_all_values_ = false;
	}

	void MainInterface::newOpenGLContextCreated()
	{
		double versionSupported = OpenGLShaderProgram::getLanguageVersion();
		unsupported_ = versionSupported < kMinOpenGlVersion;
		if (unsupported_)
		{
			NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Unsupported OpenGl Version",
				String("Complex requires OpenGL version: ") + String(kMinOpenGlVersion) +
				String("\nSupported version: ") + String(versionSupported));
			return;
		}

		shaders_ = std::make_unique<Shaders>(openGlContext_);
		openGl_.shaders = shaders_.get();
		openGl_.displayScale = displayScale_;
		lastRenderScale_ = displayScale_;

		background_.init(openGl_);
		initOpenGlComponents(openGl_);
	}

	void MainInterface::renderOpenGL()
	{
		if (unsupported_)
			return;

		// this HAS to be true otherwise we have data races between the GL and JUCE Message threads
		// if this fails then either:
		// 1. continuous repainting is on and triggerRepaint wasn't called
		// 2. component painting is off, so there's no way MessageManager could be locked
		// NB: the paint call inside the OpenGlContext renderFrame needs to be commented out
		//     otherwise it will call the paint methods the entire component tree
		COMPLEX_ASSERT(MessageManager::existsAndIsLockedByCurrentThread());

		float renderScale = (float)openGl_.context.getRenderingScale();
		if (renderScale != lastRenderScale_)
		{
			lastRenderScale_ = renderScale;
			MessageManager::callAsync([this]() { checkShouldReposition(); });
		}

		ScopedLock lock(openGlCriticalSection_);
		openGl_.displayScale = displayScale_;
		background_.render(openGl_);
		renderOpenGlComponents(openGl_, animate_);
	}

	void MainInterface::openGLContextClosing()
	{
		if (unsupported_)
			return;

		background_.destroy(openGl_);
		destroyOpenGlComponents(openGl_);
		openGl_.shaders = nullptr;
		shaders_ = nullptr;
	}

	void MainInterface::popupSelector(Component *source, Point<int> position, const PopupItems &options,
		std::function<void(int)> callback, std::function<void()> cancel)
	{
		popupSelector_->setCallback(std::move(callback));
		popupSelector_->setCancelCallback(std::move(cancel));
		popupSelector_->showSelections(options);
		Rectangle bounds{ 0, 0, getWidth(), getHeight() };
		popupSelector_->setPosition(getLocalPoint(source, position), bounds);
		popupSelector_->setVisible(true);
	}

	void MainInterface::dualPopupSelector(Component *source, Point<int> position, int width,
		const PopupItems &options, std::function<void(int)> callback)
	{
		dualPopupSelector_->setCallback(std::move(callback));
		dualPopupSelector_->showSelections(options);
		Rectangle bounds{ 0, 0, getWidth(), getHeight() };
		dualPopupSelector_->setPosition(getLocalPoint(source, position), width, bounds);
		dualPopupSelector_->setVisible(true);
	}

	void MainInterface::popupDisplay(Component *source, const std::string &text,
		BubbleComponent::BubblePlacement placement, bool primary, Skin::SectionOverride sectionOverride)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		display->setContent(text, getLocalArea(source, source->getLocalBounds()), placement, sectionOverride);
		display->setVisible(true);
	}

	void MainInterface::hideDisplay(bool primary)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		if (display)
			display->setVisible(false);
	}
}
