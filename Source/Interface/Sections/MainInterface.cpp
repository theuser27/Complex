/*
	==============================================================================

		MainInterface.cpp
		Created: 10 Oct 2022 6:02:52pm
		Author:  theuser27

	==============================================================================
*/

#include "MainInterface.h"
#include "../LookAndFeel/DefaultLookAndFeel.h"

namespace Interface
{
	MainInterface::MainInterface(GuiData *guiData) : BaseSection("full_interface")
	{
		Skin default_skin;
		setSkinValues(default_skin, true);
		default_skin.copyValuesToLookAndFeel(DefaultLookAndFeel::instance());

		/*effects_interface_ = std::make_unique<EffectsInterface>(synth_data->mono_modulations);
		addSubSection(effects_interface_.get());
		effects_interface_->setVisible(false);
		effects_interface_->addListener(this);*/


		header_ = std::make_unique<HeaderSection>();
		addSubSection(header_.get());
		header_->addListener(this);

		popup_selector_ = std::make_unique<SinglePopupSelector>();
		addSubSection(popup_selector_.get());
		popup_selector_->setVisible(false);
		popup_selector_->setAlwaysOnTop(true);
		popup_selector_->setWantsKeyboardFocus(true);

		dual_popup_selector_ = std::make_unique<DualPopupSelector>();
		addSubSection(dual_popup_selector_.get());
		dual_popup_selector_->setVisible(false);
		dual_popup_selector_->setAlwaysOnTop(true);
		dual_popup_selector_->setWantsKeyboardFocus(true);

		popup_display_1_ = std::make_unique<PopupDisplay>();
		addSubSection(popup_display_1_.get());
		popup_display_1_->setVisible(false);
		popup_display_1_->setAlwaysOnTop(true);
		popup_display_1_->setWantsKeyboardFocus(false);

		popup_display_2_ = std::make_unique<PopupDisplay>();
		addSubSection(popup_display_2_.get());
		popup_display_2_->setVisible(false);
		popup_display_2_->setAlwaysOnTop(true);
		popup_display_2_->setWantsKeyboardFocus(false);

		popup_selector_->toFront(true);
		dual_popup_selector_->toFront(true);
		popup_display_1_->toFront(true);
		popup_display_2_->toFront(true);

		//setAllValues(synth_data->controls);
		setOpaque(true);
		setSkinValues(default_skin, true);

		open_gl_context_.setContinuousRepainting(true);
		open_gl_context_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
		open_gl_context_.setSwapInterval(0);
		open_gl_context_.setRenderer(this);
		open_gl_context_.setComponentPaintingEnabled(false);
		open_gl_context_.attachTo(*this);
	}

	MainInterface::MainInterface() : BaseSection("EMPTY")
	{
		Skin default_skin;
		setSkinValues(default_skin, true);

		open_gl_context_.setContinuousRepainting(true);
		open_gl_context_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
		open_gl_context_.setSwapInterval(0);
		open_gl_context_.setRenderer(this);
		open_gl_context_.setComponentPaintingEnabled(false);
		open_gl_context_.attachTo(*this);

		reset();
		setOpaque(true);
	}

	MainInterface::~MainInterface()
	{
		open_gl_context_.detach();
		open_gl_context_.setRenderer(nullptr);
	}

	void MainInterface::paintBackground(Graphics &g)
	{
		g.fillAll(findColour(Skin::kBackground, true));
		paintChildrenShadows(g);

		if (effects_interface_ == nullptr)
			return;

		int padding = getPadding();
		int bar_width = 6 * padding;
		g.setColour(header_->findColour(Skin::kBody, true));
		int y = header_->getBottom();
		int height = keyboard_interface_->getY() - y;
		int x1 = extra_mod_section_->getRight() + padding;
		g.fillRect(x1, y, bar_width, height);

		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
	}

	void MainInterface::copySkinValues(const Skin &skin)
	{
		ScopedLock open_gl_lock(open_gl_critical_section_);
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
		if (!background_image_.isValid() || setting_all_values_)
			return;

		/*if (effects_interface_ != nullptr && effects_interface_->isParentOf(child))
			child = effects_interface_.get();*/

		background_.lock();
		Graphics g(background_image_);
		paintChildBackground(g, child);
		background_.updateBackgroundImage(background_image_);
		background_.unlock();
	}

	void MainInterface::repaintOpenGlBackground(OpenGlComponent *component)
	{
		if (!background_image_.isValid())
			return;

		background_.lock();
		Graphics g(background_image_);
		paintOpenGlBackground(g, component);
		background_.updateBackgroundImage(background_image_);
		background_.unlock();
	}

	void MainInterface::redoBackground()
	{
		int width = std::ceil(display_scale_ * getWidth());
		int height = std::ceil(display_scale_ * getHeight());
		if (width < vital::kMinWindowWidth || height < vital::kMinWindowHeight)
			return;

		ScopedLock open_gl_lock(open_gl_critical_section_);

		background_.lock();
		background_image_ = Image(Image::RGB, width, height, true);
		Graphics g(background_image_);
		paintBackground(g);
		background_.updateBackgroundImage(background_image_);
		background_.unlock();
	}

	void MainInterface::checkShouldReposition(bool resize)
	{
		float old_scale = display_scale_;
		int old_pixel_multiple = pixel_multiple_;
		display_scale_ = getDisplayScale();
		pixel_multiple_ = std::max<int>(1, display_scale_);

		if (resize && (old_scale != display_scale_ || old_pixel_multiple != pixel_multiple_))
			resized();
	}

	void MainInterface::resized()
	{
		checkShouldReposition(false);

		width_ = getWidth();
		if (!enable_redo_background_)
			return;

		resized_width_ = width_;

		ScopedLock lock(open_gl_critical_section_);
		static constexpr int kTopHeight = 48;

		if (effects_interface_ == nullptr)
			return;

		int left = 0;
		int top = 0;
		int width = std::ceil(getWidth() * display_scale_);
		int height = std::ceil(getHeight() * display_scale_);
		Rectangle<int> bounds(0, 0, width, height);

		float width_ratio = getWidth() / (1.0f * vital::kDefaultWindowWidth);
		float ratio = width_ratio * display_scale_;
		float height_ratio = getHeight() / (1.0f * vital::kDefaultWindowHeight);
		if (width_ratio > height_ratio + 1.0f / vital::kDefaultWindowHeight)
		{
			ratio = height_ratio;
			width = height_ratio * vital::kDefaultWindowWidth * display_scale_;
			left = (getWidth() - width) / 2;
		}
		if (height_ratio > width_ratio + 1.0f / vital::kDefaultWindowHeight)
		{
			ratio = width_ratio;
			height = ratio * vital::kDefaultWindowHeight * display_scale_;
			top = (getHeight() - height) / 2;
		}

		setSizeRatio(ratio);

		int padding = getPadding();
		int voice_padding = findValue(Skin::kLargePadding);
		int extra_mod_width = findValue(Skin::kModulationButtonWidth);
		int main_x = left + extra_mod_width + 2 * voice_padding;
		int top_height = kTopHeight * ratio;

		int knob_section_height = getKnobSectionHeight();
		int keyboard_section_height = knob_section_height * 0.7f;
		int voice_height = height - top_height - keyboard_section_height;

		int section_one_width = 350 * ratio;
		int section_two_width = section_one_width;
		int audio_width = section_one_width + section_two_width + padding;
		int modulation_width = width - audio_width - extra_mod_width - 4 * voice_padding;

		header_->setTabOffset(extra_mod_width + 2 * voice_padding);
		header_->setBounds(left, top, width, top_height);
		Rectangle<int> main_bounds(main_x, top + top_height, audio_width, voice_height);

		if (getWidth() && getHeight())
			redoBackground();
	}

	void MainInterface::createModulationSliders(const vital::output_map &mono_modulations,
		const vital::output_map &poly_modulations)
	{
		std::map<std::string, SynthSlider *> all_sliders = getAllSliders();
		std::map<std::string, SynthSlider *> modulatable_sliders;

		for (auto &destination : mono_modulations)
		{
			if (all_sliders.count(destination.first))
				modulatable_sliders[destination.first] = all_sliders[destination.first];
		}

		modulation_manager_ = std::make_unique<ModulationManager>(getAllModulationButtons(),
			modulatable_sliders, mono_modulations, poly_modulations);
		modulation_manager_->setOpaque(false);
		addSubSection(modulation_manager_.get());
	}

	void MainInterface::animate(bool animate)
	{
		if (animate_ != animate)
			open_gl_context_.setContinuousRepainting(animate);

		animate_ = animate;
		BaseSection::animate(animate);
	}

	void MainInterface::reset()
	{
		ScopedLock lock(open_gl_critical_section_);

		setting_all_values_ = true;
		BaseSection::reset();
		modulationChanged();
		/*if (effects_interface_ && effects_interface_->isVisible())
			effects_interface_->redoBackgroundImage();*/

		setting_all_values_ = false;
	}

	void MainInterface::setAllValues(vital::control_map &controls)
	{
		ScopedLock lock(open_gl_critical_section_);
		setting_all_values_ = true;
		BaseSection::setAllValues(controls);
		setting_all_values_ = false;
	}

	void MainInterface::newOpenGLContextCreated()
	{
		double version_supported = OpenGLShaderProgram::getLanguageVersion();
		unsupported_ = version_supported < kMinOpenGlVersion;
		if (unsupported_)
		{
			NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Unsupported OpenGl Version",
				String("Vial requires OpenGL version: ") + String(kMinOpenGlVersion) +
				String("\nSupported version: ") + String(version_supported));
			return;
		}

		shaders_ = std::make_unique<Shaders>(open_gl_context_);
		open_gl_.shaders = shaders_.get();
		open_gl_.display_scale = display_scale_;
		last_render_scale_ = display_scale_;

		background_.init(open_gl_);
		initOpenGlComponents(open_gl_);
	}

	void MainInterface::renderOpenGL()
	{
		if (unsupported_)
			return;

		float render_scale = open_gl_.context.getRenderingScale();
		if (render_scale != last_render_scale_)
		{
			last_render_scale_ = render_scale;
			MessageManager::callAsync([=] { checkShouldReposition(true); });
		}

		ScopedLock lock(open_gl_critical_section_);
		open_gl_.display_scale = display_scale_;
		background_.render(open_gl_);
		renderOpenGlComponents(open_gl_, animate_);
	}

	void MainInterface::openGLContextClosing()
	{
		if (unsupported_)
			return;

		background_.destroy(open_gl_);
		destroyOpenGlComponents(open_gl_);
		open_gl_.shaders = nullptr;
		shaders_ = nullptr;
	}

	void MainInterface::notifyChange()
	{
		if (header_)
			header_->notifyChange();
	}

	void MainInterface::notifyFresh()
	{
		if (header_)
			header_->notifyFresh();
	}

	void MainInterface::popupSelector(Component *source, Point<int> position, const PopupItems &options,
		std::function<void(int)> callback, std::function<void()> cancel)
	{
		popup_selector_->setCallback(callback);
		popup_selector_->setCancelCallback(cancel);
		popup_selector_->showSelections(options);
		Rectangle<int> bounds(0, 0, std::ceil(display_scale_ * getWidth()), std::ceil(display_scale_ * getHeight()));
		popup_selector_->setPosition(getLocalPoint(source, position), bounds);
		popup_selector_->setVisible(true);
	}

	void MainInterface::dualPopupSelector(Component *source, Point<int> position, int width,
		const PopupItems &options, std::function<void(int)> callback)
	{
		dual_popup_selector_->setCallback(callback);
		dual_popup_selector_->showSelections(options);
		Rectangle<int> bounds(0, 0, std::ceil(display_scale_ * getWidth()), std::ceil(display_scale_ * getHeight()));
		dual_popup_selector_->setPosition(getLocalPoint(source, position), width, bounds);
		dual_popup_selector_->setVisible(true);
	}

	void MainInterface::popupDisplay(Component *source, const std::string &text,
		BubbleComponent::BubblePlacement placement, bool primary)
	{
		PopupDisplay *display = primary ? popup_display_1_.get() : popup_display_2_.get();
		display->setContent(text, getLocalArea(source, source->getLocalBounds()), placement);
		display->setVisible(true);
	}

	void MainInterface::hideDisplay(bool primary)
	{
		PopupDisplay *display = primary ? popup_display_1_.get() : popup_display_2_.get();
		if (display)
			display->setVisible(false);
	}
}
