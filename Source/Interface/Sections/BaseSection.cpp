/*
	==============================================================================

		BaseSection.cpp
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#include "Generation/BaseProcessor.h"
#include "../Components/OpenGlImageComponent.h"
#include "../Components/OpenGlMultiQuad.h"
#include "BaseSection.h"
#include "../Components/BaseButton.h"
#include "../Components/BaseSlider.h"
#include "Plugin/Renderer.h"
#include "MainInterface.h"

namespace Interface
{
	class OffOverlayQuad final : public OpenGlMultiQuad
	{
	public:
		OffOverlayQuad() : OpenGlMultiQuad(1, Shaders::kColorFragment, typeid(OffOverlayQuad).name())
		{
			setQuad(0, -1.0f, -1.0f, 2.0f, 2.0f);
		}
	};

	BaseSection::BaseSection(std::string_view name) : OpenGlContainer({ name.data(), name.size() })
	{
		setWantsKeyboardFocus(true);
	}

	BaseSection::~BaseSection() = default;

	void BaseSection::resized()
	{
		if (offOverlayQuad_)
		{
			offOverlayQuad_->setBounds(getLocalBounds());
			offOverlayQuad_->setColor(getColour(Skin::kBackground).withMultipliedAlpha(0.8f));
		}
		if (activator_)
			activator_->setBounds(getPowerButtonBounds());
		if (background_)
			background_->setBounds(getLocalBounds());
	}

	void BaseSection::repaintBackground()
	{
		if (!background_)
			createBackground();

		background_->setComponentToRedraw(this);
		background_->redrawImage();
	}

	void BaseSection::showPopupSelector(const BaseComponent *source, Point<int> position,
		PopupItems options, std::function<void(int)> callback, std::function<void()> cancel) const
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->popupSelector(source, position, std::move(options), getSectionOverride(),
				std::move(callback), std::move(cancel));
	}

	void BaseSection::showPopupDisplay(BaseComponent *source, String text,
		BubbleComponent::BubblePlacement placement, bool primary)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->popupDisplay(source, std::move(text), placement, primary, getSectionOverride());
	}

	void BaseSection::hidePopupDisplay(bool primary)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->hideDisplay(primary);
	}

	Path BaseSection::getRoundedPath(Rectangle<float> bounds, float topRounding, float bottomRounding) const
	{
		float topRoundness = (topRounding != 0.0f) ? topRounding : getValue(Skin::kBodyRoundingTop);
		float bottomRoundness = (bottomRounding != 0.0f) ? bottomRounding : getValue(Skin::kBodyRoundingBottom);

		auto x = bounds.getX();
		auto y = bounds.getY();
		auto width = bounds.getWidth();
		auto height = bounds.getHeight();

		Path path{};
		path.startNewSubPath(x + width - topRoundness, y);
		path.quadraticTo(x + width, y, x + width, y + topRoundness);
		path.lineTo(x + width, y + height - bottomRoundness);
		path.quadraticTo(x + width, y + height, x + width - bottomRoundness, y + height);
		path.lineTo(x + bottomRoundness, y + height);
		path.quadraticTo(x, y + height, x, y + height - bottomRoundness);
		path.lineTo(x, y + topRoundness);
		path.quadraticTo(x, y, x + topRoundness, y);
		path.closeSubPath();

		return path;
	}

	void BaseSection::paintBody(Graphics &g, Rectangle<int> bounds, float topRounding, float bottomRounding) const
	{
		auto path = getRoundedPath(bounds.toFloat(), topRounding, bottomRounding);
		g.setColour(getColour(Skin::kBody));
		g.fillPath(path);
	}

	void BaseSection::paintBorder(Graphics &g, Rectangle<int> bounds, float topRounding, float bottomRounding) const
	{
		auto path = getRoundedPath(bounds.toFloat().reduced(0.5f), topRounding, bottomRounding);
		g.setColour(getColour(Skin::kBorder));
		g.strokePath(path, PathStrokeType (1.0f));
	}

	void BaseSection::paintTabShadow(Graphics &g, Rectangle<int> bounds)
	{
		static constexpr float kCornerScale = 0.70710678119f;
		// TODO: redo to match the different rounding, hardcoded to topRounding for now
		float corner_size = getValue(Skin::kBodyRoundingTop);
		float shadow_size = getComponentShadowWidth();
		float corner_and_shadow = corner_size + shadow_size;

		float corner_shadow_offset = corner_size - corner_and_shadow * kCornerScale;
		float corner_ratio = corner_size * 1.0f / corner_and_shadow;

		Colour shadow_color = getColour(Skin::kShadow);
		Colour transparent = shadow_color.withAlpha(0.0f);

		float left = (float)bounds.getX();
		float top = (float)bounds.getY();
		float right = (float)bounds.getRight();
		float bottom = (float)bounds.getBottom();

		g.setGradientFill(ColourGradient(shadow_color, left, 0, transparent, left - shadow_size, 0, false));
		g.fillRect(left - shadow_size, top + corner_size, shadow_size, bottom - top - corner_size * 2);

		g.setGradientFill(ColourGradient(shadow_color, right, 0, transparent, right + shadow_size, 0, false));
		g.fillRect(right, top + corner_size, shadow_size, bottom - top - corner_size * 2);

		g.setGradientFill(ColourGradient(shadow_color, 0, top, transparent, 0, top - shadow_size, false));
		g.fillRect(left + corner_size, top - shadow_size, right - left - corner_size * 2, shadow_size);

		g.setGradientFill(ColourGradient(shadow_color, 0, bottom, transparent, 0, bottom + shadow_size, false));
		g.fillRect(left + corner_size, bottom, right - left - corner_size * 2, shadow_size);

		ColourGradient top_left_corner(shadow_color, left + corner_size, top + corner_size,
			transparent, left + corner_shadow_offset, top + corner_shadow_offset, true);
		top_left_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(top_left_corner);
		g.fillRect(left - shadow_size, top - shadow_size, corner_and_shadow, corner_and_shadow);

		ColourGradient top_right_corner(shadow_color, right - corner_size, top + corner_size,
			transparent, right - corner_shadow_offset, top + corner_shadow_offset, true);
		top_right_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(top_right_corner);
		g.fillRect(right - corner_size, top - shadow_size, corner_and_shadow, corner_and_shadow);

		ColourGradient bottom_left_corner(shadow_color, left + corner_size, bottom - corner_size,
			transparent, left + corner_shadow_offset, bottom - corner_shadow_offset, true);
		bottom_left_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(bottom_left_corner);
		g.fillRect(left - shadow_size, bottom - corner_size, corner_and_shadow, corner_and_shadow);

		ColourGradient bottom_right_corner(shadow_color, right - corner_size, bottom - corner_size,
			transparent, right - corner_shadow_offset, bottom - corner_shadow_offset, true);
		bottom_right_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(bottom_right_corner);
		g.fillRect(right - corner_size, bottom - corner_size, corner_and_shadow, corner_and_shadow);
	}

	void BaseSection::reset()
	{
		for (auto &subSection : subSections_)
			subSection->reset();
	}

	void BaseSection::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::SpinNotify };
		
		if (background_)
		{
			background_.doWorkOnComponent(openGl, animate);
			COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
		}
		
		for (auto *subSection : subSections_)
			if (subSection->isVisibleSafe() && !subSection->isAlwaysOnTopSafe())
				subSection->renderOpenGlComponents(openGl, animate);

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisibleSafe() && !openGlComponent->isAlwaysOnTopSafe())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &[controlName, control] : controls_)
			if (control->isVisibleSafe() && !control->isAlwaysOnTopSafe())
				control->renderOpenGlComponents(openGl, animate);

		for (auto *subSection : subSections_)
			if (subSection->isVisibleSafe() && subSection->isAlwaysOnTopSafe())
				subSection->renderOpenGlComponents(openGl, animate);

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisibleSafe() && openGlComponent->isAlwaysOnTopSafe())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &[controlName, control] : controls_)
			if (control->isVisibleSafe() && control->isAlwaysOnTopSafe())
				control->renderOpenGlComponents(openGl, animate);
	}

	void BaseSection::destroyAllOpenGlComponents()
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
		if (background_)
			background_.deinitialise();

		for (auto &openGlComponent : openGlComponents_)
			openGlComponent.deinitialise();

		for (auto &[controlName, control] : controls_)
			control->destroyAllOpenGlComponents();

		for (auto &subSection : subSections_)
			subSection->destroyAllOpenGlComponents();
	}

	void BaseSection::guiChanged(BaseButton *button)
	{
		if (button == activator_)
			setActive(activator_->getToggleState());
	}

	void BaseSection::resizeForText(TextSelector *textSelector, int requestedWidthChange)
	{
		auto currentBounds = textSelector->getBounds();
		textSelector->setBounds(currentBounds.withWidth(currentBounds.getWidth() + requestedWidthChange));
	}

	void BaseSection::addSubSection(BaseSection *section, bool show)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

		section->setParentSafe(this);
		section->setRenderer(renderer_);

		if (show)
			addAndMakeVisible(section);

		subSections_.push_back(section);
	}

	void BaseSection::removeSubSection(BaseSection *section, bool removeChild)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
		auto location = std::ranges::find(subSections_.begin(), subSections_.end(), section);
		if (location != subSections_.end())
			subSections_.erase(location);

		if (removeChild)
			removeChildComponent(section);
	}

	void BaseSection::addControl(BaseControl *control)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
		// juce String is a copy-on-write type so the internal data is constant across all instances of the same string
		// this means that so long as the control's name isn't changed the string_view will be safe to access 
		if (control->getParameterDetails().pluginName.empty())
		{
			COMPLEX_ASSERT(!control->getName().isEmpty() && control->getName() != "" && "Every control must have a name");
			controls_[control->getName().toRawUTF8()] = control;
		}
		else
			controls_[control->getParameterDetails().pluginName] = control;
		
		control->setParentSafe(this);
		control->addListener(this);
		control->setSkinOverride(skinOverride_);
		control->setRenderer(renderer_);
		control->setScaling(scaling_);

		addAndMakeVisible(control);
	}

	void BaseSection::removeControl(BaseControl *control, bool removeChild)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
		removeChildComponent(control);
		if (removeChild)
			control->setParentSafe(nullptr);
		control->removeListener(this);

		if (control->getParameterDetails().pluginName.empty())
			controls_.erase(control->getName().toRawUTF8());
		else
			controls_.erase(control->getParameterDetails().pluginName);
	}

	void BaseSection::setSkinOverride(Skin::SectionOverride skinOverride) noexcept
	{
		skinOverride_ = skinOverride;
		for (auto &control : controls_)
			control.second->setSkinOverride(skinOverride);
	}

	void BaseSection::setRenderer(Renderer *renderer) noexcept
	{
		renderer_ = renderer;
		for (auto &subSection : subSections_)
			subSection->setRenderer(renderer_);

		for (auto &control : controls_)
			control.second->setRenderer(renderer);
	}

	void BaseSection::setScaling(float scale) noexcept
	{
		scaling_ = scale;

		for (auto &subSection : subSections_)
			subSection->setScaling(scale);

		for (auto &control : controls_)
			control.second->setScaling(scale);
	}

	void BaseSection::setActivator(PowerButton *activator)
	{
		createOffOverlay();

		activator_ = activator;
		activator->addListener(this);
		setActive(activator->getToggleState());
	}

	void BaseSection::createOffOverlay()
	{
		if (offOverlayQuad_)
			return;

		offOverlayQuad_ = makeOpenGlComponent<OffOverlayQuad>();
		addOpenGlComponent(offOverlayQuad_);
		offOverlayQuad_->setVisible(false);
		offOverlayQuad_->setAlwaysOnTop(true);
		offOverlayQuad_->setInterceptsMouseClicks(false, false);
	}

	void BaseSection::createBackground()
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

		background_ = makeOpenGlComponent<OpenGlBackground>();
		background_->setTargetComponent(this);
		background_->setContainer(this);
		background_->setParentSafe(this);
		addAndMakeVisible(background_.get());
		background_->setBounds({ 0, 0, getWidth(), getHeight() });
	}

	void BaseSection::setActive(bool active)
	{
		if (active_ == active)
			return;

		if (offOverlayQuad_)
			offOverlayQuad_->setVisible(!active);

		active_ = active;
		for (auto &control : controls_)
			if (auto *slider = dynamic_cast<BaseSlider *>(control.second))
				slider->setActive(active);

		if (background_)
			repaintBackground();
	}

	void BaseSection::updateAllValues()
	{
		for (auto &control : controls_)
			control.second->setValueFromParameter();

		for (auto &subSection : subSections_)
			subSection->updateAllValues();
	}

	std::optional<u64> ProcessorSection::getProcessorId() const noexcept
	{
		return (processor_) ?
			processor_->getProcessorId() : std::optional<u64>{ std::nullopt };
	}

}
