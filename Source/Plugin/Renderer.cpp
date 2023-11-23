/*
	==============================================================================

		Renderer.cpp
		Created: 20 Dec 2022 7:34:54pm
		Author:  theuser27

	==============================================================================
*/

#include "Renderer.h"

#include "Framework/load_save.h"
#include "Interface/Sections/MainInterface.h"
#include "Interface/LookAndFeel/DefaultLookAndFeel.h"

namespace Interface
{
	Renderer::Renderer(Plugin::ComplexPlugin &plugin, AudioProcessorEditor &topLevelComponent) : 
		plugin_(plugin), topLevelComponent_(topLevelComponent) { }

	Renderer::~Renderer() = default;

	void Renderer::newOpenGLContextCreated()
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

		shaders_ = std::make_unique<Shaders>(openGl_.context);
		openGl_.shaders = shaders_.get();
	}

	void Renderer::renderOpenGL()
	{
		if (unsupported_)
			return;

		// this MUST be true otherwise we have data races between the GL and JUCE Message threads
		// if this fails then either:
		// 1. continuous repainting is on
		// 2. component painting is off, so there's no way MessageManager could be locked
		COMPLEX_ASSERT(MessageManager::existsAndIsLockedByCurrentThread());

		doCleanupWork();

		gui_->renderOpenGlComponents(openGl_, animate_);
	}

	void Renderer::openGLContextClosing()
	{
		gui_->destroyAllOpenGlComponents();
		doCleanupWork();

		openGl_.shaders = nullptr;
		shaders_ = nullptr;
	}

	void Renderer::startUI([[maybe_unused]] Component &dummyComponent)
	{
		skinInstance_ = std::make_unique<Skin>();
		skinInstance_->copyValuesToLookAndFeel(DefaultLookAndFeel::instance());

		gui_ = std::make_unique<MainInterface>(this);

		openGlContext_.setContinuousRepainting(false);
		openGlContext_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
		openGlContext_.setSwapInterval(1);
		openGlContext_.setRenderer(this);
		openGlContext_.setComponentPaintingEnabled(true);
		// attaching the context to an empty component so that we can activate it
		// and also take advantage of componentRendering to lock the message manager
		openGlContext_.attachTo(*gui_);

		startTimerHz(kParameterUpdateIntervalHz);
	}

	void Renderer::stopUI()
	{
		stopTimer();

		openGlContext_.detach();
		openGlContext_.setRenderer(nullptr);
		gui_ = nullptr;
	}

	void Renderer::updateFullGui()
	{
		if (gui_ == nullptr)
			return;

		gui_->updateAllValues();
		gui_->reset();

		topLevelComponent_.getAudioProcessor()->updateHostDisplay();
	}

	void Renderer::setGuiScale(double scale)
	{
		if (gui_ == nullptr)
			return;

		auto windowWidth = gui_->getWidth();
		auto windowHeight = gui_->getHeight();
		auto clampedScale = clampScaleFactorToFit(scale, windowWidth, windowHeight);

		Framework::LoadSave::saveWindowScale(scale);
		gui_->setScaling((float)scale);
		topLevelComponent_.setSize((int)std::round(windowWidth * clampedScale),
			(int)std::round(windowHeight * clampedScale));
	}

	double Renderer::clampScaleFactorToFit(double desiredScale, int windowWidth, int windowHeight) const
	{
		// the available display area on screen for the window - border thickness
		Rectangle<int> displayArea = Desktop::getInstance().getDisplays().getTotalBounds(true);
		if (auto *peer = gui_->getPeer())
			peer->getFrameSize().subtractFrom(displayArea);

		auto displayWidth = (double)displayArea.getWidth();
		auto displayHeight = (double)displayArea.getHeight();

		double scaledWindowWidth = desiredScale * windowWidth;
		double scaledWindowHeight = desiredScale * windowHeight;

		if (scaledWindowWidth > displayWidth)
		{
			desiredScale *= displayWidth / scaledWindowWidth;
			scaledWindowHeight = desiredScale * windowHeight;
		}

		if (scaledWindowHeight > displayHeight)
			desiredScale *= displayHeight / scaledWindowHeight;

		return desiredScale;
	}

	MainInterface *Renderer::getGui() noexcept { return gui_.get(); }
	Skin *Renderer::getSkin() noexcept { return skinInstance_.get(); }

}
