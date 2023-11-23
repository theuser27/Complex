/*
	==============================================================================

		BaseSection.cpp
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#include "BaseSection.h"

#include "MainInterface.h"

namespace Interface
{
	BaseSection::BaseSection(std::string_view name) : Component(utils::toJuceString(name))
	{
		setWantsKeyboardFocus(true);
	}

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

	void BaseSection::paintBackground(Graphics &g)
	{
		paintKnobShadows(g);
		paintOpenGlChildrenBackgrounds(g);
	}

	void BaseSection::repaintBackground()
	{
		if (!background_)
			createBackground();

		background_->setComponentToRedraw(this);
		background_->redrawImage();
	}

	void BaseSection::paintOpenGlBackground(Graphics &g, OpenGlComponent *openGlComponent)
	{
		Graphics::ScopedSaveState s(g);

		Rectangle<int> bounds = getLocalArea(openGlComponent, openGlComponent->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());
		openGlComponent->paintBackground(g);
	}

	void BaseSection::repaintOpenGlBackground(OpenGlComponent *openGlComponent)
	{
		if (!background_)
			createBackground();

		background_->setComponentToRedraw(openGlComponent);
		background_->redrawImage(false);
	}

	void BaseSection::paintOpenGlChildrenBackgrounds(Graphics &g)
	{
		for (auto &openGlComponent : openGlComponents_)
			if (openGlComponent->isVisible())
				paintOpenGlBackground(g, openGlComponent.get());
	}

	void BaseSection::showPopupSelector(const Component *source, Point<int> position, 
		PopupItems options, std::function<void(int)> callback, std::function<void()> cancel) const
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->popupSelector(source, position, std::move(options), 
				std::move(callback), std::move(cancel));
	}

	void BaseSection::showPopupDisplay(Component *source, String text,
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

	void BaseSection::setScaling(float scale)
	{
		scaling_ = scale;

		for (auto &subSection : subSections_)
			subSection->setScaling(scale);
	}

	void BaseSection::reset()
	{
		for (auto &subSection : subSections_)
			subSection->reset();
	}

	void BaseSection::paintKnobShadows(Graphics &g)
	{
		for (auto &control : controls_)
			if (auto slider = dynamic_cast<BaseSlider *>(control.second); 
				slider && slider->isVisible() && slider->getWidth() && slider->getHeight())
				slider->drawShadow(g);
	}

	void BaseSection::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		if (background_)
		{
			background_.doWorkOnComponent(openGl, animate);
			COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
		}
		
		for (auto &subSection : subSections_)
			if (subSection->isVisible() && !subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(openGl, animate);

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && !openGlComponent->isAlwaysOnTop())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &subSection : subSections_)
			if (subSection->isVisible() && subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(openGl, animate);

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && openGlComponent->isAlwaysOnTop())
			{
				openGlComponent.doWorkOnComponent(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}
	}

	void BaseSection::destroyAllOpenGlComponents()
	{
		if (background_)
			background_.deinitialise();

		for (auto &openGlComponent : openGlComponents_)
			openGlComponent.deinitialise();

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
		section->setParent(this);
		section->setRenderer(renderer_);

		if (show)
			addAndMakeVisible(section);

		subSections_.push_back(section);
	}

	void BaseSection::removeSubSection(BaseSection *section)
	{
		auto location = std::ranges::find(subSections_.begin(), subSections_.end(), section);
		if (location != subSections_.end())
			subSections_.erase(location);

		removeChildComponent(section);
	}

	void BaseSection::addControl(BaseControl *control)
	{
		// juce String is a copy-on-write type so the internal data is constant across all instances of the same string
		// this means that so long as the control's name isn't changed the string_view will be safe to access 
		if (control->getParameterDetails().id.empty())
		{
			COMPLEX_ASSERT(!control->getName().isEmpty() && control->getName() != "" && "Every control must have a name");
			controls_[control->getName().toRawUTF8()] = control;
		}
		else
			controls_[Framework::Parameters::getEnumString(control->getParameterDetails().id)] = control;
		control->addListener(this);

		addAndMakeVisible(control);

		addOpenGlComponent(control->getLabelComponent());		
		for (const auto &component : control->getComponents())
			addOpenGlComponent(component);
	}

	void BaseSection::removeControl(BaseControl *control)
	{
		for (const auto &component : control->getComponents())
			removeOpenGlComponent(component.get());
		removeOpenGlComponent(control->getLabelComponent().get());

		removeChildComponent(control);

		control->removeListener(this);
		controls_.erase(Framework::Parameters::getEnumString(control->getParameterDetails().id));
	}

	void BaseSection::addOpenGlComponent(gl_ptr<OpenGlComponent> openGlComponent, bool toBeginning)
	{
		if (!openGlComponent)
			return;

		COMPLEX_ASSERT(std::ranges::find(openGlComponents_, openGlComponent) == openGlComponents_.end() 
			&& "We're adding a component that is already a child of this section");

		auto *rawPointer = openGlComponent.get();
		rawPointer->setParent(this);
		if (toBeginning)
			openGlComponents_.insert(openGlComponents_.begin(), std::move(openGlComponent));
		else
			openGlComponents_.emplace_back(std::move(openGlComponent));
		addAndMakeVisible(rawPointer);
	}

	void BaseSection::removeOpenGlComponent(OpenGlComponent *openGlComponent)
	{
		if (openGlComponent == nullptr)
			return;

		if (std::erase_if(openGlComponents_, [&](auto &&value) { return value.get() == openGlComponent; }))
		{
			removeChildComponent(openGlComponent);
			//openGlComponent->setParent(nullptr);
		}
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
		addOpenGlComponent(offOverlayQuad_, true);
		offOverlayQuad_->setVisible(false);
		offOverlayQuad_->setAlwaysOnTop(true);
		offOverlayQuad_->setInterceptsMouseClicks(false, false);
	}

	void BaseSection::createBackground()
	{
		background_ = makeOpenGlComponent<OpenGlBackground>();
		background_->setTargetComponent(this);
		background_->setParent(this);
		addAndMakeVisible(background_.get());
		background_->setBounds({ 0, 0, getWidth(), getHeight() });
	}

	float BaseSection::getDisplayScale() const
	{
		if (getWidth() <= 0)
			return 1.0f;

		Component *topLevelComponent = getTopLevelComponent();
		float globalWidth = (float)topLevelComponent->getLocalArea(this, getLocalBounds()).getWidth();
		float displayScale = (float)Desktop::getInstance().getDisplays()
			.getDisplayForRect(topLevelComponent->getScreenBounds())->scale;
		return displayScale * globalWidth / (float)getWidth();
	}

	Rectangle<int> BaseSection::getDividedAreaUnbuffered(Rectangle<int> full_area, int num_sections, int section, int buffer)
	{
		float component_width = (full_area.getWidth() - (num_sections + 1) * buffer) / (1.0f * num_sections);
		int x = full_area.getX() + std::round(section * (component_width + buffer) + buffer);
		int right = full_area.getX() + std::round((section + 1.0f) * (component_width + buffer));
		return Rectangle<int>(x, full_area.getY(), right - x, full_area.getHeight());
	}

	Rectangle<int> BaseSection::getDividedAreaBuffered(Rectangle<int> full_area, int num_sections, int section, int buffer)
	{
		Rectangle<int> area = getDividedAreaUnbuffered(full_area, num_sections, section, buffer);
		return area.expanded(buffer, 0);
	}

	Rectangle<int> BaseSection::getLabelBackgroundBounds(Rectangle<int> bounds, bool text_component)
	{
		int background_height = getValue(Skin::kLabelBackgroundHeight);
		int label_offset = text_component ? getValue(Skin::kTextComponentLabelOffset) : getValue(Skin::kLabelOffset);
		int background_y = bounds.getBottom() - background_height + label_offset;
		return Rectangle<int>(bounds.getX(), background_y, bounds.getWidth(), background_height);
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

		repaintBackground();
	}

	void BaseSection::updateAllValues()
	{
		for (auto &control : controls_)
			control.second->setValueFromParameter();

		for (auto &subSection : subSections_)
			subSection->updateAllValues();
	}

}
