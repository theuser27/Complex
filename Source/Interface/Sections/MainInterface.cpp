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
		Skin default_skin;
		setSkinValues(default_skin, true);
		default_skin.copyValuesToLookAndFeel(DefaultLookAndFeel::instance());

		/*effects_interface_ = std::make_unique<EffectsInterface>(synth_data->mono_modulations);
		addSubSection(effects_interface_.get());
		effects_interface_->setVisible(false);
		effects_interface_->addListener(this);*/

		headerFooter_ = std::make_unique<HeaderFooterSections>(plugin.getSoundEngine());
		addSubSection(headerFooter_.get());

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

		//setAllValues(synth_data->controls);
		setOpaque(true);
		setSkinValues(default_skin, true);

		openGlContext_.setContinuousRepainting(true);
		openGlContext_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
		openGlContext_.setSwapInterval(0);
		openGlContext_.setRenderer(this);
		openGlContext_.setComponentPaintingEnabled(false);
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
		//g.fillAll(findColour(Skin::kBackground, true));
		//g.fillAll(Colours::white);
		paintChildrenShadows(g);

		/*int padding = getPadding();
		int bar_width = 6 * padding;
		g.setColour(headerFooter_->findColour(Skin::kBody, true));
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
		setSkinValues(skin, true);
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
		if (width < kMinWindowWidth || height < kMinWindowHeight)
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


		// TODO: lay out all the things here
		int left = 0;
		int top = 0;
		int width = std::ceil(getWidth() * displayScale_);
		int height = std::ceil(getHeight() * displayScale_);
		Rectangle bounds{ 0, 0, width, height };

		headerFooter_->setBounds(0, 0, width, height);


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
		openGl_.display_scale = displayScale_;
		lastRenderScale_ = displayScale_;

		background_.init(openGl_);
		initOpenGlComponents(openGl_);
	}

	void MainInterface::renderOpenGL()
	{
		if (unsupported_)
			return;

		float renderScale = openGl_.context.getRenderingScale();
		if (renderScale != lastRenderScale_)
		{
			lastRenderScale_ = renderScale;
			MessageManager::callAsync([=] { checkShouldReposition(true); });
		}

		ScopedLock lock(openGlCriticalSection_);
		openGl_.display_scale = displayScale_;
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
		Rectangle<int> bounds(0, 0, std::ceil(displayScale_ * getWidth()), std::ceil(displayScale_ * getHeight()));
		popupSelector_->setPosition(getLocalPoint(source, position), bounds);
		popupSelector_->setVisible(true);
	}

	void MainInterface::dualPopupSelector(Component *source, Point<int> position, int width,
		const PopupItems &options, std::function<void(int)> callback)
	{
		dualPopupSelector_->setCallback(std::move(callback));
		dualPopupSelector_->showSelections(options);
		Rectangle<int> bounds(0, 0, std::ceil(displayScale_ * getWidth()), std::ceil(displayScale_ * getHeight()));
		dualPopupSelector_->setPosition(getLocalPoint(source, position), width, bounds);
		dualPopupSelector_->setVisible(true);
	}

	void MainInterface::popupDisplay(Component *source, const std::string &text,
		BubbleComponent::BubblePlacement placement, bool primary)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		display->setContent(text, getLocalArea(source, source->getLocalBounds()), placement);
		display->setVisible(true);
	}

	void MainInterface::hideDisplay(bool primary)
	{
		PopupDisplay *display = primary ? popupDisplay1_.get() : popupDisplay2_.get();
		if (display)
			display->setVisible(false);
	}
}
