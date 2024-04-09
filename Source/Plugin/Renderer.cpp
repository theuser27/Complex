/*
	==============================================================================

		Renderer.cpp
		Created: 20 Dec 2022 7:34:54pm
		Author:  theuser27

	==============================================================================
*/

#include "Renderer.h"

#include "AppConfig.h"
#include <juce_opengl/juce_opengl.h>
#include "Framework/load_save.h"
#include "Framework/parameter_bridge.h"
#include "Plugin/Complex.h"
#include "Interface/Components/OpenGlComponent.h"
#include "Interface/Sections/MainInterface.h"
#include "Interface/LookAndFeel/DefaultLookAndFeel.h"

namespace Interface
{
	class Renderer::Pimpl final : public OpenGLRenderer, Timer
	{
	public:
		Pimpl(Renderer &renderer, Plugin::ComplexPlugin &plugin) : renderer_(renderer), plugin_(plugin) { }

		void newOpenGLContextCreated() override
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
		}

		void renderOpenGL() override
		{
			if (unsupported_)
				return;

			doCleanupWork();

			renderer_.gui_->renderOpenGlComponents(openGl_, animate_);
		}

		void openGLContextClosing() override
		{
			renderer_.gui_->destroyAllOpenGlComponents();
			doCleanupWork();

			openGl_.shaders = nullptr;
			shaders_ = nullptr;
		}

		void timerCallback() override
		{
			for (auto *bridge : plugin_.getParameterBridges())
				bridge->updateUIParameter();

			openGlContext_.triggerRepaint();
		}

		void startUI()
		{
			openGlContext_.setContinuousRepainting(false);
			openGlContext_.setOpenGLVersionRequired(OpenGLContext::openGL3_2);
			openGlContext_.setRenderer(this);
			openGlContext_.setComponentPaintingEnabled(false);
			// attaching the context to an empty component so that we can activate it
			// and also take advantage of componentRendering to lock the message manager
			openGlContext_.attachTo(*renderer_.gui_);

			startTimerHz(kParameterUpdateIntervalHz);
		}

		void stopUI()
		{
			stopTimer();

			openGlContext_.detach();
			openGlContext_.setRenderer(nullptr);
		}

		void pushOpenGlComponentToDelete(OpenGlComponent *openGlComponent) 
		{ openCleanupQueue_.emplace(openGlComponent); }

		void doCleanupWork()
		{
			while (!openCleanupQueue_.empty())
			{
				auto *openGlComponent = openCleanupQueue_.front();
				openGlComponent->destroy();
				delete openGlComponent;
				openCleanupQueue_.pop();
			}
		}

	private:
		bool unsupported_ = false;
		bool animate_ = true;

		Renderer &renderer_;
		Plugin::ComplexPlugin &plugin_;
		OpenGLContext openGlContext_;
		OpenGlWrapper openGl_{ openGlContext_ };
		std::unique_ptr<Shaders> shaders_;
		std::queue<OpenGlComponent *> openCleanupQueue_{};
	};

	Renderer::Renderer(Plugin::ComplexPlugin &plugin) : pimpl_(std::make_unique<Pimpl>(*this, plugin)), plugin_(plugin)
	{
		skinInstance_ = std::make_unique<Skin>();
		skinInstance_->copyValuesToLookAndFeel(DefaultLookAndFeel::instance());

		gui_ = std::make_unique<MainInterface>(this);
	}

	Renderer::~Renderer() noexcept
	{
		pimpl_.reset();
	}

	void Renderer::startUI()
	{
		pimpl_->startUI();
	}

	void Renderer::stopUI()
	{
		pimpl_->stopUI();

		topLevelComponent_ = nullptr;
		//gui_ = nullptr;
	}

	void Renderer::updateFullGui()
	{
		if (gui_ == nullptr)
			return;

		gui_->updateAllValues();
		gui_->reset();
	}

	void Renderer::setGuiScale(double scale)
	{
		if (gui_ == nullptr || topLevelComponent_ == nullptr)
			return;

		auto windowWidth = gui_->getWidth();
		auto windowHeight = gui_->getHeight();
		auto clampedScale = clampScaleFactorToFit(scale, windowWidth, windowHeight);

		Framework::LoadSave::saveWindowScale(scale);
		gui_->setScaling((float)scale);
		topLevelComponent_->setSize((int)std::round(windowWidth * clampedScale),
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
	
	void Renderer::pushOpenGlComponentToDelete(OpenGlComponent *openGlComponent)
	{ pimpl_->pushOpenGlComponentToDelete(openGlComponent); }
}
